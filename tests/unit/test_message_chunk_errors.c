/* tests/unit/test_message_chunk_errors.c */
#include "../../src/core/message_chunk.h"
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define TEST_MAX_INBOUND 1u
#define TEST_MAX_WRITES 2u
#define TEST_MAX_WRITE_LEN 512u

typedef struct {
    int accept_count;
    opcua_byte_t inbound[TEST_MAX_INBOUND][128];
    size_t inbound_len[TEST_MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    int write_count;
    opcua_byte_t writes[TEST_MAX_WRITES][TEST_MAX_WRITE_LEN];
    size_t write_len[TEST_MAX_WRITES];
} message_chunk_transport_t;

static opcua_statuscode_t test_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static void test_shutdown(void *context) {
    (void)context;
}

static void test_close(void *context, void *handle) {
    (void)context;
    (void)handle;
}

static opcua_statuscode_t test_accept(void *context, void **handle) {
    message_chunk_transport_t *transport = (message_chunk_transport_t *)context;
    transport->accept_count++;
    *handle = (transport->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t test_read(void *context, void *handle, opcua_byte_t *buffer, size_t capacity,
                                    size_t *bytes_read) {
    message_chunk_transport_t *transport = (message_chunk_transport_t *)context;
    (void)handle;
    if (transport->read_index >= transport->inbound_count) {
        *bytes_read = 0;
        return MU_STATUS_GOOD;
    }

    size_t len = transport->inbound_len[transport->read_index];
    TEST_ASSERT_TRUE(len <= capacity);
    (void)memcpy(buffer, transport->inbound[transport->read_index], len);
    transport->read_index++;
    *bytes_read = len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t test_write(void *context, void *handle, const opcua_byte_t *buffer, size_t len,
                                     size_t *bytes_written) {
    message_chunk_transport_t *transport = (message_chunk_transport_t *)context;
    (void)handle;
    TEST_ASSERT_TRUE((size_t)transport->write_count < TEST_MAX_WRITES);
    TEST_ASSERT_TRUE(len <= sizeof(transport->writes[0]));
    (void)memcpy(transport->writes[transport->write_count], buffer, len);
    transport->write_len[transport->write_count] = len;
    transport->write_count++;
    *bytes_written = len;
    return MU_STATUS_GOOD;
}

static void enqueue_request(message_chunk_transport_t *transport, const opcua_byte_t *bytes, size_t len) {
    TEST_ASSERT_TRUE(transport->inbound_count < TEST_MAX_INBOUND);
    TEST_ASSERT_TRUE(len <= sizeof(transport->inbound[0]));
    (void)memcpy(transport->inbound[transport->inbound_count], bytes, len);
    transport->inbound_len[transport->inbound_count] = len;
    transport->inbound_count++;
}

static void patch_message_size(opcua_byte_t *buffer, size_t len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, len);
    w.position = 4;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, (opcua_uint32_t)len));
}

static size_t build_abort_msg(opcua_byte_t *out, size_t capacity, opcua_statuscode_t error_code) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, capacity);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'A';
    w.position = 4;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 77));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_statuscode(&w, error_code));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, -1));
    patch_message_size(out, w.position);
    return w.position;
}

static size_t build_non_final_msg_with_unsupported_service(opcua_byte_t *out, size_t capacity) {
    mu_binary_writer_t w;
    mu_nodeid_t request_type = {
        .identifier_type = MU_NODEID_NUMERIC,
        .namespace_index = 0,
        .identifier.numeric = 0xFFFFu,
    };

    mu_binary_writer_init(&w, out, capacity);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'C';
    w.position = 4;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 78));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &request_type));
    patch_message_size(out, w.position);
    return w.position;
}

static void assert_tcp_error_write(const message_chunk_transport_t *transport, int index,
                                   opcua_statuscode_t expected_error) {
    mu_message_header_t header;
    mu_binary_reader_t r;
    opcua_statuscode_t actual_error = MU_STATUS_GOOD;

    TEST_ASSERT_TRUE(index < transport->write_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_parse_message_header(transport->writes[index], transport->write_len[index], &header));
    TEST_ASSERT_EQUAL('E', header.message_type[0]);
    TEST_ASSERT_EQUAL('R', header.message_type[1]);
    TEST_ASSERT_EQUAL('R', header.message_type[2]);
    TEST_ASSERT_EQUAL('F', header.chunk_type);

    mu_binary_reader_init(&r, transport->writes[index], transport->write_len[index]);
    r.position = 8;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &actual_error));
    TEST_ASSERT_EQUAL_HEX32(expected_error, actual_error);
}

