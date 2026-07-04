/* tests/unit/test_subscription_publish.c
 *
 * Feature 031 US2 (T002) — RED-phase test for the publish-timer loop bound.
 *
 * `advance_publish_timer` (src/services/subscription.c) advances a
 * Subscription's next_publish_ms by the revised publishing interval until it
 * moves past the current monotonic tick. When the revised interval is 0 the
 * implementation clamps it to 1 ms; without an iteration cap a far-future
 * `now_ms` would spin the do/while nearly forever (only the UINT64 overflow
 * guard at the top of the loop body ends it). T007 adds a 100-iteration bound.
 *
 * This test verifies the bound directly and is written to FAIL before T007
 * (the unbounded loop advances next_publish_ms past now_ms, so the bounded
 * assertion does not hold) and PASS after T007 (the loop stops at ≤100
 * iterations, leaving next_publish_ms ≤ now_ms).
 *
 * Grounding: OPC-10000-4 §5.13.1.2 (Publish Service Set — publishing timer),
 * mirrored by the Subscription state machine in §5.14.1.4
 * (StartPublishingTimer / ResetLifetimeCounter).
 */
#include "unity.h"

#include "../../src/core/server_internal.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/status.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS

/* The 100-iteration bound T007 introduces. With publishing_interval clamped to
 * 1 ms, at most MU_PUBLISH_TIMER_MAX_ITERATIONS ms of forward advance can occur
 * per call, regardless of how far ahead `now_ms` is. */
#define MU_PUBLISH_TIMER_MAX_ITERATIONS 100u

/* OPC-10000-4 §5.13.1.2: a 0 (revised) publishing interval is degenerate; the
 * engine treats it as a 1 ms tick. A far-future now_ms with no loop bound would
 * spin once per millisecond up to now_ms. */
void test_advance_publish_timer_terminates_within_bound_when_interval_is_zero(void) {
    mu_subscription_t sub;
    memset(&sub, 0, sizeof(sub));

    sub.publishing_interval_ms = 0u; /* clamped to 1 ms internally */
    sub.next_publish_ms = 0u;

    /* now_ms deliberately far ahead of next_publish_ms + 100*interval so that an
     * unbounded loop would keep advancing (RED), while a bounded loop stops at
     * 100 iterations (GREEN). now_ms is kept small enough that the RED path
     * finishes quickly instead of hanging the suite. */
    const opcua_uint64_t now_ms = 1000u;

    advance_publish_timer(&sub, now_ms);

    /* After T007 the loop is capped at MU_PUBLISH_TIMER_MAX_ITERATIONS
     * iterations of the (1 ms) interval, so next_publish_ms cannot exceed 100.
     * Before T007 the unbounded loop advances next_publish_ms to now_ms + 1
     * (1001), failing this assertion (RED). */
    TEST_ASSERT_LESS_OR_EQUAL_UINT64((opcua_uint64_t)MU_PUBLISH_TIMER_MAX_ITERATIONS, sub.next_publish_ms);

    /* And it must not have run away past now_ms — the visible signature of the
     * bound kicking in. */
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(now_ms, sub.next_publish_ms);
}

/* Sanity check: with a normal interval and a small now_ms gap, the loop still
 * advances exactly one interval past now_ms (the pre-existing, intended
 * behavior must be preserved by T007). */
void test_advance_publish_timer_advances_one_interval_on_normal_gap(void) {
    mu_subscription_t sub;
    memset(&sub, 0, sizeof(sub));

    sub.publishing_interval_ms = 50u;
    sub.next_publish_ms = 100u;

    advance_publish_timer(&sub, 110u);

    TEST_ASSERT_EQUAL_UINT64(150u, sub.next_publish_ms);
}

#else

void test_subscription_publish_requires_subscriptions_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS
    RUN_TEST(test_advance_publish_timer_terminates_within_bound_when_interval_is_zero);
    RUN_TEST(test_advance_publish_timer_advances_one_interval_on_normal_gap);
#else
    RUN_TEST(test_subscription_publish_requires_subscriptions_build);
#endif
    return UNITY_END();
}
