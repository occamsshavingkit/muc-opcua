/* tests/unit/test_connection_multiplex.c */
#include "../../src/core/service_dispatch.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

#include "../../src/core/server_internal.h"

void setUp(void) {}
void tearDown(void) {}

static opcua_datetime_t multiplex_fake_time(void *context) {
    (void)context;
    return 0;
}

static opcua_statuscode_t multiplex_fake_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer != NULL) {
        (void)memset(buffer, 0x42, length);
    }
    return MU_STATUS_GOOD;
}

static void write_multiplex_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle,
                                           opcua_uint32_t auth_token_id) {
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {auth_token_id}};
    mu_nodeid_t no_header = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(writer, &auth_token);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, request_handle);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_string);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &no_header, 0);
}

static size_t build_multiplex_create_session_body(opcua_byte_t *buffer, size_t capacity,
                                                  opcua_double_t requested_timeout) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_multiplex_request_header(&writer, 17, 0);

    mu_binary_write_string(&writer, &null_string);    /* ClientDescription.applicationUri */
    mu_binary_write_string(&writer, &null_string);    /* productUri */
    mu_binary_write_byte(&writer, 0x00);              /* applicationName */
    mu_binary_write_uint32(&writer, 1);               /* applicationType = CLIENT */
    mu_binary_write_string(&writer, &null_string);    /* gatewayServerUri */
    mu_binary_write_string(&writer, &null_string);    /* discoveryProfileUri */
    mu_binary_write_int32(&writer, 0);                /* discoveryUrls[] */
    mu_binary_write_string(&writer, &null_string);    /* serverUri */
    mu_binary_write_string(&writer, &null_string);    /* endpointUrl */
    mu_binary_write_string(&writer, &null_string);    /* sessionName */
    mu_binary_write_bytestring(&writer, &null_bytes); /* clientNonce */
    mu_binary_write_bytestring(&writer, &null_bytes); /* clientCertificate */
    mu_binary_write_double(&writer, requested_timeout);
    mu_binary_write_uint32(&writer, 0); /* maxResponseMessageSize */

    return writer.position;
}

static size_t build_multiplex_activate_session_body(opcua_byte_t *buffer, size_t capacity,
                                                    opcua_uint32_t auth_token_id) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};
    mu_nodeid_t anonymous_token = {0, MU_NODEID_NUMERIC, {321}};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_multiplex_request_header(&writer, 18, auth_token_id);
    mu_binary_write_string(&writer, &null_string);    /* ClientSignature.algorithm */
    mu_binary_write_bytestring(&writer, &null_bytes); /* ClientSignature.signature */
    mu_binary_write_int32(&writer, 0);                /* ClientSoftwareCertificates[] */
    mu_binary_write_int32(&writer, 0);                /* LocaleIds[] */
    mu_binary_write_extension_object_header(&writer, &anonymous_token, 0);
    return writer.position;
}

static size_t build_multiplex_read_body(opcua_byte_t *buffer, size_t capacity, opcua_uint32_t auth_token_id) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_nodeid_t node_id = {1, MU_NODEID_NUMERIC, {1000}};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_multiplex_request_header(&writer, 19, auth_token_id);
    mu_binary_write_double(&writer, 0.0);      /* maxAge */
    mu_binary_write_uint32(&writer, 3);        /* timestampsToReturn = Neither */
    mu_binary_write_int32(&writer, 1);         /* nodesToRead[] */
    mu_binary_write_nodeid(&writer, &node_id); /* nodeId */
    mu_binary_write_uint32(&writer, 13);       /* attributeId = Value */
    mu_binary_write_string(&writer, &null_string);
    mu_binary_write_uint16(&writer, 0); /* dataEncoding.namespaceIndex */
    mu_binary_write_string(&writer, &null_string);
    return writer.position;
}

static void skip_multiplex_response_header(mu_binary_reader_t *reader, opcua_uint32_t *request_handle,
                                           opcua_statuscode_t *service_result) {
    opcua_int64_t timestamp;
    opcua_byte_t diagnostics_encoding;
    opcua_int32_t string_table_length;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(reader, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(reader, request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &diagnostics_encoding));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(reader, &string_table_length));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(reader, &additional_header_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &additional_header_encoding));
}

static void init_multiplex_server(mu_server_t *server) {
    (void)memset(server, 0, sizeof(*server));
    server->config.time_adapter.get_time = multiplex_fake_time;
    server->config.entropy_adapter.generate_random = multiplex_fake_entropy;
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    server->conns[0].client_handle = (void *)0x10;
    server->conns[0].secure_channel.is_open = true;
    server->conns[0].secure_channel.channel_id = 1;
    server->conns[1].client_handle = (void *)0x20;
    server->conns[1].secure_channel.is_open = true;
    server->conns[1].secure_channel.channel_id = 2;
#else
    server->secure_channel.is_open = true;
    server->secure_channel.channel_id = 1;
#endif
}

