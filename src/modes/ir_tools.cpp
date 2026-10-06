// IR Tools Mode - hardware lifecycle, capture, replay, NEC send
#include "ir_tools.h"
#include "../core/config.h"
#include "../core/heap_gates.h"
#include "../core/heap_health.h"
#include "../core/sd_layout.h"
#include <SD.h>
#include <IRutils.h>

bool IrTools::running = false;
IrTools::Status IrTools::status = IrTools::Status::Ok;
IRrecv* IrTools::irrecv = nullptr;
IRsend* IrTools::irsend = nullptr;
bool IrTools::rxActive = false;
uint16_t IrTools::nextFileIndex = 1;
uint16_t IrTools::savedCount = 0;
IrTools::Capture IrTools::last;
bool IrTools::haveCapture = false;
bool IrTools::pendingSave = false;
char IrTools::sessionList[IR_SESSION_LIST_MAX][24];
uint8_t IrTools::sessionListLen = 0;
uint32_t IrTools::pendingReplayMask = 0;
uint32_t IrTools::pendingNecValue = 0;

void IrTools::init() {
    // Nothing at boot: the IRrecv buffer is heap-built in start() behind a
    // HeapGates check because the library restarts the device on OOM.
}

void IrTools::start() {
    if (running) return;
    running = true;
    haveCapture = false;
    pendingSave = false;
    sessionListLen = 0;
    savedCount = 0;

    // HeapGates before the library's internal new (it ESP.Restart()s on OOM,
    // which bypasses every Porkchop heap rule).
    char gateErr[96];
    auto gate = HeapGates::checkGate(20000, 4096);
    if (!HeapGates::canMeet(gate, gateErr, sizeof(gateErr))) {
        Serial.printf("[IR] start refused by HeapGates: %s\n", gateErr);
        status = Status::HeapBlocked;
        return;
    }

    irrecv = new IRrecv(Config::ir().rxPin, IR_RAW_BUF_SIZE,
                        15 /*ms timeout*/, false /*save_buffer*/);
    irrecv->enableIRIn();
    rxActive = true;

    irsend = new IRsend(Config::ir().txPin);
    irsend->begin();

    scanExistingCaptures();
    status = Status::RadioAbsent;
    Serial.printf("[IR] IR tools start (rx=%u tx=%u buf=%u)\n",
                  Config::ir().rxPin, Config::ir().txPin, IR_RAW_BUF_SIZE);
}

void IrTools::stop() {
    // Idempotent: rapid exit-re-enter must not double-free or leak.
    if (irrecv) {
        if (rxActive) {
            irrecv->disableIRIn();
            rxActive = false;
        }
        delete irrecv;   // IRrecv dtor frees the capture buffer
        irrecv = nullptr;
    }
    if (irsend) {
        delete irsend;
        irsend = nullptr;
    }
    pendingReplayMask = 0;
    pendingSave = false;
    if (running) Serial.println("[IR] IR tools stop");
    running = false;
}

