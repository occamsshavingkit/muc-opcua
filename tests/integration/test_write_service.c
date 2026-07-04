/* tests/integration/test_write_service.c */
#include "../../src/core/server_internal.h"
#include "../../src/services/write.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
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

static opcua_statuscode_t last_written_value = 0;
static opcua_statuscode_t test_write_handler(void *handle, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                             const mu_variant_t *value) {
    (void)handle;
    (void)attribute_id;
    if (node_id->identifier.numeric == 1001 && value->type == MU_TYPE_INT32) {
        last_written_value = value->value.i32;
        return MU_STATUS_GOOD;
    }
    return MU_STATUS_BAD_NOTWRITABLE;
}

void test_write_service_integration(void) {
#ifdef MUC_OPCUA_SERVICE_WRITE
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    /* Address Space Definition */
    static const mu_reference_t obj_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1001}}, true},
                                              {{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1002}}, true}};
    static const mu_reference_t var_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
    static const mu_value_source_t mutable_value = {MU_VALUESOURCE_STATIC,
                                                    {.static_value = {MU_TYPE_INT32, {.i32 = 10}}}};
    static const mu_value_source_t readonly_value = {MU_VALUESOURCE_STATIC,
                                                     {.static_value = {MU_TYPE_INT32, {.i32 = 20}}}};
    static const mu_node_t nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                       MU_NODECLASS_OBJECT,
                                       {7, (const opcua_byte_t *)"Objects"},
                                       {7, (const opcua_byte_t *)"Objects"},
                                       obj_refs,
                                       2,
                                       NULL},
                                      {{1, MU_NODEID_NUMERIC, {1001}},
                                       MU_NODECLASS_VARIABLE,
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       var_refs,
                                       1,
                                       &mutable_value},
                                      {{1, MU_NODEID_NUMERIC, {1002}},
                                       MU_NODECLASS_VARIABLE,
                                       {6, (const opcua_byte_t *)"MyVar2"},
                                       {6, (const opcua_byte_t *)"MyVar2"},
                                       var_refs,
                                       1,
                                       &readonly_value}};
    static const mu_address_space_t space = {nodes, 3};

    /* ---- Build the inbound queue ---- */
    opcua_byte_t tmp[512];
    mu_binary_writer_t w;
    opcua_byte_t chunk[512];
    size_t clen;

    /* 1. HEL */
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

    /* 2. OPN */
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

    /* 3. CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    {
        mu_string_t ns = {-1, NULL};
        mu_bytestring_t nb = {-1, NULL};

        mu_binary_write_string(&w, &ns);     /* ClientDescription.applicationUri */
        mu_binary_write_string(&w, &ns);     /* productUri */
        mu_binary_write_byte(&w, 0x00);      /* applicationName */
        mu_binary_write_uint32(&w, 1);       /* applicationType = Client */
        mu_binary_write_string(&w, &ns);     /* gatewayServerUri */
        mu_binary_write_string(&w, &ns);     /* discoveryProfileUri */
        mu_binary_write_int32(&w, 0);        /* discoveryUrls[] */
        mu_binary_write_string(&w, &ns);     /* serverUri */
        mu_binary_write_string(&w, &ns);     /* endpointUrl */
        mu_binary_write_string(&w, &ns);     /* sessionName */
        mu_binary_write_bytestring(&w, &nb); /* clientNonce */
        mu_binary_write_bytestring(&w, &nb); /* clientCertificate */
        mu_binary_write_double(&w, 0.0);     /* requestedSessionTimeout */
        mu_binary_write_uint32(&w, 0);       /* maxResponseMessageSize */
    }
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* 4. ActivateSession */
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
    // Anonymous
    {
        mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {321}};
        mu_binary_write_extension_object_header(&w, &token_type, 20);
        mu_string_t policy = {16, (const opcua_byte_t *)"anonymous_policy"};
        mu_binary_write_string(&w, &policy);
    }
    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* 5. WriteRequest */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_WRITEREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);

    /* Write values array size: 4 items (happy, read-only, type-mismatch, wrong attribute) */
    mu_binary_write_int32(&w, 4);

    /* Item 0: Happy Path (Write Int32 42 to Node 1001) */
    mu_nodeid_t nid1 = {1, MU_NODEID_NUMERIC, {1001}};
    mu_binary_write_nodeid(&w, &nid1);
    mu_binary_write_uint32(&w, 13); /* attributeId: Value */
    mu_string_t range = {-1, NULL};
    mu_binary_write_string(&w, &range);
    mu_datavalue_t val1;
    (void)memset(&val1, 0, sizeof(val1));
    val1.has_value = true;
    val1.value.type = MU_TYPE_INT32;
    val1.value.value.i32 = 42;
    mu_binary_write_datavalue(&w, &val1);

    /* Item 1: Read-Only (Write Int32 42 to Node 1002) */
    mu_nodeid_t nid2 = {1, MU_NODEID_NUMERIC, {1002}};
    mu_binary_write_nodeid(&w, &nid2);
    mu_binary_write_uint32(&w, 13);
    mu_binary_write_string(&w, &range);
    mu_binary_write_datavalue(&w, &val1);

    /* Item 2: Type Mismatch (Write Float 4.2 to Node 1001) */
    mu_binary_write_nodeid(&w, &nid1);
    mu_binary_write_uint32(&w, 13);
    mu_binary_write_string(&w, &range);
    mu_datavalue_t val2;
    (void)memset(&val2, 0, sizeof(val2));
    val2.has_value = true;
    val2.value.type = MU_TYPE_FLOAT;
    val2.value.value.f = 4.2f;
    mu_binary_write_datavalue(&w, &val2);

    /* Item 3: Invalid Attribute (Write Int32 42 to Node 1001, Attribute DisplayName = 4) */
    mu_binary_write_nodeid(&w, &nid1);
    mu_binary_write_uint32(&w, 4);
    mu_binary_write_string(&w, &range);
    mu_binary_write_datavalue(&w, &val1);

    clen = build_msg(chunk, sizeof(chunk), 4, 4, tmp, w.position);
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
    config.address_space = &space;

    /* Write Handler registration */
    config.write_handler = test_write_handler;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> Response */
    mu_server_poll(server); /* CreateSession -> Response */
    mu_server_poll(server); /* ActivateSession -> Response */

    /* Process WriteRequest */
    mu_server_poll(server);

    /* Decode and check WriteResponse results */
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, mock.last_write, mock.last_write_len);
    r.position = 24;

    mu_nodeid_t resp_type;
    mu_binary_read_nodeid(&r, &resp_type);
    TEST_ASSERT_EQUAL(MU_ID_WRITERESPONSE, resp_type.identifier.numeric);

    opcua_uint64_t ts;
    opcua_uint32_t h;
    opcua_statuscode_t res;
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &h);
    mu_binary_read_statuscode(&r, &res);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, res);

    /* Skip serviceDiagnostics, stringTable, and additionalHeader */
    opcua_byte_t diag;
    mu_binary_read_byte(&r, &diag);
    opcua_int32_t string_table_len;
    mu_binary_read_int32(&r, &string_table_len);
    mu_binary_skip_extension_object(&r);

    /* Decode write results array */
    opcua_int32_t arr_len;
    mu_binary_read_int32(&r, &arr_len);
    TEST_ASSERT_EQUAL(4, arr_len);

    opcua_statuscode_t r0, r1, r2, r3;
    mu_binary_read_statuscode(&r, &r0);
    mu_binary_read_statuscode(&r, &r1);
    mu_binary_read_statuscode(&r, &r2);
    mu_binary_read_statuscode(&r, &r3);

    /* Happy Path */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, r0);
    TEST_ASSERT_EQUAL(42, last_written_value);

    /* Read-Only variable node */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTWRITABLE, r1);

    /* Mismatched type */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TYPEMISMATCH, r2);

    /* Invalid attribute range */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTWRITABLE, r3);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_service_integration);
    return UNITY_END();
}
