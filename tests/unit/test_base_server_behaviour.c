/* tests/unit/test_base_server_behaviour.c
 *
 * Base Server Behaviour Facet (spec 064). Conformance evidence for the MANDATORY
 * "Session General Service Behaviour" CU of the Core 2017 Server Facet
 * (OPC profile-DB id 1673, isOptional=false): "checking the authentication token,
 * returning the requestHandle in responses, respecting a timeoutHint".
 *
 * The auth-token rejection matrix and the requestHandle echo are already pinned by
 * existing suites (mapped in docs/conformance/base-server-behaviour.md):
 *   - auth token: test_service_state_errors.c (token=0 / unknown -> Bad_SessionIdInvalid,
 *     not-activated -> Bad_SessionNotActivated), test_dispatch_services.c
 *     (closed session token -> Bad_SessionClosed).
 *   - requestHandle echo: test_write_response.c (ResponseHeader echoes request_handle).
 *
 * This file pins the remaining sub-requirement — respecting a timeoutHint — which had
 * no direct test: an expired parked Publish request is dropped (a Bad_Timeout fault is
 * emitted and the request is not answered from the queue). Behaviour lives in
 * publish_request_dequeue_valid (src/services/subscription_publish/publish_due.c).
 */
#include "unity.h"

#include "../../src/core/server_internal.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/status.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS

#include "../../src/services/subscription_publish/common.h"

/* Park a Publish request at now=enqueued_ms with the given timeoutHint, then attempt to
   dequeue it at now=dequeue_ms. Returns publish_request_dequeue_valid's verdict. */
static bool park_then_dequeue(opcua_uint64_t enqueued_ms, opcua_uint32_t timeout_hint, opcua_uint64_t dequeue_ms) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    mu_subscriptions_init(&server.subs);

    mu_publish_request_t *parked = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_publish_request_enqueue(&server.subs, 1u, 10u, 20u, enqueued_ms, &parked));
    TEST_ASSERT_NOT_NULL(parked);
    parked->timeout_hint = timeout_hint;

    mu_publish_request_t out;
    (void)memset(&out, 0, sizeof(out));
    return publish_request_dequeue_valid(&server, 1u, dequeue_ms, &out);
}

/* Within the timeoutHint window the parked request is still valid (returned to the
   caller to be answered). Deadline = enqueued_ms + timeout_hint. */
void test_publish_within_timeout_hint_is_valid(void) {
    TEST_ASSERT_TRUE(park_then_dequeue(1000u, 100u, 1050u));
}

/* Past the timeoutHint window the parked request is dropped (invalid) — the server
   respects timeoutHint (OPC-10000-4 §7.32; Session General Service Behaviour CU). */
void test_publish_past_timeout_hint_is_dropped(void) {
    TEST_ASSERT_FALSE(park_then_dequeue(1000u, 100u, 1200u));
}

/* timeoutHint == 0 means "no timeout": never dropped, however late. */
void test_publish_zero_timeout_hint_never_expires(void) {
    TEST_ASSERT_TRUE(park_then_dequeue(1000u, 0u, 9999u));
}

#else

void test_base_server_behaviour_requires_subscriptions(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS
    RUN_TEST(test_publish_within_timeout_hint_is_valid);
    RUN_TEST(test_publish_past_timeout_hint_is_dropped);
    RUN_TEST(test_publish_zero_timeout_hint_never_expires);
#else
    RUN_TEST(test_base_server_behaviour_requires_subscriptions);
#endif
    return UNITY_END();
}
