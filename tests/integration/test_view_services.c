/* tests/integration/test_view_services.c
 * Nano View Service Set completion (OPC 10000-4 §5.9): drive mu_server_poll through
 * connect/session, then RegisterNodes (§5.9.5) and UnregisterNodes (§5.9.6).
 * Claude-authored test (RED first); Codex implements the handlers to make it pass. */
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "service_builders.h"
#include "unity.h"
#include <string.h>

/* Response NodeIds not yet defined in opcua_ids.h are written as literals here so
   the test compiles before the implementation lands. */
#define ID_REGISTERNODESREQUEST 560
#define ID_REGISTERNODESRESPONSE 563
#define ID_UNREGISTERNODESREQUEST 566
#define ID_UNREGISTERNODESRESPONSE 569

void setUp(void) {}
void tearDown(void) {}

#define MAX_INBOUND 8
typedef struct {
    int accept_count;
    opcua_byte_t inbound[MAX_INBOUND][512];
    size_t inbound_len[MAX_INBOUND];
    size_t inbound_count, read_index;
    opcua_byte_t last_write[8192];
    size_t last_write_len;
} mock_t;

/* Channel ID assigned by the server (T009: incrementing counter, not always 1).
   Set from the OPN response header before building any MSG chunks. */
static opcua_uint32_t s_channel_id = 0;

static opcua_uint32_t read_opn_channel_id(const opcua_byte_t *buf, size_t len) {
    if (len < 12) {
        return 1;
    }
    return (opcua_uint32_t)buf[8] | ((opcua_uint32_t)buf[9] << 8u) | ((opcua_uint32_t)buf[10] << 16u) |
           ((opcua_uint32_t)buf[11] << 24u);
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
static opcua_statuscode_t mock_accept(void *c, void **h) {
    mock_t *m = (mock_t *)c;
    m->accept_count++;
    *h = (m->accept_count == 1) ? (void *)1 : NULL;
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

/* Controllable clock for the live-CurrentTime test. */
#define TEST_NOW_DT 132000000000000000LL
static opcua_datetime_t s_test_now(void *c) {
    (void)c;
    return TEST_NOW_DT;
}

static void enqueue(mock_t *m, const opcua_byte_t *b, size_t len) {
    (void)memcpy(m->inbound[m->inbound_count], b, len);
    m->inbound_len[m->inbound_count] = len;
    m->inbound_count++;
}
static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t auth, opcua_uint32_t handle) {
    mu_nodeid_t a = {0, MU_NODEID_NUMERIC, {auth}}, nul = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t ns = {-1, NULL};
    mu_binary_write_nodeid(w, &a);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &ns);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &nul, 0);
}
static size_t build_msg(opcua_byte_t *out, size_t cap, opcua_uint32_t seq, opcua_uint32_t reqid,
                        const opcua_byte_t *body, size_t blen) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, cap);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, (opcua_uint32_t)(24 + blen));
    mu_binary_write_uint32(&w, s_channel_id);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    (void)memcpy(out + 24, body, blen);
    return 24 + blen;
}
static opcua_uint32_t parse_response(const opcua_byte_t *buf, size_t len, mu_binary_reader_t *body) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    r.position = 24;
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
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

/* Append HEL and OPN(None, seq 1) only. Must be followed by mu_server_poll for
   accept/HEL/OPN, then set s_channel_id from the OPN response, then call
   enqueue_session before any service requests. */
static void enqueue_connect(mock_t *mock) {
    opcua_byte_t chunk[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'H';
    chunk[1] = 'E';
    chunk[2] = 'L';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    {
        mu_string_t u = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&w, &u);
    }
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, chunk, sizeof(chunk));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    enqueue(mock, chunk, w.position);

    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'O';
    chunk[1] = 'P';
    chunk[2] = 'N';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    {
        mu_string_t p = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
        mu_binary_write_string(&w, &p);
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

/* Append CreateSession(seq 2) and ActivateSession(seq 3) as MSG chunks using
   the channel_id read from the OPN response. Call after s_channel_id is set. */
static void enqueue_session(mock_t *mock) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
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
        mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {321}};
        mu_binary_write_extension_object_header(&w, &anon, 0);
    }
    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(mock, chunk, clen);
}

