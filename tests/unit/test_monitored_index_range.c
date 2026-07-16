/* tests/unit/test_monitored_index_range.c
 *
 * Spec 073 long tail, CU 5208 (Monitor Value Change V2). A MonitoredItem created
 * with an IndexRange must sample only the selected element or slice of an
 * array-valued Attribute, not the whole array.
 *
 * Grounding: OPC-10000-4 §7.22 NumericRange; the IndexRange parameter of the
 * MonitoredItem ReadValueId (§7.11). read_monitored_item_value() applies the
 * parsed range (index_range_start/index_range_end on the item) via the shared
 * apply_numeric_index_range() helper.
 */
#include "unity.h"

#include <string.h>

#include "../../src/core/server_internal.h"

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC

static const opcua_int32_t s_array[5] = {10, 20, 30, 40, 50};

/* A static Int32[5] Variable node the monitored item resolves to. */
static mu_node_t make_array_node(const mu_value_source_t *vs) {
    mu_node_t node = {
        {0, MU_NODEID_NUMERIC, {.numeric = 5000u}}, MU_NODECLASS_VARIABLE, {0, NULL}, {0, NULL}, NULL, 0u, vs};
    return node;
}

static mu_monitored_item_t make_item(const mu_node_t *node, opcua_int32_t start, opcua_int32_t end) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.resolved_node = node;
    item.node_id = node->node_id;
    item.attribute_id = 13u; /* Value */
    item.index_range_start = start;
    item.index_range_end = end;
    return item;
}

/* IndexRange "2" selects a single element -> scalar 30. */
void test_index_range_single_element_selects_scalar(void) {
    mu_value_source_t vs = {
        MU_VALUESOURCE_STATIC,
        {.static_value = {.type = MU_TYPE_INT32, .value.array = s_array, .is_array = true, .array_length = 5}}};
    mu_node_t node = make_array_node(&vs);
    mu_monitored_item_t item = make_item(&node, 2, -1);

    mu_variant_t cur;
    (void)memset(&cur, 0, sizeof(cur));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_monitored_item_value(&item, &cur));
    TEST_ASSERT_FALSE(cur.is_array);
    TEST_ASSERT_EQUAL_INT32(30, cur.value.i32);
}

/* IndexRange "1:3" selects a 3-element slice {20,30,40}. */
void test_index_range_slice_selects_subarray(void) {
    mu_value_source_t vs = {
        MU_VALUESOURCE_STATIC,
        {.static_value = {.type = MU_TYPE_INT32, .value.array = s_array, .is_array = true, .array_length = 5}}};
    mu_node_t node = make_array_node(&vs);
    mu_monitored_item_t item = make_item(&node, 1, 3);

    mu_variant_t cur;
    (void)memset(&cur, 0, sizeof(cur));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_monitored_item_value(&item, &cur));
    TEST_ASSERT_TRUE(cur.is_array);
    TEST_ASSERT_EQUAL_INT32(3, cur.array_length);
    const opcua_int32_t *out = (const opcua_int32_t *)cur.value.array;
    TEST_ASSERT_EQUAL_INT32(20, out[0]);
    TEST_ASSERT_EQUAL_INT32(30, out[1]);
    TEST_ASSERT_EQUAL_INT32(40, out[2]);
}

/* No IndexRange (start == -1) returns the whole array untouched. */
void test_no_index_range_returns_whole_array(void) {
    mu_value_source_t vs = {
        MU_VALUESOURCE_STATIC,
        {.static_value = {.type = MU_TYPE_INT32, .value.array = s_array, .is_array = true, .array_length = 5}}};
    mu_node_t node = make_array_node(&vs);
    mu_monitored_item_t item = make_item(&node, -1, -1);

    mu_variant_t cur;
    (void)memset(&cur, 0, sizeof(cur));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_monitored_item_value(&item, &cur));
    TEST_ASSERT_TRUE(cur.is_array);
    TEST_ASSERT_EQUAL_INT32(5, cur.array_length);
}

/* A start index past the end of the array yields Bad_IndexRangeNoData. */
void test_index_range_out_of_bounds_reports_no_data(void) {
    mu_value_source_t vs = {
        MU_VALUESOURCE_STATIC,
        {.static_value = {.type = MU_TYPE_INT32, .value.array = s_array, .is_array = true, .array_length = 5}}};
    mu_node_t node = make_array_node(&vs);
    mu_monitored_item_t item = make_item(&node, 9, -1);

    mu_variant_t cur;
    (void)memset(&cur, 0, sizeof(cur));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_INDEXRANGENODATA, read_monitored_item_value(&item, &cur));
}

#else

void test_monitored_index_range_requires_subscription_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_CU_SUBSCRIPTION_BASIC is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    RUN_TEST(test_index_range_single_element_selects_scalar);
    RUN_TEST(test_index_range_slice_selects_subarray);
    RUN_TEST(test_no_index_range_returns_whole_array);
    RUN_TEST(test_index_range_out_of_bounds_reports_no_data);
#else
    RUN_TEST(test_monitored_index_range_requires_subscription_build);
#endif
    return UNITY_END();
}
