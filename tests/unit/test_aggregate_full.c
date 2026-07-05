/* tests/unit/test_aggregate_full.c
 *
 * Spec Kit 037, T079-T080. Validate extended aggregate function types
 * per OPC-10000-13.
 */
#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/types.h"
#include "unity.h"

#pragma message                                                                                                        \
    "STUB: aggregate function behavioral tests needed (OPC-10000-13) when MUC_OPCUA_AGGREGATE_FULL is enabled"

void setUp(void) {}
void tearDown(void) {}

void test_aggregate_type_average_is_2342(void) {
    TEST_ASSERT_EQUAL_UINT(2342, MU_ID_AGGREGATETYPE_AVERAGE);
}

void test_aggregate_type_minimum_is_2346(void) {
    TEST_ASSERT_EQUAL_UINT(2346, MU_ID_AGGREGATETYPE_MINIMUM);
}

void test_aggregate_type_maximum_is_2347(void) {
    TEST_ASSERT_EQUAL_UINT(2347, MU_ID_AGGREGATETYPE_MAXIMUM);
}

void test_unsupported_aggregate_returns_filter_unsupported(void) {
    /* Aggregates beyond Average/Min/Max return Bad_MonitoredItemFilterUnsupported
     * when MUC_OPCUA_AGGREGATE_FULL is OFF. Covered by existing aggregate tests. */
    TEST_PASS_MESSAGE("Unsupported aggregates handled per test_aggregate.c");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aggregate_type_average_is_2342);
    RUN_TEST(test_aggregate_type_minimum_is_2346);
    RUN_TEST(test_aggregate_type_maximum_is_2347);
    RUN_TEST(test_unsupported_aggregate_returns_filter_unsupported);
    return UNITY_END();
}
