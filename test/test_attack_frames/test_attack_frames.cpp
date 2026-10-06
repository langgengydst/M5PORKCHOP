// WiFi attack frame builder tests (beacon spam / probe spam layout).
#include <unity.h>
#include <cstring>
#include <cstdio>
#include "../mocks/testable_functions.h"

void setUp() {}
void tearDown() {}

void test_beacon_frame_layout() {
    const char* ssid = "PIG_ON_AIR";
    const uint8_t bssid[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    uint8_t f[128];
    size_t n = porkchop_buildBeaconFrame(f, sizeof(f), ssid, strlen(ssid), bssid, 6, 0x1234);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_UINT8(0x80, f[0]);   // beacon subtype
    TEST_ASSERT_EQUAL_UINT8(0x00, f[1]);
    // DA broadcast
    for (int i = 4; i < 10; i++) TEST_ASSERT_EQUAL_UINT8(0xFF, f[i]);
    // SA and BSSID carry the spoofed AP mac
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bssid, f + 10, 6);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bssid, f + 16, 6);
    // sequence control: seq bits 3:0 in byte 22 (fragment nibble zero),
    // seq bits 11:4 in byte 23 — 0x1234 encodes as 0x04, 0x23
    TEST_ASSERT_EQUAL_UINT8(0x04, f[22]);
    TEST_ASSERT_EQUAL_UINT8(0x23, f[23]);
    // fixed params: timestamp 8 zero bytes, interval 100 TU at 32, cap ESS at 34
    TEST_ASSERT_EQUAL_UINT8(0x64, f[32]);
    TEST_ASSERT_EQUAL_UINT8(0x21, f[34]);
    // SSID IE at 36
    TEST_ASSERT_EQUAL_UINT8(0x00, f[36]);
    TEST_ASSERT_EQUAL_UINT8(strlen(ssid), f[37]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ssid, f + 38, strlen(ssid));
    // DS param carries the channel
    size_t dsAt = 38 + strlen(ssid);
    TEST_ASSERT_EQUAL_UINT8(0x03, f[dsAt]);
    TEST_ASSERT_EQUAL_UINT8(0x01, f[dsAt + 1]);
    TEST_ASSERT_EQUAL_UINT8(6, f[dsAt + 2]);
    // rates IE closes the frame
    TEST_ASSERT_EQUAL_UINT8_ARRAY(PORKCHOP_RATES_IE, f + dsAt + 3,
                                  sizeof(PORKCHOP_RATES_IE));
    TEST_ASSERT_EQUAL_size_t(dsAt + 3 + sizeof(PORKCHOP_RATES_IE), n);
}

void test_beacon_frame_max_ssid() {
    char ssid[33];
    memset(ssid, 'X', 32);
    ssid[32] = '\0';
    const uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
    uint8_t f[128];
    size_t n = porkchop_buildBeaconFrame(f, sizeof(f), ssid, 32, bssid, 11, 0);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_UINT8(32, f[37]);
    // over-length SSID rejected, not truncated
    TEST_ASSERT_EQUAL_size_t(0, porkchop_buildBeaconFrame(f, sizeof(f), ssid, 33, bssid, 11, 0));
}

void test_probe_frame_layout() {
    const char* ssid = "OINKNET_5G";
    const uint8_t src[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t f[128];
    size_t n = porkchop_buildProbeReqFrame(f, sizeof(f), ssid, strlen(ssid), src, 0x00FF);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_UINT8(0x40, f[0]);   // probe request subtype
    // DA broadcast, SA our random mac, BSSID wildcard
    for (int i = 4; i < 10; i++) TEST_ASSERT_EQUAL_UINT8(0xFF, f[i]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(src, f + 10, 6);
    for (int i = 16; i < 22; i++) TEST_ASSERT_EQUAL_UINT8(0xFF, f[i]);
    // SSID IE right after the 24-byte header
    TEST_ASSERT_EQUAL_UINT8(0x00, f[24]);
    TEST_ASSERT_EQUAL_UINT8(strlen(ssid), f[25]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ssid, f + 26, strlen(ssid));
    TEST_ASSERT_EQUAL_size_t(26 + strlen(ssid) + sizeof(PORKCHOP_RATES_IE), n);
}

void test_buffer_too_small_rejected() {
    const uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    uint8_t small[8];
    TEST_ASSERT_EQUAL_size_t(0, porkchop_buildBeaconFrame(small, sizeof(small), "X", 1, mac, 1, 0));
    TEST_ASSERT_EQUAL_size_t(0, porkchop_buildProbeReqFrame(small, sizeof(small), "X", 1, mac, 0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_beacon_frame_layout);
    RUN_TEST(test_beacon_frame_max_ssid);
    RUN_TEST(test_probe_frame_layout);
    RUN_TEST(test_buffer_too_small_rejected);
    return UNITY_END();
}
