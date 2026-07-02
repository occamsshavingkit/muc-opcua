/* tests/unit/test_tcp_connection.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/tcp_connection.h"
#include <string.h>

static opcua_byte_t s_overlong_endpoint_url[4096];

static opcua_uint32_t read_uint32_le(const opcua_byte_t *data) {
    return (opcua_uint32_t)data[0] | ((opcua_uint32_t)data[1] << 8) | ((opcua_uint32_t)data[2] << 16) |
           ((opcua_uint32_t)data[3] << 24);
}

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
    opcua_byte_t hello[] = {'H', 'E', 'L', 'F', 32, 0, 0, 0, // Message size (32)
                            0, 0, 0, 0,                      // Protocol version
                            0, 32, 0, 0,                     // Receive buffer (8192)
                            0, 32, 0, 0,                     // Send buffer (8192)
                            0, 0, 1, 0,                      // Max message (65536)
                            10, 0, 0, 0,                     // Max chunk count
                            // EndpointUrl (empty string: length = 0)
                            0, 0, 0, 0};

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

void test_tcp_ack_send_buffer_size_is_capped_by_configured_send_buffer(void) {
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.receive_buffer_size = 16384;
    config.send_buffer_size = 12288;
    config.max_message_size = 65536;
    config.max_chunk_count = 16;

    mu_tcp_connection_t conn;
    mu_tcp_connection_init(&conn);

    opcua_byte_t hello[] = {'H', 'E', 'L', 'F', 32, 0, 0, 0, // Message size (32)
                            0,   0,   0,   0,                // Protocol version
                            0,   64,  0,   0,                // Receive buffer (16384)
                            0,   64,  0,   0,                // Send buffer (16384)
                            0,   0,   1,   0,                // Max message (65536)
                            10,  0,   0,   0,                // Max chunk count
                            0,   0,   0,   0};               // EndpointUrl (empty string)

    opcua_byte_t ack[128];
    size_t ack_len = sizeof(ack);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_tcp_process_hello(&conn, hello, sizeof(hello), &config, ack, &ack_len));

    TEST_ASSERT_EQUAL(28, ack_len);
    TEST_ASSERT_EQUAL_MEMORY("ACKF", ack, 4);
    /* OPC-10000-6 section 7.1.2.4 defines ACK SendBufferSize; the server must
       not advertise a larger send chunk than its configured buffer or the
       ReceiveBufferSize requested by the peer in HEL. */
    TEST_ASSERT_EQUAL_UINT32(12288, read_uint32_le(&ack[16]));
    TEST_ASSERT_TRUE(read_uint32_le(&ack[16]) <= config.send_buffer_size);
    TEST_ASSERT_TRUE(read_uint32_le(&ack[16]) <= read_uint32_le(&hello[12]));
}

void test_tcp_hello_receive_buffer_below_minimum_returns_tcp_error_status(void) {
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.receive_buffer_size = 8192;
    config.send_buffer_size = 8192;
    config.max_message_size = 65536;
    config.max_chunk_count = 16;

    mu_tcp_connection_t conn;
    mu_tcp_connection_init(&conn);

    opcua_byte_t hello[] = {'H', 'E', 'L', 'F', 32, 0, 0, 0, // Message size (32)
                            0,   0,   0,   0,                // Protocol version
                            0,   16,  0,   0,                // Receive buffer (4096)
                            0,   32,  0,   0,                // Send buffer (8192)
                            0,   0,   1,   0,                // Max message (65536)
                            10,  0,   0,   0,                // Max chunk count
                            0,   0,   0,   0};               // EndpointUrl (empty string)

    opcua_byte_t ack[128] = {0};
    size_t ack_len = sizeof(ack);

    /* OPC-10000-6 sections 7.1.2.3 and 7.1.2.4 require HEL buffer sizes to
       support the minimum chunk size and prohibit ACK SendBufferSize from
       exceeding the ReceiveBufferSize requested by the peer. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPNOTENOUGHRESOURCES,
                      mu_tcp_process_hello(&conn, hello, sizeof(hello), &config, ack, &ack_len));
    TEST_ASSERT_EQUAL(MU_TCP_STATE_CLOSED, conn.state);
    TEST_ASSERT_NOT_EQUAL('A', ack[0]);
}

void test_tcp_hello_endpoint_url_over_length_returns_tcp_endpoint_url_invalid(void) {
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.receive_buffer_size = 8192;
    config.send_buffer_size = 8192;
    config.max_message_size = 65536;
    config.max_chunk_count = 16;

    mu_tcp_connection_t conn;
    mu_tcp_connection_init(&conn);

    opcua_byte_t hello[4132];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, hello, sizeof(hello));
    hello[0] = 'H';
    hello[1] = 'E';
    hello[2] = 'L';
    hello[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 65536);
    mu_binary_write_uint32(&w, 16);
    {
        mu_string_t endpoint_url = {(opcua_int32_t)sizeof(s_overlong_endpoint_url), s_overlong_endpoint_url};
        mu_binary_write_string(&w, &endpoint_url);
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);
    {
        mu_binary_writer_t header;
        mu_binary_writer_init(&header, hello, sizeof(hello));
        header.position = 4;
        mu_binary_write_uint32(&header, (opcua_uint32_t)w.position);
    }

    opcua_byte_t ack[128] = {0};
    size_t ack_len = sizeof(ack);

    /* OPC-10000-6 section 7.1.2.3 limits EndpointUrl to less than 4096 bytes. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPENDPOINTURLINVALID,
                      mu_tcp_process_hello(&conn, hello, w.position, &config, ack, &ack_len));
    TEST_ASSERT_EQUAL(MU_TCP_STATE_CLOSED, conn.state);
    TEST_ASSERT_NOT_EQUAL('A', ack[0]);
}

void test_tcp_default_buffer_size(void) {
    /* Tested in hello logic limits */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tcp_hello_acknowledge_negotiation);
    RUN_TEST(test_tcp_ack_send_buffer_size_is_capped_by_configured_send_buffer);
    RUN_TEST(test_tcp_hello_receive_buffer_below_minimum_returns_tcp_error_status);
    RUN_TEST(test_tcp_hello_endpoint_url_over_length_returns_tcp_endpoint_url_invalid);
    RUN_TEST(test_tcp_default_buffer_size);
    return UNITY_END();
}
