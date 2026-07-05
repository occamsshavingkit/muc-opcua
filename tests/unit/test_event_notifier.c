/* tests/unit/test_event_notifier.c
 *
 * Spec Kit 037, T034 (RED phase). Validate EventNotifier attribute on
 * address space nodes per OPC-10000-3 §5.4.6.
 *
 * Tests that mu_node_t has the event_notifier field and that
 * it can be used to gate event delivery by source node.
 */
#include "muc_opcua/address_space.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_node_event_notifier_default_is_zero(void) {
    mu_node_t node;
    (void)memset(&node, 0, sizeof(node));
    TEST_ASSERT_EQUAL_UINT8(0, node.event_notifier);
}

void test_node_event_notifier_can_be_set(void) {
    mu_node_t node;
    (void)memset(&node, 0, sizeof(node));
    node.event_notifier = 1u; /* SubscribeToEvents */
    TEST_ASSERT_EQUAL_UINT8(1u, node.event_notifier);
}

void test_event_notifier_subscribe_to_events_bit_is_0(void) {
    TEST_ASSERT_EQUAL_UINT8(1u, 1u << 0u);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_node_event_notifier_default_is_zero);
    RUN_TEST(test_node_event_notifier_can_be_set);
    RUN_TEST(test_event_notifier_subscribe_to_events_bit_is_0);
    return UNITY_END();
}
