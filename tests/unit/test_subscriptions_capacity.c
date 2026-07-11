/* tests/unit/test_subscriptions_capacity.c
 *
 * Feature 005 US1 capacity checks for the Standard DataChange Subscription
 * 2017 Server Facet. These tests are active only when the Standard facet is
 * compiled in; the default micro/nano test build leaves them as no-ops.
 *
 * OPC-10000-4 5.13.2.4: Bad_TooManyMonitoredItems.
 * OPC-10000-4 5.13.5: SetTriggering link storage is bounded locally.
 * OPC-10000-4 5.14.2: Bad_TooManySubscriptions.
 * OPC-10000-4 5.14.5: Bad_TooManyPublishRequests.
 */
#include "unity.h"

#include "../../src/core/server_internal.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/status.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD

#define STATUS_INFO_OVERFLOW 0x00000480u

static void assert_standard_capacity_configuration(void) {
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MAX_SUBSCRIPTIONS >= 2u, "Standard facet requires at least 2 subscriptions");
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MAX_MONITORED_ITEMS >= 100u,
                             "Standard facet requires at least 100 monitored items");
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MAX_PUBLISH_REQUESTS >= 5u,
                             "Standard facet requires at least 5 parked Publish requests");
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MONITORED_QUEUE_DEPTH >= 2u,
                             "Standard facet requires monitored-item queue depth at least 2");
}

static opcua_uint32_t count_active_subscriptions(const mu_subscriptions_t *subs) {
    opcua_uint32_t count = 0u;
    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
        if (subs->subscriptions[i].in_use) {
            ++count;
        }
    }
    return count;
}

static opcua_uint32_t count_active_monitored_items(const mu_subscriptions_t *subs) {
    opcua_uint32_t count = 0u;
    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_MONITORED_ITEMS; ++i) {
        if (subs->monitored_items[i].in_use) {
            ++count;
        }
    }
    return count;
}

static opcua_uint32_t count_queued_publish_requests(const mu_subscriptions_t *subs) {
    opcua_uint32_t count = 0u;
    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_PUBLISH_REQUESTS; ++i) {
        if (subs->publish_queue[i].in_use) {
            ++count;
        }
    }
    return count;
}

static opcua_int32_t s_queue_value;

static opcua_statuscode_t read_queue_value(void *context, const mu_nodeid_t *node_id, mu_variant_t *value) {
    (void)context;
    (void)node_id;
    value->type = MU_TYPE_INT32;
    value->value.i32 = s_queue_value;
    value->is_array = false;
    value->array_length = 0;
    return MU_STATUS_GOOD;
}

void test_standard_capacity_macros_meet_profile_minimums(void) {
    assert_standard_capacity_configuration();
}

void test_accepts_required_subscriptions_and_rejects_capacity_exhaustion_without_storage_growth(void) {
    mu_subscriptions_t subs;
    mu_subscriptions_init(&subs);
    assert_standard_capacity_configuration();

    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = NULL;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                                mu_subscription_create(&subs, 1u, 100u, 30u, 10u, 0u, 0u, true, 0u, &sub));
        TEST_ASSERT_NOT_NULL(sub);
    }

    mu_subscription_t *extra = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYSUBSCRIPTIONS,
                            mu_subscription_create(&subs, 1u, 100u, 30u, 10u, 0u, 0u, true, 0u, &extra));
    TEST_ASSERT_NULL(extra);
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_SUBSCRIPTIONS, count_active_subscriptions(&subs));
}

void test_accepts_required_monitored_items_and_rejects_capacity_exhaustion_without_storage_growth(void) {
    mu_subscriptions_t subs;
    mu_subscriptions_init(&subs);
    assert_standard_capacity_configuration();

    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = NULL;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&subs, 1u, &item));
        TEST_ASSERT_NOT_NULL(item);
        TEST_ASSERT_NOT_EQUAL(0u, item->monitored_item_id);
    }
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_MONITORED_ITEMS, count_active_monitored_items(&subs));
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_MONITORED_ITEMS, subs.active_monitored_items_count);

    mu_monitored_item_t *extra = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYMONITOREDITEMS, mu_monitored_item_alloc(&subs, 1u, &extra));
    TEST_ASSERT_NULL(extra);
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_MONITORED_ITEMS, count_active_monitored_items(&subs));
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_MONITORED_ITEMS, subs.active_monitored_items_count);
}

