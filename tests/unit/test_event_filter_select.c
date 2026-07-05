/* tests/unit/test_event_filter_select.c
 *
 * Spec Kit 037, T033 (RED phase). Validate select-clause extraction
 * from SimpleAttributeOperand BrowsePaths for all 9 BaseEventType fields
 * per OPC-10000-5 §6.4.2.
 */
#include "muc_opcua/types.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_EVENT_FILTER_WHERE

void test_select_clause_event_id_maps_to_0(void) {
    TEST_ASSERT_EQUAL_INT(0, mu_event_filter_resolve_select_clause("/EventId", 8));
}

void test_select_clause_event_type_maps_to_1(void) {
    TEST_ASSERT_EQUAL_INT(1, mu_event_filter_resolve_select_clause("/EventType", 9));
}

void test_select_clause_source_node_maps_to_2(void) {
    TEST_ASSERT_EQUAL_INT(2, mu_event_filter_resolve_select_clause("/SourceNode", 10));
}

void test_select_clause_source_name_maps_to_3(void) {
    TEST_ASSERT_EQUAL_INT(3, mu_event_filter_resolve_select_clause("/SourceName", 10));
}

void test_select_clause_time_maps_to_4(void) {
    TEST_ASSERT_EQUAL_INT(4, mu_event_filter_resolve_select_clause("/Time", 4));
}

void test_select_clause_receive_time_maps_to_5(void) {
    TEST_ASSERT_EQUAL_INT(5, mu_event_filter_resolve_select_clause("/ReceiveTime", 11));
}

void test_select_clause_local_time_maps_to_6(void) {
    TEST_ASSERT_EQUAL_INT(6, mu_event_filter_resolve_select_clause("/LocalTime", 9));
}

void test_select_clause_message_maps_to_7(void) {
    TEST_ASSERT_EQUAL_INT(7, mu_event_filter_resolve_select_clause("/Message", 7));
}

void test_select_clause_severity_maps_to_8(void) {
    TEST_ASSERT_EQUAL_INT(8, mu_event_filter_resolve_select_clause("/Severity", 8));
}

void test_select_clause_unknown_path_returns_minus_1(void) {
    TEST_ASSERT_EQUAL_INT(-1, mu_event_filter_resolve_select_clause("/Unknown", 7));
}

#else

void test_select_clause_requires_event_filter_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_EVENT_FILTER_WHERE is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_EVENT_FILTER_WHERE
    RUN_TEST(test_select_clause_event_id_maps_to_0);
    RUN_TEST(test_select_clause_event_type_maps_to_1);
    RUN_TEST(test_select_clause_source_node_maps_to_2);
    RUN_TEST(test_select_clause_source_name_maps_to_3);
    RUN_TEST(test_select_clause_time_maps_to_4);
    RUN_TEST(test_select_clause_receive_time_maps_to_5);
    RUN_TEST(test_select_clause_local_time_maps_to_6);
    RUN_TEST(test_select_clause_message_maps_to_7);
    RUN_TEST(test_select_clause_severity_maps_to_8);
    RUN_TEST(test_select_clause_unknown_path_returns_minus_1);
#else
    RUN_TEST(test_select_clause_requires_event_filter_build);
#endif
    return UNITY_END();
}
