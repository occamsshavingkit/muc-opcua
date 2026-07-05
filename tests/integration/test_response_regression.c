/* tests/integration/test_response_regression.c
 * FR-014 / SC-003 byte-identical service response backstop.
 *
 * The test drives the real server over the mock transport through
 * HEL -> OPN -> CreateSession -> ActivateSession, then captures encoded service
 * response body bytes for Browse (OPC 10000-4 5.9.2), Read (OPC 10000-4
 * 5.11.2), and, when MUC_OPCUA_SUBSCRIPTIONS is enabled, Publish
 * (OPC 10000-4 5.14.5).
 *
 * Capture mode is intentional for the first orchestrator run: the golden arrays
 * below are empty placeholders. While an array length is 0, the test prints the
 * captured bytes as a C array literal and passes. The follow-up is mechanical:
 * paste the printed bytes into the matching golden_* array. Once non-empty, the
 * same harness asserts byte-for-byte equality.
 */
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "service_builders.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const opcua_byte_t golden_browse[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x23, 0x01,
    0x01, 0x01, 0xE8, 0x03, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x4D, 0x79, 0x56, 0x61, 0x72, 0x31, 0x02, 0x06, 0x00,
    0x00, 0x00, 0x4D, 0x79, 0x56, 0x61, 0x72, 0x31, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const opcua_byte_t golden_read[] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x2A,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#if MUC_OPCUA_SUBSCRIPTIONS
static const opcua_byte_t golden_publish[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif

#define MAX_INBOUND 12
typedef struct {
    int accept_count;
    opcua_byte_t inbound[MAX_INBOUND][1024];
    size_t inbound_len[MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    opcua_byte_t last_write[8192];
    size_t last_write_len;
} mock_t;

static opcua_uint32_t s_channel_id = 1u;

static opcua_uint32_t read_opn_channel_id(const opcua_byte_t *buf, size_t len) {
    if (len < 12) {
        return 1u;
    }
    return (opcua_uint32_t)buf[8] | ((opcua_uint32_t)buf[9] << 8u) | ((opcua_uint32_t)buf[10] << 16u) |
           ((opcua_uint32_t)buf[11] << 24u);
}

static void patch_msg_channel_ids(mock_t *m, opcua_uint32_t channel_id) {
    for (size_t i = 0; i < m->inbound_count; ++i) {
        if (m->inbound_len[i] >= 12 && ((m->inbound[i][0] == 'M' && m->inbound[i][1] == 'S' &&
                                         m->inbound[i][2] == 'G' && m->inbound[i][3] == 'F') ||
                                        (m->inbound[i][0] == 'C' && m->inbound[i][1] == 'L' &&
                                         m->inbound[i][2] == 'O' && m->inbound[i][3] == 'F'))) {
            m->inbound[i][8] = (opcua_byte_t)(channel_id);
            m->inbound[i][9] = (opcua_byte_t)(channel_id >> 8u);
            m->inbound[i][10] = (opcua_byte_t)(channel_id >> 16u);
            m->inbound[i][11] = (opcua_byte_t)(channel_id >> 24u);
        }
    }
}

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
    TEST_ASSERT_TRUE(len <= sizeof(m->last_write));
    (void)memcpy(m->last_write, buf, len);
    m->last_write_len = len;
    *n = len;
    return MU_STATUS_GOOD;
}

static void enqueue(mock_t *m, const opcua_byte_t *bytes, size_t len) {
    TEST_ASSERT_TRUE(m->inbound_count < MAX_INBOUND);
    TEST_ASSERT_TRUE(len <= sizeof(m->inbound[0]));
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
    mu_binary_write_uint32(&w, s_channel_id);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    (void)memcpy(out + 24, body, body_len);
    return 24 + body_len;
}

static void enqueue_connect(mock_t *mock) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;

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
    {
        mu_string_t url = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&w, &url);
    }
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    enqueue(mock, tmp, w.position);

    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'O';
    chunk[1] = 'P';
    chunk[2] = 'N';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    {
        mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
        mu_binary_write_string(&w, &pol);
    }
    mu_binary_write_int32(&w, -1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
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
    enqueue(mock, chunk, w.position);

    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_create_session_body(&w, 2, 60000.0);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(mock, chunk, clen);

    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 3);
    {
        mu_string_t ns = {-1, NULL};
        mu_bytestring_t nb = {-1, NULL};
        mu_binary_write_string(&w, &ns);
        mu_binary_write_bytestring(&w, &nb);
    }
    mu_binary_write_int32(&w, 0);
    mu_binary_write_int32(&w, 0);
    {
        mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY}};
        mu_binary_write_extension_object_header(&w, &anon, 0);
    }
    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(mock, chunk, clen);
}