void IrTools::adoptDecode(const decode_results& r) {
    haveCapture = true;
    pendingSave = true;
    last.repeat = r.repeat;
    last.overflow = r.overflow;
    last.bits = r.bits;
    last.rawlen = r.rawlen;
    last.protocol = static_cast<uint16_t>(r.decode_type);
    last.stateful = hasACState(r.decode_type);

    // Union discipline: value/address/command are garbage for stateful AC
    // protocols, state[] is garbage for value protocols. Mirror the branch.
    if (last.stateful) {
        last.value = 0;
        last.address = 0;
        last.command = 0;
        last.stateLen = sizeof(last.state);
        memcpy(last.state, r.state, last.stateLen);
    } else {
        last.value = r.value;
        last.address = r.address;
        last.command = r.command;
        last.stateLen = 0;
    }

    // Protocol name: IRutils returns Arduino String. The String rule allows
    // one-shot conversions, not per-tick churn: this runs once per decode
    // EVENT (not per update tick) and only when the protocol actually
    // changed, so a held remote key repeating the same protocol allocates
    // nothing after the first capture.
    static decode_type_t sLastNamed = decode_type_t::UNKNOWN;
    if (r.decode_type != sLastNamed) {
        String name = typeToString(r.decode_type);
        strncpy(last.protocolName, name.c_str(), sizeof(last.protocolName) - 1);
        last.protocolName[sizeof(last.protocolName) - 1] = '\0';
        sLastNamed = r.decode_type;
    }

    // Raw replay copy in microseconds (kRawTick=2 on ESP32), header entry
    // skipped per kStartOffset. Longer captures replay truncated.
    const uint16_t usable = (r.rawlen > 1) ? (r.rawlen - 1) : 0;
    last.rawCopyLen = (usable < IR_RAW_COPY_MAX) ? usable : IR_RAW_COPY_MAX;
    for (uint16_t i = 0; i < last.rawCopyLen; i++) {
        uint32_t us = (uint32_t)r.rawbuf[i + 1] * kRawTick;
        last.rawCopy[i] = (us > 0xFFFF) ? 0xFFFF : (uint16_t)us;
    }

    status = (r.decode_type == decode_type_t::UNKNOWN || r.overflow)
                 ? Status::Overflow
                 : Status::CaptureReady;
}

void IrTools::update() {
    if (!running || !irrecv) return;

    // One TX action per tick (bit-bang blocks 68ms NEC / 100-300ms raw);
    // polling resumes the tick after, so backtick-exit always lands.
    if (pendingReplayMask) {
        runScheduledTx();
        return;
    }

    decode_results r;
    if (irrecv->decode(&r)) {
        adoptDecode(r);
        savePending();
        irrecv->resume();
    } else if (pendingSave) {
        savePending();  // retry a deferred save (SD back / pressure dropped)
    }
}

void IrTools::savePending() {
    if (!pendingSave) return;

    if (!Config::isSDAvailable()) {
        status = Status::SdAbsent;  // capture held in RAM, visible state
        return;
    }
    if (HeapHealth::getPressureLevel() > HeapPressureLevel::Caution) {
        status = Status::PressureHeld;  // deferred, not dropped
        return;
    }

    char body[IR_JSON_BODY_MAX];
    captureJsonBody(body, sizeof(body));
    char path[64];
    captureFilename(path, sizeof(path), nextFileIndex);

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[IR] save failed: %s\n", path);
        status = Status::SdAbsent;
        return;
    }
    size_t n = strlen(body);
    bool ok = (f.write((const uint8_t*)body, n) == n);
    f.close();

    if (ok) {
        if (sessionListLen < IR_SESSION_LIST_MAX) {
            snprintf(sessionList[sessionListLen], sizeof(sessionList[0]),
                     "ir_%03u.json", (unsigned)nextFileIndex);
            sessionListLen++;
        }
        nextFileIndex++;
        savedCount++;
        pendingSave = false;
        status = Status::Saved;
        Serial.printf("[IR] saved %s (%u bytes)\n", path, (unsigned)n);
    } else {
        status = Status::SdAbsent;
    }
}

void IrTools::scanExistingCaptures() {
    nextFileIndex = 1;
    if (!Config::isSDAvailable()) return;
    const char* dir = SDLayout::irDir();
    if (!SD.exists(dir)) return;
    File d = SD.open(dir);
    if (!d || !d.isDirectory()) return;
    // One-shot boot-time scan of a small dir; not on the per-tick path.
    for (File ent = d.openNextFile(); ent; ent = d.openNextFile()) {
        const char* nm = ent.name();
        const char* base = strrchr(nm, '/');
        base = base ? base + 1 : nm;
        if (strncmp(base, "ir_", 3) == 0) {
            unsigned idx = 0;
            if (sscanf(base + 3, "%5u", &idx) == 1 && idx <= 999 &&
                (uint16_t)(idx + 1) > nextFileIndex) {
                nextFileIndex = (uint16_t)(idx + 1);
            }
        }
        ent.close();
    }
    d.close();
    Serial.printf("[IR] capture files continue at ir_%03u\n", (unsigned)nextFileIndex);
}

