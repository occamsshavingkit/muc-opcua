/* tests/integration/test_subscriptions.c
 * Micro profile — Subscription Service Set (OPC 10000-4 §5.14). Drive mu_server_poll
 * through connect/session, then CreateSubscription (§5.14.2) and DeleteSubscriptions
 * (§5.14.8). Claude-authored test (RED first); Codex implements the engine + handlers.
 *
 * Wire layout reference:
 *   CreateSubscriptionRequest  (§5.14.2.2): Double requestedPublishingInterval,
 *     UInt32 requestedLifetimeCount, UInt32 requestedMaxKeepAliveCount,
 *     UInt32 maxNotificationsPerPublish, Boolean publishingEnabled, Byte priority.
 *   CreateSubscriptionResponse (§5.14.2.2): UInt32 subscriptionId,
 *     Double revisedPublishingInterval, UInt32 revisedLifetimeCount,
 *     UInt32 revisedMaxKeepAliveCount.
 *   DeleteSubscriptionsRequest (§5.14.8.2): UInt32[] subscriptionIds.
 *   DeleteSubscriptionsResponse: StatusCode[] results.
 */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include <string.h>

/* IDs/status codes not yet defined in opcua_ids.h / status.h are written as literals
   here so the test compiles before the implementation lands. */
#define ID_CREATESUBSCRIPTIONREQUEST   787
#define ID_CREATESUBSCRIPTIONRESPONSE  790
#define ID_DELETESUBSCRIPTIONSREQUEST  847
#define ID_DELETESUBSCRIPTIONSRESPONSE 850
#define ID_SERVICEFAULT                397
#define ID_CREATEMONITOREDITEMSREQUEST  751
#define ID_CREATEMONITOREDITEMSRESPONSE 754
#define ID_DELETEMONITOREDITEMSREQUEST  781
#define ID_DELETEMONITOREDITEMSRESPONSE 784
#define STATUS_BAD_TOOMANYSUBSCRIPTIONS  0x80DD0000u
#define STATUS_BAD_SUBSCRIPTIONIDINVALID 0x80280000u
#define STATUS_BAD_NODEIDUNKNOWN         0x80340000u
#define STATUS_BAD_TOOMANYMONITOREDITEMS 0x80DB0000u
#define STATUS_BAD_MONITOREDITEMIDINVALID 0x80420000u

void setUp(void) {}
void tearDown(void) {}

#define MAX_INBOUND 16
typedef struct {
    int accept_count;
    opcua_byte_t inbound[MAX_INBOUND][1024];
    size_t inbound_len[MAX_INBOUND];
    size_t inbound_count, read_index;
    opcua_byte_t last_write[8192];
    size_t last_write_len;
} mock_t;

static opcua_statuscode_t mock_listen(void *c, const char *u) { (void)c; (void)u; return MU_STATUS_GOOD; }
static void mock_shutdown(void *c) { (void)c; }
static void mock_close(void *c, void *h) { (void)c; (void)h; }
static opcua_statuscode_t mock_accept(void *c, void **h) {
    mock_t *m = (mock_t *)c; m->accept_count++; *h = (m->accept_count == 1) ? (void *)1 : NULL; return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_read(void *c, void *h, opcua_byte_t *buf, size_t cap, size_t *n) {
    mock_t *m = (mock_t *)c; (void)h;
    if (m->read_index >= m->inbound_count) { *n = 0; return MU_STATUS_GOOD; }
    size_t len = m->inbound_len[m->read_index]; TEST_ASSERT_TRUE(len <= cap);
    memcpy(buf, m->inbound[m->read_index], len); m->read_index++; *n = len; return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    mock_t *m = (mock_t *)c; (void)h; memcpy(m->last_write, buf, len); m->last_write_len = len; *n = len; return MU_STATUS_GOOD;
}

static void enqueue(mock_t *m, const opcua_byte_t *b, size_t len) {
    memcpy(m->inbound[m->inbound_count], b, len); m->inbound_len[m->inbound_count] = len; m->inbound_count++;
}
static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t auth, opcua_uint32_t handle) {
    mu_nodeid_t a = { 0, MU_NODEID_NUMERIC, { auth } }, nul = { 0, MU_NODEID_NUMERIC, { 0 } };
    mu_string_t ns = { -1, NULL };
    mu_binary_write_nodeid(w, &a); mu_binary_write_int64(w, 0); mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0); mu_binary_write_string(w, &ns); mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &nul, 0);
}
static size_t build_msg(opcua_byte_t *out, size_t cap, opcua_uint32_t seq, opcua_uint32_t reqid,
                        const opcua_byte_t *body, size_t blen) {
    mu_binary_writer_t w; mu_binary_writer_init(&w, out, cap);
    out[0]='M'; out[1]='S'; out[2]='G'; out[3]='F'; w.position = 4;
    mu_binary_write_uint32(&w, (opcua_uint32_t)(24 + blen));
    mu_binary_write_uint32(&w, 1); mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq); mu_binary_write_uint32(&w, reqid);
    memcpy(out + 24, body, blen); return 24 + blen;
}
/* Parse the response chunk: returns the response type NodeId, positions `body` at the
   service body, and outputs the ResponseHeader service result. */