static void enqueue_browse_objects(mock_t *mock, opcua_uint32_t seq) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_BROWSEREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    {
        mu_nodeid_t view = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&w, &view);
    }
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t objects = {0, MU_NODEID_NUMERIC, {85}};
        mu_nodeid_t hierarchical = {0, MU_NODEID_NUMERIC, {33}};
        mu_binary_write_nodeid(&w, &objects);
        mu_binary_write_uint32(&w, 0);
        mu_binary_write_nodeid(&w, &hierarchical);
        mu_binary_write_boolean(&w, true);
        mu_binary_write_uint32(&w, 0);
        mu_binary_write_uint32(&w, 0x3F);
    }
    enqueue(mock, chunk, build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position));
}

static void enqueue_read_myvar1(mock_t *mock, opcua_uint32_t seq) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_READREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_double(&w, 0.0);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t v = {1, MU_NODEID_NUMERIC, {1000}};
        mu_string_t ns = {-1, NULL};
        mu_binary_write_nodeid(&w, &v);
        mu_binary_write_uint32(&w, 13);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_uint16(&w, 0);
        mu_binary_write_string(&w, &ns);
    }
    enqueue(mock, chunk, build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position));
}

#if MUC_OPCUA_SUBSCRIPTIONS
static opcua_uint64_t s_tick_ms = 0;
static opcua_uint64_t test_get_tick_ms(void *c) {
    (void)c;
    return s_tick_ms;
}

static void enqueue_create_subscription(mock_t *mock, opcua_uint32_t seq) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESUBSCRIPTIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_double(&w, 100.0);
    mu_binary_write_uint32(&w, 30);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0);
    enqueue(mock, chunk, build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position));
}

static void enqueue_publish(mock_t *mock, opcua_uint32_t seq) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_PUBLISHREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_int32(&w, 0);
    enqueue(mock, chunk, build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position));
}
#endif

static size_t response_body_offset(const opcua_byte_t *buf, size_t len, opcua_uint32_t expected_type) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    TEST_ASSERT_TRUE(len >= 24);
    TEST_ASSERT_EQUAL_MEMORY("MSGF", buf, 4);
    r.position = 24;

    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL_UINT16(0, type.namespace_index);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, type.identifier_type);
    TEST_ASSERT_EQUAL_UINT32(expected_type, type.identifier.numeric);

    opcua_int64_t timestamp;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_byte_t diagnostic_mask;
    opcua_int32_t string_table_len;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&r, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &service_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&r, &diagnostic_mask));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &string_table_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &additional_header_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&r, &additional_header_encoding));
    (void)timestamp;
    (void)request_handle;
    (void)diagnostic_mask;
    (void)string_table_len;
    (void)additional_header_type;
    (void)additional_header_encoding;
    return r.position;
}

static const opcua_byte_t *capture_service_body(const mock_t *mock, opcua_uint32_t expected_type, size_t *body_len) {
    size_t offset = response_body_offset(mock->last_write, mock->last_write_len, expected_type);
    TEST_ASSERT_TRUE(offset <= mock->last_write_len);
    *body_len = mock->last_write_len - offset;
    return mock->last_write + offset;
}

static void print_c_array_literal(const char *name, const opcua_byte_t *bytes, size_t len) {
    (void)printf("\n%s capture (%zu bytes):\n", name, len);
    (void)printf("static const opcua_byte_t %s[] = {", name);
    if (len > 0) {
        (void)printf("\n");
    }
    for (size_t i = 0; i < len; ++i) {
        if ((i % 12u) == 0u) {
            (void)printf("    ");
        }
        (void)printf("0x%02X", (unsigned int)bytes[i]);
        if (i + 1u < len) {
            (void)printf(",");
        }
        (void)printf(((i % 12u) == 11u || i + 1u == len) ? "\n" : " ");
    }
    (void)printf("};\n");
}

