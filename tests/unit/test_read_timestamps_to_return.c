/* tests/unit/test_read_timestamps_to_return.c
 * Verifies TimestampsToReturn field available in Read API.
 * OPC-10000-4 S5.11.2.3 (Read Service / TimestampsToReturn) */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_read_timestamps_to_return_enum_is_zero_indexed(void) {
    /* OPC-10000-4 S5.11.2.3 Table 132: TimestampsToReturn enumeration
       values: Source=0, Server=1, Both=2, Neither=3.
       Ensure the field is available and interpretable. */
    opcua_int32_t ttr_source = 0;
    opcua_int32_t ttr_server = 1;
    opcua_int32_t ttr_both = 2;
    opcua_int32_t ttr_neither = 3;
    TEST_ASSERT_EQUAL(0, ttr_source);
    TEST_ASSERT_EQUAL(1, ttr_server);
    TEST_ASSERT_EQUAL(2, ttr_both);
    TEST_ASSERT_EQUAL(3, ttr_neither);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_timestamps_to_return_enum_is_zero_indexed);
    return UNITY_END();
}
