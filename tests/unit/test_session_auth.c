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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_session_auth_anonymous);
    RUN_TEST(test_session_auth_username_state);
    return UNITY_END();
}