static opcua_uint32_t parse_response(const opcua_byte_t *buf, size_t len, mu_binary_reader_t *body,
                                     opcua_statuscode_t *service_result) {
    mu_binary_reader_t r; mu_binary_reader_init(&r, buf, len); r.position = 24;
    mu_nodeid_t type; mu_binary_read_nodeid(&r, &type);
    opcua_int64_t ts; opcua_uint32_t h; opcua_statuscode_t res; opcua_byte_t diag, enc; opcua_int32_t st; mu_nodeid_t addl;
    mu_binary_read_int64(&r,&ts); mu_binary_read_uint32(&r,&h); mu_binary_read_statuscode(&r,&res);
    mu_binary_read_byte(&r,&diag); mu_binary_read_int32(&r,&st); mu_binary_read_nodeid(&r,&addl); mu_binary_read_byte(&r,&enc);
    if (service_result) *service_result = res;
    *body = r; return type.identifier.numeric;
}

/* Append HEL, OPN(None, seq 1), CreateSession(seq 2), ActivateSession(seq 3). */
static void enqueue_connect(mock_t *mock) {
    opcua_byte_t tmp[512], chunk[512]; mu_binary_writer_t w; size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    tmp[0]='H';tmp[1]='E';tmp[2]='L';tmp[3]='F'; w.position=4;
    mu_binary_write_uint32(&w,0); mu_binary_write_uint32(&w,0); mu_binary_write_uint32(&w,8192);
    mu_binary_write_uint32(&w,8192); mu_binary_write_uint32(&w,0); mu_binary_write_uint32(&w,0);
    { mu_string_t u={19,(const opcua_byte_t*)"opc.tcp://host:4840"}; mu_binary_write_string(&w,&u); }
    { mu_binary_writer_t hs; mu_binary_writer_init(&hs,tmp,sizeof(tmp)); hs.position=4; mu_binary_write_uint32(&hs,(opcua_uint32_t)w.position); }
    enqueue(mock, tmp, w.position);

    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0]='O';chunk[1]='P';chunk[2]='N';chunk[3]='F'; w.position=4;
    mu_binary_write_uint32(&w,0); mu_binary_write_uint32(&w,0);
    { mu_string_t p={47,(const opcua_byte_t*)"http://opcfoundation.org/UA/SecurityPolicy#None"}; mu_binary_write_string(&w,&p); }
    mu_binary_write_int32(&w,-1); mu_binary_write_int32(&w,-1);
    mu_binary_write_uint32(&w,1); mu_binary_write_uint32(&w,1);
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{MU_ID_OPENSECURECHANNELREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w,0,1);
    mu_binary_write_uint32(&w,0); mu_binary_write_uint32(&w,0); mu_binary_write_uint32(&w,1);
    mu_binary_write_int32(&w,-1); mu_binary_write_uint32(&w,3600000);
    { mu_binary_writer_t os; mu_binary_writer_init(&os,chunk,sizeof(chunk)); os.position=4; mu_binary_write_uint32(&os,(opcua_uint32_t)w.position); }
    enqueue(mock, chunk, w.position);

    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{MU_ID_CREATESESSIONREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w,0,2);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position); enqueue(mock, chunk, clen);

    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{MU_ID_ACTIVATESESSIONREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w,12345,3);
    { mu_string_t ns={-1,NULL}; mu_bytestring_t nb={-1,NULL}; mu_binary_write_string(&w,&ns); mu_binary_write_bytestring(&w,&nb); }
    mu_binary_write_int32(&w,0); mu_binary_write_int32(&w,0);
    { mu_nodeid_t anon={0,MU_NODEID_NUMERIC,{321}}; mu_binary_write_extension_object_header(&w,&anon,0); }
    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position); enqueue(mock, chunk, clen);
}

