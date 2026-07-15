/* tests/unit/test_session_auth.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

#include "../../src/services/session.h"

void setUp(void) {}
void tearDown(void) {}

static opcua_uint64_t bits(double d) {
    opcua_uint64_t b;
    (void)memcpy(&b, &d, sizeof(b));
    return b;
}

void test_session_auth_anonymous(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);

    /* Anonymous identity token (321) should transition state to ACTIVATED */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&session, auth_token, 321));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, session.state);
}

void test_session_auth_username_state(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);

    /* UserName identity token (324) should transition state to ACTIVATED */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&session, auth_token, 324));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, session.state);
}

void test_session_auth_initial_activation_still_accepts_created_session_without_change_user_cu(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);

    /* OPC-10000-4 sections 5.7.2 and 5.7.3: initial ActivateSession for a
       CREATED Session is base Session behavior and must not depend on
       opc_cu_2400 / MUC_OPCUA_CU_SESSION_CHANGE_USER. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&session, auth_token, 321));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, session.state);
    TEST_ASSERT_EQUAL_UINT32(session_id, session.session_id);
    TEST_ASSERT_EQUAL_UINT32(auth_token, session.auth_token);
}

#ifdef MUC_OPCUA_CU_SESSION_CHANGE_USER
void test_session_auth_reactivation_preserves_session_identifiers(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);

    /* SCN-002 / CASE-004 / opc_cu_2400, OPC-10000-4 sections 5.7.2 and 5.7.3:
       an already activated Session may be activated again with a valid
       replacement identity token, while invalid replacements are rejected. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&session, auth_token, 321));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&session, auth_token, 324));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, session.state);
    TEST_ASSERT_EQUAL_UINT32(session_id, session.session_id);
    TEST_ASSERT_EQUAL_UINT32(auth_token, session.auth_token);

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_IDENTITYTOKENINVALID, mu_session_activate(&session, auth_token, 325));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, session.state);
    TEST_ASSERT_EQUAL_UINT32(session_id, session.session_id);
    TEST_ASSERT_EQUAL_UINT32(auth_token, session.auth_token);
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_session_auth_anonymous);
    RUN_TEST(test_session_auth_username_state);
    RUN_TEST(test_session_auth_initial_activation_still_accepts_created_session_without_change_user_cu);
#ifdef MUC_OPCUA_CU_SESSION_CHANGE_USER
    RUN_TEST(test_session_auth_reactivation_preserves_session_identifiers);
#endif
    return UNITY_END();
}
