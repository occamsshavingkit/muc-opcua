/* tests/unit/test_event_serializer.c */
#include "../../src/core/server_internal.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/status.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_event_queue_circular_buffer(void) {
#ifdef MUC_OPCUA_EVENTS
    mu_subscriptions_t subs;
    mu_subscriptions_init(&subs);

    mu_subscription_t *sub = NULL;
    opcua_statuscode_t status = mu_subscription_create(&subs, 1u, 100u, 30u, 10u, 0u, 0u, true, 0u, &sub);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_NOT_NULL(sub);

    /* Construct a server shell containing the subscriptions */
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.subs = subs;

    /* Enqueue 8 events, verifying counts */
    for (uint16_t i = 0; i < 8; ++i) {
        mu_event_notification_t ev;
        (void)memset(&ev, 0, sizeof(ev));
        ev.severity = i + 10;

        status = mu_server_trigger_event(&server, &ev);
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);

        /* The queue of the active subscription should reflect the event */
        mu_subscription_t *active_sub = &server.subs.subscriptions[sub - subs.subscriptions];
        TEST_ASSERT_EQUAL(i + 1, active_sub->event_queue.count);
        TEST_ASSERT_EQUAL(0, active_sub->event_queue.head);
        TEST_ASSERT_EQUAL((i + 1) % 8, active_sub->event_queue.tail);
    }

    /* Enqueue 9th event: should discard oldest event (severity 10) and enqueue the new one (severity 18) */
    mu_event_notification_t ev9;
    (void)memset(&ev9, 0, sizeof(ev9));
    ev9.severity = 18;
    status = mu_server_trigger_event(&server, &ev9);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);

    mu_subscription_t *active_sub = &server.subs.subscriptions[sub - subs.subscriptions];
    TEST_ASSERT_EQUAL(8, active_sub->event_queue.count);
    TEST_ASSERT_EQUAL(1, active_sub->event_queue.head);
    TEST_ASSERT_EQUAL(1, active_sub->event_queue.tail);
    TEST_ASSERT_EQUAL(11, active_sub->event_queue.queue[active_sub->event_queue.head].severity);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_event_queue_circular_buffer);
    return UNITY_END();
}
