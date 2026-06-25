/* tests/unit/test_binary_string_errors.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_binary_string_truncated(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T139");
}

void test_binary_string_negative_length(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T139");
}

void test_binary_string_excessive_length(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T139");
}

void test_binary_string_embedded_overrun(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T139");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_string_truncated);
    RUN_TEST(test_binary_string_negative_length);
    RUN_TEST(test_binary_string_excessive_length);
    RUN_TEST(test_binary_string_embedded_overrun);
    return UNITY_END();
}
