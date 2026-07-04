/* tests/integration/test_user_auth_plaintext.c */
#include "../../src/core/server_internal.h"
#include "../../src/services/discovery.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <stdbool.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define MAX_INBOUND 8
typedef struct {
    int accept_count;
    opcua_byte_t inbound[MAX_INBOUND][512];
    size_t inbound_len[MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    opcua_byte_t last_write[8192];
    size_t last_write_len;
} mock_t;

static opcua_statuscode_t mock_listen(void *c, const char *u) {
    (void)c;
    (void)u;
    return MU_STATUS_GOOD;
}
static void mock_shutdown(void *c) {
    (void)c;
}
static void mock_close(void *c, void *h) {
    (void)c;
    (void)h;
}

static opcua_statuscode_t mock_accept(void *c, void **handle) {
    mock_t *m = (mock_t *)c;
    m->accept_count++;
    *handle = (m->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_read(void *c, void *h, opcua_byte_t *buf, size_t cap, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    if (m->read_index >= m->inbound_count) {
        *n = 0;
        return MU_STATUS_GOOD;
    }
    size_t len = m->inbound_len[m->read_index];
    TEST_ASSERT_TRUE(len <= cap);
    (void)memcpy(buf, m->inbound[m->read_index], len);
    m->read_index++;
    *n = len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    (void)memcpy(m->last_write, buf, len);
    m->last_write_len = len;
    *n = len;
    return MU_STATUS_GOOD;
}

static void enqueue(mock_t *m, const opcua_byte_t *bytes, size_t len) {
    (void)memcpy(m->inbound[m->inbound_count], bytes, len);
    m->inbound_len[m->inbound_count] = len;
    m->inbound_count++;
}

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t auth_token, opcua_uint32_t handle) {
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {auth_token}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(w, &auth);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &null_str);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &null_id, 0);
}

static void write_create_session_request_body_fields(mu_binary_writer_t *w) {
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};

    mu_binary_write_string(w, &null_string);    /* ClientDescription.applicationUri */
    mu_binary_write_string(w, &null_string);    /* productUri */
    mu_binary_write_byte(w, 0x00);              /* applicationName */
    mu_binary_write_uint32(w, 1);               /* applicationType = Client */
    mu_binary_write_string(w, &null_string);    /* gatewayServerUri */
    mu_binary_write_string(w, &null_string);    /* discoveryProfileUri */
    mu_binary_write_int32(w, 0);                /* discoveryUrls[] */
    mu_binary_write_string(w, &null_string);    /* serverUri */
    mu_binary_write_string(w, &null_string);    /* endpointUrl */
    mu_binary_write_string(w, &null_string);    /* sessionName */
    mu_binary_write_bytestring(w, &null_bytes); /* clientNonce */
    mu_binary_write_bytestring(w, &null_bytes); /* clientCertificate */
    mu_binary_write_double(w, 0.0);             /* requestedSessionTimeout */
    mu_binary_write_uint32(w, 0);               /* maxResponseMessageSize */
}

static size_t build_msg(opcua_byte_t *out, size_t cap, opcua_uint32_t seq, opcua_uint32_t reqid,
                        const opcua_byte_t *body, size_t body_len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, cap);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, (opcua_uint32_t)(24 + body_len));
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    (void)memcpy(out + 24, body, body_len);
    return 24 + body_len;
}

static opcua_uint32_t parse_response(const opcua_byte_t *buf, size_t len, mu_binary_reader_t *body) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    r.position = 24;
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    opcua_uint64_t ts;
    opcua_uint32_t h;
    opcua_statuscode_t res;
    opcua_byte_t diag;
    opcua_int32_t st;
    mu_nodeid_t addl;
    opcua_byte_t enc;
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &h);
    mu_binary_read_statuscode(&r, &res);
    mu_binary_read_byte(&r, &diag);
    mu_binary_read_int32(&r, &st);
    mu_binary_read_nodeid(&r, &addl);
    mu_binary_read_byte(&r, &enc);
    *body = r;
    return type.identifier.numeric;
}

static void assert_response_service_result(const opcua_byte_t *buf, size_t len, opcua_uint32_t expected_type,
                                           opcua_uint32_t expected_request_handle,
                                           opcua_statuscode_t expected_service_result) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    r.position = 24;

    mu_nodeid_t resp_type;
    opcua_uint64_t ts;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &resp_type));
    TEST_ASSERT_EQUAL(expected_type, resp_type.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&r, &ts));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &request_handle));
    TEST_ASSERT_EQUAL(expected_request_handle, request_handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &service_result));
    TEST_ASSERT_EQUAL_HEX32(expected_service_result, service_result);
}

