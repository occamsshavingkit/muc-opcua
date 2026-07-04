/* tests/unit/test_subscription_deadband.c
 *
 * Spec Kit 031, T001 (RED phase). Unit test for the deadband NONE semantics
 * of the static helper monitored_item_change_reportable().
 *
 * Grounding: OPC-10000-4 §7.22.2 DataChangeFilter.deadbandType.
 *   DeadbandType None (0) = "No Deadband calculation should be applied." A
 *   DataChange monitored item with trigger including value changes reports
 *   only on an actual value change. The audit (T27) found that the helper
 *   early-returns true for deadband_type != ABSOLUTE, so NONE reports even
 *   when the value is unchanged. T006 fixes it to fall through to a value
 *   equality comparison.
 *
 * Access note: monitored_item_change_reportable() has internal linkage, and
 * the live tick path can never reach it with an unchanged value (the caller
 * is gated by monitored_item_change_changed(), which already filters equal
 * samples). The semantics can therefore only be exercised by calling the
 * helper directly. To do so, subscription.c is compiled into this translation
 * unit; every external-linkage symbol it defines is aliased to a local name so
 * it cannot clash with the same symbols provided by the linked muc_opcua
 * library. The static helpers (including the one under test) are then visible
 * to this TU. Add new public functions in subscription.c to the alias block
 * below if a multiple-definition link error appears.
 */
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD

#define mu_server_trigger_event               deadband_local_trigger_event
#define advance_publish_timer                 deadband_local_advance_publish_timer
#define mu_subscriptions_init                 deadband_local_subs_init
#define mu_publish_request_enqueue            deadband_local_pub_enqueue
#define mu_subscription_create                deadband_local_sub_create
#define mu_subscription_apply_parameters      deadband_local_sub_apply
#define mu_subscription_find                  deadband_local_sub_find
#define mu_subscription_acknowledge           deadband_local_sub_ack
#define mu_subscription_republish             deadband_local_sub_republish
#define mu_subscription_delete                deadband_local_sub_delete
#define mu_monitored_item_alloc               deadband_local_mi_alloc
#define mu_monitored_item_delete              deadband_local_mi_delete
#define mu_monitored_item_add_trigger_link    deadband_local_mi_add_link
#define mu_monitored_item_remove_trigger_link deadband_local_mi_rm_link
#define mu_subscription_get_monitored_items   deadband_local_sub_get_items
#define mu_subscription_request_resend_data   deadband_local_sub_resend
#define mu_subscriptions_tick                 deadband_local_subs_tick

#include "../../src/services/subscription.c"

static mu_variant_t make_int32(opcua_int32_t v) {
    mu_variant_t out;
    (void)memset(&out, 0, sizeof(out));
    out.type = MU_TYPE_INT32;
    out.value.i32 = v;
    return out;
}

/* Build a monitored item that has already reported `last`, with deadband NONE
 * and GOOD status — the preconditions required to reach the deadband clause. */
static mu_monitored_item_t make_none_item(opcua_int32_t last) {
    mu_monitored_item_t item;
    (void)memset(&item, 0, sizeof(item));
    item.has_reported = true;
    item.last_status = MU_STATUS_GOOD;
    item.deadband_type = MU_DEADBAND_TYPE_NONE;
    item.last_value = make_int32(last);
    item.last_reported_numeric = (opcua_double_t)last;
    return item;
}

void test_deadband_none_first_sample_is_reportable(void) {
    mu_monitored_item_t item = make_none_item(42);
    item.has_reported = false;
    mu_variant_t cur = make_int32(42);
    /* First report after create is always reportable (§7.22.2 baseline). */
    TEST_ASSERT_TRUE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

void test_deadband_none_unchanged_value_is_not_reportable(void) {
    mu_monitored_item_t item = make_none_item(42);
    mu_variant_t cur = make_int32(42);
    /* RED until T006: with None deadband an unchanged value must NOT be
     * reported. The current code returns true here (the bug). */
    TEST_ASSERT_FALSE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

void test_deadband_none_changed_value_is_reportable(void) {
    mu_monitored_item_t item = make_none_item(42);
    mu_variant_t cur = make_int32(99);
    /* A genuine value change is always reportable under None deadband. */
    TEST_ASSERT_TRUE(monitored_item_change_reportable(&item, &cur, MU_STATUS_GOOD));
}

#else

void test_deadband_requires_standard_subscription_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS_STANDARD is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    RUN_TEST(test_deadband_none_first_sample_is_reportable);
    RUN_TEST(test_deadband_none_unchanged_value_is_not_reportable);
    RUN_TEST(test_deadband_none_changed_value_is_reportable);
#else
    RUN_TEST(test_deadband_requires_standard_subscription_build);
#endif
    return UNITY_END();
}
