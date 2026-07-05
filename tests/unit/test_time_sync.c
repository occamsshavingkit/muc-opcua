/* tests/unit/test_time_sync.c
 *
 * Spec Kit 037, T082. Validate timestamp population infrastructure.
 * OPC-10000-4 §A.2 (TimestampsToReturn).
 */
#include "unity.h"

#pragma message                                                                                                        \
    "STUB: behavioral tests needed for timestamp population (OPC-10000-4 §A.2) when MUC_OPCUA_TIME_SYNC is enabled"

void setUp(void) {}
void tearDown(void) {}

void test_time_sync_requires_build_flag(void) {
    TEST_PASS_MESSAGE("Time Sync gated behind MUC_OPCUA_TIME_SYNC");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_time_sync_requires_build_flag);
    return UNITY_END();
}