static bool string_equals_cstr(const mu_string_t *s, const char *expected) {
    size_t expected_len = strlen(expected);
    return s->length == (opcua_int32_t)expected_len && s->data != NULL && memcmp(s->data, expected, expected_len) == 0;
}

static void skip_application_description(mu_binary_reader_t *reader) {
    mu_string_t s;
    opcua_byte_t localized_text_mask;
    opcua_uint32_t application_type;
    opcua_int32_t discovery_url_count;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* applicationUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* productUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &localized_text_mask));
    if ((localized_text_mask & 0x01u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* locale */
    }
    if ((localized_text_mask & 0x02u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* text */
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(reader, &application_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* gatewayServerUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* discoveryProfileUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(reader, &discovery_url_count));
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, discovery_url_count);
    for (opcua_int32_t i = 0; i < discovery_url_count; ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s));
    }
}

static void write_hello_message(mu_binary_writer_t *w, opcua_byte_t *buffer, size_t capacity) {
    mu_string_t url = {20, (const opcua_byte_t *)"opc.tcp://host:4840"};

    mu_binary_writer_init(w, buffer, capacity);
    buffer[0] = 'H';
    buffer[1] = 'E';
    buffer[2] = 'L';
    buffer[3] = 'F';
    w->position = 4;
    mu_binary_write_uint32(w, 0);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_uint32(w, 8192);
    mu_binary_write_uint32(w, 8192);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &url);
    {
        mu_binary_writer_t header;
        mu_binary_writer_init(&header, buffer, capacity);
        header.position = 4;
        mu_binary_write_uint32(&header, (opcua_uint32_t)w->position);
    }
}

static void write_open_secure_channel_none_message(mu_binary_writer_t *w, opcua_byte_t *buffer, size_t capacity) {
    mu_string_t policy = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
    mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};

    mu_binary_writer_init(w, buffer, capacity);
    buffer[0] = 'O';
    buffer[1] = 'P';
    buffer[2] = 'N';
    buffer[3] = 'F';
    w->position = 4;
    mu_binary_write_uint32(w, 0);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &policy);
    mu_binary_write_int32(w, -1);
    mu_binary_write_int32(w, -1);
    mu_binary_write_uint32(w, 1);
    mu_binary_write_uint32(w, 1);
    mu_binary_write_nodeid(w, &opn_type);
    write_request_header(w, 0, 1);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_uint32(w, 1);
    mu_binary_write_int32(w, -1);
    mu_binary_write_uint32(w, 3600000);
    {
        mu_binary_writer_t header;
        mu_binary_writer_init(&header, buffer, capacity);
        header.position = 4;
        mu_binary_write_uint32(&header, (opcua_uint32_t)w->position);
    }
}

static size_t write_get_endpoints_message(opcua_byte_t *message, size_t capacity, opcua_uint32_t sequence_number,
                                          opcua_uint32_t request_id, opcua_uint32_t request_handle) {
    opcua_byte_t body[256];
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};

    mu_binary_writer_init(&writer, body, sizeof(body));
    {
        mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_GETENDPOINTSREQUEST}};
        mu_binary_write_nodeid(&writer, &type);
    }
    write_request_header(&writer, 0, request_handle);
    mu_binary_write_string(&writer, &null_string); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);             /* localeIds[] */
    mu_binary_write_int32(&writer, 0);             /* profileUris[] */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    return build_msg(message, capacity, sequence_number, request_id, body, writer.position);
}

