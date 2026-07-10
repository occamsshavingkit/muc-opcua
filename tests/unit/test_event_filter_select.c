/* tests/unit/test_event_filter_select.c
 *
 * Validate the unified SimpleAttributeOperand BrowseName → BaseEventType field
 * index resolver (spec 061 consolidation) for all 9 fields, OPC-10000-5 §6.4.2.
 */
#include "muc_opcua/config.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MUC_OPCUA_EVENTS

#include "services/event_filter.h"

static mu_event_field_t resolve(const char *name) {
    mu_string_t s;
    s.data = (const opcua_byte_t *)name;
    s.length = (opcua_int32_t)strlen(name);
    return mu_event_field_from_name(&s);
}

void test_resolve_all_base_event_fields(void) {
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_EVENTID, resolve("EventId"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_EVENTTYPE, resolve("EventType"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_SOURCENODE, resolve("SourceNode"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_SOURCENAME, resolve("SourceName"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_TIME, resolve("Time"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_RECEIVETIME, resolve("ReceiveTime"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_LOCALTIME, resolve("LocalTime"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_MESSAGE, resolve("Message"));
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_SEVERITY, resolve("Severity"));
}

void test_resolve_field_index_values(void) {
    /* Indices must be the stable 0-8 order (OPC-10000-5 §6.4.2). */
    TEST_ASSERT_EQUAL_INT(0, resolve("EventId"));
    TEST_ASSERT_EQUAL_INT(8, resolve("Severity"));
}

void test_resolve_unknown_returns_none(void) {
    TEST_ASSERT_EQUAL_INT(MU_EVENT_FIELD_NONE, resolve("Unknown"));
}

#else

void test_select_clause_requires_events_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_EVENTS is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_EVENTS
    RUN_TEST(test_resolve_all_base_event_fields);
    RUN_TEST(test_resolve_field_index_values);
    RUN_TEST(test_resolve_unknown_returns_none);
#else
    RUN_TEST(test_select_clause_requires_events_build);
#endif
    return UNITY_END();
}