void test_set_triggering_rejects_link_capacity_exhaustion_without_corrupting_existing_links(void) {
    mu_subscriptions_t subs;
    mu_subscriptions_init(&subs);
    assert_standard_capacity_configuration();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(MU_INTERN_MAX_TRIGGER_LINKS + 2u, MU_INTERN_MAX_MONITORED_ITEMS);

    mu_subscription_t *sub = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_subscription_create(&subs, 1u, 100u, 30u, 10u, 0u, 0u, true, 0u, &sub));
    TEST_ASSERT_NOT_NULL(sub);

    mu_monitored_item_t *triggering_item = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&subs, sub->subscription_id, &triggering_item));
    TEST_ASSERT_NOT_NULL(triggering_item);

    opcua_uint32_t linked_ids[MU_INTERN_MAX_TRIGGER_LINKS + 1u];
    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_TRIGGER_LINKS + 1u; ++i) {
        mu_monitored_item_t *linked_item = NULL;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&subs, sub->subscription_id, &linked_item));
        TEST_ASSERT_NOT_NULL(linked_item);
        linked_ids[i] = linked_item->monitored_item_id;
    }

    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_TRIGGER_LINKS; ++i) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                                mu_monitored_item_add_trigger_link(&subs, sub->subscription_id,
                                                                   triggering_item->monitored_item_id, linked_ids[i]));
        TEST_ASSERT_EQUAL_UINT32(i + 1u, triggering_item->triggered_count);
    }

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS,
                            mu_monitored_item_add_trigger_link(&subs, sub->subscription_id,
                                                               triggering_item->monitored_item_id,
                                                               linked_ids[MU_INTERN_MAX_TRIGGER_LINKS]));
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_TRIGGER_LINKS, triggering_item->triggered_count);
    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_TRIGGER_LINKS; ++i) {
        TEST_ASSERT_EQUAL_UINT32(linked_ids[i], triggering_item->triggered_items[i]);
    }
}

void test_rejects_publish_request_queue_capacity_exhaustion_without_storage_growth(void) {
    mu_subscriptions_t subs;
    mu_subscriptions_init(&subs);
    assert_standard_capacity_configuration();

    for (opcua_uint32_t i = 0u; i < MU_INTERN_MAX_PUBLISH_REQUESTS; ++i) {
        mu_publish_request_t *request = NULL;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_publish_request_enqueue(&subs, 1u, 100u + i, 200u + i, i, &request));
        TEST_ASSERT_NOT_NULL(request);
    }
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_PUBLISH_REQUESTS, count_queued_publish_requests(&subs));

    mu_publish_request_t *extra = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYPUBLISHREQUESTS,
                            mu_publish_request_enqueue(&subs, 1u, 999u, 999u, 999u, &extra));
    TEST_ASSERT_NULL(extra);
    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MAX_PUBLISH_REQUESTS, count_queued_publish_requests(&subs));
}

void test_monitored_item_queue_clamps_to_fixed_depth_and_marks_overflow(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    mu_subscriptions_init(&server.subs);
    assert_standard_capacity_configuration();

    mu_subscription_t *sub = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_subscription_create(&server.subs, 1u, 10000u, 30u, 10u, 0u, 0u, true, 0u, &sub));
    TEST_ASSERT_NOT_NULL(sub);

    mu_value_source_t value_source = {MU_VALUESOURCE_CALLBACK, {.callback = {read_queue_value, NULL}}};
    mu_node_t node = {{1u, MU_NODEID_NUMERIC, {.numeric = 1000u}},
                      MU_NODECLASS_VARIABLE,
                      {0, NULL},
                      {0, NULL},
                      NULL,
                      0u,
                      &value_source};

    mu_monitored_item_t *item = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, sub->subscription_id, &item));
    TEST_ASSERT_NOT_NULL(item);
    item->resolved_node = &node;
    item->node_id = node.node_id;
    item->attribute_id = 13u;
    item->sampling_interval_ms = 1u;
    item->monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
    item->queue_size = MU_INTERN_MONITORED_QUEUE_DEPTH + 4u;
    item->discard_oldest = true;
    item->last_status = MU_STATUS_GOOD;
    item->last_value.type = MU_TYPE_INT32;
    item->last_value.value.i32 = 0;
    item->has_value = true;
    item->has_reported = true;
    item->next_sample_ms = 1u;

    for (opcua_uint32_t i = 1u; i <= MU_INTERN_MONITORED_QUEUE_DEPTH + 1u; ++i) {
        s_queue_value = (opcua_int32_t)i;
        mu_subscriptions_tick(&server, i);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(MU_INTERN_MONITORED_QUEUE_DEPTH, item->queue_count);
    }

    TEST_ASSERT_EQUAL_UINT32(MU_INTERN_MONITORED_QUEUE_DEPTH, item->queue_size);
    TEST_ASSERT_EQUAL_UINT8(MU_INTERN_MONITORED_QUEUE_DEPTH, item->queue_count);
    TEST_ASSERT_TRUE(item->pending);
    TEST_ASSERT_TRUE(item->queue_overflow);
    TEST_ASSERT_EQUAL_INT32(2, item->queue[item->queue_head].value.value.i32);
    TEST_ASSERT_EQUAL_HEX32(STATUS_INFO_OVERFLOW, item->queue[item->queue_head].status & STATUS_INFO_OVERFLOW);
}