typedef struct {
    void *handles[8];
    int handle_count;
    int next_handle;
    int close_count;
    void *closed_handles[8];
    int write_count;
    opcua_byte_t last_write_buf[256];
    size_t last_write_len;
} mock_tcp_t;

static opcua_statuscode_t mock_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static void mock_shutdown(void *context) {
    (void)context;
}

static void mock_close(void *context, void *handle) {
    mock_tcp_t *tcp = (mock_tcp_t *)context;
    if (tcp->close_count < 8) {
        tcp->closed_handles[tcp->close_count++] = handle;
    }
    for (int i = 0; i < tcp->handle_count; ++i) {
        if (tcp->handles[i] == handle) {
            tcp->handles[i] = NULL;
        }
    }
}

static opcua_statuscode_t mock_accept(void *context, void **handle) {
    mock_tcp_t *tcp = (mock_tcp_t *)context;
    if (tcp->next_handle < tcp->handle_count) {
        *handle = tcp->handles[tcp->next_handle++];
    } else {
        *handle = NULL;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_read(void *context, void *handle, opcua_byte_t *buffer, size_t capacity,
                                    size_t *bytes_read) {
    (void)context;
    (void)handle;
    (void)buffer;
    (void)capacity;
    *bytes_read = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_write(void *context, void *handle, const opcua_byte_t *buffer, size_t len,
                                     size_t *bytes_written) {
    mock_tcp_t *tcp = (mock_tcp_t *)context;
    (void)handle;
    tcp->write_count++;
    if (len <= sizeof(tcp->last_write_buf)) {
        (void)memcpy(tcp->last_write_buf, buffer, len);
        tcp->last_write_len = len;
    }
    *bytes_written = len;
    return MU_STATUS_GOOD;
}

void test_connection_limits(void) {
/* Accept exactly MU_INTERN_MAX_CONNECTIONS, then verify the next connection is
   rejected+closed. Capacity-aware: the mock handle pool (mock_tcp_t.handles[8])
   bounds how large a limit we can drive, so profiles whose connection ceiling
   exceeds it (e.g. full=100) are exercised for this path by the micro/embedded
   matrix runs instead. */
#if defined(MUC_OPCUA_MULTIPLE_CONNECTIONS) && (MU_INTERN_MAX_CONNECTIONS + 1) <= 8
    mock_tcp_t tcp;
    (void)memset(&tcp, 0, sizeof(tcp));

    /* One distinct handle per connection slot, plus one extra to be rejected. */
    for (int i = 0; i <= MU_INTERN_MAX_CONNECTIONS; ++i) {
        tcp.handles[i] = (void *)((size_t)0x10 * (size_t)(i + 1));
    }
    tcp.handle_count = MU_INTERN_MAX_CONNECTIONS + 1;
    tcp.next_handle = 0;

    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    /* This test only exercises TCP/connection-level multiplexing up to
       MU_INTERN_MAX_CONNECTIONS (no sessions are created here); max_sessions only
       needs to be a value mu_server_config_validate() accepts, i.e. within the
       compiled MU_INTERN_MAX_SESSIONS ceiling. */
    config.max_sessions = MU_INTERN_MAX_SESSIONS;
    config.max_secure_channels = MU_INTERN_MAX_CONNECTIONS;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.endpoint_url = "opc.tcp://localhost:4840";
    static opcua_byte_t rx_buf[8192];
    static opcua_byte_t tx_buf[8192];
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = &tcp;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES + 2048];
    } storage;
    mu_server_t *server = NULL;
    opcua_statuscode_t status = mu_server_init(storage.bytes, sizeof(storage), &config, &server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    /* Fill every connection slot. */
    for (int i = 0; i < MU_INTERN_MAX_CONNECTIONS; ++i) {
        status = mu_server_poll(server);
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
        TEST_ASSERT_EQUAL_PTR(tcp.handles[i], server->conns[i].client_handle);
    }

    /* The server is now full; the next connection must be rejected and closed. */
    void *reject_handle = tcp.handles[MU_INTERN_MAX_CONNECTIONS];
    status = mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    int rejected = 0;
    for (int i = 0; i < tcp.close_count; ++i) {
        if (tcp.closed_handles[i] == reject_handle) {
            rejected = 1;
        }
    }
    TEST_ASSERT_EQUAL(1, rejected);

    mu_server_close(server);
#else
    TEST_IGNORE_MESSAGE("MU_INTERN_MAX_CONNECTIONS exceeds mock handle pool; "
                        "over-limit reject path covered by micro/embedded matrix");
#endif
}

void test_activate_session_rejects_session_created_on_different_secure_channel(void) {
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    mu_server_t server;
    init_multiplex_server(&server);

    server.active_conn = &server.conns[0];
    opcua_byte_t create_request[256];
    size_t create_request_length = build_multiplex_create_session_body(create_request, sizeof(create_request), 60000.0);
    opcua_byte_t create_response[1024];
    size_t create_response_length = sizeof(create_response);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, create_request, create_request_length,
                                          create_response, &create_response_length));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);

    mu_binary_reader_t create_reader;
    mu_binary_reader_init(&create_reader, create_response, create_response_length);
    mu_nodeid_t create_response_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&create_reader, &create_response_type));
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, create_response_type.identifier.numeric);
    opcua_uint32_t create_handle;
    opcua_statuscode_t create_result;
    skip_multiplex_response_header(&create_reader, &create_handle, &create_result);
    TEST_ASSERT_EQUAL(17, create_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, create_result);

    mu_nodeid_t session_id;
    mu_nodeid_t authentication_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&create_reader, &session_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&create_reader, &authentication_token));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, session_id.identifier_type);
    TEST_ASSERT_EQUAL(0, session_id.namespace_index);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, authentication_token.identifier_type);
    TEST_ASSERT_EQUAL(0, authentication_token.namespace_index);

    server.active_conn = &server.conns[1];
    opcua_byte_t activate_request[256];
    size_t activate_request_length = build_multiplex_activate_session_body(activate_request, sizeof(activate_request),
                                                                           authentication_token.identifier.numeric);
    opcua_byte_t activate_response[256];
    size_t activate_response_length = sizeof(activate_response);

    /* OPC-10000-4 section 5.7.2.1 binds the authenticationToken to the Session
       created on its SecureChannel; section 7.38.2 requires Bad_SecureChannelIdInvalid
       when a request specifies a SecureChannel that is no longer valid for it. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, activate_request,
                                          activate_request_length, activate_response, &activate_response_length));

    mu_binary_reader_t activate_reader;
    mu_binary_reader_init(&activate_reader, activate_response, activate_response_length);
    mu_nodeid_t activate_response_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&activate_reader, &activate_response_type));
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, activate_response_type.identifier.numeric);
    opcua_uint32_t activate_handle;
    opcua_statuscode_t activate_result;
    skip_multiplex_response_header(&activate_reader, &activate_handle, &activate_result);
    TEST_ASSERT_EQUAL(18, activate_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SECURECHANNELIDINVALID, activate_result);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);
#endif
}

void test_session_bound_service_rejects_token_from_different_secure_channel(void) {
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    mu_server_t server;
    init_multiplex_server(&server);

    server.active_conn = &server.conns[0];
    opcua_byte_t create_request[256];
    size_t create_request_length = build_multiplex_create_session_body(create_request, sizeof(create_request), 60000.0);
    opcua_byte_t create_response[1024];
    size_t create_response_length = sizeof(create_response);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, create_request, create_request_length,
                                          create_response, &create_response_length));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);

    mu_binary_reader_t create_reader;
    mu_binary_reader_init(&create_reader, create_response, create_response_length);
    mu_nodeid_t create_response_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&create_reader, &create_response_type));
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, create_response_type.identifier.numeric);
    opcua_uint32_t create_handle;
    opcua_statuscode_t create_result;
    skip_multiplex_response_header(&create_reader, &create_handle, &create_result);
    TEST_ASSERT_EQUAL(17, create_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, create_result);

    mu_nodeid_t session_id;
    mu_nodeid_t authentication_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&create_reader, &session_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&create_reader, &authentication_token));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, authentication_token.identifier_type);
    TEST_ASSERT_EQUAL(0, authentication_token.namespace_index);

    opcua_byte_t activate_request[256];
    size_t activate_request_length = build_multiplex_activate_session_body(activate_request, sizeof(activate_request),
                                                                           authentication_token.identifier.numeric);
    opcua_byte_t activate_response[256];
    size_t activate_response_length = sizeof(activate_response);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, activate_request,
                                          activate_request_length, activate_response, &activate_response_length));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);

    server.active_conn = &server.conns[1];
    opcua_byte_t read_request[256];
    size_t read_request_length =
        build_multiplex_read_body(read_request, sizeof(read_request), authentication_token.identifier.numeric);
    opcua_byte_t read_response[256];
    size_t read_response_length = sizeof(read_response);

    /* OPC-10000-4 section 5.7.2.1 binds the authenticationToken to the Session
       created on its SecureChannel; section 7.38.2 requires Bad_SecureChannelIdInvalid
       when a session-bound service is sent through a different SecureChannel. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SECURECHANNELIDINVALID,
                            mu_service_dispatch(&server, MU_ID_READREQUEST, read_request, read_request_length,
                                                read_response, &read_response_length));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_connection_limits);
    RUN_TEST(test_activate_session_rejects_session_created_on_different_secure_channel);
    RUN_TEST(test_session_bound_service_rejects_token_from_different_secure_channel);
    return UNITY_END();
}
