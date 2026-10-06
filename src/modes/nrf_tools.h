// NRF Tools Mode - cap NRF24 scanner / address-discovery sniffer / ping / mousejack
#pragma once

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

// Cap NRF24 wiring (compile constants; matches the locked cap pin map):
// CE=G3, CSN=G5, SPI shared with SD via Config::sdSpi() (SCK G40 MISO G39 MOSI G14).
namespace NrfPins {
    static constexpr uint8_t CE  = 3;
    static constexpr uint8_t CSN = 5;
}

// 2.4 GHz ISM legal ceiling in Indonesia (Kepmen Kominfo 260/2024):
// 2400-2483.5 MHz = nRF24 channels 0..83. Never scan above 83.
static constexpr uint8_t NRF_CHANNEL_MAX = 83;
static constexpr uint8_t NRF_CHANNEL_COUNT = NRF_CHANNEL_MAX + 1;

// Scanner dwell: ticks spent per channel before stepping.
static constexpr uint8_t NRF_SCAN_DWELL_TICKS = 4;

// Address-discovery sniffer: curated seed prefixes from public mousejack
// research (Logitech/Microsoft device classes). Own-lab targets intended;
// the table is compile-time user-extensible. Addresses are MSB-first,
// 5 bytes: prefix + cycling low byte + fixed tail byte.
static constexpr uint8_t NRF_KNOWN_PREFIX_COUNT = 12;
static constexpr uint16_t NRF_SUFFIX_CYCLE_MAX = 256;  // candidates per prefix

// Discovered-address ring and payload snapshot sizes.
static constexpr uint8_t NRF_FOUND_MAX = 6;
static constexpr uint8_t NRF_PAYLOAD_MAX = 32;

// Ping: enhanced-ShockBurst echo against a second node running the stock
// RF24 GettingStarted pair (addresses "1Node"/"2Node", default channel 76).
// write() returning true IS the pong (auto-ack).
static constexpr uint32_t NRF_PING_AUTO_INTERVAL_MS = 500;

// Mousejack: Logitech Unifying-class frames, own-lab framing, hard caps.
static constexpr uint8_t NRF_MJ_MAX_KEYS = 8;      // hard cap on keystrokes
static constexpr char NRF_MJ_PROOF_TEXT[] = "OINK";  // typed once as proof

class NrfTools {
public:
    enum class RadioState : uint8_t {
        NotProbed = 0,
        CapMissing,     // radio.begin() failed: no cap (or dead socket)
        Unresponsive,   // begin passed but register sanity read failed
        LoRaConflict,   // GPS source CAP_LORA owns G3/G13/G15
        Ready
    };

    enum class Tool : uint8_t { None = 0, Scanner, Sniffer, Ping, Mousejack };

    enum class PingState : uint8_t { Off = 0, Armed, Waiting, Pong, Miss };

    enum class MjState : uint8_t {
        Off = 0,
        Scanning,   // prefix scan, one candidate per tick
        Ready,      // target found, waiting for user GO
        Injecting,  // keystroke frames, one write per tick, abortable
        Finished,
        Aborted
    };

    struct Found {
        uint8_t addr[5];
        uint8_t channel;
        uint8_t payload[NRF_PAYLOAD_MAX];
        uint8_t payloadLen;
    };

    static void init();
    static void start();
    static void stop();
    static void update();
    static bool isRunning() { return running; }

    static RadioState radioState() { return rState; }
    static const char* radioStateLine();

    // Scanner
    static void beginScanner();
    static void endScanner();
    static bool scannerActive() { return tool == Tool::Scanner; }
    static uint16_t channelHits(uint8_t ch) {
        return (ch < NRF_CHANNEL_COUNT) ? chanHits[ch] : 0;
    }
    static uint8_t scanChannel() { return scanCh; }

    // Address-discovery sniffer
    static void beginSniffer();
    static void endSniffer();
    static bool snifferActive() { return tool == Tool::Sniffer; }
    static uint8_t sniffChannel() { return sniffCh; }
    static void sniffChannelStep(int8_t delta);  // clamped 0..NRF_CHANNEL_MAX
    static uint8_t foundCount() { return foundLen; }
    static const Found* found(uint8_t i) { return (i < foundLen) ? &foundList[i] : nullptr; }
    static uint16_t sniffProgress() { return candIdx; }          // 0..255
    static uint8_t sniffPrefixIndex() { return prefixIdx; }      // 0..NRF_KNOWN_PREFIX_COUNT-1

    // True when the radio object is on and the bus is ours.
    static bool radioUp() { return rState == RadioState::Ready; }

    // Ping
    static void beginPing();
    static void endPing();
    static bool pingActive() { return tool == Tool::Ping; }
    static void pingToggleAuto();
    static void pingFireOnce();
    static PingState pingState() { return pingSt; }
    static uint16_t pingSent() { return pingSentCount; }
    static uint16_t pingPongs() { return pingPongCount; }
    static uint16_t pingStreak() { return pingStreakCount; }
    static bool pingAuto() { return pingAutoOn; }

    // Mousejack
    static void beginMousejack();
    static void endMousejack();
    static bool mousejackActive() { return tool == Tool::Mousejack; }
    static MjState mousejackState() { return mjSt; }
    static void mousejackGo();     // Ready -> Injecting
    static void mousejackAbort();  // any state -> Aborted
    static const uint8_t* mousejackTarget();
    static uint8_t mousejackProgress() { return mjKeyIdx; }  // of NRF_MJ_MAX_KEYS
    static uint8_t mousejackChannel() { return sniffCh; }
    static void mousejackChannelStep(int8_t delta);

    // Pure helpers mirrored in test/mocks/testable_functions.h.
    static void candidateFromPrefix(const uint8_t prefix[3], uint16_t idx,
                                    uint8_t out[5]);
    static uint8_t scannerNextChannel(uint8_t ch);
    static const uint8_t* knownPrefix(uint8_t i);
    // Mousejack frame builders (Logitech Unifying class).
    static size_t buildKeepaliveFrame(uint8_t out[NRF_PAYLOAD_MAX]);
    static size_t buildKeystrokeFrame(uint8_t out[NRF_PAYLOAD_MAX], uint8_t mod,
                                      const uint8_t keys[6]);
    static bool hidKeyCode(char c, uint8_t& out);

private:
    static bool running;
    static Tool tool;
    static RadioState rState;
    static RF24 radio;

    // Scanner state
    static uint8_t scanCh;
    static uint8_t scanDwell;
    static uint16_t chanHits[NRF_CHANNEL_COUNT];

    // Sniffer state
    static uint8_t sniffCh;
    static uint8_t prefixIdx;
    static uint16_t candIdx;
    static Found foundList[NRF_FOUND_MAX];
    static uint8_t foundLen;

    // Ping state
    static PingState pingSt;
    static uint16_t pingSentCount;
    static uint16_t pingPongCount;
    static uint16_t pingStreakCount;
    static bool pingAutoOn;
    static uint32_t pingLastFireMs;
    static uint32_t pingPayload;

    // Mousejack state
    static MjState mjSt;
    static uint8_t mjTarget[5];
    static uint8_t mjKeyIdx;
    static uint8_t mjTickFlip;     // alternate keepalive / keystroke frames

    static void applyBusDiscipline();  // CSN + SD CS high
    static void scannerTick();
    static void snifferTick();
    static void pingTick();
    static void mousejackTick();
    static void rxProbeSetup();  // common sniffer/mousejack radio config
};