static mu_server_t *make_server(mock_t *mock, opcua_byte_t *storage, size_t storage_size, mu_server_config_t *config,
                                const mu_address_space_t *space) {
    (void)memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri = "urn:t";
    config->product_uri = "urn:t";
    config->application_name = "t";
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
    config->tcp_adapter.context = mock;
    config->tcp_adapter.listen = mock_listen;
    config->tcp_adapter.accept = mock_accept;
    config->tcp_adapter.read = mock_read;
    config->tcp_adapter.write = mock_write;
    config->tcp_adapter.close_connection = mock_close;
    config->tcp_adapter.shutdown = mock_shutdown;
    config->address_space = space;
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, storage_size, config, &server));
    return server;
}

/* RegisterNodes echoes nodesToRegister; UnregisterNodes returns Good. */
void test_register_and_unregister_nodes(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL */
    mu_server_poll(server); /* OPN */
    s_channel_id = read_opn_channel_id(mock.last_write, mock.last_write_len);
    enqueue_session(&mock);

    /* RegisterNodes (seq 4): nodesToRegister = [ ns=1;i=1000 ]. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_REGISTERNODESREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);
    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t n = {1, MU_NODEID_NUMERIC, {1000}};
        mu_binary_write_nodeid(&w, &n);
    }
    clen = build_msg(chunk, sizeof(chunk), 4, 4, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* UnregisterNodes (seq 5): nodesToUnregister = [ ns=1;i=1000 ]. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_UNREGISTERNODESREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t n = {1, MU_NODEID_NUMERIC, {1000}};
        mu_binary_write_nodeid(&w, &n);
    }
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateSession */
    mu_server_poll(server); /* ActivateSession */

    mu_server_poll(server); /* RegisterNodes */
#ifdef MUC_OPCUA_SERVICE_REGISTER_NODES
    TEST_ASSERT_EQUAL(ID_REGISTERNODESRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
    {
        opcua_int32_t n;
        mu_binary_read_int32(&body, &n);
        TEST_ASSERT_EQUAL(1, n);
        mu_nodeid_t got;
        mu_binary_read_nodeid(&body, &got);
        TEST_ASSERT_EQUAL(1, got.namespace_index);
        TEST_ASSERT_EQUAL(1000, got.identifier.numeric);
    }

    mu_server_poll(server); /* UnregisterNodes */
    TEST_ASSERT_EQUAL(ID_UNREGISTERNODESRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
#else
    /* RegisterNodes/UnregisterNodes aren't compiled into this profile at all
       (their dispatch-table rows are #ifdef MUC_OPCUA_SERVICE_REGISTER_NODES),
       so both requests are correctly rejected as unsupported services. */
    TEST_ASSERT_EQUAL(MU_ID_SERVICEFAULT, parse_response(mock.last_write, mock.last_write_len, &body));

    mu_server_poll(server); /* UnregisterNodes */
    TEST_ASSERT_EQUAL(MU_ID_SERVICEFAULT, parse_response(mock.last_write, mock.last_write_len, &body));
#endif
}

/* BrowseNext: this server issues no ContinuationPoints, so any continuation point
   supplied is unknown -> per-operation Bad_ContinuationPointInvalid, but the server
   must still answer with a BrowseNextResponse (not Bad_ServiceUnsupported). */
#define ID_BROWSENEXTRESPONSE 536
#define STATUS_BAD_CONTINUATIONPOINTINVALID 0x804A0000u

void test_browse_next_invalid_continuation_point(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL */
    mu_server_poll(server); /* OPN */
    s_channel_id = read_opn_channel_id(mock.last_write, mock.last_write_len);
    enqueue_session(&mock);

    /* BrowseNext (seq 4): releaseContinuationPoints=false, one 8-byte continuation point. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_BROWSENEXTREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);
    mu_binary_write_boolean(&w, false); /* releaseContinuationPoints */
    mu_binary_write_int32(&w, 1);       /* continuationPoints[] count */
    {
        opcua_byte_t cp[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        mu_bytestring_t bs = {8, cp};
        mu_binary_write_bytestring(&w, &bs);
    }
    clen = build_msg(chunk, sizeof(chunk), 4, 4, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateSession */
    mu_server_poll(server); /* ActivateSession */

    mu_server_poll(server); /* BrowseNext */
    TEST_ASSERT_EQUAL(ID_BROWSENEXTRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
    /* results[]: one BrowseResult with Bad_ContinuationPointInvalid, null CP, no refs. */
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(1, nres);
    opcua_statuscode_t st;
    mu_binary_read_statuscode(&body, &st);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_CONTINUATIONPOINTINVALID, st);
    mu_bytestring_t cp;
    mu_binary_read_bytestring(&body, &cp);
    TEST_ASSERT_EQUAL(-1, cp.length); /* null continuationPoint */
    opcua_int32_t nrefs;
    mu_binary_read_int32(&body, &nrefs);
    TEST_ASSERT_EQUAL(0, nrefs);
}

/* TranslateBrowsePathsToNodeIds: resolve a RelativePath over the address space. */
#define ID_TRANSLATEBROWSEPATHSREQUEST 554
#define ID_TRANSLATEBROWSEPATHSRESPONSE 557
#define STATUS_BAD_NOMATCH 0x806F0000u

/* Objects(85) -Organizes(35)-> MyVar1(ns=1;i=1000, BrowseName "MyVar1"). */
static const mu_reference_t tbp_obj_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true}};
static const mu_reference_t tbp_var_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
static const mu_value_source_t tbp_var_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
static const mu_node_t tbp_nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                       MU_NODECLASS_OBJECT,
                                       {7, (const opcua_byte_t *)"Objects"},
                                       {7, (const opcua_byte_t *)"Objects"},
                                       tbp_obj_refs,
                                       1,
                                       NULL},
                                      {{1, MU_NODEID_NUMERIC, {1000}},
                                       MU_NODECLASS_VARIABLE,
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       tbp_var_refs,
                                       1,
                                       &tbp_var_value}};
