/* tests/unit/test_event_filter_where.c
 *
 * Operator KATs for the EventFilter WhereClause (ContentFilter) evaluator
 * (spec 061). Each test hand-builds a compact mu_where_clause_t operand tree and
 * checks the verdict against synthetic event fields. Verdicts are hand-computed
 * (non-circular). OPC-10000-4 §7.7.
 */
#include "muc_opcua/config.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_EVENT_FILTER_WHERE

#include "services/event_filter.h"

static mu_event_fields_t make_event(opcua_uint16_t severity, const char *msg) {
    mu_event_fields_t f;
    (void)memset(&f, 0, sizeof(f));
    f.severity = severity;
    if (msg != NULL) {
        f.message.data = (const opcua_byte_t *)msg;
        f.message.length = (opcua_int32_t)strlen(msg);
    }
    f.event_type.namespace_index = 0;
    f.event_type.identifier_type = MU_NODEID_NUMERIC;
    f.event_type.identifier.numeric = 2041u; /* BaseEventType */
    return f;
}

/* One element with a Severity SimpleAttribute (op0) vs an integer literal (op1). */
static mu_where_clause_t severity_cmp(opcua_byte_t op, opcua_int64_t threshold) {
    mu_where_clause_t wc;
    (void)memset(&wc, 0, sizeof(wc));
    wc.element_count = 1;
    wc.operand_count = 2;
    wc.elements[0].op = op;
    wc.elements[0].operand_base = 0;
    wc.elements[0].operand_count = 2;
    wc.operands[0].kind = MU_WHERE_OPERAND_ATTRIBUTE;
    wc.operands[0].field = MU_EVENT_FIELD_SEVERITY;
    wc.operands[1].kind = MU_WHERE_OPERAND_LITERAL;
    wc.operands[1].lit_type = MU_TYPE_UINT16;
    wc.operands[1].num.i = threshold;
    return wc;
}

void test_empty_clause_matches_all(void) {
    mu_where_clause_t wc;
    (void)memset(&wc, 0, sizeof(wc));
    mu_event_fields_t e = make_event(500u, "x");
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &e));
    TEST_ASSERT_TRUE(mu_where_clause_eval(NULL, &e));
}

void test_equals(void) {
    mu_where_clause_t wc = severity_cmp(0u /* Equals */, 500);
    mu_event_fields_t hit = make_event(500u, "x");
    mu_event_fields_t miss = make_event(300u, "x");
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &hit));
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &miss));
}

void test_greater_than_and_gte(void) {
    mu_where_clause_t gt = severity_cmp(2u /* GreaterThan */, 500);
    mu_where_clause_t gte = severity_cmp(4u /* GreaterThanOrEqual */, 500);
    mu_event_fields_t at = make_event(500u, "x");
    mu_event_fields_t above = make_event(800u, "x");
    TEST_ASSERT_FALSE(mu_where_clause_eval(&gt, &at));
    TEST_ASSERT_TRUE(mu_where_clause_eval(&gt, &above));
    TEST_ASSERT_TRUE(mu_where_clause_eval(&gte, &at));
}

void test_less_than_and_lte(void) {
    mu_where_clause_t lt = severity_cmp(3u /* LessThan */, 100);
    mu_where_clause_t lte = severity_cmp(5u /* LessThanOrEqual */, 200);
    mu_event_fields_t e = make_event(200u, "x");
    TEST_ASSERT_FALSE(mu_where_clause_eval(&lt, &e));
    TEST_ASSERT_TRUE(mu_where_clause_eval(&lte, &e));
}

void test_isnull_on_unpopulated_field(void) {
    mu_where_clause_t wc;
    (void)memset(&wc, 0, sizeof(wc));
    wc.element_count = 1;
    wc.operand_count = 1;
    wc.elements[0].op = 1u; /* IsNull */
    wc.elements[0].operand_count = 1;
    wc.operands[0].kind = MU_WHERE_OPERAND_ATTRIBUTE;
    wc.operands[0].field = MU_EVENT_FIELD_SOURCENAME; /* not emitted → Null */
    mu_event_fields_t e = make_event(500u, "x");
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &e));
    /* Severity is populated → not null */
    wc.operands[0].field = MU_EVENT_FIELD_SEVERITY;
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &e));
}

void test_between(void) {
    mu_where_clause_t wc;
    (void)memset(&wc, 0, sizeof(wc));
    wc.element_count = 1;
    wc.operand_count = 3;
    wc.elements[0].op = 8u; /* Between */
    wc.elements[0].operand_count = 3;
    wc.operands[0].kind = MU_WHERE_OPERAND_ATTRIBUTE;
    wc.operands[0].field = MU_EVENT_FIELD_SEVERITY;
    wc.operands[1].kind = MU_WHERE_OPERAND_LITERAL;
    wc.operands[1].lit_type = MU_TYPE_UINT16;
    wc.operands[1].num.i = 400;
    wc.operands[2].kind = MU_WHERE_OPERAND_LITERAL;
    wc.operands[2].lit_type = MU_TYPE_UINT16;
    wc.operands[2].num.i = 600;
    mu_event_fields_t in = make_event(500u, "x");
    mu_event_fields_t out = make_event(700u, "x");
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &in));
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &out));
}

