/* tests/unit/test_session.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_session_create(void) {
    TEST_IGNORE_MESSAGE("Implement CreateSession test");
}

void test_session_activate_anonymous(void) {
    TEST_IGNORE_MESSAGE("Implement ActivateSession anonymous identity policy test");
}

void test_session_close(void) {
    TEST_IGNORE_MESSAGE("Implement CloseSession test");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_session_create);
    RUN_TEST(test_session_activate_anonymous);
    RUN_TEST(test_session_close);
    return UNITY_END();
}
