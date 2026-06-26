/* tests/unit/test_session.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/session.h"

/* Wire SessionTimeout is a Duration (Double); the API works on its raw bits. */
static opcua_uint64_t bits(double d) { opcua_uint64_t b; memcpy(&b, &d, sizeof(b)); return b; }

void test_session_create(void) {
    mu_session_t session;
    mu_session_init(&session);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, session.state);

    opcua_uint64_t revised;
    opcua_uint32_t session_id, auth_token;

    /* Below the 10000 ms minimum -> clamped up. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_create(&session, bits(5000.0), &revised, &session_id, &auth_token));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, session.state);
    TEST_ASSERT_EQUAL_HEX64(bits(10000.0), revised);
    TEST_ASSERT_EQUAL(1, session_id);

    /* In range -> unchanged. */
    mu_session_init(&session);
    mu_session_create(&session, bits(60000.0), &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(60000.0), revised);

    /* Above the 3600000 ms maximum -> clamped down. */
    mu_session_init(&session);
    mu_session_create(&session, bits(9999999.0), &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(3600000.0), revised);

    /* Negative / zero -> minimum. */
    mu_session_init(&session);
    mu_session_create(&session, bits(-1.0), &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(10000.0), revised);
    mu_session_init(&session);
    mu_session_create(&session, 0, &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(10000.0), revised);
}

void test_session_activate_anonymous(void) {
    mu_session_t session;
    mu_session_init(&session);
    
    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);
    
    /* Unknown identity token */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_IDENTITYTOKENINVALID, 
                      mu_session_activate(&session, auth_token, 324)); /* UserIdentityToken */
                      
    /* Wrong auth token */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, 
                      mu_session_activate(&session, 9999, 321));
                      
    /* Success */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, 
                      mu_session_activate(&session, auth_token, 321)); /* AnonymousIdentityToken */
                      
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, session.state);
}

void test_session_close(void) {
    mu_session_t session;
    mu_session_init(&session);
    
    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, mu_session_close(&session, 0, true));
    
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);
    
    /* Wrong auth token */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, mu_session_close(&session, 9999, true));
    
    /* Success */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_close(&session, auth_token, true));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, session.state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_session_create);
    RUN_TEST(test_session_activate_anonymous);
    RUN_TEST(test_session_close);
    return UNITY_END();
}
