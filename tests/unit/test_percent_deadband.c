/* tests/unit/test_percent_deadband.c
 *
 * Spec Kit 037, T013/T014 (RED phase). Unit test for percent deadband
 * evaluation in monitored_item_change_reportable().
 *
 * Grounding: OPC-10000-4 §7.22.2 DataChangeFilter.deadbandType.
 *   Percent deadband (2) = "absolute change exceeds (deadbandValue / 100.0)
 *   * (EURange.High - EURange.Low)". If EURange is absent, the server
 *   returns Bad_DeadbandFilterInvalid.
 */
#include "unity.h"

#include <string.h>

#include "../../src/core/server_internal.h"

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_DATA_ACCESS

static mu_variant_t make_double(opcua_double_t v) {
    mu_variant_t out;
    (void)memset(&out, 0, sizeof(out));
    out.type = MU_TYPE_DOUBLE;
    out.value.d = v;
    return out;
}

static mu_monitored_item_t make_percent_item(opcua_double_t last, opcua_double_t span, opcua_double_t pct) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.has_reported = true;
    item.last_status = MU_STATUS_GOOD;
    item.deadband_type = MU_DEADBAND_TYPE_PERCENT;
    item.deadband_value = pct;
    item.deadband_span = span;
    item.last_value = make_double(last);
    item.last_reported_numeric = last;
    return item;
}

/* T013: percent deadband with EURange [0, 100], deadband 10%.
 * Change of 5 → below threshold → NOT reportable.
 * Change of 15 → above threshold → reportable. */
void test_percent_deadband_below_threshold_not_reportable(void) {
    mu_monitored_item_t item = make_percent_item(50.0, 100.0, 10.0);
    mu_variant_t cur = make_double(55.0);
    TEST_ASSERT_FALSE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

void test_percent_deadband_above_threshold_reportable(void) {
    mu_monitored_item_t item = make_percent_item(50.0, 100.0, 10.0);
    mu_variant_t cur = make_double(65.0);
    TEST_ASSERT_TRUE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

/* T013: exact boundary — change equals threshold → reportable (>= semantics) */
void test_percent_deadband_at_threshold_reportable(void) {
    mu_monitored_item_t item = make_percent_item(50.0, 100.0, 10.0);
    mu_variant_t cur = make_double(60.0);
    TEST_ASSERT_TRUE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

/* T013: first sample (has_reported=false) bypasses deadband per §7.22.2 */
void test_percent_deadband_first_sample_reportable(void) {
    mu_monitored_item_t item = make_percent_item(50.0, 100.0, 10.0);
    item.has_reported = false;
    mu_variant_t cur = make_double(50.0);
    TEST_ASSERT_TRUE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

/* T014: percent deadband with span=0 (no EURange) —
 * threshold computes to 0.0, so any positive change is reported.
 * The Bad_DeadbandFilterInvalid is raised at item creation, not at evaluation. */
void test_percent_deadband_zero_span_any_change_reportable(void) {
    mu_monitored_item_t item = make_percent_item(50.0, 0.0, 10.0);
    mu_variant_t cur = make_double(50.1);
    TEST_ASSERT_TRUE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

#else

void test_percent_deadband_requires_data_access_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_DATA_ACCESS is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_DATA_ACCESS
    RUN_TEST(test_percent_deadband_below_threshold_not_reportable);
    RUN_TEST(test_percent_deadband_above_threshold_reportable);
    RUN_TEST(test_percent_deadband_at_threshold_reportable);
    RUN_TEST(test_percent_deadband_first_sample_reportable);
    RUN_TEST(test_percent_deadband_zero_span_any_change_reportable);
#else
    RUN_TEST(test_percent_deadband_requires_data_access_build);
#endif
    return UNITY_END();
}
