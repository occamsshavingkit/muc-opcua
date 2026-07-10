/* tests/unit/test_analog_item.c
 *
 * Data Access value types (spec 060). The DA type-system NODES (AnalogItemType
 * and its property model) are verified non-circularly in test_da_type_nodes.c;
 * this file covers the C value types and the DeadbandType enum the codecs and
 * filter path rely on (OPC-10000-8 §5.6, §7.2).
 */
#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/types.h"
#if MUC_OPCUA_DATA_ACCESS
#include "services/subscription.h"
#endif
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_DATA_ACCESS
void test_range_type_is_defined(void) {
    mu_range_t r = {.low = 0.0, .high = 100.0};
    TEST_ASSERT_TRUE(r.low == 0.0);
    TEST_ASSERT_TRUE(r.high == 100.0);
}

/* EUInformation displayName/description are LocalizedText (locale + text), per
   OPC-10000-8 §5.6.4.3 — not plain strings. */
void test_eu_information_type_is_defined(void) {
    mu_eu_information_t eu;
    (void)memset(&eu, 0, sizeof(eu));
    TEST_ASSERT_NULL(eu.namespace_uri.data);
    TEST_ASSERT_EQUAL_INT(0, eu.unit_id);
    TEST_ASSERT_NULL(eu.display_name.text.data);
    TEST_ASSERT_NULL(eu.description.locale.data);
}

void test_deadband_type_enum(void) {
    TEST_ASSERT_EQUAL_INT(0, MU_DEADBAND_TYPE_NONE);
    TEST_ASSERT_EQUAL_INT(1, MU_DEADBAND_TYPE_ABSOLUTE);
    TEST_ASSERT_EQUAL_INT(2, MU_DEADBAND_TYPE_PERCENT);
}

/* The one globally-fixed DA type NodeId the library references by constant. */
void test_analogitemtype_nodeid(void) {
    TEST_ASSERT_EQUAL_UINT(2368, MU_ID_ANALOGITEMTYPE);
}
#endif

#if !MUC_OPCUA_DATA_ACCESS
void test_data_access_disabled(void) {
    TEST_IGNORE_MESSAGE("Data Access feature not enabled");
}
#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_DATA_ACCESS
    RUN_TEST(test_range_type_is_defined);
    RUN_TEST(test_eu_information_type_is_defined);
    RUN_TEST(test_deadband_type_enum);
    RUN_TEST(test_analogitemtype_nodeid);
#else
    RUN_TEST(test_data_access_disabled);
#endif
    return UNITY_END();
}
