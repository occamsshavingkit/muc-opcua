/* tests/unit/test_service_state_errors.c */
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include <string.h>

#define TEST_MAX_INBOUND 4
#define TEST_MAX_WRITES 4
#define TEST_MSG_HEADER_SIZE 24u

typedef struct {
    int accept_count;
    opcua_byte_t inbound[TEST_MAX_INBOUND][512];
    size_t inbound_len[TEST_MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    int write_count;
    opcua_byte_t writes[TEST_MAX_WRITES][512];
    size_t write_len[TEST_MAX_WRITES];
} state_error_transport_t;

static opcua_uint32_t s_channel_id = 1u;

static opcua_uint32_t read_opn_channel_id(const opcua_byte_t *buf, size_t len) {
    if (len < 12) {
        return 1u;
    }
    return (opcua_uint32_t)buf[8] | ((opcua_uint32_t)buf[9] << 8u) | ((opcua_uint32_t)buf[10] << 16u) |
           ((opcua_uint32_t)buf[11] << 24u);
}

static void patch_msg_channel_ids(state_error_transport_t *t, opcua_uint32_t channel_id) {
    for (size_t i = 0; i < t->inbound_count; ++i) {
        if (t->inbound_len[i] >= 12 && t->inbound[i][0] == 'M' && t->inbound[i][1] == 'S' && t->inbound[i][2] == 'G' &&
            t->inbound[i][3] == 'F') {
            t->inbound[i][8] = (opcua_byte_t)(channel_id);
            t->inbound[i][9] = (opcua_byte_t)(channel_id >> 8u);
            t->inbound[i][10] = (opcua_byte_t)(channel_id >> 16u);
            t->inbound[i][11] = (opcua_byte_t)(channel_id >> 24u);
        }
    }
}

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
    state_error_transport_t *transport = (state_error_transport_t *)context;
    transport->accept_count++;
    *handle = (transport->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t test_read(void *context, void *handle, opcua_byte_t *buffer, size_t capacity,
                                    size_t *bytes_read) {
    state_error_transport_t *transport = (state_error_transport_t *)context;
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
    state_error_transport_t *transport = (state_error_transport_t *)context;
    (void)handle;
    TEST_ASSERT_TRUE(transport->write_count < TEST_MAX_WRITES);
    TEST_ASSERT_TRUE(len <= sizeof(transport->writes[0]));
    (void)memcpy(transport->writes[transport->write_count], buffer, len);
    transport->write_len[transport->write_count] = len;
    transport->write_count++;
    *bytes_written = len;
    return MU_STATUS_GOOD;
}

static void enqueue_request(state_error_transport_t *transport, const opcua_byte_t *bytes, size_t len) {
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
    (void)mu_binary_write_uint32(&w, (opcua_uint32_t)len);
}

static size_t build_hello(opcua_byte_t *out, size_t capacity) {
    static const opcua_byte_t endpoint[] = "opc.tcp://host:4840";
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, capacity);
    out[0] = 'H';
    out[1] = 'E';
    out[2] = 'L';
    out[3] = 'F';
    w.position = 4;
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 8192);
    (void)mu_binary_write_uint32(&w, 8192);
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 0);
    mu_string_t url = {(opcua_int32_t)(sizeof(endpoint) - 1u), endpoint};
    (void)mu_binary_write_string(&w, &url);
    patch_message_size(out, w.position);
    return w.position;
}

static void write_request_header(mu_binary_writer_t *w, const mu_nodeid_t *auth, opcua_uint32_t handle) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    (void)mu_binary_write_nodeid(w, auth);
    (void)mu_binary_write_int64(w, 0);
    (void)mu_binary_write_uint32(w, handle);
    (void)mu_binary_write_uint32(w, 0);
    (void)mu_binary_write_string(w, &null_str);
    (void)mu_binary_write_uint32(w, 0);
    (void)mu_binary_write_extension_object_header(w, &null_id, 0);
}