static const mu_address_space_t tbp_space = {tbp_nodes, 2};

/* Write one BrowsePath: start at Objects(85), one element via HierarchicalReferences
   (i=33, includeSubtypes) to QualifiedName {ns=1, name}. */
static void write_browse_path(mu_binary_writer_t *w, const char *name) {
    mu_nodeid_t start = {0, MU_NODEID_NUMERIC, {85}};
    mu_nodeid_t hier = {0, MU_NODEID_NUMERIC, {33}};
    mu_binary_write_nodeid(w, &start); /* startingNode */
    mu_binary_write_int32(w, 1);       /* relativePath.elements count */
    mu_binary_write_nodeid(w, &hier);  /* referenceTypeId */
    mu_binary_write_boolean(w, false); /* isInverse */
    mu_binary_write_boolean(w, true);  /* includeSubtypes */
    mu_binary_write_uint16(w, 1);      /* targetName.namespaceIndex */
    {
        mu_string_t s = {(opcua_int32_t)strlen(name), (const opcua_byte_t *)name};
        mu_binary_write_string(w, &s);
    }
}

void test_translate_browse_paths(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &tbp_space);
    mu_binary_reader_t body;

    mu_server_poll(server);
    mu_server_poll(server);
    mu_server_poll(server); /* accept,HEL,OPN */
    s_channel_id = read_opn_channel_id(mock.last_write, mock.last_write_len);
    enqueue_session(&mock);

    /* TranslateBrowsePaths (seq 4): path 1 -> MyVar1 (Good), path 2 -> bad name (NoMatch). */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_TRANSLATEBROWSEPATHSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);
    mu_binary_write_int32(&w, 2); /* browsePaths count */
    write_browse_path(&w, "MyVar1");
    write_browse_path(&w, "Nope");
    clen = build_msg(chunk, sizeof(chunk), 4, 4, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server);
    mu_server_poll(server); /* Create,Activate */

    mu_server_poll(server); /* TranslateBrowsePaths */
    TEST_ASSERT_EQUAL(ID_TRANSLATEBROWSEPATHSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(2, nres);

    /* result[0]: Good, one target = ns=1;i=1000, remainingPathIndex = UInt32.Max. */
    opcua_statuscode_t st0;
    mu_binary_read_statuscode(&body, &st0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, st0);
    opcua_int32_t nt0;
    mu_binary_read_int32(&body, &nt0);
    TEST_ASSERT_EQUAL(1, nt0);
    mu_nodeid_t tgt;
    mu_binary_read_nodeid(&body, &tgt); /* ExpandedNodeId (local) reads as NodeId */
    TEST_ASSERT_EQUAL(1, tgt.namespace_index);
    TEST_ASSERT_EQUAL(1000, tgt.identifier.numeric);
    opcua_uint32_t rem;
    mu_binary_read_uint32(&body, &rem);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFFu, rem);

    /* result[1]: Bad_NoMatch, zero targets. */
    opcua_statuscode_t st1;
    mu_binary_read_statuscode(&body, &st1);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_NOMATCH, st1);
    opcua_int32_t nt1;
    mu_binary_read_int32(&body, &nt1);
    TEST_ASSERT_EQUAL(0, nt1);
}