void test_and_or_not_tree(void) {
    /* element0 = AND(element1, element2); element1 = Severity>=500; element2 = Severity<=900 */
    mu_where_clause_t wc;
    (void)memset(&wc, 0, sizeof(wc));
    wc.element_count = 3;
    wc.operand_count = 6;
    /* element 0: AND(elem1, elem2) */
    wc.elements[0].op = 10u; /* And */
    wc.elements[0].operand_base = 0;
    wc.elements[0].operand_count = 2;
    wc.operands[0].kind = MU_WHERE_OPERAND_ELEMENT;
    wc.operands[0].element_index = 1;
    wc.operands[1].kind = MU_WHERE_OPERAND_ELEMENT;
    wc.operands[1].element_index = 2;
    /* element 1: Severity >= 500 */
    wc.elements[1].op = 4u;
    wc.elements[1].operand_base = 2;
    wc.elements[1].operand_count = 2;
    wc.operands[2].kind = MU_WHERE_OPERAND_ATTRIBUTE;
    wc.operands[2].field = MU_EVENT_FIELD_SEVERITY;
    wc.operands[3].kind = MU_WHERE_OPERAND_LITERAL;
    wc.operands[3].lit_type = MU_TYPE_UINT16;
    wc.operands[3].num.i = 500;
    /* element 2: Severity <= 900 */
    wc.elements[2].op = 5u;
    wc.elements[2].operand_base = 4;
    wc.elements[2].operand_count = 2;
    wc.operands[4].kind = MU_WHERE_OPERAND_ATTRIBUTE;
    wc.operands[4].field = MU_EVENT_FIELD_SEVERITY;
    wc.operands[5].kind = MU_WHERE_OPERAND_LITERAL;
    wc.operands[5].lit_type = MU_TYPE_UINT16;
    wc.operands[5].num.i = 900;

    mu_event_fields_t mid = make_event(700u, "x");
    mu_event_fields_t low = make_event(300u, "x");
    mu_event_fields_t high = make_event(1000u, "x");
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &mid));
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &low));
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &high));

    /* Flip root to OR: low (fails >=500 but passes <=900) now passes */
    wc.elements[0].op = 11u; /* Or */
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &low));

    /* NOT(element1): Severity>=500 negated */
    wc.element_count = 2;
    wc.elements[0].op = 7u; /* Not */
    wc.elements[0].operand_count = 1;
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &low));  /* 300 not >=500 → NOT true */
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &mid)); /* 700 >=500 → NOT false */
}

void test_like_message(void) {
    mu_where_clause_t wc;
    (void)memset(&wc, 0, sizeof(wc));
    wc.element_count = 1;
    wc.operand_count = 2;
    wc.blob_len = 6;
    memcpy(wc.blob, "%fault", 6);
    wc.elements[0].op = 6u; /* Like */
    wc.elements[0].operand_count = 2;
    wc.operands[0].kind = MU_WHERE_OPERAND_ATTRIBUTE;
    wc.operands[0].field = MU_EVENT_FIELD_MESSAGE;
    wc.operands[1].kind = MU_WHERE_OPERAND_LITERAL;
    wc.operands[1].lit_type = MU_TYPE_STRING;
    wc.operands[1].blob_off = 0;
    wc.operands[1].blob_len = 6;
    mu_event_fields_t hit = make_event(1u, "sensor fault");
    mu_event_fields_t miss = make_event(1u, "all good");
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &hit));
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &miss));
}

void test_oftype_exact(void) {
    mu_where_clause_t wc;
    (void)memset(&wc, 0, sizeof(wc));
    wc.element_count = 1;
    wc.operand_count = 1;
    wc.elements[0].op = 14u; /* OfType */
    wc.elements[0].operand_count = 1;
    wc.operands[0].kind = MU_WHERE_OPERAND_LITERAL;
    wc.operands[0].lit_type = MU_TYPE_NODEID;
    wc.operands[0].nodeid_ns = 0;
    wc.operands[0].num.nodeid_numeric = 2041u;     /* BaseEventType */
    mu_event_fields_t match = make_event(1u, "x"); /* event_type=2041 */
    mu_event_fields_t other = make_event(1u, "x");
    other.event_type.identifier.numeric = 2782u; /* ConditionType */
    TEST_ASSERT_TRUE(mu_where_clause_eval(&wc, &match));
    TEST_ASSERT_FALSE(mu_where_clause_eval(&wc, &other));
}

#else

void test_event_filter_where_requires_build_flag(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_EVENT_FILTER_WHERE is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_EVENT_FILTER_WHERE
    RUN_TEST(test_empty_clause_matches_all);
    RUN_TEST(test_equals);
    RUN_TEST(test_greater_than_and_gte);
    RUN_TEST(test_less_than_and_lte);
    RUN_TEST(test_isnull_on_unpopulated_field);
    RUN_TEST(test_between);
    RUN_TEST(test_and_or_not_tree);
    RUN_TEST(test_like_message);
    RUN_TEST(test_oftype_exact);
#else
    RUN_TEST(test_event_filter_where_requires_build_flag);
#endif
    return UNITY_END();
}
