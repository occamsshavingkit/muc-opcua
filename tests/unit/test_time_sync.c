/* tests/unit/test_time_sync.c
 *
 * Placeholder test file for time sync functionality.
 * OPC-10000-4 §A.2 (TimestampsToReturn).
 * Behavioral tests to be added when MUC_OPCUA_TIME_SYNC is implemented.
 */
/* STUB: time sync tests not yet implemented - feature gated behind MUC_OPCUA_TIME_SYNC */

#include "unity.h"

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
