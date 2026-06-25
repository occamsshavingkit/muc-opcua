/* tests/unit/test_binary_nodeid_errors.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_binary_nodeid_invalid_encoding_mask(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T140");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_nodeid_invalid_encoding_mask);
    return UNITY_END();
}
