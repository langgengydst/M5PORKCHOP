// Native suite for the config blob v2 IR-pin migration.
// Mirrors test/mocks/testable_functions.h (same-commit sync rule).

#include <unity.h>
#include <cstddef>
#include "../mocks/testable_functions.h"

void setUp() {}
void tearDown() {}

// A fresh v2 blob with defaults round-trips untouched.
void test_v2_defaults_round_trip() {
    uint8_t rx = PORKCHOP_IR_RX_DEFAULT;
    uint8_t tx = PORKCHOP_IR_TX_DEFAULT;
    TEST_ASSERT_FALSE(porkchop_blobNeedsIRMigration(PORKCHOP_CONFIG_VERSION, rx, tx));
    porkchop_irSanitizePins(rx, tx);
    TEST_ASSERT_EQUAL_UINT8(PORKCHOP_IR_RX_DEFAULT, rx);
    TEST_ASSERT_EQUAL_UINT8(PORKCHOP_IR_TX_DEFAULT, tx);
}

// A v1 blob (zeroed tail from the min-size read) migrates to 4/44.
void test_v1_truncated_migrates_to_defaults() {
    uint8_t rx = 0, tx = 0;  // zeroed tail fields, exactly what readBlobFrom yields
    TEST_ASSERT_TRUE(porkchop_blobNeedsIRMigration(1, rx, tx));
    porkchop_irSanitizePins(rx, tx);
    TEST_ASSERT_EQUAL_UINT8(4, rx);
    TEST_ASSERT_EQUAL_UINT8(44, tx);
}

// A blob stamped by NEWER firmware migrates too: != not <, so a future
// version's unknown tail can never be extracted as pin bytes.
void test_future_version_migrates() {
    TEST_ASSERT_TRUE(porkchop_blobNeedsIRMigration(3, 4, 44));
    TEST_ASSERT_TRUE(porkchop_blobNeedsIRMigration(99, 4, 44));
    TEST_ASSERT_FALSE(porkchop_blobNeedsIRMigration(2, 4, 44));
}

// A v2 blob carrying bus pins (denied) resets both pins to defaults.
void test_v2_denylist_pins_reset() {
    uint8_t rx = 5, tx = 12;  // CSN and SD CS
    TEST_ASSERT_TRUE(porkchop_blobNeedsIRMigration(2, rx, tx));
    porkchop_irSanitizePins(rx, tx);
    TEST_ASSERT_EQUAL_UINT8(4, rx);
    TEST_ASSERT_EQUAL_UINT8(44, tx);
}

// Valid custom pins on a v2 blob survive: no forced defaults.
void test_v2_valid_custom_pins_kept() {
    uint8_t rx = 6, tx = 13;  // cap free header pins
    TEST_ASSERT_FALSE(porkchop_blobNeedsIRMigration(2, rx, tx));
    porkchop_irSanitizePins(rx, tx);
    TEST_ASSERT_EQUAL_UINT8(6, rx);
    TEST_ASSERT_EQUAL_UINT8(13, tx);
}

// One bad pin resets the pair, not just the bad one.
void test_one_bad_pin_resets_pair() {
    uint8_t rx = 4, tx = 40;  // tx on SPI SCK
    porkchop_irSanitizePins(rx, tx);
    TEST_ASSERT_EQUAL_UINT8(4, rx);
    TEST_ASSERT_EQUAL_UINT8(44, tx);
}

// The denylist rejects every shared-bus pin plus G0, and the S3 GPIO ceiling.
void test_pin_denylist() {
    const uint8_t denied[] = {0, 3, 5, 12, 14, 39, 40, 49, 255};
    for (uint8_t p : denied) TEST_ASSERT_FALSE(porkchop_irPinValid(p));
    const uint8_t allowed[] = {1, 2, 4, 6, 13, 15, 44, 48};
    for (uint8_t p : allowed) TEST_ASSERT_TRUE(porkchop_irPinValid(p));
}

// The v2 extension is tail-only: IR fields sit past the entire v1 layout.
void test_blob_extension_is_tail_only() {
    TEST_ASSERT_EQUAL_UINT32(sizeof(PorkchopBlobV1Mirror),
                             offsetof(PorkchopBlobV2Mirror, irRxPin));
    TEST_ASSERT_EQUAL_UINT32(sizeof(PorkchopBlobV1Mirror) + 1,
                             offsetof(PorkchopBlobV2Mirror, irTxPin));
    TEST_ASSERT_EQUAL_UINT32(sizeof(PorkchopBlobV1Mirror) + 2,
                             sizeof(PorkchopBlobV2Mirror));
    TEST_ASSERT_TRUE(sizeof(PorkchopBlobV2Mirror) > sizeof(PorkchopBlobV1Mirror));
}

// Version constant: the mirror tracks the source of truth.
void test_config_version_is_two() {
    TEST_ASSERT_EQUAL_UINT16(2, PORKCHOP_CONFIG_VERSION);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_v2_defaults_round_trip);
    RUN_TEST(test_v1_truncated_migrates_to_defaults);
    RUN_TEST(test_future_version_migrates);
    RUN_TEST(test_v2_denylist_pins_reset);
    RUN_TEST(test_v2_valid_custom_pins_kept);
    RUN_TEST(test_one_bad_pin_resets_pair);
    RUN_TEST(test_pin_denylist);
    RUN_TEST(test_blob_extension_is_tail_only);
    RUN_TEST(test_config_version_is_two);
    return UNITY_END();
}