static void assert_or_capture(const char *name, const opcua_byte_t *golden, size_t golden_len,
                              const opcua_byte_t *actual, size_t actual_len) {
    if (golden_len == 0u) {
        print_c_array_literal(name, actual, actual_len);
        return;
    }
    TEST_ASSERT_EQUAL(golden_len, actual_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(golden, actual, golden_len);
}

static const mu_reference_t regression_obj_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true}};
static const mu_reference_t regression_var_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
static const mu_value_source_t regression_var_value = {MU_VALUESOURCE_STATIC,
                                                       {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
static const mu_node_t regression_nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                              MU_NODECLASS_OBJECT,
                                              {7, (const opcua_byte_t *)"Objects"},
                                              {7, (const opcua_byte_t *)"Objects"},
                                              regression_obj_refs,
                                              1,
                                              NULL},
                                             {{1, MU_NODEID_NUMERIC, {1000}},
                                              MU_NODECLASS_VARIABLE,
                                              {6, (const opcua_byte_t *)"MyVar1"},
                                              {6, (const opcua_byte_t *)"MyVar1"},
                                              regression_var_refs,
                                              1,
                                              &regression_var_value}};
static const mu_address_space_t regression_space = {regression_nodes, 2};

static mu_server_t *make_server(mock_t *mock, opcua_byte_t *storage, size_t storage_size, mu_server_config_t *config) {
    (void)memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri = "urn:response-regression";
    config->product_uri = "urn:response-regression";
    config->application_name = "response-regression";
    static opcua_byte_t rx[8192], tx[8192];
    config->receive_buffer = rx;
    config->receive_buffer_size = sizeof(rx);
    config->send_buffer = tx;
    config->send_buffer_size = sizeof(tx);
    config->max_chunk_count = 1;
    config->max_message_size = 8192;
    config->max_sessions = 1;
    config->max_secure_channels = 1;
    fake_platform_init(NULL, &config->time_adapter, &config->entropy_adapter);
#if MUC_OPCUA_SUBSCRIPTIONS
    s_tick_ms = 0;
    config->time_adapter.get_tick_ms = test_get_tick_ms;
#endif
    config->tcp_adapter.context = mock;
    config->tcp_adapter.listen = mock_listen;
    config->tcp_adapter.accept = mock_accept;
    config->tcp_adapter.read = mock_read;
    config->tcp_adapter.write = mock_write;
    config->tcp_adapter.close_connection = mock_close;
    config->tcp_adapter.shutdown = mock_shutdown;
    config->address_space = &regression_space;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, storage_size, config, &server));
    return server;
}

static void run_connect(mu_server_t *server, mock_t *mock) {
    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> Response */
    s_channel_id = read_opn_channel_id(mock->last_write, mock->last_write_len);
    patch_msg_channel_ids(mock, s_channel_id);
    mu_server_poll(server); /* CreateSession -> Response */
    mu_server_poll(server); /* ActivateSession -> Response */
}

void test_read_browse_and_publish_response_bytes_are_stable(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_browse_objects(&mock, 4);
    enqueue_read_myvar1(&mock, 5);
#if MUC_OPCUA_SUBSCRIPTIONS
    enqueue_create_subscription(&mock, 6);
    enqueue_publish(&mock, 7);
#endif

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config);

    run_connect(server, &mock);

    size_t body_len;
    const opcua_byte_t *body;

    mu_server_poll(server);
    body = capture_service_body(&mock, MU_ID_BROWSERESPONSE, &body_len);
    assert_or_capture("golden_browse", golden_browse, sizeof(golden_browse), body, body_len);

    mu_server_poll(server);
    body = capture_service_body(&mock, MU_ID_READRESPONSE, &body_len);
    assert_or_capture("golden_read", golden_read, sizeof(golden_read), body, body_len);

#if MUC_OPCUA_SUBSCRIPTIONS
    mu_server_poll(server);
    body = capture_service_body(&mock, MU_ID_CREATESUBSCRIPTIONRESPONSE, &body_len);
    TEST_ASSERT_TRUE(body_len > 0);

    mu_server_poll(server);
    s_tick_ms = 100;
    mu_server_poll(server);
    body = capture_service_body(&mock, MU_ID_PUBLISHRESPONSE, &body_len);
    assert_or_capture("golden_publish", golden_publish, sizeof(golden_publish), body, body_len);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_browse_and_publish_response_bytes_are_stable);
    return UNITY_END();
}
