// IR Tools Menu UI (pig tone) + native tests for the pure IR helpers.
#include <unity.h>
#include <string.h>
#include <cstdio>
#include "../mocks/testable_functions.h"

void setUp() {}
void tearDown() {}

void test_parse_hex32_valid() {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(porkchop_irParseHex32("00FF00FF", v));
    TEST_ASSERT_EQUAL_UINT32(0x00FF00FF, v);
    TEST_ASSERT_TRUE(porkchop_irParseHex32("1", v));
    TEST_ASSERT_EQUAL_UINT32(0x1, v);
    TEST_ASSERT_TRUE(porkchop_irParseHex32("deadBEEF", v));
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, v);
}

void test_parse_hex32_invalid() {
    uint32_t v = 0;
    TEST_ASSERT_FALSE(porkchop_irParseHex32("", v));
    TEST_ASSERT_FALSE(porkchop_irParseHex32("00FF00FFX", v));  // bad char at end
    TEST_ASSERT_FALSE(porkchop_irParseHex32("123456789", v));  // 9 chars
    TEST_ASSERT_FALSE(porkchop_irParseHex32("0x1F", v));       // no 0x prefix support
}

void test_capture_json_value_protocol() {
    char buf[192];
    size_t n = porkchop_irCaptureJson(buf, sizeof(buf), "NEC", false,
                                      0x00FF00FFULL, 0xFF00, 0x21, 32, 68, false, false,
                                      nullptr, 0);
    TEST_ASSERT_TRUE(n > 0 && n < sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"protocol\":\"NEC\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"value\":\"0x0000000000FF00FF\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"address\":\"0x0000FF00\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"command\":\"0x00000021\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"bits\":32"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"rawlen\":68"));
    // stateful fields must NOT appear for value protocols (union garbage rule)
    TEST_ASSERT_NULL(strstr(buf, "\"state\""));
    // well-formed object
    TEST_ASSERT_EQUAL_CHAR('{', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('}', buf[n - 1]);
}

void test_capture_json_state_protocol() {
    uint8_t state[4] = {0x01, 0x02, 0xAB, 0xCD};
    char buf[192];
    size_t n = porkchop_irCaptureJson(buf, sizeof(buf), "KELVINATOR", true,
                                      0, 0, 0, 104, 300, false, false,
                                      state, 4);
    TEST_ASSERT_TRUE(n > 0 && n < sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"state\":[\"01\",\"02\",\"AB\",\"CD\"]"));
    // union fields must NOT appear for stateful protocols
    TEST_ASSERT_NULL(strstr(buf, "\"value\""));
    TEST_ASSERT_NULL(strstr(buf, "\"address\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"rawlen\":300"));
}

void test_capture_json_overflow_flag() {
    char buf[128];
    porkchop_irCaptureJson(buf, sizeof(buf), "UNKNOWN", false,
                           0, 0, 0, 0, 600, false, true, nullptr, 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"overflow\":true"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"repeat\":false"));
}

void test_capture_json_truncation_is_safe() {
    char buf[24];
    size_t n = porkchop_irCaptureJson(buf, sizeof(buf), "NEC-LIKE-LONG-NAME", false,
                                      0x1234ULL, 0x11, 0x22, 32, 68, false, false,
                                      nullptr, 0);
    TEST_ASSERT_TRUE(n <= sizeof(buf) - 1);
    TEST_ASSERT_EQUAL_CHAR('\0', buf[n]);  // snprintf always terminates our writes
    (void)buf;
}

void test_capture_filename_monotonic() {
    char buf[40];
    porkchop_irCaptureFilename(buf, sizeof(buf), "/m5porkchop/ir", 7);
    TEST_ASSERT_EQUAL_STRING("/m5porkchop/ir/ir_007.json", buf);
    porkchop_irCaptureFilename(buf, sizeof(buf), "/m5porkchop/ir", 123);
    TEST_ASSERT_EQUAL_STRING("/m5porkchop/ir/ir_123.json", buf);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parse_hex32_valid);
    RUN_TEST(test_parse_hex32_invalid);
    RUN_TEST(test_capture_json_value_protocol);
    RUN_TEST(test_capture_json_state_protocol);
    RUN_TEST(test_capture_json_overflow_flag);
    RUN_TEST(test_capture_json_truncation_is_safe);
    RUN_TEST(test_capture_filename_monotonic);
    return UNITY_END();
}
