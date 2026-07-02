/* tests/integration/test_server_handshake.c
 * End-to-end: drive mu_server_poll through HEL -> OPN -> CreateSession ->
 * ActivateSession -> Browse -> Read with a mock transport that feeds one inbound
 * chunk per poll and captures each response. Proves the wired dispatch + UASC
 * framing produce a real handshake. */
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "service_builders.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
#define GET_CLIENT_HANDLE(s) ((s)->conns[0].client_handle)
#else
#define GET_CLIENT_HANDLE(s) ((s)->client_handle)
#endif

/* ---- mock transport: a queue of inbound chunks, captures the last write ---- */
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
    *handle = (m->accept_count == 1) ? (void *)1 : NULL; /* one client, no second */
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
    memcpy(buf, m->inbound[m->read_index], len);
    m->read_index++;
    *n = len;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    memcpy(m->last_write, buf, len);
    m->last_write_len = len;
    *n = len;
    return MU_STATUS_GOOD;
}

/* ---- inbound chunk builders ---- */
static void enqueue(mock_t *m, const opcua_byte_t *bytes, size_t len) {
    memcpy(m->inbound[m->inbound_count], bytes, len);
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

/* Wrap a service body (request-type NodeId + params) into a MSG chunk. */
static size_t build_msg(opcua_byte_t *out, size_t cap, opcua_uint32_t seq, opcua_uint32_t reqid,
                        const opcua_byte_t *body, size_t body_len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, cap);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, (opcua_uint32_t)(24 + body_len)); /* size */
    mu_binary_write_uint32(&w, 1);                               /* SecureChannelId */
    mu_binary_write_uint32(&w, 1);                               /* TokenId */
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    memcpy(out + 24, body, body_len);
    return 24 + body_len;
}

static void enqueue_hello(mock_t *mock) {
    opcua_byte_t tmp[512];
    mu_binary_writer_t w;

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
}

static void enqueue_open_secure_channel_none(mock_t *mock) {
    opcua_byte_t chunk[512];
    mu_binary_writer_t w;

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
}

static void enqueue_close_secure_channel(mock_t *mock, opcua_uint32_t seq, opcua_uint32_t reqid) {
    opcua_byte_t chunk[24];
    mu_binary_writer_t w;

    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'C';
    chunk[1] = 'L';
    chunk[2] = 'O';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, (opcua_uint32_t)sizeof(chunk));
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    enqueue(mock, chunk, sizeof(chunk));
}

static bool server_channel_is_open(const mu_server_t *server) {
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    return server->conns[0].secure_channel.is_open;
#else
    return server->secure_channel.is_open;
#endif
}

/* Read past a response's chunk header + ResponseHeader, returning the response
   type id and a reader positioned at the service body. */
static opcua_uint32_t parse_response(const opcua_byte_t *buf, size_t len, mu_binary_reader_t *body) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    r.position = 24; /* MSG body offset */
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    /* ResponseHeader: timestamp(8) handle(4) result(4) diag(1) strtbl(4) addl(nodeid+enc) */
    opcua_int64_t ts;
    opcua_uint32_t h;
    opcua_statuscode_t res;
    opcua_byte_t diag, enc;
    opcua_int32_t st;
    mu_nodeid_t addl;
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