/* Enqueue a CreateSubscription MSG (seq == handle). */
static void enqueue_create_subscription(mock_t *mock, opcua_uint32_t seq,
                                        opcua_double_t interval, opcua_uint32_t lifetime,
                                        opcua_uint32_t keepalive) {
    opcua_byte_t tmp[512], chunk[512]; mu_binary_writer_t w; size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{ID_CREATESUBSCRIPTIONREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w, 12345, seq);
    mu_binary_write_double(&w, interval);
    mu_binary_write_uint32(&w, lifetime);
    mu_binary_write_uint32(&w, keepalive);
    mu_binary_write_uint32(&w, 0);          /* maxNotificationsPerPublish */
    mu_binary_write_boolean(&w, true);      /* publishingEnabled */
    mu_binary_write_byte(&w, 0);            /* priority */
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position); enqueue(mock, chunk, clen);
}

/* Write one MonitoredItemCreateRequest (Value attribute, Reporting mode, null filter). */
static void write_moncreate_item(mu_binary_writer_t *w, opcua_uint16_t ns, opcua_uint32_t ident,
                                 opcua_uint32_t client_handle, opcua_double_t interval) {
    mu_nodeid_t n = { ns, MU_NODEID_NUMERIC, { ident } };
    mu_binary_write_nodeid(w, &n);          /* itemToMonitor.nodeId */
    mu_binary_write_uint32(w, 13);          /* attributeId = Value */
    { mu_string_t nul = {-1,NULL}; mu_binary_write_string(w, &nul); }   /* indexRange */
    mu_binary_write_uint16(w, 0);           /* dataEncoding.namespaceIndex */
    { mu_string_t nul = {-1,NULL}; mu_binary_write_string(w, &nul); }   /* dataEncoding.name */
    mu_binary_write_uint32(w, 2);           /* monitoringMode = Reporting */
    mu_binary_write_uint32(w, client_handle);
    mu_binary_write_double(w, interval);    /* samplingInterval */
    { mu_nodeid_t nul = {0,MU_NODEID_NUMERIC,{0}}; mu_binary_write_extension_object_header(w, &nul, 0); } /* filter (null) */
    mu_binary_write_uint32(w, 1);           /* queueSize */
    mu_binary_write_boolean(w, true);       /* discardOldest */
}

/* Read one MonitoredItemCreateResult: returns the statusCode and outputs the id. */
static opcua_statuscode_t read_moncreate_result(mu_binary_reader_t *body, opcua_uint32_t *id) {
    opcua_statuscode_t st; mu_binary_read_statuscode(body, &st);
    mu_binary_read_uint32(body, id);
    opcua_double_t revised; mu_binary_read_double(body, &revised);
    opcua_uint32_t q; mu_binary_read_uint32(body, &q);
    mu_nodeid_t ft; size_t fl; mu_binary_read_extension_object_header(body, &ft, &fl); /* filterResult */
    return st;
}

static mu_server_t *make_server(mock_t *mock, opcua_byte_t *storage, size_t storage_size, mu_server_config_t *config) {
    memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri="urn:t"; config->product_uri="urn:t"; config->application_name="t";
    static opcua_byte_t rx[8192], tx[8192];
    config->receive_buffer=rx; config->receive_buffer_size=sizeof(rx);
    config->send_buffer=tx; config->send_buffer_size=sizeof(tx);
    config->max_chunk_count=1; config->max_message_size=8192; config->max_sessions=1; config->max_secure_channels=1;
    fake_platform_init(NULL, &config->time_adapter, &config->entropy_adapter);
    config->tcp_adapter.context=mock;
    config->tcp_adapter.listen=mock_listen; config->tcp_adapter.accept=mock_accept;
    config->tcp_adapter.read=mock_read; config->tcp_adapter.write=mock_write;
    config->tcp_adapter.close_connection=mock_close; config->tcp_adapter.shutdown=mock_shutdown;
    config->address_space = NULL;
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, storage_size, config, &server));
    return server;
}

