/* tests/unit/test_service_state_errors.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_browse_before_activate_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server.secure_channel.is_open = true;
    server.session.state = MU_SESSION_STATE_CLOSED;
    
    opcua_byte_t req[1], resp[1]; size_t resp_len = 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, mu_service_dispatch(&server, MU_ID_BROWSEREQUEST, req, 1, resp, &resp_len));
}

void test_read_before_activate_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server.secure_channel.is_open = true;
    server.session.state = MU_SESSION_STATE_CREATED; /* Created but not activated */
    
    opcua_byte_t req[1], resp[1]; size_t resp_len = 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, mu_service_dispatch(&server, MU_ID_READREQUEST, req, 1, resp, &resp_len));
}

void test_session_before_secure_channel(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server.secure_channel.is_open = false; /* Not open */
    
    opcua_byte_t req[1], resp[1]; size_t resp_len = 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURECHANNELIDINVALID, mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, req, 1, resp, &resp_len));
}

void test_service_before_hello(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_CONNECTED; /* Not established yet */
    
    opcua_byte_t req[1], resp[1]; size_t resp_len = 1;
    /* If called before HEL/ACK, it should reject. 
       In OPC UA, a Service cannot be invoked before SecureChannel, 
       but if we are at TCP layer, before HEL it's not even a SecureChannel issue yet.
       Wait, let's just make service_dispatch return BAD_SECURECHANNELIDINVALID 
       since SecureChannel isn't open either. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURECHANNELIDINVALID, mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, 1, resp, &resp_len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browse_before_activate_session);
    RUN_TEST(test_read_before_activate_session);
    RUN_TEST(test_session_before_secure_channel);
    RUN_TEST(test_service_before_hello);
    return UNITY_END();
}
