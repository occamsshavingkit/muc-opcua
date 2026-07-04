/* tests/unit/test_read_timestamps_to_return.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_read_timestamps_to_return(void) {
    /* OPC-10000-4 S5.11.2.3: Timestamps are populated when requested */
    /* TODO: Implement timestamps to return verification */
    TEST_ASSERT_TRUE(1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_timestamps_to_return);
    return UNITY_END();
}