/* US2a: with NO integrator address space, the library's default Base Information
   node set must answer Read of ServerStatus.State and Browse of Server. This
   default node set (src/address_space/base_nodes.c's s_base_nodes[]) is itself
   entirely #ifdef MUC_OPCUA_BASE_NODES -- Nano/Micro leave it off (the
   application is expected to supply its own address space instead), so
   ServerStatus.State genuinely does not exist there and Read correctly returns
   Bad_NodeIdUnknown rather than a value. */
#if MUC_OPCUA_BASE_NODES
void test_base_information_default_nodes(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL); /* no integrator space */
    mu_binary_reader_t body;

    mu_server_poll(server);
    mu_server_poll(server);
    mu_server_poll(server); /* accept,HEL,OPN */
    s_channel_id = read_opn_channel_id(mock.last_write, mock.last_write_len);
    enqueue_session(&mock);

    /* Read (seq 4): ServerStatus.State (ns=0;i=2259) Value. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_READREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);
    mu_binary_write_double(&w, 0.0); /* maxAge */
    mu_binary_write_uint32(&w, 3);   /* TimestampsToReturn Neither */
    mu_binary_write_int32(&w, 1);    /* nodesToRead count */
    {
        mu_nodeid_t n = {0, MU_NODEID_NUMERIC, {2259}};
        mu_string_t ns = {-1, NULL};
        mu_binary_write_nodeid(&w, &n);
        mu_binary_write_uint32(&w, 13); /* AttributeId Value */
        mu_binary_write_string(&w, &ns);
        mu_binary_write_uint16(&w, 0);
        mu_binary_write_string(&w, &ns);
    }
    clen = build_msg(chunk, sizeof(chunk), 4, 4, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* Browse (seq 5): Server (ns=0;i=2253), forward HierarchicalReferences. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_BROWSEREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 5);
    {
        mu_nodeid_t e = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&w, &e);
        mu_binary_write_int64(&w, 0);
        mu_binary_write_uint32(&w, 0);
    }
    mu_binary_write_uint32(&w, 0); /* RequestedMaxReferencesPerNode */
    mu_binary_write_int32(&w, 1);  /* nodesToBrowse count */
    {
        mu_nodeid_t srv = {0, MU_NODEID_NUMERIC, {2253}}, hier = {0, MU_NODEID_NUMERIC, {33}};
        mu_binary_write_nodeid(&w, &srv);
        mu_binary_write_uint32(&w, 0);
        mu_binary_write_nodeid(&w, &hier);
        mu_binary_write_boolean(&w, true);
        mu_binary_write_uint32(&w, 0);
        mu_binary_write_uint32(&w, 0x3F);
    }
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server);
    mu_server_poll(server); /* Create,Activate */

    mu_server_poll(server); /* Read ServerStatus.State -> Int32 Running(0) */
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
        TEST_ASSERT_EQUAL(0, val); /* ServerState Running */
    }

    mu_server_poll(server); /* Browse Server -> children */
    TEST_ASSERT_EQUAL(MU_ID_BROWSERESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
    {
        opcua_int32_t nr;
        mu_binary_read_int32(&body, &nr);
        TEST_ASSERT_EQUAL(1, nr);
        opcua_statuscode_t opst;
        mu_binary_read_statuscode(&body, &opst);
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, opst);
        mu_bytestring_t cp;
        mu_binary_read_bytestring(&body, &cp);
        opcua_int32_t nrefs;
        mu_binary_read_int32(&body, &nrefs);
        TEST_ASSERT_TRUE(nrefs >= 2);
    }
}
#endif /* MUC_OPCUA_BASE_NODES */

