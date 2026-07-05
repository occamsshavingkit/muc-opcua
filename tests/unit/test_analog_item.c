/* tests/unit/test_analog_item.c
 *
 * Spec Kit 037, T015. Validate AnalogItemType property definitions
 * and Range/EUInformation types per OPC-10000-3 §5.6.2.
 */
#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/types.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_eurange_nodeid_is_113(void) {
    TEST_ASSERT_EQUAL_UINT(113, MU_ID_EURANGE);
}

void test_engineering_units_nodeid_is_115(void) {
    TEST_ASSERT_EQUAL_UINT(115, MU_ID_ENGINEERINGUNITS);
}

void test_instrument_range_nodeid_is_117(void) {
    TEST_ASSERT_EQUAL_UINT(117, MU_ID_INSTRUMENTRANGE);
}

void test_analog_item_type_nodeid_is_2368(void) {
    TEST_ASSERT_EQUAL_UINT(2368, MU_ID_ANALOGITEMTYPE);
}

void test_percent_deadband_type_is_3(void) {
    TEST_ASSERT_EQUAL_UINT(3, MU_ID_PERCENTDEADBAND);
}

#if MUC_OPCUA_DATA_ACCESS
void test_range_type_is_defined(void) {
    mu_range_t r = {.low = 0.0, .high = 100.0};
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.low);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, r.high);
}

void test_eu_information_type_is_defined(void) {
    mu_eu_information_t eu;
    (void)memset(&eu, 0, sizeof(eu));
    TEST_ASSERT_NULL(eu.namespace_uri.data);
    TEST_ASSERT_EQUAL_INT(0, eu.unit_id);
}

void test_deadband_type_enum_has_percent(void) {
    TEST_ASSERT_EQUAL_INT(2, MU_DEADBAND_TYPE_PERCENT);
    TEST_ASSERT_EQUAL_INT(0, MU_DEADBAND_TYPE_NONE);
    TEST_ASSERT_EQUAL_INT(1, MU_DEADBAND_TYPE_ABSOLUTE);
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_eurange_nodeid_is_113);
    RUN_TEST(test_engineering_units_nodeid_is_115);
    RUN_TEST(test_instrument_range_nodeid_is_117);
    RUN_TEST(test_analog_item_type_nodeid_is_2368);
    RUN_TEST(test_percent_deadband_type_is_3);
#if MUC_OPCUA_DATA_ACCESS
    RUN_TEST(test_range_type_is_defined);
    RUN_TEST(test_eu_information_type_is_defined);
    RUN_TEST(test_deadband_type_enum_has_percent);
#endif
    return UNITY_END();
}
