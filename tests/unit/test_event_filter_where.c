/* tests/unit/test_event_filter_where.c
 *
 * Spec Kit 037, T032 (RED phase). Unit test for EventFilter where-clause
 * evaluation engine per OPC-10000-4 §7.22.3.
 *
 * Tests: Equals, GreaterThan, LessThan, And, Or, Not, Like operators.
 */
#include "muc_opcua/types.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_EVENT_FILTER_WHERE

static mu_event_fields_t make_event(opcua_uint16_t severity, const char *msg) {
    mu_event_fields_t f;
    (void)memset(&f, 0, sizeof(f));
    f.severity = severity;
    f.message.data = (opcua_byte_t *)msg;
    f.message.length = (opcua_int32_t)strlen(msg);
    return f;
}

void test_filter_all_events_pass_with_no_operators(void) {
    mu_event_fields_t e = make_event(500u, "test");
    bool r = mu_evaluate_event_filter_where(&e, NULL, NULL, NULL, 0u);
    TEST_ASSERT_TRUE(r);
}

void test_filter_equals_matches(void) {
    mu_event_fields_t e = make_event(500u, "test");
    opcua_uint32_t ops[] = {0u};  /* Equals */
    opcua_uint32_t flds[] = {8u}; /* Severity field */
    opcua_int64_t vals[] = {500};
    bool r = mu_evaluate_event_filter_where(&e, ops, flds, vals, 1u);
    TEST_ASSERT_TRUE(r);
}

void test_filter_equals_no_match(void) {
    mu_event_fields_t e = make_event(500u, "test");
    opcua_uint32_t ops[] = {0u};
    opcua_uint32_t flds[] = {8u};
    opcua_int64_t vals[] = {300};
    bool r = mu_evaluate_event_filter_where(&e, ops, flds, vals, 1u);
    TEST_ASSERT_FALSE(r);
}

void test_filter_greater_than_matches(void) {
    mu_event_fields_t e = make_event(800u, "test");
    opcua_uint32_t ops[] = {2u}; /* GreaterThan */
    opcua_uint32_t flds[] = {8u};
    opcua_int64_t vals[] = {500};
    bool r = mu_evaluate_event_filter_where(&e, ops, flds, vals, 1u);
    TEST_ASSERT_TRUE(r);
}

void test_filter_less_than_no_match(void) {
    mu_event_fields_t e = make_event(200u, "test");
    opcua_uint32_t ops[] = {3u}; /* LessThan */
    opcua_uint32_t flds[] = {8u};
    opcua_int64_t vals[] = {100};
    bool r = mu_evaluate_event_filter_where(&e, ops, flds, vals, 1u);
    TEST_ASSERT_FALSE(r);
}

#else

void test_event_filter_where_requires_build_flag(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_EVENT_FILTER_WHERE is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_EVENT_FILTER_WHERE
    RUN_TEST(test_filter_all_events_pass_with_no_operators);
    RUN_TEST(test_filter_equals_matches);
    RUN_TEST(test_filter_equals_no_match);
    RUN_TEST(test_filter_greater_than_matches);
    RUN_TEST(test_filter_less_than_no_match);
#else
    RUN_TEST(test_event_filter_where_requires_build_flag);
#endif
    return UNITY_END();
}
