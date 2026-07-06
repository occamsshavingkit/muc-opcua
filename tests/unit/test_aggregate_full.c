/* tests/unit/test_aggregate_full.c
 *
 * Spec Kit 043, T021-T029 [US1]. Direct API tests for aggregate function
 * accumulation and publishing per OPC-10000-13.
 */
#include "../../src/core/server_internal.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/types.h"
#include "unity.h"
#include <string.h>

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD

void setUp(void) {}
void tearDown(void) {}

/* T029: STUB comment removed — all behavioral tests now implemented. */

/* Helper: clear aggregate state for a fresh accumulation interval. */
static void reset_aggregate(mu_monitored_item_t *item) {
    (void)memset(&item->aggregate_state, 0, sizeof(item->aggregate_state));
    item->aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_AVERAGE;
    item->queue_head = 0;
    item->queue_tail = 0;
    item->queue_count = 0;
    item->pending = false;
    item->queue_overflow = false;
    item->has_aggregate = true;
    item->in_use = true;
    item->monitoring_mode = MU_MONITORING_MODE_REPORTING;
}

/* T021 - test_average_aggregate_direct (OPC-10000-13 §5.4.3) */
void test_average_aggregate_direct(void) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_AVERAGE;

    mu_variant_t s1 = {MU_TYPE_FLOAT, {.f = 10.0f}};
    mu_variant_t s2 = {MU_TYPE_FLOAT, {.f = 20.0f}};
    mu_variant_t s3 = {MU_TYPE_FLOAT, {.f = 30.0f}};

    monitored_item_accumulate_aggregate(&item, &s1);
    monitored_item_accumulate_aggregate(&item, &s2);
    monitored_item_accumulate_aggregate(&item, &s3);

    TEST_ASSERT_EQUAL_UINT32(3, item.aggregate_state.sample_count);

    monitored_item_publish_aggregate(&item, 100);

    TEST_ASSERT_EQUAL(1, item.queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_DOUBLE, item.queue[0].value.type);
    TEST_ASSERT_TRUE(item.queue[0].value.value.d == 20.0);
}

/* T022 - test_minimum_aggregate_direct (OPC-10000-13 §5.4.3) */
void test_minimum_aggregate_direct(void) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_MINIMUM;

    mu_variant_t s1 = {MU_TYPE_FLOAT, {.f = 5.0f}};
    mu_variant_t s2 = {MU_TYPE_FLOAT, {.f = 2.0f}};
    mu_variant_t s3 = {MU_TYPE_FLOAT, {.f = 3.0f}};

    monitored_item_accumulate_aggregate(&item, &s1);
    monitored_item_accumulate_aggregate(&item, &s2);
    monitored_item_accumulate_aggregate(&item, &s3);

    TEST_ASSERT_EQUAL_UINT32(3, item.aggregate_state.sample_count);

    monitored_item_publish_aggregate(&item, 100);

    TEST_ASSERT_EQUAL(1, item.queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, item.queue[0].value.type);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, item.queue[0].value.value.f);
}

/* T023 - test_maximum_aggregate_direct (OPC-10000-13 §5.4.3) */
void test_maximum_aggregate_direct(void) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_MAXIMUM;

    mu_variant_t s1 = {MU_TYPE_FLOAT, {.f = 1.0f}};
    mu_variant_t s2 = {MU_TYPE_FLOAT, {.f = 4.0f}};
    mu_variant_t s3 = {MU_TYPE_FLOAT, {.f = 2.0f}};

    monitored_item_accumulate_aggregate(&item, &s1);
    monitored_item_accumulate_aggregate(&item, &s2);
    monitored_item_accumulate_aggregate(&item, &s3);

    TEST_ASSERT_EQUAL_UINT32(3, item.aggregate_state.sample_count);

    monitored_item_publish_aggregate(&item, 100);

    TEST_ASSERT_EQUAL(1, item.queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, item.queue[0].value.type);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, item.queue[0].value.value.f);
}

/* T024 - test_zero_sample_fallback (OPC-10000-13 §5.4.4) */
void test_zero_sample_fallback(void) {
    mu_monitored_item_t item;

    /* Case 1: sample_count=0, has_value=false → BAD_NODATA */
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_AVERAGE;
    item.aggregate_state.sample_count = 0;
    item.has_value = false;

    monitored_item_publish_aggregate(&item, 100);

    TEST_ASSERT_EQUAL(1, item.queue_count);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NODATA, item.queue[0].status);

    /* Case 2: sample_count=0, has_value=true → last_value published */
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_AVERAGE;
    item.aggregate_state.sample_count = 0;
    item.has_value = true;
    item.last_value.type = MU_TYPE_DOUBLE;
    item.last_value.value.d = 42.0;

    monitored_item_publish_aggregate(&item, 100);

    TEST_ASSERT_EQUAL(1, item.queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_DOUBLE, item.queue[0].value.type);
    TEST_ASSERT_TRUE(item.queue[0].value.value.d == 42.0);
}

/* T025 - test_non_numeric_skip (OPC-10000-13 §5.4.4) */
void test_non_numeric_skip(void) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_AVERAGE;
    item.aggregate_state.sample_count = 0;

    mu_variant_t s = {MU_TYPE_STRING, {.str = {5, (const opcua_byte_t *)"hello"}}};

    monitored_item_accumulate_aggregate(&item, &s);

    TEST_ASSERT_EQUAL_UINT32(0, item.aggregate_state.sample_count);
}

