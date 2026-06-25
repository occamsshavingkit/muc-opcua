/* tests/unit/test_security_identity_errors.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_unsupported_security_policy(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T143");
}

void test_unsupported_identity_token(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T144");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unsupported_security_policy);
    RUN_TEST(test_unsupported_identity_token);
    return UNITY_END();
}