/* US2b: ServerStatus.CurrentTime (i=2258) is a live DateTime sourced from the time
   adapter via a runtime-bound value source in the base node set. Same
   MUC_OPCUA_BASE_NODES dependency as test_base_information_default_nodes above. */
#if MUC_OPCUA_BASE_NODES
void test_server_status_current_time_is_live(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    /* Point the (live) time adapter at a known clock; CurrentTime must read it. */
    server->config.time_adapter.get_time = s_test_now;
    mu_binary_reader_t body;

    mu_server_poll(server);
    mu_server_poll(server);
    mu_server_poll(server); /* accept,HEL,OPN */
    s_channel_id = read_opn_channel_id(mock.last_write, mock.last_write_len);
    enqueue_session(&mock);

    /* Read (seq 4): ServerStatus.CurrentTime (ns=0;i=2258) Value. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_READREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 4);
    mu_binary_write_double(&w, 0.0);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t n = {0, MU_NODEID_NUMERIC, {2258}};
        mu_string_t ns = {-1, NULL};
        mu_binary_write_nodeid(&w, &n);
        mu_binary_write_uint32(&w, 13);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_uint16(&w, 0);
        mu_binary_write_string(&w, &ns);
    }
    clen = build_msg(chunk, sizeof(chunk), 4, 4, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server);
    mu_server_poll(server); /* Create,Activate */

    mu_server_poll(server); /* Read CurrentTime -> DateTime == s_test_now() */
    TEST_ASSERT_EQUAL(MU_ID_READRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body));
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(1, nres);
    opcua_byte_t mask;
    mu_binary_read_byte(&body, &mask);
    TEST_ASSERT_EQUAL(0x01, mask);
    opcua_byte_t vt;
    mu_binary_read_byte(&body, &vt);
    TEST_ASSERT_EQUAL(MU_TYPE_DATETIME, vt);
    opcua_int64_t dt;
    mu_binary_read_int64(&body, &dt);
    TEST_ASSERT_EQUAL_HEX64(TEST_NOW_DT, dt);
}
#endif /* MUC_OPCUA_BASE_NODES */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_register_and_unregister_nodes);
    RUN_TEST(test_browse_next_invalid_continuation_point);
    RUN_TEST(test_translate_browse_paths);
#if MUC_OPCUA_BASE_NODES
    RUN_TEST(test_base_information_default_nodes);
    RUN_TEST(test_server_status_current_time_is_live);
#endif
    return UNITY_END();
}
