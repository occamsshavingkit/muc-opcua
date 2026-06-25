/* tests/unit/test_session.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/session.h"

void test_session_create(void) {
    mu_session_t session;
    mu_session_init(&session);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, session.state);
    
    double revised_timeout;
    opcua_uint32_t session_id, auth_token;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_create(&session, 5000.0, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, session.state);
    TEST_ASSERT_EQUAL(10000.0, revised_timeout); /* Bounded */
    TEST_ASSERT_EQUAL(1, session_id);
}

void test_session_activate_anonymous(void) {
    mu_session_t session;
    mu_session_init(&session);
    
    double revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, 5000.0, &revised_timeout, &session_id, &auth_token);
    
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
    TEST_IGNORE_MESSAGE("Implement CloseSession test");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_session_create);
    RUN_TEST(test_session_activate_anonymous);
    RUN_TEST(test_session_close);
    return UNITY_END();
}
