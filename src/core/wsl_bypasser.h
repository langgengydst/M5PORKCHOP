// WSL Bypasser - Bypass ESP32 WiFi frame validation
#pragma once

#include <Arduino.h>

namespace WSLBypasser {

// Initialize the bypasser (call early in setup)
void init();

// Randomize MAC address (call before promiscuous mode or scan)
// Sets a locally-administered random MAC to avoid device fingerprinting
void randomizeMAC();

// Send a deauth frame (returns true if sent successfully)
bool sendDeauthFrame(const uint8_t* bssid, uint8_t channel, const uint8_t* staMac, uint8_t reason);

// Send a disassoc frame
bool sendDisassocFrame(const uint8_t* bssid, uint8_t channel, const uint8_t* staMac, uint8_t reason);

// Attack extras (OINK selector): spoofed beacon and probe request injection.
// Both send on the radio's current channel; caller sets the channel.
bool sendBeaconFrame(const char* ssid, uint8_t ssidLen,
                     const uint8_t bssid[6], uint8_t channel);
bool sendProbeReqFrame(const char* ssid, uint8_t ssidLen, uint8_t channel);

}
