/* tests/unit/test_browse_service.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_browse_service_static_references(void) {
    TEST_IGNORE_MESSAGE("Implement Browse service test for static references");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browse_service_static_references);
    return UNITY_END();
}