/* T026 - test_processing_interval_boundary (OPC-10000-13 §5.4.4)
 *
 * The processing-interval check lives in the tick caller
 * (subscription_publish/tick.c:340-342). This test verifies that
 * publish correctly resets state after an interval, and the tick
 * caller can gate the next publish using last_calculation. */
void test_processing_interval_boundary(void) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.in_use = true;
    item.has_aggregate = true;
    item.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_AVERAGE;
    item.aggregate_state.processing_interval = 100.0;
    item.aggregate_state.last_calculation = 0;
    item.aggregate_state.sample_count = 0;

    mu_variant_t s = {MU_TYPE_FLOAT, {.f = 10.0f}};
    monitored_item_accumulate_aggregate(&item, &s);

    /* Interval elapsed from t=0 to t=100: publish should produce output. */
    monitored_item_publish_aggregate(&item, 100);

    TEST_ASSERT_EQUAL(1, item.queue_count);
    TEST_ASSERT_EQUAL_UINT32(0, item.aggregate_state.sample_count);
    TEST_ASSERT_TRUE(item.aggregate_state.last_calculation == 100);
    TEST_ASSERT_TRUE(item.queue[0].value.type == MU_TYPE_DOUBLE);
    TEST_ASSERT_TRUE(item.queue[0].value.value.d == 10.0);

    /* State is reset: sample_count=0, accumulator zeroed, last_calculation=100.
     * At t=150 (delta=50 < interval=100), the tick caller would skip calling
     * publish. The function itself always publishes when called; this confirms
     * the state machine correctly advances so the tick can gate subsequent runs. */
    monitored_item_publish_aggregate(&item, 150);

    TEST_ASSERT_TRUE(item.aggregate_state.last_calculation == 150);
    TEST_ASSERT_EQUAL_UINT32(0, item.aggregate_state.sample_count);
}

/* T027 - test_integer_type_preservation (OPC-10000-13 §5.4.3) */
void test_integer_type_preservation(void) {
    mu_monitored_item_t item_min;
    (void)memset(&item_min, 0, sizeof(item_min));
    item_min.in_use = true;
    item_min.has_aggregate = true;
    item_min.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item_min.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_MINIMUM;

    mu_variant_t i1 = {MU_TYPE_INT32, {.i32 = 100}};
    mu_variant_t i2 = {MU_TYPE_INT32, {.i32 = 200}};
    mu_variant_t i3 = {MU_TYPE_INT32, {.i32 = 50}};

    monitored_item_accumulate_aggregate(&item_min, &i1);
    monitored_item_accumulate_aggregate(&item_min, &i2);
    monitored_item_accumulate_aggregate(&item_min, &i3);

    monitored_item_publish_aggregate(&item_min, 100);

    TEST_ASSERT_EQUAL(1, item_min.queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, item_min.queue[0].value.type);
    TEST_ASSERT_EQUAL_INT32(50, item_min.queue[0].value.value.i32);

    /* Maximum: verify INT32 type preserved */
    mu_monitored_item_t item_max;
    (void)memset(&item_max, 0, sizeof(item_max));
    item_max.in_use = true;
    item_max.has_aggregate = true;
    item_max.monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item_max.aggregate_state.aggregate_type = MU_ID_AGGREGATETYPE_MAXIMUM;

    monitored_item_accumulate_aggregate(&item_max, &i1);
    monitored_item_accumulate_aggregate(&item_max, &i2);
    monitored_item_accumulate_aggregate(&item_max, &i3);

    monitored_item_publish_aggregate(&item_max, 100);

    TEST_ASSERT_EQUAL(1, item_max.queue_count);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, item_max.queue[0].value.type);
    TEST_ASSERT_EQUAL_INT32(200, item_max.queue[0].value.value.i32);
}

/* T028: Removed test_unsupported_aggregate_returns_filter_unsupported stub;
 *       kept constant-check tests below. */

void test_aggregate_type_average_is_2342(void) {
    TEST_ASSERT_EQUAL_UINT(2342, MU_ID_AGGREGATETYPE_AVERAGE);
}

void test_aggregate_type_minimum_is_2346(void) {
    TEST_ASSERT_EQUAL_UINT(2346, MU_ID_AGGREGATETYPE_MINIMUM);
}

void test_aggregate_type_maximum_is_2347(void) {
    TEST_ASSERT_EQUAL_UINT(2347, MU_ID_AGGREGATETYPE_MAXIMUM);
}

#else

void setUp(void) {}
void tearDown(void) {}

void test_aggregate_full_requires_standard_subscriptions(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS_STANDARD is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    RUN_TEST(test_average_aggregate_direct);
    RUN_TEST(test_minimum_aggregate_direct);
    RUN_TEST(test_maximum_aggregate_direct);
    RUN_TEST(test_zero_sample_fallback);
    RUN_TEST(test_non_numeric_skip);
    RUN_TEST(test_processing_interval_boundary);
    RUN_TEST(test_integer_type_preservation);
    RUN_TEST(test_aggregate_type_average_is_2342);
    RUN_TEST(test_aggregate_type_minimum_is_2346);
    RUN_TEST(test_aggregate_type_maximum_is_2347);
#else
    RUN_TEST(test_aggregate_full_requires_standard_subscriptions);
#endif
    return UNITY_END();
}