#if MUC_OPCUA_ENHANCED_DATACHANGE
/* Enhanced DataChange Subscription 2017 Server Facet (profile-DB id 1678), mandated
   by the StandardUA2017 profile our standard/full builds advertise. All four minima
   are grounded from the OPC profile DB; see docs/conformance/enhanced-datachange.md. */
static void assert_enhanced_capacity_configuration(void) {
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MAX_MONITORED_ITEMS >= 500u,
                             "Enhanced facet requires at least 500 MonitoredItems per Subscription");
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MONITORED_QUEUE_DEPTH >= 5u,
                             "Enhanced facet requires monitored-item queue depth at least 5 (MinQueueSize_05)");
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MAX_SUBSCRIPTIONS >= 5u,
                             "Enhanced facet requires at least 5 Subscriptions per Session");
    TEST_ASSERT_TRUE_MESSAGE(MU_INTERN_MAX_PUBLISH_REQUESTS >= 10u,
                             "Enhanced facet requires at least 10 parked Publish requests");
}

void test_enhanced_capacity_macros_meet_profile_minimums(void) {
    assert_enhanced_capacity_configuration();
}

/* Behavioral proof of MinQueueSize_05: a monitored item whose client-requested queue
   size is 5 must actually retain 5 distinct samples before discarding — not clamp to the
   Standard-tier depth of 2. Fills the queue one sample per tick and asserts the fifth
   sample is held with no overflow, then the sixth overflows (discard_oldest). */
void test_enhanced_monitored_item_queue_holds_five_before_overflow(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    mu_subscriptions_init(&server.subs);
    assert_enhanced_capacity_configuration();

    mu_subscription_t *sub = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_subscription_create(&server.subs, 1u, 10000u, 30u, 10u, 0u, 0u, true, 0u, &sub));
    TEST_ASSERT_NOT_NULL(sub);

    mu_value_source_t value_source = {MU_VALUESOURCE_CALLBACK, {.callback = {read_queue_value, NULL}}};
    mu_node_t node = {{1u, MU_NODEID_NUMERIC, {.numeric = 1000u}},
                      MU_NODECLASS_VARIABLE,
                      {0, NULL},
                      {0, NULL},
                      NULL,
                      0u,
                      &value_source};

    mu_monitored_item_t *item = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, sub->subscription_id, &item));
    TEST_ASSERT_NOT_NULL(item);
    item->resolved_node = &node;
    item->node_id = node.node_id;
    item->attribute_id = 13u;
    item->sampling_interval_ms = 1u;
    item->monitoring_mode = MU_MONITORING_MODE_REPORTING;
    item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
    item->queue_size = 5u; /* client requests the Enhanced minimum */
    item->discard_oldest = true;
    item->last_status = MU_STATUS_GOOD;
    item->last_value.type = MU_TYPE_INT32;
    item->last_value.value.i32 = 0;
    item->has_value = true;
    item->has_reported = true;
    item->next_sample_ms = 1u;

    /* Five distinct samples fit without overflow. */
    for (opcua_uint32_t i = 1u; i <= 5u; ++i) {
        s_queue_value = (opcua_int32_t)i;
        mu_subscriptions_tick(&server, i);
    }
    TEST_ASSERT_EQUAL_UINT32(5u, item->queue_size);
    TEST_ASSERT_EQUAL_UINT8(5u, item->queue_count);
    TEST_ASSERT_FALSE(item->queue_overflow);

    /* The sixth sample overflows the depth-5 queue. */
    s_queue_value = 6;
    mu_subscriptions_tick(&server, 6u);
    TEST_ASSERT_EQUAL_UINT8(5u, item->queue_count);
    TEST_ASSERT_TRUE(item->queue_overflow);
    TEST_ASSERT_EQUAL_HEX32(STATUS_INFO_OVERFLOW, item->queue[item->queue_head].status & STATUS_INFO_OVERFLOW);
}
#endif /* MUC_OPCUA_ENHANCED_DATACHANGE */

#else

void test_standard_capacity_tests_require_standard_subscription_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS_STANDARD is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    RUN_TEST(test_standard_capacity_macros_meet_profile_minimums);
    RUN_TEST(test_accepts_required_subscriptions_and_rejects_capacity_exhaustion_without_storage_growth);
    RUN_TEST(test_accepts_required_monitored_items_and_rejects_capacity_exhaustion_without_storage_growth);
    RUN_TEST(test_set_triggering_rejects_link_capacity_exhaustion_without_corrupting_existing_links);
    RUN_TEST(test_rejects_publish_request_queue_capacity_exhaustion_without_storage_growth);
    RUN_TEST(test_monitored_item_queue_clamps_to_fixed_depth_and_marks_overflow);
#if MUC_OPCUA_ENHANCED_DATACHANGE
    RUN_TEST(test_enhanced_capacity_macros_meet_profile_minimums);
    RUN_TEST(test_enhanced_monitored_item_queue_holds_five_before_overflow);
#endif
#else
    RUN_TEST(test_standard_capacity_tests_require_standard_subscription_build);
#endif
    return UNITY_END();
}
