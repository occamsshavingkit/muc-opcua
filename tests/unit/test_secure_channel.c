/* tests/unit/test_secure_channel.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_secure_channel_open_none(void) {
    TEST_IGNORE_MESSAGE("Implement OpenSecureChannel SecurityPolicy None test");
}

void test_secure_channel_close(void) {
    TEST_IGNORE_MESSAGE("Implement CloseSecureChannel test");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_secure_channel_open_none);
    RUN_TEST(test_secure_channel_close);
    return UNITY_END();
}
