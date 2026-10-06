// NRF Tools pure-logic tests: candidate generation + scanner channel law.
#include <unity.h>
#include <cstring>
#include <cstdio>
#include "../mocks/testable_functions.h"

void setUp() {}
void tearDown() {}

void test_candidate_layout_on_air_order() {
    const uint8_t prefix[3] = {0xC2, 0x39, 0x30};
    uint8_t out[5] = {0};
    porkchop_nrfCandidateFromPrefix(prefix, 0x7B, out);
    // RF24 arrays transmit out[4] first: the written-order prefix C2:39:30
    // lives reversed at the tail, cycling suffix byte leads on air.
    TEST_ASSERT_EQUAL_UINT8(0x00, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0x7B, out[1]);
    TEST_ASSERT_EQUAL_UINT8(0x30, out[2]);
    TEST_ASSERT_EQUAL_UINT8(0x39, out[3]);
    TEST_ASSERT_EQUAL_UINT8(0xC2, out[4]);
}

void test_candidate_cycle_wraps_suffix() {
    const uint8_t prefix[3] = {0xC2, 0x39, 0x30};
    uint8_t a[5], b[5];
    porkchop_nrfCandidateFromPrefix(prefix, 0x00FF, a);
    porkchop_nrfCandidateFromPrefix(prefix, 0x0100, b);
    // only the low byte cycles: idx 0x100 folds to suffix 0x00 again
    TEST_ASSERT_EQUAL_UINT8(0xFF, a[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, b[1]);
    TEST_ASSERT_EQUAL_UINT8(a[4], b[4]);
}

void test_scanner_step_wraps_at_legal_ceiling() {
    TEST_ASSERT_EQUAL_UINT8(1, porkchop_nrfScannerNextChannel(0));
    TEST_ASSERT_EQUAL_UINT8(83, porkchop_nrfScannerNextChannel(82));
    // wrap at the 2400-2483.5MHz band edge (Kepmen Kominfo 260/2024)
    TEST_ASSERT_EQUAL_UINT8(0, porkchop_nrfScannerNextChannel(83));
    // never produces a channel above 83 from any input
    TEST_ASSERT_TRUE(porkchop_nrfScannerNextChannel(120) <= 83);
    TEST_ASSERT_TRUE(porkchop_nrfScannerNextChannel(250) <= 83);
}

void test_channel_count_is_84() {
    TEST_ASSERT_EQUAL_UINT8(84, PORKCHOP_NRF_CHANNEL_COUNT);
    TEST_ASSERT_EQUAL_UINT8(83, PORKCHOP_NRF_CHANNEL_MAX);
}

void test_keepalive_frame_layout() {
    uint8_t f[PORKCHOP_NRF_PAYLOAD_MAX] = {0};
    size_t n = porkchop_nrfBuildKeepalive(f);
    TEST_ASSERT_EQUAL_size_t(3, n);
    TEST_ASSERT_EQUAL_UINT8(0x00, f[0]);
    TEST_ASSERT_EQUAL_UINT8(0x40, f[1]);
    TEST_ASSERT_EQUAL_UINT8(0x04, f[2]);
}

void test_keystroke_frame_layout() {
    uint8_t f[PORKCHOP_NRF_PAYLOAD_MAX] = {0};
    const uint8_t keys[6] = {0x12, 0, 0, 0, 0, 0};  // 'o'
    size_t n = porkchop_nrfBuildKeystroke(f, 0x02, keys);
    TEST_ASSERT_EQUAL_size_t(11, n);
    TEST_ASSERT_EQUAL_UINT8(0x00, f[0]);   // dev
    TEST_ASSERT_EQUAL_UINT8(0xC1, f[1]);   // inject
    TEST_ASSERT_EQUAL_UINT8(0x00, f[2]);   // wIndex
    TEST_ASSERT_EQUAL_UINT8(0x02, f[3]);   // modifier: left shift
    TEST_ASSERT_EQUAL_UINT8(0x00, f[4]);   // reserved
    TEST_ASSERT_EQUAL_UINT8(0x12, f[5]);   // key slot 1
    TEST_ASSERT_EQUAL_UINT8(0x00, f[10]);  // key slot 6 zeroed
}

void test_hid_keycode_mapping() {
    uint8_t c = 0;
    TEST_ASSERT_TRUE(porkchop_nrfHidKeyCode('o', c)); TEST_ASSERT_EQUAL_UINT8(0x12, c);
    TEST_ASSERT_TRUE(porkchop_nrfHidKeyCode('a', c)); TEST_ASSERT_EQUAL_UINT8(0x04, c);
    TEST_ASSERT_TRUE(porkchop_nrfHidKeyCode('z', c)); TEST_ASSERT_EQUAL_UINT8(0x1D, c);
    TEST_ASSERT_TRUE(porkchop_nrfHidKeyCode('1', c)); TEST_ASSERT_EQUAL_UINT8(0x1E, c);
    TEST_ASSERT_TRUE(porkchop_nrfHidKeyCode('0', c)); TEST_ASSERT_EQUAL_UINT8(0x27, c);
    TEST_ASSERT_TRUE(porkchop_nrfHidKeyCode(' ', c)); TEST_ASSERT_EQUAL_UINT8(0x2C, c);
    TEST_ASSERT_FALSE(porkchop_nrfHidKeyCode('!', c));  // shifted symbols not in v1
    TEST_ASSERT_FALSE(porkchop_nrfHidKeyCode(0, c));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_candidate_layout_on_air_order);
    RUN_TEST(test_candidate_cycle_wraps_suffix);
    RUN_TEST(test_scanner_step_wraps_at_legal_ceiling);
    RUN_TEST(test_channel_count_is_84);
    RUN_TEST(test_keepalive_frame_layout);
    RUN_TEST(test_keystroke_frame_layout);
    RUN_TEST(test_hid_keycode_mapping);
    return UNITY_END();
}
