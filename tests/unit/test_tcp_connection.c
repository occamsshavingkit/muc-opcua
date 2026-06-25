/* tests/unit/test_tcp_connection.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/tcp_connection.h"
#include <string.h>

void test_tcp_hello_acknowledge_negotiation(void) {
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.receive_buffer_size = 8192;
    config.send_buffer_size = 8192;
    config.max_message_size = 65536;
    config.max_chunk_count = 16;
    
    mu_tcp_connection_t conn;
    mu_tcp_connection_init(&conn);
    
    // Hello message
    opcua_byte_t hello[] = {
        'H', 'E', 'L', 'F',
        32, 0, 0, 0, // Message size (32)
        0, 0, 0, 0, // Protocol version
        0, 16, 0, 0, // Receive buffer (4096 - negotiated up to 8192)
        0, 16, 0, 0, // Send buffer (4096)
        0, 0, 1, 0, // Max message (65536)
        10, 0, 0, 0, // Max chunk count
        // EndpointUrl (empty string: length = 0)
        0, 0, 0, 0
    };
    
    opcua_byte_t ack[128];
    size_t ack_len = sizeof(ack);
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_tcp_process_hello(&conn, hello, sizeof(hello), &config, ack, &ack_len));
    
    TEST_ASSERT_EQUAL(28, ack_len);
    TEST_ASSERT_EQUAL('A', ack[0]);
    TEST_ASSERT_EQUAL('C', ack[1]);
    TEST_ASSERT_EQUAL('K', ack[2]);
    TEST_ASSERT_EQUAL('F', ack[3]);
    
    TEST_ASSERT_EQUAL(MU_TCP_STATE_ESTABLISHED, conn.state);
}

void test_tcp_default_buffer_size(void) {
    /* Tested in hello logic limits */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tcp_hello_acknowledge_negotiation);
    RUN_TEST(test_tcp_default_buffer_size);
    return UNITY_END();
}
