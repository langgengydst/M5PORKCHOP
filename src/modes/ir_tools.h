// IR Tools Mode - cap VS1838B capture, G44 replay, NEC send
#pragma once

#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>

// Capture buffer sizing: default kRawBuf=100 truncates AC-class remotes, so
// IR TOOLS allocates explicitly. 600 entries x 2B = ~1.2KB behind HeapGates.
static constexpr uint16_t IR_RAW_BUF_SIZE = 600;
// Raw replay copy (marks/spaces in us, header entry skipped per kStartOffset).
// Longer captures replay truncated; NEC-class frames fit entirely.
static constexpr uint16_t IR_RAW_COPY_MAX = 256;
// In-memory session list of saved captures.
static constexpr uint8_t IR_SESSION_LIST_MAX = 16;
// JSON body cap: state protocols dump up to 16 state bytes.
static constexpr size_t IR_JSON_BODY_MAX = 256;

class IrTools {
public:
    enum class Status : uint8_t {
        Ok = 0,
        HeapBlocked,     // start() refused: HeapGates said no
        RadioAbsent,     // start() succeeded but nothing decoded yet
        CaptureReady,    // new decode this tick
        Overflow,        // decoded but protocol UNKNOWN / buffer overflowed
        Saving,
        Saved,
        SdAbsent,        // SD missing: capture held in RAM only
        PressureHeld,    // heap pressure > level 1: save deferred, visible
        Sending
    };

    struct Capture {
        char protocolName[24];
        uint16_t protocol;
        bool stateful;
        uint64_t value;
        uint32_t address;
        uint32_t command;
        uint8_t state[16];
        uint16_t stateLen;
        uint16_t bits;
        uint16_t rawlen;
        bool repeat;
        bool overflow;
        uint16_t rawCopy[IR_RAW_COPY_MAX];
        uint16_t rawCopyLen;
    };

    static void init();
    static void start();
    static void stop();
    static void update();
    static bool isRunning() { return running; }

    static Status getStatus() { return status; }
    static const Capture* lastCapture() { return haveCapture ? &last : nullptr; }
    static uint16_t sessionSavedCount() { return savedCount; }
    static uint8_t sessionListCount() { return sessionListLen; }
    static const char* sessionFilename(uint8_t i) {
        return (i < sessionListLen) ? sessionList[i] : "";
    }
    // True when the held capture still needs an SD write (SdAbsent/PressureHeld).
    static bool hasPendingSave() { return pendingSave; }

    // Schedule one replay frame of the last capture (runs on next update tick).
    static bool replayLast();
    // Schedule one NEC send (runs on next update tick).
    static bool sendNecValue(uint32_t value);

    // Pure helpers mirrored in test/mocks/testable_functions.h.
    static size_t captureJsonBody(char* out, size_t outLen);
    static void captureFilename(char* out, size_t outLen, uint16_t index);
    static bool parseHex32(const char* s, uint32_t& out);

private:
    static bool running;
    static Status status;
    static IRrecv* irrecv;
    static IRsend* irsend;
    static bool rxActive;
    static uint16_t nextFileIndex;   // continues after files already on SD
    static uint16_t savedCount;
    static Capture last;
    static bool haveCapture;
    static bool pendingSave;
    static char sessionList[IR_SESSION_LIST_MAX][24];
    static uint8_t sessionListLen;
    static uint32_t pendingReplayMask;  // bit0: raw replay, bit1: NEC send
    static uint32_t pendingNecValue;

    static void adoptDecode(const class decode_results& r);
    static void savePending();
    static void scanExistingCaptures();
    static void runScheduledTx();
};