static size_t build_opn(opcua_byte_t *out, size_t capacity) {
    static const opcua_byte_t policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#None";
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, capacity);
    out[0] = 'O';
    out[1] = 'P';
    out[2] = 'N';
    out[3] = 'F';
    w.position = 4;
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 0);
    mu_string_t policy = {(opcua_int32_t)(sizeof(policy_uri) - 1u), policy_uri};
    (void)mu_binary_write_string(&w, &policy);
    (void)mu_binary_write_int32(&w, -1);
    (void)mu_binary_write_int32(&w, -1);
    (void)mu_binary_write_uint32(&w, 1);
    (void)mu_binary_write_uint32(&w, 1);
    mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
    (void)mu_binary_write_nodeid(&w, &opn_type);
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    write_request_header(&w, &null_id, 1);
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 0);
    (void)mu_binary_write_uint32(&w, 1);
    (void)mu_binary_write_int32(&w, -1);
    (void)mu_binary_write_uint32(&w, 3600000);
    patch_message_size(out, w.position);
    return w.position;
}

static size_t build_msg(opcua_byte_t *out, size_t capacity, opcua_uint32_t sequence_number, opcua_uint32_t request_id,
                        const opcua_byte_t *body, size_t body_len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, capacity);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'F';
    w.position = 4;
    (void)mu_binary_write_uint32(&w, (opcua_uint32_t)(TEST_MSG_HEADER_SIZE + body_len));
    (void)mu_binary_write_uint32(&w, s_channel_id);
    (void)mu_binary_write_uint32(&w, 1);
    (void)mu_binary_write_uint32(&w, sequence_number);
    (void)mu_binary_write_uint32(&w, request_id);
    (void)memcpy(out + TEST_MSG_HEADER_SIZE, body, body_len);
    return TEST_MSG_HEADER_SIZE + body_len;
}

static void configure_transport_server(mu_server_config_t *config, state_error_transport_t *transport, opcua_byte_t *rx,
                                       opcua_byte_t *tx) {
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

static opcua_statuscode_t read_response_service_result_at(const opcua_byte_t *buffer, size_t len, size_t offset,
                                                          opcua_uint32_t *response_type,
                                                          opcua_statuscode_t *service_result) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, len);
    r.position = offset;
    mu_nodeid_t type;
    opcua_statuscode_t status = mu_binary_read_nodeid(&r, &type);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    if (type.identifier_type != MU_NODEID_NUMERIC || type.namespace_index != 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    *response_type = type.identifier.numeric;

    opcua_int64_t timestamp;
    opcua_uint32_t handle;
    status = mu_binary_read_int64(&r, &timestamp);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_read_uint32(&r, &handle);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    return mu_binary_read_statuscode(&r, service_result);
}

static void prepare_created_session(mu_server_t *server, opcua_uint32_t *auth_token) {
    (void)memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    fake_platform_init(NULL, &server->config.time_adapter, &server->config.entropy_adapter);
    mu_session_init(&server->sessions[0]);
    opcua_uint64_t revised;
    opcua_uint32_t session_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_create(&server->sessions[0], 0, &revised, &session_id, auth_token));
}

static size_t build_activate_body(opcua_byte_t *buffer, size_t capacity, const mu_nodeid_t *auth_token,
                                  const mu_nodeid_t *identity_token_type) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, capacity);
    mu_string_t null_str = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};
    write_request_header(&w, auth_token, 77);
    (void)mu_binary_write_string(&w, &null_str);
    (void)mu_binary_write_bytestring(&w, &null_bytes);
    (void)mu_binary_write_int32(&w, 0);
    (void)mu_binary_write_int32(&w, 0);
    (void)mu_binary_write_extension_object_header(&w, identity_token_type, 0);
    return w.position;
}

void test_browse_before_activate_session(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server.secure_channel.is_open = true;
    server.sessions[0].state = MU_SESSION_STATE_CLOSED;

    opcua_byte_t req[1], resp[1];
    size_t resp_len = 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID,
                      mu_service_dispatch(&server, MU_ID_BROWSEREQUEST, req, 1, resp, &resp_len));
}

void test_read_before_activate_session(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server.secure_channel.is_open = true;
    server.sessions[0].state = MU_SESSION_STATE_CREATED; /* Created but not activated */

    opcua_byte_t req[1], resp[1];
    size_t resp_len = 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID,
                      mu_service_dispatch(&server, MU_ID_READREQUEST, req, 1, resp, &resp_len));
}