/* User authentication callback: only accepts "admin" / "admin" */
static opcua_statuscode_t test_auth_handler(void *handle, const mu_string_t *username, const mu_bytestring_t *password,
                                            const mu_string_t *policy_id) {
    (void)handle;
    (void)policy_id;

    if (username == NULL || password == NULL) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    if (username->length == 5 && memcmp(username->data, "admin", 5) == 0 && password->length == 5 &&
        memcmp(password->data, "admin", 5) == 0) {
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
}

void test_plaintext_user_auth_flow_rejects_username_password_over_security_policy_none(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    /* ---- Build the inbound queue ---- */
    opcua_byte_t tmp[512];
    mu_binary_writer_t w;
    opcua_byte_t chunk[512];
    size_t clen;

    /* HEL */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    tmp[0] = 'H';
    tmp[1] = 'E';
    tmp[2] = 'L';
    tmp[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t url = {20, (const opcua_byte_t *)"opc.tcp://host:4840"};
    mu_binary_write_string(&w, &url);
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, tmp, w.position);

    /* OPN (OpenSecureChannelRequest) */
    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'O';
    chunk[1] = 'P';
    chunk[2] = 'N';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
    mu_binary_write_string(&w, &pol);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
    mu_binary_write_nodeid(&w, &opn_type);
    write_request_header(&w, 0, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 3600000);
    {
        mu_binary_writer_t os;
        mu_binary_writer_init(&os, chunk, sizeof(chunk));
        os.position = 4;
        mu_binary_write_uint32(&os, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, chunk, w.position);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    write_create_session_request_body_fields(&w);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* ---- Configure the server ---- */
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:test";
    config.product_uri = "urn:test";
    config.application_name = "test";
    static opcua_byte_t rx[8192], tx[8192];
    config.receive_buffer = rx;
    config.receive_buffer_size = sizeof(rx);
    config.send_buffer = tx;
    config.send_buffer_size = sizeof(tx);
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    /* Attach user auth handler */
    config.user_auth_handler = test_auth_handler;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> Response */
    mu_server_poll(server); /* CreateSession -> Response */

    /* ActivateSession with UserNameIdentityToken over SecurityPolicy#None.
       OPC-10000-4 sections 5.7.3.3 and 7.40.2.1 reject plaintext passwords
       before calling the configured user auth handler. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, server->sessions[0].auth_token, 3);
    {
        mu_string_t ns = {-1, NULL};
        mu_bytestring_t nb = {-1, NULL};
        mu_binary_write_string(&w, &ns);
        mu_binary_write_bytestring(&w, &nb);
    }
    mu_binary_write_int32(&w, 0);
    mu_binary_write_int32(&w, 0);

    {
        mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {324}};
        mu_string_t policy = {15, (const opcua_byte_t *)"username_policy"};
        mu_string_t username = {5, (const opcua_byte_t *)"admin"};
        mu_bytestring_t password = {5, (const opcua_byte_t *)"admin"};
        mu_string_t alg = {-1, NULL};

        mu_binary_write_extension_object_header(&w, &token_type, 19 + 9 + 9 + 4);
        mu_binary_write_string(&w, &policy);
        mu_binary_write_string(&w, &username);
        mu_binary_write_bytestring(&w, &password);
        mu_binary_write_string(&w, &alg);
    }

    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* ActivateSession -> Response */

    assert_response_service_result(mock.last_write, mock.last_write_len, MU_ID_ACTIVATESESSIONRESPONSE, 3,
                                   MU_STATUS_BAD_IDENTITYTOKENREJECTED);
}

void test_anonymous_identity_token_succeeds_over_security_policy_none(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    opcua_byte_t tmp[512];
    mu_binary_writer_t w;
    opcua_byte_t chunk[512];
    size_t clen;

    /* HEL */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    tmp[0] = 'H';
    tmp[1] = 'E';
    tmp[2] = 'L';
    tmp[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t url = {20, (const opcua_byte_t *)"opc.tcp://host:4840"};
    mu_binary_write_string(&w, &url);
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, tmp, w.position);

    /* OPN (OpenSecureChannelRequest) */
    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'O';
    chunk[1] = 'P';
    chunk[2] = 'N';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
    mu_binary_write_string(&w, &pol);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
    mu_binary_write_nodeid(&w, &opn_type);
    write_request_header(&w, 0, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 3600000);
    {
        mu_binary_writer_t os;
        mu_binary_writer_init(&os, chunk, sizeof(chunk));
        os.position = 4;
        mu_binary_write_uint32(&os, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, chunk, w.position);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    write_create_session_request_body_fields(&w);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:test";
    config.product_uri = "urn:test";
    config.application_name = "test";
    static opcua_byte_t rx[8192], tx[8192];
    config.receive_buffer = rx;
    config.receive_buffer_size = sizeof(rx);
    config.send_buffer = tx;
    config.send_buffer_size = sizeof(tx);
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> Response */
    mu_server_poll(server); /* CreateSession -> Response */

    /* ActivateSession with AnonymousIdentityToken.
       OPC-10000-4 section 5.5.4.2: GetEndpoints userIdentityTokens advertise
       the identity tokens a client may pass to ActivateSession; the
       SecurityPolicy None endpoint must remain usable with anonymous identity. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, server->sessions[0].auth_token, 3);
    {
        mu_string_t null_string = {-1, NULL};
        mu_bytestring_t null_bytes = {-1, NULL};
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_bytestring(&w, &null_bytes);
        mu_binary_write_int32(&w, 0);
        mu_binary_write_int32(&w, 0);

        {
            mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {321}};
            mu_string_t policy = {9, (const opcua_byte_t *)"anonymous"};
            mu_binary_write_extension_object_header(&w, &token_type, 13);
            mu_binary_write_string(&w, &policy);
        }

        mu_binary_write_string(&w, &null_string);
        mu_binary_write_bytestring(&w, &null_bytes);
    }

    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* ActivateSession -> Response */

    assert_response_service_result(mock.last_write, mock.last_write_len, MU_ID_ACTIVATESESSIONRESPONSE, 3,
                                   MU_STATUS_GOOD);
}

void test_getendpoints_security_policy_none_omits_username_identity_tokens(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));

    opcua_byte_t tmp[512];
    opcua_byte_t chunk[512];
    mu_binary_writer_t writer;
    size_t clen;

    write_hello_message(&writer, tmp, sizeof(tmp));
    enqueue(&mock, tmp, writer.position);

    write_open_secure_channel_none_message(&writer, chunk, sizeof(chunk));
    enqueue(&mock, chunk, writer.position);

    clen = write_get_endpoints_message(chunk, sizeof(chunk), 2, 2, 7);
    enqueue(&mock, chunk, clen);

    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:test";
    config.product_uri = "urn:test";
    config.application_name = "test";
    static opcua_byte_t rx[8192], tx[8192];
    config.receive_buffer = rx;
    config.receive_buffer_size = sizeof(rx);
    config.send_buffer = tx;
    config.send_buffer_size = sizeof(tx);
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;
    config.user_auth_handler = test_auth_handler;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> OpenSecureChannelResponse */
    mu_server_poll(server); /* GetEndpoints -> GetEndpointsResponse */

    mu_binary_reader_t body;
    assert_response_service_result(mock.last_write, mock.last_write_len, MU_ID_GETENDPOINTSRESPONSE, 7, MU_STATUS_GOOD);
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));

    opcua_int32_t endpoint_count = 0;
    bool saw_security_policy_none = false;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&body, &endpoint_count));
    TEST_ASSERT_GREATER_THAN_INT32(0, endpoint_count);

    for (opcua_int32_t endpoint_index = 0; endpoint_index < endpoint_count; ++endpoint_index) {
        mu_string_t text;
        mu_bytestring_t certificate;
        opcua_uint32_t security_mode = 0;
        opcua_int32_t token_count = 0;
        opcua_byte_t security_level = 0;

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&body, &text)); /* endpointUrl */
        skip_application_description(&body);
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&body, &certificate)); /* serverCertificate */
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&body, &security_mode));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&body, &text)); /* securityPolicyUri */

        bool is_security_policy_none = security_mode == MU_MESSAGE_SECURITY_MODE_NONE &&
                                       string_equals_cstr(&text, "http://opcfoundation.org/UA/SecurityPolicy#None");
        if (is_security_policy_none) {
            saw_security_policy_none = true;
        }

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&body, &token_count));
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, token_count);
        for (opcua_int32_t token_index = 0; token_index < token_count; ++token_index) {
            mu_string_t policy_id;
            opcua_uint32_t token_type = 0;

            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&body, &policy_id));
            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&body, &token_type));

            /* OPC-10000-4 section 5.5.4.2: GetEndpoints returns
               UserTokenPolicy[] for each endpoint; SecurityPolicy#None must
               not advertise username/password tokens on the wire. */
            if (is_security_policy_none) {
                TEST_ASSERT_NOT_EQUAL(MU_USER_TOKEN_TYPE_USERNAME, token_type);
                TEST_ASSERT_FALSE(string_equals_cstr(&policy_id, "username"));
            }

            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&body, &text)); /* issuedTokenType */
            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&body, &text)); /* issuerEndpointUrl */
            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&body, &text)); /* securityPolicyUri */
        }

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&body, &text)); /* transportProfileUri */
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&body, &security_level));
    }

    TEST_ASSERT_TRUE(saw_security_policy_none);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, body.status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_plaintext_user_auth_flow_rejects_username_password_over_security_policy_none);
    RUN_TEST(test_anonymous_identity_token_succeeds_over_security_policy_none);
    RUN_TEST(test_getendpoints_security_policy_none_omits_username_identity_tokens);
    return UNITY_END();
}
