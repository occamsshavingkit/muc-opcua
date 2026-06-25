/* tests/unit/test_discovery_endpoint.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_findservers_no_session(void) {
    TEST_IGNORE_MESSAGE("Implement FindServers no-session test");
}

void test_getendpoints_no_session(void) {
    TEST_IGNORE_MESSAGE("Implement GetEndpoints no-session test");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_findservers_no_session);
    RUN_TEST(test_getendpoints_no_session);
    return UNITY_END();
}