bool IrTools::replayLast() {
    if (!running || !haveCapture || !irsend) return false;
    if (pendingReplayMask) return false;
    pendingReplayMask |= 0x1;
    return true;
}

bool IrTools::sendNecValue(uint32_t value) {
    if (!running || !irsend) return false;
    if (pendingReplayMask) return false;
    pendingNecValue = value;
    pendingReplayMask |= 0x2;
    return true;
}

void IrTools::runScheduledTx() {
    if (!irsend) {
        pendingReplayMask = 0;
        return;
    }
    status = Status::Sending;
    if (pendingReplayMask & 0x1) {
        if (last.protocol == (uint16_t)decode_type_t::NEC && !last.stateful) {
            irsend->sendNEC(last.value, last.bits ? last.bits : 32);
        } else if (last.rawCopyLen >= 2) {
            // Blocking bit-bang: one frame per tick, next backtick poll lands
            // right after (NEC ~68ms, long raw 100-300ms).
            irsend->sendRaw(last.rawCopy, last.rawCopyLen, 38);
        }
    } else if (pendingReplayMask & 0x2) {
        irsend->sendNEC(pendingNecValue, 32);
    }
    pendingReplayMask = 0;
}

// ---- Pure helpers (mirrored in test/mocks/testable_functions.h; keep in sync) ----

static bool irParseHex32Impl(const char* s, uint32_t& out) {
    if (!s) return false;
    uint32_t v = 0;
    size_t n = 0;
    for (; s[n] != '\0'; n++) {
        if (n >= 8) return false;
        char c = s[n];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint32_t)(c - 'A' + 10);
        else return false;
    }
    if (n == 0) return false;
    out = v;
    return true;
}

static size_t irCaptureJsonImpl(char* out, size_t outLen, const char* protocolName,
                                bool stateful, uint64_t value, uint32_t address,
                                uint32_t command, uint16_t bits, uint16_t rawlen,
                                bool repeat, bool overflow, const uint8_t* state,
                                uint16_t stateLen) {
    size_t n = 0;
    #define IRJ_APPEND(...) do { \
        if (n >= outLen) break; \
        int w = snprintf(out + n, outLen - n, __VA_ARGS__); \
        if (w < 0) break; \
        n += (size_t)w; \
        if (n >= outLen) { n = outLen - 1; break; } \
    } while (0)

    IRJ_APPEND("{\"protocol\":\"%s\",\"bits\":%u,\"repeat\":%s,\"overflow\":%s",
               protocolName, (unsigned)bits, repeat ? "true" : "false",
               overflow ? "true" : "false");
    if (stateful) {
        IRJ_APPEND(",\"state\":[");
        for (uint16_t i = 0; i < stateLen; i++) {
            IRJ_APPEND(i ? ",\"%02X\"" : "\"%02X\"", state[i]);
        }
        IRJ_APPEND("]");
    } else {
        IRJ_APPEND(",\"value\":\"0x%016llX\",\"address\":\"0x%08X\",\"command\":\"0x%08X\"",
                   (unsigned long long)value, (unsigned)address, (unsigned)command);
    }
    IRJ_APPEND(",\"rawlen\":%u}", (unsigned)rawlen);

    #undef IRJ_APPEND
    return n;
}

static void irCaptureFilenameImpl(char* out, size_t outLen, const char* dir,
                                  uint16_t index) {
    snprintf(out, outLen, "%s/ir_%03u.json", dir, (unsigned)index);
}

size_t IrTools::captureJsonBody(char* out, size_t outLen) {
    return irCaptureJsonImpl(out, outLen, last.protocolName, last.stateful,
                             last.value, last.address, last.command,
                             last.bits, last.rawlen, last.repeat,
                             last.overflow, last.state, last.stateLen);
}

void IrTools::captureFilename(char* out, size_t outLen, uint16_t index) {
    irCaptureFilenameImpl(out, outLen, SDLayout::irDir(), index);
}

bool IrTools::parseHex32(const char* s, uint32_t& out) {
    return irParseHex32Impl(s, out);
}