void test_read_with_known_inactive_session_returns_bad_sessionnotactivated(void) {
    mu_server_t server;
    opcua_uint32_t auth_token;
    prepare_created_session(&server, &auth_token);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);
    TEST_ASSERT_EQUAL(auth_token, server.sessions[0].auth_token);

    opcua_byte_t request[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, request, sizeof(request));
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {auth_token}};
    write_request_header(&w, &auth, 88);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_double(&w, 0.0); /* MaxAge */
    mu_binary_write_uint32(&w, 3);   /* TimestampsToReturn = Neither */
    mu_binary_write_int32(&w, 1);    /* NodesToRead */
    mu_nodeid_t objects = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &objects);
    mu_binary_write_uint32(&w, 13);        /* AttributeId = Value */
    mu_binary_write_string(&w, &null_str); /* IndexRange */
    mu_binary_write_uint16(&w, 0);         /* DataEncoding.namespaceIndex */
    mu_binary_write_string(&w, &null_str); /* DataEncoding.name */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    /* OPC-10000-4 section 7.38.2: a valid, known Session that has not been
       activated must be rejected as Bad_SessionNotActivated, not unknown. */
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_SESSIONNOTACTIVATED,
        mu_service_dispatch(&server, MU_ID_READREQUEST, request, w.position, response, &response_len));
}

void test_read_with_unknown_session_token_returns_bad_sessionidinvalid(void) {
    mu_server_t server;
    opcua_uint32_t known_auth_token;
    prepare_created_session(&server, &known_auth_token);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);

    opcua_uint32_t unknown_auth_token = known_auth_token + 1u;
    TEST_ASSERT_NOT_EQUAL(known_auth_token, unknown_auth_token);

    opcua_byte_t request[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, request, sizeof(request));
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {unknown_auth_token}};
    write_request_header(&w, &auth, 89);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_double(&w, 0.0); /* MaxAge */
    mu_binary_write_uint32(&w, 3);   /* TimestampsToReturn = Neither */
    mu_binary_write_int32(&w, 1);    /* NodesToRead */
    mu_nodeid_t objects = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &objects);
    mu_binary_write_uint32(&w, 13);        /* AttributeId = Value */
    mu_binary_write_string(&w, &null_str); /* IndexRange */
    mu_binary_write_uint16(&w, 0);         /* DataEncoding.namespaceIndex */
    mu_binary_write_string(&w, &null_str); /* DataEncoding.name */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    /* OPC-10000-4 section 7.38.2: a request with an unknown Session
       authentication token is rejected as Bad_SessionIdInvalid. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SESSIONIDINVALID, mu_service_dispatch(&server, MU_ID_READREQUEST, request,
                                                                                w.position, response, &response_len));
}

void test_session_before_secure_channel(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_ESTABLISHED;
    server.secure_channel.is_open = false; /* Not open */

    opcua_byte_t req[1], resp[1];
    size_t resp_len = 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURECHANNELIDINVALID,
                      mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, req, 1, resp, &resp_len));
}