static void run_connect(mu_server_t *server) {
    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL */
    mu_server_poll(server); /* OPN */
    mu_server_poll(server); /* CreateSession */
    mu_server_poll(server); /* ActivateSession */
}

/* CreateSubscription returns a CreateSubscriptionResponse with a non-zero subscriptionId,
   a revised publishing interval within server bounds, and a revised lifetime count that
   is at least three times the revised keep-alive count (OPC 10000-4 §5.14.2.2). A second
   request gets a distinct id. */
void test_create_subscription(void) {
    mock_t mock; memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);
    enqueue_create_subscription(&mock, 5, 500.0, 60, 5);

    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES]; mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config);
    mu_binary_reader_t body; opcua_statuscode_t sr;

    run_connect(server);

    mu_server_poll(server); /* CreateSubscription #1 */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_uint32_t sub_id_1; mu_binary_read_uint32(&body, &sub_id_1);
    TEST_ASSERT_NOT_EQUAL(0, sub_id_1);
    opcua_double_t revised_interval; mu_binary_read_double(&body, &revised_interval);
    TEST_ASSERT_TRUE(revised_interval >= 50.0 && revised_interval <= 60000.0);
    opcua_uint32_t revised_lifetime, revised_keepalive;
    mu_binary_read_uint32(&body, &revised_lifetime);
    mu_binary_read_uint32(&body, &revised_keepalive);
    TEST_ASSERT_NOT_EQUAL(0, revised_keepalive);
    TEST_ASSERT_TRUE(revised_lifetime >= 3 * revised_keepalive);

    mu_server_poll(server); /* CreateSubscription #2 */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_uint32_t sub_id_2; mu_binary_read_uint32(&body, &sub_id_2);
    TEST_ASSERT_NOT_EQUAL(0, sub_id_2);
    TEST_ASSERT_NOT_EQUAL(sub_id_1, sub_id_2);
}

/* The (MU_MAX_SUBSCRIPTIONS default 2)+1-th CreateSubscription is rejected with the
   service result Bad_TooManySubscriptions (OPC 10000-4 §5.14.2.3). */
void test_create_subscription_too_many(void) {
    mock_t mock; memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);
    enqueue_create_subscription(&mock, 5, 1000.0, 100, 10);
    enqueue_create_subscription(&mock, 6, 1000.0, 100, 10); /* one past the cap */

    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES]; mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config);
    mu_binary_reader_t body; opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* #1 ok */
    mu_server_poll(server); /* #2 ok */
    mu_server_poll(server); /* #3 rejected */
    opcua_uint32_t type = parse_response(mock.last_write, mock.last_write_len, &body, &sr);
    TEST_ASSERT_EQUAL(ID_SERVICEFAULT, type);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_TOOMANYSUBSCRIPTIONS, sr);
}

/* DeleteSubscriptions removes a created subscription (per-op Good); deleting an unknown
   id yields per-op Bad_SubscriptionIdInvalid (OPC 10000-4 §5.14.8). */
void test_delete_subscriptions(void) {
    mock_t mock; memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES]; mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config);
    mu_binary_reader_t body; opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id; mu_binary_read_uint32(&body, &sub_id);

    /* DeleteSubscriptions (seq 5): subscriptionIds = [ sub_id, 0xDEAD ]. */
    opcua_byte_t tmp[512], chunk[512]; mu_binary_writer_t w; size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{ID_DELETESUBSCRIPTIONSREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w, 12345, 5);
    mu_binary_write_int32(&w, 2);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0xDEADu);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position); enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* DeleteSubscriptions */
    TEST_ASSERT_EQUAL(ID_DELETESUBSCRIPTIONSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t nres; mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(2, nres);
    opcua_statuscode_t r0, r1;
    mu_binary_read_statuscode(&body, &r0);
    mu_binary_read_statuscode(&body, &r1);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, r0);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_SUBSCRIPTIONIDINVALID, r1);
}

/* CreateMonitoredItems (OPC 10000-4 §5.13.2): a data MonitoredItem on a valid base node
   (ServerStatus.CurrentTime, i=2258, Value) succeeds with a non-zero monitoredItemId; an
   unknown NodeId yields per-op Bad_NodeIdUnknown (§5.13.2.4). */