static void configure_transport_server(mu_server_config_t *config, message_chunk_transport_t *transport,
                                       opcua_byte_t *rx, opcua_byte_t *tx) {
    (void)memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri = "urn:test";
    config->product_uri = "urn:test";
    config->application_name = "test";
    config->receive_buffer = rx;
    config->receive_buffer_size = 8192;
    config->send_buffer = tx;
    config->send_buffer_size = 8192;
    config->max_chunk_count = 1;
    config->max_message_size = 8192;
    config->max_sessions = 1;
    config->max_secure_channels = 1;
    fake_platform_init(NULL, &config->time_adapter, &config->entropy_adapter);
    config->tcp_adapter.context = transport;
    config->tcp_adapter.listen = test_listen;
    config->tcp_adapter.accept = test_accept;
    config->tcp_adapter.read = test_read;
    config->tcp_adapter.write = test_write;
    config->tcp_adapter.close_connection = test_close;
    config->tcp_adapter.shutdown = test_shutdown;
}

static void mark_transport_and_channel_established(mu_server_t *server) {
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    TEST_ASSERT_NOT_NULL(server->conns[0].client_handle);
    server->conns[0].tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server->conns[0].secure_channel.is_open = true;
    server->conns[0].secure_channel.channel_id = 1;
    server->conns[0].secure_channel.token_id = 1;
#else
    TEST_ASSERT_NOT_NULL(server->client_handle);
    server->tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server->secure_channel.is_open = true;
    server->secure_channel.channel_id = 1;
    server->secure_channel.token_id = 1;
#endif
}

