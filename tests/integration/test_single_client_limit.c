/* tests/integration/test_single_client_limit.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_single_client_limit_second_connection(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T145");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_single_client_limit_second_connection);
    return UNITY_END();
}