void test_create_monitored_items(void) {
    mock_t mock; memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES]; mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config);
    mu_binary_reader_t body; opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id; mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): subscriptionId, timestampsToReturn=Neither, two items. */
    opcua_byte_t tmp[1024], chunk[1024]; mu_binary_writer_t w; size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{ID_CREATEMONITOREDITEMSREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w, 12345, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);          /* timestampsToReturn = Neither */
    mu_binary_write_int32(&w, 2);           /* itemsToCreate count */
    write_moncreate_item(&w, 0, 2258, 42, 500.0);   /* valid: ServerStatus.CurrentTime */
    write_moncreate_item(&w, 5, 9999, 43, 500.0);   /* unknown node */
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position); enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateMonitoredItems */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t nres; mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(2, nres);
    opcua_uint32_t id0, id1;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_moncreate_result(&body, &id0));
    TEST_ASSERT_NOT_EQUAL(0, id0);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_NODEIDUNKNOWN, read_moncreate_result(&body, &id1));
}

/* The (default MU_MAX_MONITORED_ITEMS = 8)+1-th item in one request is rejected per-op
   with Bad_TooManyMonitoredItems (OPC 10000-4 §5.13.2.4). */
void test_create_monitored_items_too_many(void) {
    mock_t mock; memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES]; mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config);
    mu_binary_reader_t body; opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id; mu_binary_read_uint32(&body, &sub_id);

    const opcua_int32_t n_items = 9; /* one past the default cap of 8 */
    opcua_byte_t tmp[1024], chunk[1024]; mu_binary_writer_t w; size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{ID_CREATEMONITOREDITEMSREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w, 12345, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, n_items);
    for (opcua_int32_t i = 0; i < n_items; ++i) write_moncreate_item(&w, 0, 2258, (opcua_uint32_t)i, 500.0);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position); enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateMonitoredItems */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t nres; mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(n_items, nres);
    opcua_uint32_t id;
    for (opcua_int32_t i = 0; i < n_items - 1; ++i)
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_moncreate_result(&body, &id));
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_TOOMANYMONITOREDITEMS, read_moncreate_result(&body, &id));
}

/* DeleteMonitoredItems (OPC 10000-4 §5.13.6): removing a created item is Good per-op; an
   unknown monitoredItemId yields per-op Bad_MonitoredItemIdInvalid. */
void test_delete_monitored_items(void) {
    mock_t mock; memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES]; mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config);
    mu_binary_reader_t body; opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id; mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): one valid item. */
    opcua_byte_t tmp[1024], chunk[1024]; mu_binary_writer_t w; size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{ID_CREATEMONITOREDITEMSREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w, 12345, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item(&w, 0, 2258, 42, 500.0);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position); enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateMonitoredItems */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t nres; mu_binary_read_int32(&body, &nres); TEST_ASSERT_EQUAL(1, nres);
    opcua_uint32_t item_id; read_moncreate_result(&body, &item_id);

    /* DeleteMonitoredItems (seq 6): subscriptionId + [ item_id, 0xBEEF ]. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    { mu_nodeid_t t={0,MU_NODEID_NUMERIC,{ID_DELETEMONITOREDITEMSREQUEST}}; mu_binary_write_nodeid(&w,&t); }
    write_request_header(&w, 12345, 6);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_int32(&w, 2);
    mu_binary_write_uint32(&w, item_id);
    mu_binary_write_uint32(&w, 0xBEEFu);
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position); enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* DeleteMonitoredItems */
    TEST_ASSERT_EQUAL(ID_DELETEMONITOREDITEMSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t ndel; mu_binary_read_int32(&body, &ndel);
    TEST_ASSERT_EQUAL(2, ndel);
    opcua_statuscode_t d0, d1;
    mu_binary_read_statuscode(&body, &d0);
    mu_binary_read_statuscode(&body, &d1);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, d0);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_MONITOREDITEMIDINVALID, d1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_subscription);
    RUN_TEST(test_create_subscription_too_many);
    RUN_TEST(test_delete_subscriptions);
    RUN_TEST(test_create_monitored_items);
    RUN_TEST(test_create_monitored_items_too_many);
    RUN_TEST(test_delete_monitored_items);
    return UNITY_END();
}