void test_message_chunk_too_small(void) {
    opcua_byte_t buffer[7] = {'H', 'E', 'L', 'F', 0, 0, 0};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPINTERNALERROR, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_too_large(void) {
    /* If the message size is extremely large, but length doesn't match, we return too large */
    /* Wait, the specification returns MU_STATUS_BAD_TCPMESSAGETOOLARGE if size > length in this implementation */
    opcua_byte_t buffer[12] = {'M', 'S', 'G', 'F', 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETOOLARGE, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_inconsistent_length(void) {
    opcua_byte_t buffer[12] = {'M', 'S', 'G', 'F', 16, 0, 0, 0, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETOOLARGE, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_msg_at_message_header_boundary_rejected(void) {
    /* OPC-10000-6 section 6.7.2: a MSG chunk needs the MessageHeader plus SecureChannelId before body bytes. */
    opcua_byte_t buffer[8] = {'M', 'S', 'G', 'F', 8, 0, 0, 0};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPINTERNALERROR, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_invalid_message_type(void) {
    /* OPC-10000-6 sections 6.7.2 and 7.1.2.2 require invalid MessageType values to return Bad_TcpMessageTypeInvalid. */
    opcua_byte_t buffer[12] = {'X', 'Y', 'Z', 'F', 12, 0, 0, 0, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETYPEINVALID, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_abort_chunk_returns_tcp_error_status(void) {
    /* OPC-10000-6 section 6.7.2: IsFinal 'A' is an abort chunk, not a service payload chunk. */
    opcua_byte_t buffer[32];
    size_t len = build_abort_msg(buffer, sizeof(buffer), MU_STATUS_BAD_TCPINTERNALERROR);
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPINTERNALERROR, mu_parse_message_header(buffer, len, &header));
}

void test_message_chunk_abort_chunk_missing_error_code_rejected(void) {
    /* OPC-10000-6 section 6.7.3: abort chunks must include the Error StatusCode before optional Reason text. */
    opcua_byte_t buffer[16] = {'M', 'S', 'G', 'A', 16, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPINTERNALERROR, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

void test_message_chunk_abort_chunk_does_not_dispatch_service_payload(void) {
    /* OPC-10000-6 section 6.7.2: abort chunks carry Error/Reason and must not dispatch MessageBody bytes. */
    message_chunk_transport_t transport;
    opcua_byte_t chunk[128];
    mu_server_config_t config;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;
    size_t len;

    (void)memset(&transport, 0, sizeof(transport));
    len = build_abort_msg(chunk, sizeof(chunk), MU_STATUS_BAD_TCPINTERNALERROR);
    enqueue_request(&transport, chunk, len);

    configure_transport_server(&config, &transport, rx, tx);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    mark_transport_and_channel_established(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));

    TEST_ASSERT_EQUAL(1u, transport.read_index);
    TEST_ASSERT_EQUAL(0, transport.write_count);
}

#ifdef MUC_OPCUA_MULTI_CHUNK
void test_message_chunk_non_final_chunk_in_single_chunk_mode_returns_tcp_error_without_dispatch(void) {
    /* OPC-10000-6 section 6.7.2: IsFinal 'C' is a continuation chunk; it is buffered for multi-chunk reassembly. */
    message_chunk_transport_t transport;
    opcua_byte_t chunk[128];
    mu_server_config_t config;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;
    size_t len;

    (void)memset(&transport, 0, sizeof(transport));
    len = build_non_final_msg_with_unsupported_service(chunk, sizeof(chunk));
    enqueue_request(&transport, chunk, len);

    configure_transport_server(&config, &transport, rx, tx);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    mark_transport_and_channel_established(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));

    TEST_ASSERT_EQUAL(1u, transport.read_index);
    TEST_ASSERT_EQUAL(0, transport.write_count);
}

void test_message_chunk_incomplete_declared_chunk_waits_without_dispatch(void) {
    /* OPC-10000-6 section 6.7.2: MessageSize defines the chunk byte boundary; incomplete chunks are not dispatched. */
    message_chunk_transport_t transport;
    opcua_byte_t chunk_prefix[8] = {'M', 'S', 'G', 'F', 24, 0, 0, 0};
    mu_server_config_t config;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;

    (void)memset(&transport, 0, sizeof(transport));
    enqueue_request(&transport, chunk_prefix, sizeof(chunk_prefix));

    configure_transport_server(&config, &transport, rx, tx);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    mark_transport_and_channel_established(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));

    TEST_ASSERT_EQUAL(1u, transport.read_index);
    TEST_ASSERT_EQUAL(0, transport.write_count);
}

void test_message_chunk_continuation_followed_by_abort_sends_only_continuation_error(void) {
    /* OPC-10000-6 section 6.7.3: a 'C' chunk followed by abort: 'C' is buffered, abort triggers assembler reset. */
    message_chunk_transport_t transport;
    opcua_byte_t chunks[128];
    mu_server_config_t config;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;
    size_t continuation_len;
    size_t abort_len;

    memset(&transport, 0, sizeof(transport));
    continuation_len = build_non_final_msg_with_unsupported_service(chunks, sizeof(chunks));
    abort_len =
        build_abort_msg(chunks + continuation_len, sizeof(chunks) - continuation_len, MU_STATUS_BAD_TCPINTERNALERROR);
    enqueue_request(&transport, chunks, continuation_len + abort_len);

    configure_transport_server(&config, &transport, rx, tx);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    mark_transport_and_channel_established(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));

    TEST_ASSERT_EQUAL(1u, transport.read_index);
    TEST_ASSERT_EQUAL(0, transport.write_count);
}
#endif /* MUC_OPCUA_MULTI_CHUNK */

void test_message_chunk_invalid_chunk_type(void) {
    opcua_byte_t buffer[12] = {'M', 'S', 'G', 'X', 12, 0, 0, 0, 0, 0, 0, 1};
    mu_message_header_t header;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPMESSAGETYPEINVALID, mu_parse_message_header(buffer, sizeof(buffer), &header));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_message_chunk_too_small);
    RUN_TEST(test_message_chunk_too_large);
    RUN_TEST(test_message_chunk_inconsistent_length);
    RUN_TEST(test_message_chunk_msg_at_message_header_boundary_rejected);
    RUN_TEST(test_message_chunk_invalid_message_type);
    RUN_TEST(test_message_chunk_abort_chunk_returns_tcp_error_status);
    RUN_TEST(test_message_chunk_abort_chunk_missing_error_code_rejected);
    RUN_TEST(test_message_chunk_abort_chunk_does_not_dispatch_service_payload);
#ifdef MUC_OPCUA_MULTI_CHUNK
    RUN_TEST(test_message_chunk_non_final_chunk_in_single_chunk_mode_returns_tcp_error_without_dispatch);
#endif
    RUN_TEST(test_message_chunk_incomplete_declared_chunk_waits_without_dispatch);
#ifdef MUC_OPCUA_MULTI_CHUNK
    RUN_TEST(test_message_chunk_continuation_followed_by_abort_sends_only_continuation_error);
#endif
    RUN_TEST(test_message_chunk_invalid_chunk_type);
    return UNITY_END();
}