void test_server_handshake_connect_browse_read(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));

    /* Address space: Objects(85) Organizes MyVar1(1000, Int32=42). */
    static const mu_reference_t obj_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true}};
    static const mu_reference_t var_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
    static const mu_value_source_t var_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
    static const mu_node_t nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                       MU_NODECLASS_OBJECT,
                                       {7, (const opcua_byte_t *)"Objects"},
                                       {7, (const opcua_byte_t *)"Objects"},
                                       obj_refs,
                                       1,
                                       NULL},
                                      {{1, MU_NODEID_NUMERIC, {1000}},
                                       MU_NODECLASS_VARIABLE,
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       var_refs,
                                       1,
                                       &var_value}};
    static const mu_address_space_t space = {nodes, 2};

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
    mu_binary_write_uint32(&w, 0);    /* size placeholder, set below */
    mu_binary_write_uint32(&w, 0);    /* ProtocolVersion */
    mu_binary_write_uint32(&w, 8192); /* ReceiveBufferSize */
    mu_binary_write_uint32(&w, 8192); /* SendBufferSize */
    mu_binary_write_uint32(&w, 0);    /* MaxMessageSize */
    mu_binary_write_uint32(&w, 0);    /* MaxChunkCount */
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
    mu_binary_write_uint32(&w, 0); /* size placeholder */
    mu_binary_write_uint32(&w, 0); /* SecureChannelId */
    mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
    mu_binary_write_string(&w, &pol);
    mu_binary_write_int32(&w, -1); /* SenderCert */
    mu_binary_write_int32(&w, -1); /* ReceiverThumbprint */
    mu_binary_write_uint32(&w, 1); /* SequenceNumber */
    mu_binary_write_uint32(&w, 1); /* RequestId */
    mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
    mu_binary_write_nodeid(&w, &opn_type);
    write_request_header(&w, 0, 1);
    mu_binary_write_uint32(&w, 0);       /* ClientProtocolVersion */
    mu_binary_write_uint32(&w, 0);       /* RequestType ISSUE */
    mu_binary_write_uint32(&w, 1);       /* SecurityMode NONE */
    mu_binary_write_int32(&w, -1);       /* ClientNonce */
    mu_binary_write_uint32(&w, 3600000); /* RequestedLifetime */
    {
        mu_binary_writer_t os;
        mu_binary_writer_init(&os, chunk, sizeof(chunk));
        os.position = 4;
        mu_binary_write_uint32(&os, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, chunk, w.position);

    /* GetEndpoints (on the open channel, before a session) */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_GETENDPOINTSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 6);
    {
        mu_string_t null_str = {-1, NULL};
        mu_binary_write_string(&w, &null_str);
        mu_binary_write_int32(&w, 0);
        mu_binary_write_int32(&w, 0);
    }
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_create_session_body(&w, 2, 60000.0);
    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* ActivateSession */
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
    mu_binary_write_int32(&w, 0); /* ClientSoftwareCertificates */
    mu_binary_write_int32(&w, 0); /* LocaleIds */
    {
        mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {321}};
        mu_binary_write_extension_object_header(&w, &anon, 0);
    }
    clen = build_msg(chunk, sizeof(chunk), 4, 4, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* Browse Objects */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_BROWSEREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);
    {
        mu_nodeid_t e = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&w, &e);
        mu_binary_write_int64(&w, 0);
        mu_binary_write_uint32(&w, 0);
    }
    mu_binary_write_uint32(&w, 0); /* RequestedMaxReferencesPerNode */
    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t o = {0, MU_NODEID_NUMERIC, {85}};
        mu_nodeid_t e = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&w, &o);
        mu_binary_write_uint32(&w, 0);
        mu_binary_write_nodeid(&w, &e);
        mu_binary_write_boolean(&w, false);
        mu_binary_write_uint32(&w, 0);
        mu_binary_write_uint32(&w, 0x3F);
    }
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* Read MyVar1 Value */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_READREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_double(&w, 0.0);
    mu_binary_write_uint32(&w, 3); /* NEITHER */
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
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* ---- Configure the server ---- */
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
    config.address_space = &space;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_binary_reader_t body;

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.last_write, 4);

    mu_server_poll(server); /* OPN -> OpenSecureChannelResponse */
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock.last_write, 4);

    mu_server_poll(server); /* GetEndpoints -> GetEndpointsResponse */
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));

    mu_server_poll(server); /* CreateSession */
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));

    mu_server_poll(server); /* ActivateSession */
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));

    mu_server_poll(server); /* Browse */
    TEST_ASSERT_EQUAL(MU_ID_BROWSERESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
    {
        opcua_int32_t nres;
        mu_binary_read_int32(&body, &nres);
        TEST_ASSERT_EQUAL(1, nres);
        opcua_statuscode_t opst;
        mu_binary_read_statuscode(&body, &opst);
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, opst);
        opcua_int32_t cp;
        mu_binary_read_int32(&body, &cp);
        opcua_int32_t nrefs;
        mu_binary_read_int32(&body, &nrefs);
        TEST_ASSERT_EQUAL(1, nrefs);
    }

    mu_server_poll(server); /* Read */
    TEST_ASSERT_EQUAL(MU_ID_READRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
    {
        opcua_int32_t nres;
        mu_binary_read_int32(&body, &nres);
        TEST_ASSERT_EQUAL(1, nres);
        opcua_byte_t mask;
        mu_binary_read_byte(&body, &mask);
        TEST_ASSERT_EQUAL(0x01, mask);
        opcua_byte_t vt;
        mu_binary_read_byte(&body, &vt);
        TEST_ASSERT_EQUAL(MU_TYPE_INT32, vt);
        opcua_int32_t val;
        mu_binary_read_int32(&body, &val);
        TEST_ASSERT_EQUAL(42, val);
    }
}

/* A MSG that skips a SequenceNumber (replay/gap) must abort the connection. */
void test_server_rejects_sequence_gap(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
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
    enqueue(&mock, tmp, w.position);

    /* OPN None, SequenceNumber 1 */
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
    enqueue(&mock, chunk, w.position);

    /* GetEndpoints with a gapped SequenceNumber (99 instead of 2). */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_GETENDPOINTSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 7);
    {
        mu_string_t null_str = {-1, NULL};
        mu_binary_write_string(&w, &null_str);
        mu_binary_write_int32(&w, 0);
        mu_binary_write_int32(&w, 0);
    }
    clen = build_msg(chunk, sizeof(chunk), 99, 7, tmp, w.position);
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

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN (seq 1) -> response, channel open */
    TEST_ASSERT_NOT_NULL(GET_CLIENT_HANDLE(server));
    mu_server_poll(server); /* GetEndpoints (seq 99, gap) -> abort */
    TEST_ASSERT_NULL(GET_CLIENT_HANDLE(server));
}

void test_close_secure_channel_message_closes_channel(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));

    enqueue_hello(&mock);
    enqueue_open_secure_channel_none(&mock);
    enqueue_close_secure_channel(&mock, 2, 2);

    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:test";
    config.product_uri = "urn:test";
    config.application_name = "test";
    opcua_byte_t rx[8192];
    opcua_byte_t tx[8192];
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
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.last_write, 4);
    mu_server_poll(server); /* OPN -> OpenSecureChannelResponse */
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock.last_write, 4);
    TEST_ASSERT_TRUE(server_channel_is_open(server));
    TEST_ASSERT_NOT_NULL(GET_CLIENT_HANDLE(server));

    /* OPC-10000-4 §5.6.3 / OPC-10000-6 §6.7.3: a CLO message closes the
       SecureChannel and the underlying transport connection. */
    mu_server_poll(server);
    TEST_ASSERT_FALSE(server_channel_is_open(server));
    TEST_ASSERT_NULL(GET_CLIENT_HANDLE(server));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_server_handshake_connect_browse_read);
    RUN_TEST(test_server_rejects_sequence_gap);
    RUN_TEST(test_close_secure_channel_message_closes_channel);
    return UNITY_END();
}