void test_service_before_hello(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.tcp_conn.state = MU_TCP_STATE_CONNECTED; /* Not established yet */

    opcua_byte_t req[1], resp[1];
    size_t resp_len = 1;
    /* If called before HEL/ACK, it should reject.
       In OPC UA, a Service cannot be invoked before SecureChannel,
       but if we are at TCP layer, before HEL it's not even a SecureChannel issue yet.
       Wait, let's just make service_dispatch return BAD_SECURECHANNELIDINVALID
       since SecureChannel isn't open either. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURECHANNELIDINVALID,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, 1, resp, &resp_len));
}

void test_request_type_non_numeric_nodeid_rejected_with_bad_decodingerror(void) {
    state_error_transport_t transport;
    (void)memset(&transport, 0, sizeof(transport));

    opcua_byte_t chunk[512];
    size_t len = build_hello(chunk, sizeof(chunk));
    enqueue_request(&transport, chunk, len);
    len = build_opn(chunk, sizeof(chunk));
    enqueue_request(&transport, chunk, len);

    opcua_byte_t body[32];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, body, sizeof(body));
    mu_string_t string_id = {1, (const opcua_byte_t *)"x"};
    mu_nodeid_t request_type;
    request_type.namespace_index = 0;
    request_type.identifier_type = MU_NODEID_STRING;
    request_type.identifier.string = string_id;
    (void)mu_binary_write_nodeid(&w, &request_type);
    len = build_msg(chunk, sizeof(chunk), 2, 2, body, w.position);
    enqueue_request(&transport, chunk, len);

    mu_server_config_t config;
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
    configure_transport_server(&config, &transport, rx, tx);
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES];
        struct mu_server align;
    } storage;
    mu_server_t *server = NULL;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));
    s_channel_id = read_opn_channel_id(transport.writes[1], transport.write_len[1]);
    patch_msg_channel_ids(&transport, s_channel_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(server));

    TEST_ASSERT_EQUAL(3, transport.write_count);
    opcua_uint32_t response_type;
    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      read_response_service_result_at(transport.writes[2], transport.write_len[2], TEST_MSG_HEADER_SIZE,
                                                      &response_type, &service_result));
    TEST_ASSERT_EQUAL(MU_ID_SERVICEFAULT, response_type);
    /* OPC-10000-6 §5.2.2.9 encodes a NodeId's identifier type separately from
       its value; the leading request type-id must be ns=0 numeric. OPC-10000-4
       §7.38.2 defines Bad_DecodingError for invalid stream data. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, service_result);
}

void test_activate_session_rejects_non_ns0_numeric_authentication_token(void) {
    mu_server_t server;
    opcua_uint32_t token;
    prepare_created_session(&server, &token);

    opcua_byte_t request[256];
    mu_nodeid_t auth = {1, MU_NODEID_NUMERIC, {token}};
    mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY}};
    size_t request_len = build_activate_body(request, sizeof(request), &auth, &anon);

    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, request, request_len,
                                                          response, &response_len));

    opcua_uint32_t response_type;
    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      read_response_service_result_at(response, response_len, 0, &response_type, &service_result));
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, response_type);
    /* OPC-10000-6 §5.2.2.9 makes the namespace part of the NodeId encoding; a
       non-ns=0 authentication token is not this server's numeric session token.
       OPC-10000-4 §7.38.2 defines the StatusCodes used for service rejection. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, service_result);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);
}

void test_activate_session_rejects_non_numeric_user_identity_token_typeid(void) {
    mu_server_t server;
    opcua_uint32_t token;
    prepare_created_session(&server, &token);

    opcua_byte_t type_name[MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY];
    (void)memset(type_name, 'A', sizeof(type_name));
    mu_string_t non_numeric_name = {(opcua_int32_t)sizeof(type_name), type_name};
    mu_nodeid_t unsupported_type;
    unsupported_type.namespace_index = 0;
    unsupported_type.identifier_type = MU_NODEID_STRING;
    unsupported_type.identifier.string = non_numeric_name;

    opcua_byte_t request[512];
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {token}};
    size_t request_len = build_activate_body(request, sizeof(request), &auth, &unsupported_type);

    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, request, request_len,
                                                          response, &response_len));

    opcua_uint32_t response_type;
    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      read_response_service_result_at(response, response_len, 0, &response_type, &service_result));
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, response_type);
    /* OPC-10000-6 §5.2.2.9 encodes String and Numeric NodeIds with different
       identifier forms. The UserIdentityToken ExtensionObject typeId must not be
       read through the numeric union arm; OPC-10000-4 §7.38.2 defines
       Bad_IdentityTokenInvalid for invalid user identity tokens. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_IDENTITYTOKENINVALID, service_result);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browse_before_activate_session);
    RUN_TEST(test_read_before_activate_session);
    RUN_TEST(test_read_with_known_inactive_session_returns_bad_sessionnotactivated);
    RUN_TEST(test_read_with_unknown_session_token_returns_bad_sessionidinvalid);
    RUN_TEST(test_session_before_secure_channel);
    RUN_TEST(test_service_before_hello);
    RUN_TEST(test_request_type_non_numeric_nodeid_rejected_with_bad_decodingerror);
    RUN_TEST(test_activate_session_rejects_non_ns0_numeric_authentication_token);
    RUN_TEST(test_activate_session_rejects_non_numeric_user_identity_token_typeid);
    return UNITY_END();
}
