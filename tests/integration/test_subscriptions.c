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
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <string.h>

/* IDs/status codes not yet defined in opcua_ids.h / status.h are written as literals
   here so the test compiles before the implementation lands. */
#define ID_CREATESUBSCRIPTIONREQUEST 787
#define ID_CREATESUBSCRIPTIONRESPONSE 790
#define ID_DELETESUBSCRIPTIONSREQUEST 847
#define ID_DELETESUBSCRIPTIONSRESPONSE 850
#define ID_SERVICEFAULT 397
#define ID_CREATEMONITOREDITEMSREQUEST 751
#define ID_CREATEMONITOREDITEMSRESPONSE 754
#define ID_DELETEMONITOREDITEMSREQUEST 781
#define ID_DELETEMONITOREDITEMSRESPONSE 784
#define STATUS_BAD_TOOMANYSUBSCRIPTIONS 0x80770000u
#define STATUS_BAD_SUBSCRIPTIONIDINVALID 0x80280000u
#define STATUS_BAD_NODEIDUNKNOWN 0x80340000u
#define STATUS_BAD_TOOMANYMONITOREDITEMS 0x80DB0000u
#define STATUS_BAD_MONITOREDITEMIDINVALID 0x80420000u
/* tests/support/fake_platform.c returns entropy byte 0x42. The first CreateSession
   authenticationToken is generated from that entropy by src/services/session.c. */
#define TEST_FIRST_AUTH_TOKEN 0xE7E7E7E7u

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

static void enqueue(mock_t *m, const opcua_byte_t *b, size_t len) {
    memcpy(m->inbound[m->inbound_count], b, len);
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

static void write_create_session_request_body_fields(mu_binary_writer_t *w) {
    mu_string_t ns = {-1, NULL};
    mu_bytestring_t nb = {-1, NULL};

    mu_binary_write_string(w, &ns);     /* ClientDescription.applicationUri */
    mu_binary_write_string(w, &ns);     /* productUri */
    mu_binary_write_byte(w, 0x00);      /* applicationName */
    mu_binary_write_uint32(w, 1);       /* applicationType = Client */
    mu_binary_write_string(w, &ns);     /* gatewayServerUri */
    mu_binary_write_string(w, &ns);     /* discoveryProfileUri */
    mu_binary_write_int32(w, 0);        /* discoveryUrls[] */
    mu_binary_write_string(w, &ns);     /* serverUri */
    mu_binary_write_string(w, &ns);     /* endpointUrl */
    mu_binary_write_string(w, &ns);     /* sessionName */
    mu_binary_write_bytestring(w, &nb); /* clientNonce */
    mu_binary_write_bytestring(w, &nb); /* clientCertificate */
    mu_binary_write_double(w, 0.0);     /* requestedSessionTimeout */
    mu_binary_write_uint32(w, 0);       /* maxResponseMessageSize */
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
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    memcpy(out + 24, body, blen);
    return 24 + blen;
}
/* Parse the response chunk: returns the response type NodeId, positions `body` at the
   service body, and outputs the ResponseHeader service result. */
static opcua_uint32_t parse_response(const opcua_byte_t *buf, size_t len, mu_binary_reader_t *body,
                                     opcua_statuscode_t *service_result) {
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
    if (service_result)
        *service_result = res;
    *body = r;
    return type.identifier.numeric;
}

/* Append HEL, OPN(None, seq 1), CreateSession(seq 2), ActivateSession(seq 3). */
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
        mu_string_t u = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&w, &u);
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

    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    write_create_session_request_body_fields(&w);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(mock, chunk, clen);

    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 3);
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

/* Enqueue a CreateSubscription MSG (seq == handle). */
static void enqueue_create_subscription(mock_t *mock, opcua_uint32_t seq, opcua_double_t interval,
                                        opcua_uint32_t lifetime, opcua_uint32_t keepalive) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATESUBSCRIPTIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_double(&w, interval);
    mu_binary_write_uint32(&w, lifetime);
    mu_binary_write_uint32(&w, keepalive);
    mu_binary_write_uint32(&w, 0);     /* maxNotificationsPerPublish */
    mu_binary_write_boolean(&w, true); /* publishingEnabled */
    mu_binary_write_byte(&w, 0);       /* priority */
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(mock, chunk, clen);
}

/* Write one MonitoredItemCreateRequest (Value attribute, Reporting mode, null filter). */
static void write_moncreate_item(mu_binary_writer_t *w, opcua_uint16_t ns, opcua_uint32_t ident,
                                 opcua_uint32_t client_handle, opcua_double_t interval) {
    mu_nodeid_t n = {ns, MU_NODEID_NUMERIC, {ident}};
    mu_binary_write_nodeid(w, &n); /* itemToMonitor.nodeId */
    mu_binary_write_uint32(w, 13); /* attributeId = Value */
    /* indexRange */
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint16(w, 0); /* dataEncoding.namespaceIndex */
    /* dataEncoding.name */
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint32(w, 2); /* monitoringMode = Reporting */
    mu_binary_write_uint32(w, client_handle);
    mu_binary_write_double(w, interval); /* samplingInterval */
    /* filter (null) */
    {
        mu_nodeid_t nul = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_extension_object_header(w, &nul, 0);
    }
    mu_binary_write_uint32(w, 1);     /* queueSize */
    mu_binary_write_boolean(w, true); /* discardOldest */
}

/* Read one MonitoredItemCreateResult: returns the statusCode and outputs the id. */
static opcua_statuscode_t read_moncreate_result(mu_binary_reader_t *body, opcua_uint32_t *id) {
    opcua_statuscode_t st;
    mu_binary_read_statuscode(body, &st);
    mu_binary_read_uint32(body, id);
    opcua_double_t revised;
    mu_binary_read_double(body, &revised);
    opcua_uint32_t q;
    mu_binary_read_uint32(body, &q);
    mu_nodeid_t ft;
    size_t fl;
    mu_binary_read_extension_object_header(body, &ft, &fl); /* filterResult */
    return st;
}

/* Controllable monotonic clock for the sampling/publish-timer tests. Reset to 0 by
   make_server so each test starts at tick 0; advance s_tick to fire timers. */
static opcua_uint64_t s_tick = 0;
static opcua_uint64_t test_get_tick_ms(void *c) {
    (void)c;
    return s_tick;
}

static mu_server_t *make_server(mock_t *mock, opcua_byte_t *storage, size_t storage_size, mu_server_config_t *config,
                                const mu_address_space_t *space) {
    memset(config, 0, sizeof(*config));
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
    s_tick = 0;
    config->time_adapter.get_tick_ms = test_get_tick_ms; /* controllable clock */
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
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);
    enqueue_create_subscription(&mock, 5, 500.0, 60, 5);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);

    mu_server_poll(server); /* CreateSubscription #1 */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_uint32_t sub_id_1;
    mu_binary_read_uint32(&body, &sub_id_1);
    TEST_ASSERT_NOT_EQUAL(0, sub_id_1);
    opcua_double_t revised_interval;
    mu_binary_read_double(&body, &revised_interval);
    TEST_ASSERT_TRUE(revised_interval >= 50.0 && revised_interval <= 60000.0);
    opcua_uint32_t revised_lifetime, revised_keepalive;
    mu_binary_read_uint32(&body, &revised_lifetime);
    mu_binary_read_uint32(&body, &revised_keepalive);
    TEST_ASSERT_NOT_EQUAL(0, revised_keepalive);
    TEST_ASSERT_TRUE(revised_lifetime >= 3 * revised_keepalive);

    mu_server_poll(server); /* CreateSubscription #2 */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_uint32_t sub_id_2;
    mu_binary_read_uint32(&body, &sub_id_2);
    TEST_ASSERT_NOT_EQUAL(0, sub_id_2);
    TEST_ASSERT_NOT_EQUAL(sub_id_1, sub_id_2);
}

/* The (MU_MAX_SUBSCRIPTIONS default 2)+1-th CreateSubscription is rejected with the
   service result Bad_TooManySubscriptions (OPC 10000-4 §5.14.2.3). */
void test_create_subscription_too_many(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);
    enqueue_create_subscription(&mock, 5, 1000.0, 100, 10);
    enqueue_create_subscription(&mock, 6, 1000.0, 100, 10); /* one past the cap */

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

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
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* DeleteSubscriptions (seq 5): subscriptionIds = [ sub_id, 0xDEAD ]. */
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_DELETESUBSCRIPTIONSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_int32(&w, 2);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0xDEADu);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* DeleteSubscriptions */
    TEST_ASSERT_EQUAL(ID_DELETESUBSCRIPTIONSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
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
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): subscriptionId, timestampsToReturn=Neither, two items. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);                /* timestampsToReturn = Neither */
    mu_binary_write_int32(&w, 2);                 /* itemsToCreate count */
    write_moncreate_item(&w, 0, 2258, 42, 500.0); /* valid: ServerStatus.CurrentTime */
    write_moncreate_item(&w, 5, 9999, 43, 500.0); /* unknown node */
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateMonitoredItems */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(2, nres);
    opcua_uint32_t id0, id1;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_moncreate_result(&body, &id0));
    TEST_ASSERT_NOT_EQUAL(0, id0);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_NODEIDUNKNOWN, read_moncreate_result(&body, &id1));
}

/* The (default MU_MAX_MONITORED_ITEMS = 8)+1-th item in one request is rejected per-op
   with Bad_TooManyMonitoredItems (OPC 10000-4 §5.13.2.4). */
void test_create_monitored_items_too_many(void) {
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD && MU_MAX_MONITORED_ITEMS > 32
    TEST_PASS_MESSAGE("Standard-facet raised capacity is covered by test_subscriptions_capacity");
    return;
#endif

    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    const opcua_int32_t n_items = 9; /* one past the default cap of 8 */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, n_items);
    for (opcua_int32_t i = 0; i < n_items; ++i)
        write_moncreate_item(&w, 0, 2258, (opcua_uint32_t)i, 500.0);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateMonitoredItems */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(n_items, nres);
    opcua_uint32_t id;
    for (opcua_int32_t i = 0; i < n_items - 1; ++i)
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_moncreate_result(&body, &id));
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_TOOMANYMONITOREDITEMS, read_moncreate_result(&body, &id));
}

/* DeleteMonitoredItems (OPC 10000-4 §5.13.6): removing a created item is Good per-op; an
   unknown monitoredItemId yields per-op Bad_MonitoredItemIdInvalid. */
void test_delete_monitored_items(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): one valid item. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item(&w, 0, 2258, 42, 500.0);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateMonitoredItems */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(1, nres);
    opcua_uint32_t item_id;
    read_moncreate_result(&body, &item_id);

    /* DeleteMonitoredItems (seq 6): subscriptionId + [ item_id, 0xBEEF ]. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_DELETEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 6);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_int32(&w, 2);
    mu_binary_write_uint32(&w, item_id);
    mu_binary_write_uint32(&w, 0xBEEFu);
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* DeleteMonitoredItems */
    TEST_ASSERT_EQUAL(ID_DELETEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t ndel;
    mu_binary_read_int32(&body, &ndel);
    TEST_ASSERT_EQUAL(2, ndel);
    opcua_statuscode_t d0, d1;
    mu_binary_read_statuscode(&body, &d0);
    mu_binary_read_statuscode(&body, &d1);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, d0);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_MONITOREDITEMIDINVALID, d1);
}

/* A controllable Int32 variable node (ns=1;i=5000) for change-detection testing. */
static opcua_int32_t s_mon_val = 0;
static opcua_statuscode_t mon_read_cb(void *ctx, const mu_nodeid_t *id, mu_variant_t *v) {
    (void)ctx;
    (void)id;
    memset(v, 0, sizeof(*v));
    v->type = MU_TYPE_INT32;
    v->value.i32 = s_mon_val;
    return MU_STATUS_GOOD;
}
static const mu_value_source_t mon_value = {MU_VALUESOURCE_CALLBACK, {.callback = {mon_read_cb, NULL}}};
static const mu_node_t samp_nodes[] = {{{1, MU_NODEID_NUMERIC, {5000}},
                                        MU_NODECLASS_VARIABLE,
                                        {7, (const opcua_byte_t *)"SampVar"},
                                        {7, (const opcua_byte_t *)"SampVar"},
                                        NULL,
                                        0,
                                        &mon_value}};
static const mu_address_space_t samp_space = {samp_nodes, 1};

/* Poll-driven sampling (OPC 10000-4 §5.12.1.6, §7.17.2): mu_subscriptions_tick samples a
   REPORTING MonitoredItem each sampling interval and flags a pending notification only when
   the value changes (StatusValue trigger). White-box check of the engine state via the
   controllable clock + value node. */
void test_sampling_detects_change(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    s_mon_val = 10;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): item on ns=1;i=5000, sampling 200 ms. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item(&w, 1, 5000, 7, 200.0);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server); /* CreateMonitoredItems — initial sample taken */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));

    mu_monitored_item_t *item = &server->subs.monitored_items[0];
    TEST_ASSERT_TRUE(item->in_use);
    TEST_ASSERT_TRUE(item->has_value);
    TEST_ASSERT_EQUAL_INT32(10, item->last_value.value.i32);

    /* Establish a known baseline, then a tick with no value change leaves pending clear. */
    item->pending = false;
    s_tick = 250;
    mu_server_poll(server);
    TEST_ASSERT_FALSE(item->pending);

    /* A value change at the next sampling tick sets pending and updates last_value. */
    s_mon_val = 20;
    s_tick = 450;
    mu_server_poll(server);
    TEST_ASSERT_TRUE(item->pending);
    TEST_ASSERT_EQUAL_INT32(20, item->last_value.value.i32);
}

#define ID_PUBLISHREQUEST 826
#define ID_PUBLISHRESPONSE 829
#define ID_DATACHANGENOTIFICATION 811 /* DataChangeNotification_Encoding_DefaultBinary */

/* Enqueue a PublishRequest with no SubscriptionAcknowledgements (seq == handle). */
static void enqueue_publish(mock_t *m, opcua_uint32_t seq) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_PUBLISHREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_int32(&w, 0); /* subscriptionAcknowledgements: empty */
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(m, chunk, clen);
}

/* Parse a PublishResponse body: outputs subscriptionId, notificationMessage.sequenceNumber,
   and the number of NotificationData entries. If there is a DataChangeNotification, outputs
   the first MonitoredItemNotification's clientHandle and DataValue. Returns notif-data count. */
static opcua_int32_t parse_publish_response(mu_binary_reader_t *body, opcua_uint32_t *sub_id, opcua_uint32_t *seq_num,
                                            opcua_uint32_t *client_handle, mu_datavalue_t *dv) {
    mu_binary_read_uint32(body, sub_id);
    opcua_int32_t navail;
    mu_binary_read_int32(body, &navail); /* availableSequenceNumbers */
    for (opcua_int32_t i = 0; i < navail; ++i) {
        opcua_uint32_t a;
        mu_binary_read_uint32(body, &a);
    }
    opcua_byte_t more;
    mu_binary_read_byte(body, &more);     /* moreNotifications */
    mu_binary_read_uint32(body, seq_num); /* notificationMessage.sequenceNumber */
    opcua_int64_t publish_time;
    mu_binary_read_int64(body, &publish_time);
    opcua_int32_t ndata;
    mu_binary_read_int32(body, &ndata); /* notificationData[] */
    if (ndata > 0) {
        mu_nodeid_t type;
        size_t len;
        mu_binary_read_extension_object_header(body, &type, &len);
        if (type.identifier.numeric == ID_DATACHANGENOTIFICATION) {
            opcua_int32_t nitems;
            mu_binary_read_int32(body, &nitems); /* monitoredItems[] */
            if (nitems > 0) {
                mu_binary_read_uint32(body, client_handle);
                mu_binary_read_datavalue(body, dv);
            }
        }
    }
    return ndata;
}

/* Advance the clock and poll a few times so a due publishing timer (and any sampling) fires. */
static void tick_to(mu_server_t *server, opcua_uint64_t when) {
    s_tick = when;
    mu_server_poll(server);
    mu_server_poll(server);
}

/* Publish (OPC 10000-4 §5.14.5): a parked PublishRequest is answered asynchronously by the
   publishing timer with a DataChangeNotification carrying the monitored item's clientHandle
   and new value. */
void test_publish_delivers_data_change(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0, 1000, 1000); /* large keep-alive: no keep-alives here */

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    s_mon_val = 10;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): item on ns=1;i=5000, sampling 100 ms, clientHandle 7. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item(&w, 1, 5000, 7, 100.0);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server); /* CreateMonitoredItems (initial sample = 10) */

    /* Publish #1 (seq 6): parked, then drained with the initial value (10) by the timer. */
    enqueue_publish(&mock, 6);
    mu_server_poll(server); /* parks the request (timer not yet due) */
    tick_to(server, 300);   /* publishing timer (200) fires */
    opcua_uint32_t sid, seqn, ch;
    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(1, parse_publish_response(&body, &sid, &seqn, &ch, &dv));
    TEST_ASSERT_EQUAL(sub_id, sid);
    TEST_ASSERT_EQUAL(7, ch);
    TEST_ASSERT_EQUAL_INT32(10, dv.value.value.i32);
    TEST_ASSERT_EQUAL(1, seqn);

    /* Publish #2 (seq 7): a value change is sampled and delivered (20), next sequence number. */
    s_mon_val = 20;
    enqueue_publish(&mock, 7);
    mu_server_poll(server); /* parks #2 */
    tick_to(server, 700);   /* sample picks up 20, publishing timer fires */
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(1, parse_publish_response(&body, &sid, &seqn, &ch, &dv));
    TEST_ASSERT_EQUAL(7, ch);
    TEST_ASSERT_EQUAL_INT32(20, dv.value.value.i32);
    TEST_ASSERT_EQUAL(2, seqn);
}

/* Keep-alive (OPC 10000-4 §5.14.1.1/§5.14.1.2): with no data changes, after
   max_keep_alive_count publishing intervals a parked PublishRequest is answered with an
   empty NotificationMessage (no notificationData). */
void test_publish_keep_alive(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0, 100, 3); /* keep-alive count 3 */

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    s_mon_val = 10;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* Park a Publish request (seq 5); no monitored items, so no data ever. */
    enqueue_publish(&mock, 5);
    mu_server_poll(server); /* parks it */

    /* Advance several publishing intervals with no data; a keep-alive must be emitted. */
    for (opcua_uint64_t t = 250; t <= 1450; t += 200) {
        s_tick = t;
        mu_server_poll(server);
    }

    opcua_uint32_t sid, seqn, ch = 0;
    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t ndata = parse_publish_response(&body, &sid, &seqn, &ch, &dv);
    TEST_ASSERT_EQUAL(sub_id, sid);
    TEST_ASSERT_EQUAL(0, ndata); /* keep-alive: empty NotificationMessage */
}

#define ID_REPUBLISHREQUEST 832
#define ID_REPUBLISHRESPONSE 835
#define STATUS_BAD_MESSAGENOTAVAILABLE 0x807B0000u

/* Enqueue a PublishRequest carrying one SubscriptionAcknowledgement {sub_id, ack_seq}. */
static void enqueue_publish_ack(mock_t *m, opcua_uint32_t seq, opcua_uint32_t sub_id, opcua_uint32_t ack_seq) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_PUBLISHREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_int32(&w, 1); /* one acknowledgement */
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, ack_seq);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(m, chunk, clen);
}

/* Enqueue a RepublishRequest {subscriptionId, retransmitSequenceNumber}. */
static void enqueue_republish(mock_t *m, opcua_uint32_t seq, opcua_uint32_t sub_id, opcua_uint32_t retransmit_seq) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_REPUBLISHREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, retransmit_seq);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(m, chunk, clen);
}

/* Publish acknowledgement results (OPC 10000-4 §5.14.5.4): an acknowledgement for a
   sequence number unknown to the Server is reported per-ack as Bad_SequenceNumberUnknown
   while the Publish service result remains Good. */
void test_publish_acknowledgement_unknown_sequence_returns_result(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0, 100, 3);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    enqueue_publish_ack(&mock, 5, sub_id, 0xDEADBEEFu);
    mu_server_poll(server); /* parks the Publish request */

    for (opcua_uint64_t t = 250; t <= 1450; t += 200) {
        s_tick = t;
        mu_server_poll(server);
    }

    opcua_uint32_t sid, seqn, ch = 0;
    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    TEST_ASSERT_EQUAL(0, parse_publish_response(&body, &sid, &seqn, &ch, &dv));
    TEST_ASSERT_EQUAL(sub_id, sid);

    opcua_int32_t ack_results_count;
    opcua_statuscode_t ack_result;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&body, &ack_results_count));
    TEST_ASSERT_EQUAL(1, ack_results_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&body, &ack_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SEQUENCENUMBERUNKNOWN, ack_result);
}

/* Republish (OPC 10000-4 §5.14.6): a retained NotificationMessage can be re-sent; once
   acknowledged it is purged and Republish returns Bad_MessageNotAvailable. */
void test_republish_and_acknowledge(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0, 1000, 1000);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    s_mon_val = 10;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5) on ns=1;i=5000, clientHandle 7. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item(&w, 1, 5000, 7, 100.0);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server); /* CreateMonitoredItems */

    /* Publish #1 (seq 6): deliver the initial value (seq 1) so a message is retained. */
    enqueue_publish(&mock, 6);
    mu_server_poll(server);
    tick_to(server, 300);
    opcua_uint32_t sid, seqn, ch;
    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(1, parse_publish_response(&body, &sid, &seqn, &ch, &dv));
    TEST_ASSERT_EQUAL(1, seqn);

    /* Republish (seq 7) of the retained sequence 1 re-sends the NotificationMessage. */
    enqueue_republish(&mock, 7, sub_id, 1);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_REPUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    /* RepublishResponse body is a NotificationMessage: sequenceNumber, publishTime, data[]. */
    opcua_uint32_t rseq;
    mu_binary_read_uint32(&body, &rseq);
    TEST_ASSERT_EQUAL(1, rseq);
    opcua_int64_t pt;
    mu_binary_read_int64(&body, &pt);
    opcua_int32_t ndata;
    mu_binary_read_int32(&body, &ndata);
    TEST_ASSERT_EQUAL(1, ndata);

    /* Republish (seq 8) of an unknown sequence -> Bad_MessageNotAvailable. */
    enqueue_republish(&mock, 8, sub_id, 5000);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SERVICEFAULT, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_MESSAGENOTAVAILABLE, sr);

    /* Publish #2 (seq 9) acknowledges sequence 1 -> the retained message is purged. */
    enqueue_publish_ack(&mock, 9, sub_id, 1);
    mu_server_poll(server);

    /* Republish (seq 10) of the now-purged sequence 1 -> Bad_MessageNotAvailable. */
    enqueue_republish(&mock, 10, sub_id, 1);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SERVICEFAULT, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_MESSAGENOTAVAILABLE, sr);
}

#define ID_MODIFYSUBSCRIPTIONREQUEST 793
#define ID_MODIFYSUBSCRIPTIONRESPONSE 796
#define ID_SETPUBLISHINGMODEREQUEST 799
#define ID_SETPUBLISHINGMODERESPONSE 802
#define ID_MODIFYMONITOREDITEMSREQUEST 763
#define ID_MODIFYMONITOREDITEMSRESPONSE 766
#define ID_SETMONITORINGMODEREQUEST 767
#define ID_SETMONITORINGMODERESPONSE 770

/* Create a subscription + one monitored item on samp_space ns=1;i=5000, returning ids. */
static mu_server_t *setup_sub_with_item(mock_t *mock, opcua_byte_t *storage, size_t ssize, mu_server_config_t *config,
                                        opcua_double_t interval, opcua_uint32_t lifetime, opcua_uint32_t keepalive,
                                        opcua_double_t sampling, opcua_uint32_t *sub_id_out,
                                        opcua_uint32_t *item_id_out) {
    enqueue_connect(mock);
    enqueue_create_subscription(mock, 4, interval, lifetime, keepalive);
    mu_server_t *server = make_server(mock, storage, ssize, config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE,
                      parse_response(mock->last_write, mock->last_write_len, &body, &sr));
    mu_binary_read_uint32(&body, sub_id_out);

    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, *sub_id_out);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item(&w, 1, 5000, 7, sampling);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock->last_write, mock->last_write_len, &body, &sr));
    opcua_int32_t n;
    mu_binary_read_int32(&body, &n);
    read_moncreate_result(&body, item_id_out);
    return server;
}

/* Republish (OPC 10000-4 §5.14.6.3): requesting a NotificationMessage sequence number
   that is not available returns Bad_MessageNotAvailable. */
void test_republish_unknown_sequence_returns_bad_message_not_available(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    opcua_uint32_t sub_id;
    opcua_uint32_t item_id;
    s_mon_val = 10;
    mu_server_t *server =
        setup_sub_with_item(&mock, storage, sizeof(storage), &config, 200.0, 1000, 1000, 100.0, &sub_id, &item_id);
    (void)item_id;

    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    enqueue_publish(&mock, 6);
    mu_server_poll(server);
    tick_to(server, 300);

    opcua_uint32_t sid, seqn, ch;
    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(1, parse_publish_response(&body, &sid, &seqn, &ch, &dv));
    TEST_ASSERT_EQUAL(sub_id, sid);
    TEST_ASSERT_EQUAL(1, seqn);

    enqueue_republish(&mock, 7, sub_id, 5000);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SERVICEFAULT, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_MESSAGENOTAVAILABLE, sr);
}

/* ModifySubscription (OPC 10000-4 §5.14.3): revised publishing interval reflects the new
   requested value. */
void test_modify_subscription(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 100, 10);
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_MODIFYSUBSCRIPTIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_double(&w, 333.0); /* requestedPublishingInterval */
    mu_binary_write_uint32(&w, 100);   /* requestedLifetimeCount */
    mu_binary_write_uint32(&w, 10);    /* requestedMaxKeepAliveCount */
    mu_binary_write_uint32(&w, 0);     /* maxNotificationsPerPublish */
    mu_binary_write_byte(&w, 0);       /* priority */
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_MODIFYSUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_double_t revised;
    mu_binary_read_double(&body, &revised);
    TEST_ASSERT_TRUE(revised >= 332.0 && revised <= 334.0);
}

/* SetPublishingMode (OPC 10000-4 §5.14.4): disabling publishing suppresses data
   notifications — only keep-alives are sent even when a value changes. */
void test_set_publishing_mode(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    opcua_uint32_t sub_id, item_id;
    s_mon_val = 10;
    mu_server_t *server =
        setup_sub_with_item(&mock, storage, sizeof(storage), &config, 200.0, 100, 2, 100.0, &sub_id, &item_id);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    /* SetPublishingMode(false, [sub_id]) (seq 6). */
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_SETPUBLISHINGMODEREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 6);
    mu_binary_write_boolean(&w, false);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, sub_id);
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SETPUBLISHINGMODERESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(1, nres);
    opcua_statuscode_t r0;
    mu_binary_read_statuscode(&body, &r0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, r0);

    /* A value change with publishing disabled yields a keep-alive (no data). */
    s_mon_val = 20;
    enqueue_publish(&mock, 7);
    mu_server_poll(server);
    for (opcua_uint64_t t = 250; t <= 1050; t += 200) {
        s_tick = t;
        mu_server_poll(server);
    }
    opcua_uint32_t sid, seqn, ch = 0;
    mu_datavalue_t dv;
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(0, parse_publish_response(&body, &sid, &seqn, &ch, &dv)); /* keep-alive, no data */
}

/* ModifyMonitoredItems (OPC 10000-4 §5.13.3): the revised sampling interval reflects the
   new requested value. */
void test_modify_monitored_items(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    opcua_uint32_t sub_id, item_id;
    s_mon_val = 10;
    mu_server_t *server =
        setup_sub_with_item(&mock, storage, sizeof(storage), &config, 1000.0, 100, 10, 500.0, &sub_id, &item_id);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_MODIFYMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 6);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);       /* timestampsToReturn */
    mu_binary_write_int32(&w, 1);        /* itemsToModify */
    mu_binary_write_uint32(&w, item_id); /* monitoredItemId */
    mu_binary_write_uint32(&w, 7);       /* clientHandle */
    mu_binary_write_double(&w, 250.0);   /* samplingInterval */
    {
        mu_nodeid_t nul = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_extension_object_header(&w, &nul, 0);
    }
    mu_binary_write_uint32(&w, 1);     /* queueSize */
    mu_binary_write_boolean(&w, true); /* discardOldest */
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position);
    enqueue(&mock, chunk, clen);

    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_MODIFYMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(1, nres);
    opcua_statuscode_t st;
    mu_binary_read_statuscode(&body, &st);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, st);
    opcua_double_t revised;
    mu_binary_read_double(&body, &revised);
    TEST_ASSERT_TRUE(revised >= 249.0 && revised <= 251.0);
}

/* SetMonitoringMode (OPC 10000-4 §5.13.4): DISABLED stops change detection; REPORTING
   resumes it. */
void test_set_monitoring_mode(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    opcua_uint32_t sub_id, item_id;
    s_mon_val = 10;
    mu_server_t *server =
        setup_sub_with_item(&mock, storage, sizeof(storage), &config, 1000.0, 100, 10, 100.0, &sub_id, &item_id);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    mu_monitored_item_t *item = &server->subs.monitored_items[0];

    /* SetMonitoringMode(Disabled=0, [item]) (seq 6). */
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_SETMONITORINGMODEREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 6);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 0); /* monitoringMode = Disabled */
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, item_id);
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SETMONITORINGMODERESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(1, nres);
    opcua_statuscode_t r0;
    mu_binary_read_statuscode(&body, &r0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, r0);

    /* Disabled: a value change is NOT detected. */
    item->pending = false;
    s_mon_val = 20;
    s_tick = 300;
    mu_server_poll(server);
    TEST_ASSERT_FALSE(item->pending);

    /* SetMonitoringMode(Reporting=2) (seq 7) resumes detection. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_SETMONITORINGMODEREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 7);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 2); /* Reporting */
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, item_id);
    clen = build_msg(chunk, sizeof(chunk), 7, 7, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SETMONITORINGMODERESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));

    s_mon_val = 30;
    s_tick = 600;
    mu_server_poll(server);
    TEST_ASSERT_TRUE(item->pending);
    TEST_ASSERT_EQUAL_INT32(30, item->last_value.value.i32);
}

#define ID_READREQUEST 631
#define ID_READRESPONSE 634
#define STATUS_BAD_TOOMANYSESSIONS 0x80560000u

/* Enqueue a complete CreateSession request body. */
static void enqueue_create_session(mock_t *m, opcua_uint32_t seq) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, seq);
    write_create_session_request_body_fields(&w);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(m, chunk, clen);
}

/* Enqueue an ActivateSession request authenticated by the given token. */
static void enqueue_activate_session(mock_t *m, opcua_uint32_t seq, opcua_uint32_t token) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, token, seq);
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
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(m, chunk, clen);
}

/* Enqueue a Read of ServerStatus.State (i=2259) Value, authenticated by token. */
static void enqueue_read_state(mock_t *m, opcua_uint32_t seq, opcua_uint32_t token) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_READREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, token, seq);
    mu_binary_write_double(&w, 0.0); /* maxAge */
    mu_binary_write_uint32(&w, 0);   /* timestampsToReturn = Source */
    mu_binary_write_int32(&w, 1);    /* nodesToRead count */
    {
        mu_nodeid_t n = {0, MU_NODEID_NUMERIC, {2259}};
        mu_binary_write_nodeid(&w, &n);
    }
    mu_binary_write_uint32(&w, 13); /* attributeId = Value */
    /* indexRange */
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(&w, &nul);
    }
    mu_binary_write_uint16(&w, 0); /* dataEncoding.namespaceIndex */
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(&w, &nul);
    } /* dataEncoding.name */
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(m, chunk, clen);
}

/* Parse a CreateSessionResponse far enough to read sessionId + authenticationToken. */
static opcua_uint32_t parse_create_session_token(mu_binary_reader_t *body, opcua_uint32_t *session_id) {
    mu_nodeid_t sid, tok;
    mu_binary_read_nodeid(body, &sid); /* sessionId */
    mu_binary_read_nodeid(body, &tok); /* authenticationToken */
    if (session_id)
        *session_id = sid.identifier.numeric;
    return tok.identifier.numeric;
}

/* Only HEL + OpenSecureChannel (seq 1), leaving sessions to the test. */
static void enqueue_hel_opn(mock_t *mock) {
    opcua_byte_t tmp[512], chunk[512];
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
        mu_string_t u = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&w, &u);
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

/* Two concurrent sessions on one secure channel (OPC 10000-4 §5.6.2). Each
   CreateSession yields a distinct authenticationToken; both can be activated and used;
   the (MU_MAX_SESSIONS default 2)+1-th CreateSession returns Bad_TooManySessions. */
void test_two_sessions(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_hel_opn(&mock);
    enqueue_create_session(&mock, 2);
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL */
    mu_server_poll(server); /* OPN */
    mu_server_poll(server); /* CreateSession #1 */
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sid1, tok1 = parse_create_session_token(&body, &sid1);
    TEST_ASSERT_NOT_EQUAL(0, tok1);

    enqueue_activate_session(&mock, 3, tok1);
    enqueue_create_session(&mock, 4);
    mu_server_poll(server); /* ActivateSession #1 */
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    mu_server_poll(server); /* CreateSession #2 */
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sid2, tok2 = parse_create_session_token(&body, &sid2);
    TEST_ASSERT_NOT_EQUAL(0, tok2);
    TEST_ASSERT_NOT_EQUAL(tok1, tok2); /* distinct tokens */

    enqueue_activate_session(&mock, 5, tok2);
    enqueue_read_state(&mock, 6, tok1);
    enqueue_read_state(&mock, 7, tok2);
    enqueue_create_session(&mock, 8);
    mu_server_poll(server); /* ActivateSession #2 */
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    mu_server_poll(server); /* Read via session 1 */
    TEST_ASSERT_EQUAL(ID_READRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    mu_server_poll(server); /* Read via session 2 */
    TEST_ASSERT_EQUAL(ID_READRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    mu_server_poll(server); /* CreateSession #3 -> Bad_TooManySessions */
    TEST_ASSERT_EQUAL(ID_SERVICEFAULT, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_TOOMANYSESSIONS, sr);
}

/* A subscription created on session 1 is not operable from session 2 (OPC 10000-4
   §5.14.1.3: subscriptions are owned by the session that created them). */
void test_subscription_session_isolation(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_hel_opn(&mock);
    enqueue_create_session(&mock, 2);
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, NULL);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    mu_server_poll(server);
    mu_server_poll(server);
    mu_server_poll(server); /* accept,HEL,OPN */
    mu_server_poll(server); /* CreateSession #1 */
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t tok1 = parse_create_session_token(&body, NULL);
    enqueue_activate_session(&mock, 3, tok1);
    enqueue_create_session(&mock, 4);
    mu_server_poll(server); /* ActivateSession #1 */
    mu_server_poll(server); /* CreateSession #2 */
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t tok2 = parse_create_session_token(&body, NULL);
    enqueue_activate_session(&mock, 5, tok2);
    mu_server_poll(server); /* ActivateSession #2 */

    /* CreateSubscription on session 1 (seq 6, token 1). */
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATESUBSCRIPTIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, tok1, 6);
    mu_binary_write_double(&w, 1000.0);
    mu_binary_write_uint32(&w, 100);
    mu_binary_write_uint32(&w, 10);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0);
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* DeleteSubscriptions from session 2 (seq 7, token 2): per-op Bad_SubscriptionIdInvalid. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_DELETESUBSCRIPTIONSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, tok2, 7);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, sub_id);
    clen = build_msg(chunk, sizeof(chunk), 7, 7, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_DELETESUBSCRIPTIONSRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t nres;
    mu_binary_read_int32(&body, &nres);
    TEST_ASSERT_EQUAL(1, nres);
    opcua_statuscode_t r0;
    mu_binary_read_statuscode(&body, &r0);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_SUBSCRIPTIONIDINVALID, r0);
}

/* ===================================================================================
   US1 — Standard DataChange Subscription 2017 Server Facet delta (feature 005).
   RED tests authored by Claude; Codex implements the engine + dispatch.
   =================================================================================== */

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD

#define ID_DATACHANGEFILTER_ENC_BINARY 724 /* DataChangeFilter_Encoding_DefaultBinary (i=724) */

/* Write one MonitoredItemCreateRequest carrying a DataChangeFilter (OPC 10000-4 §7.22.2):
   trigger + deadbandType + deadbandValue, with a configurable queueSize/discardOldest. */
static void write_moncreate_item_filter(mu_binary_writer_t *w, opcua_uint16_t ns, opcua_uint32_t ident,
                                        opcua_uint32_t client_handle, opcua_double_t interval, opcua_uint32_t trigger,
                                        opcua_uint32_t deadband_type, opcua_double_t deadband_value,
                                        opcua_uint32_t queue_size, bool discard_oldest) {
    mu_nodeid_t n = {ns, MU_NODEID_NUMERIC, {ident}};
    mu_binary_write_nodeid(w, &n); /* itemToMonitor.nodeId */
    mu_binary_write_uint32(w, 13); /* attributeId = Value */
    /* indexRange */
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint16(w, 0); /* dataEncoding.namespaceIndex */
    /* dataEncoding.name */
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint32(w, 2); /* monitoringMode = Reporting */
    mu_binary_write_uint32(w, client_handle);
    mu_binary_write_double(w, interval); /* samplingInterval */
    /* filter: DataChangeFilter ExtensionObject, 16-byte body (§7.22.2). */
    {
        mu_nodeid_t ft = {0, MU_NODEID_NUMERIC, {ID_DATACHANGEFILTER_ENC_BINARY}};
        mu_binary_write_extension_object_header(w, &ft, 16);
    }
    mu_binary_write_uint32(w, trigger);       /* DataChangeTrigger */
    mu_binary_write_uint32(w, deadband_type); /* DeadbandType: 0 None,1 Absolute,2 Percent */
    mu_binary_write_double(w, deadband_value);
    mu_binary_write_uint32(w, queue_size);      /* queueSize */
    mu_binary_write_boolean(w, discard_oldest); /* discardOldest */
}

/* US1 — Absolute Deadband DataChangeFilter (OPC 10000-4 §7.22.2, DeadbandType=Absolute);
   `Monitored Items Deadband Filter` CU. A numeric value change smaller than the deadband is
   NOT reported; a change at or above it is (compared against the last *reported* value).
   RED until Codex implements deadband eval (T013) + filter decode (T017). */
void test_monitored_item_absolute_deadband(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0, 100, 3); /* keepalive 3 → keep-alives fire */

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    s_mon_val = 10;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): item with absolute deadband 5.0, sampling 100 ms. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3); /* timestampsToReturn */
    mu_binary_write_int32(&w, 1);  /* itemsToCreate count */
    write_moncreate_item_filter(&w, 1, 5000, 7, 100.0, 1 /*StatusValue*/, 1 /*Absolute*/, 5.0, 1, true);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server); /* CreateMonitoredItems (initial sample = 10) */
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t ncreate;
    mu_binary_read_int32(&body, &ncreate);
    TEST_ASSERT_EQUAL(1, ncreate);
    opcua_uint32_t item_id;
    opcua_statuscode_t cstat = read_moncreate_result(&body, &item_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, cstat);

    opcua_uint32_t sid, seqn, ch;
    mu_datavalue_t dv;

    /* Publish #1 (seq 6): initial value 10 delivered. */
    enqueue_publish(&mock, 6);
    mu_server_poll(server);
    tick_to(server, 300);
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(1, parse_publish_response(&body, &sid, &seqn, &ch, &dv));
    TEST_ASSERT_EQUAL_INT32(10, dv.value.value.i32);

    /* Sub-threshold change to 12 (|12-10| = 2 < 5): suppressed. Park a Publish and advance
       past maxKeepAliveCount intervals → a keep-alive (0 data) proves suppression. */
    s_mon_val = 12;
    enqueue_publish(&mock, 7);
    mu_server_poll(server);
    for (opcua_uint64_t t = 500; t <= 1700; t += 200) {
        s_tick = t;
        mu_server_poll(server);
    }
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(0, parse_publish_response(&body, &sid, &seqn, &ch, &dv)); /* suppressed */

    /* At/above threshold: 20 (|20-10| = 10 >= 5): reported. */
    s_mon_val = 20;
    enqueue_publish(&mock, 8);
    mu_server_poll(server);
    tick_to(server, 2100);
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(1, parse_publish_response(&body, &sid, &seqn, &ch, &dv));
    TEST_ASSERT_EQUAL_INT32(20, dv.value.value.i32);
}

/* Overflow InfoBit: InfoType=DataValue (0x0400) | Overflow (0x0080) per OPC-10000-4 §7.20.1
   and the OPC-10000-6 StatusCode encoding. Codex must set exactly this on the oldest retained
   value when the queue overflows. */
#define STATUS_INFO_OVERFLOW 0x00000480u

/* Parse a PublishResponse DataChangeNotification into arrays of clientHandle + DataValue;
   returns the number of MonitoredItemNotifications delivered. */
static opcua_int32_t parse_publish_notifications(mu_binary_reader_t *body, opcua_uint32_t *sub_id,
                                                 opcua_uint32_t *handles, mu_datavalue_t *dvs, opcua_int32_t maxn) {
    mu_binary_read_uint32(body, sub_id);
    opcua_int32_t navail;
    mu_binary_read_int32(body, &navail);
    for (opcua_int32_t i = 0; i < navail; ++i) {
        opcua_uint32_t a;
        mu_binary_read_uint32(body, &a);
    }
    opcua_byte_t more;
    mu_binary_read_byte(body, &more);
    opcua_uint32_t seqn;
    mu_binary_read_uint32(body, &seqn);
    opcua_int64_t pt;
    mu_binary_read_int64(body, &pt);
    opcua_int32_t ndata;
    mu_binary_read_int32(body, &ndata);
    opcua_int32_t total = 0;
    for (opcua_int32_t d = 0; d < ndata; ++d) {
        mu_nodeid_t type;
        size_t len;
        mu_binary_read_extension_object_header(body, &type, &len);
        if (type.identifier.numeric == ID_DATACHANGENOTIFICATION) {
            opcua_int32_t nitems;
            mu_binary_read_int32(body, &nitems);
            for (opcua_int32_t i = 0; i < nitems; ++i) {
                opcua_uint32_t ch;
                mu_datavalue_t dv;
                mu_binary_read_uint32(body, &ch);
                mu_binary_read_datavalue(body, &dv);
                if (total < maxn) {
                    handles[total] = ch;
                    dvs[total] = dv;
                }
                total++;
            }
            opcua_int32_t ndiag;
            mu_binary_read_int32(body, &ndiag); /* diagnosticInfos[] */
        }
    }
    return total;
}

#if MU_MONITORED_QUEUE_DEPTH >= 2
/* US1 — MonitoredItem notification queue (OPC 10000-4 §5.13.2 queueSize/discardOldest,
   §7.20.1 overflow). queueSize=2, discardOldest=true: three rapid changes (20,30,40) between
   publishes drop the oldest (20) and deliver the two most-recent (30,40); the oldest retained
   value (30) carries the Overflow InfoBit. RED until Codex implements the queue (T014). */
void test_monitored_item_queue_overflow(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 1000.0, 1000, 1000); /* publishing 1000 ms */

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    s_mon_val = 10;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): queueSize=2, discardOldest=true, no deadband, sampling 100 ms. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item_filter(&w, 1, 5000, 7, 100.0, 1 /*StatusValue*/, 0 /*None*/, 0.0, 2, true);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t ncreate;
    mu_binary_read_int32(&body, &ncreate);
    TEST_ASSERT_EQUAL(1, ncreate);
    opcua_uint32_t item_id;
    opcua_statuscode_t cstat = read_moncreate_result(&body, &item_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, cstat);

    opcua_uint32_t sid;
    opcua_uint32_t hs[8];
    mu_datavalue_t dvs[8];

    /* Publish #1 (seq 6) at t=1000: drains the initial value (10). */
    enqueue_publish(&mock, 6);
    mu_server_poll(server);
    tick_to(server, 1000);
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL(1, parse_publish_notifications(&body, &sid, hs, dvs, 8));
    TEST_ASSERT_EQUAL_INT32(10, dvs[0].value.value.i32);

    /* Three changes sampled before the next publish timer (2000): 20,30,40. queue cap 2 +
       discardOldest drops 20. */
    s_mon_val = 20;
    s_tick = 1100;
    mu_server_poll(server);
    s_mon_val = 30;
    s_tick = 1200;
    mu_server_poll(server);
    s_mon_val = 40;
    s_tick = 1300;
    mu_server_poll(server);

    /* Publish #2 (seq 7) at t=2000: delivers 30 (Overflow InfoBit) then 40. */
    enqueue_publish(&mock, 7);
    mu_server_poll(server);
    tick_to(server, 2000);
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t n = parse_publish_notifications(&body, &sid, hs, dvs, 8);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL_INT32(30, dvs[0].value.value.i32);
    TEST_ASSERT_EQUAL_HEX32(STATUS_INFO_OVERFLOW, dvs[0].status & STATUS_INFO_OVERFLOW);
    TEST_ASSERT_EQUAL_INT32(40, dvs[1].value.value.i32);
}
#endif /* MU_MONITORED_QUEUE_DEPTH >= 2 */

#define ID_SETTRIGGERINGREQUEST 773  /* SetTriggeringRequest (i=773), OPC 10000-4 §5.13.5 */
#define ID_SETTRIGGERINGRESPONSE 776 /* SetTriggeringResponse (i=776) */

/* Write one MonitoredItemCreateRequest with an explicit monitoringMode and null filter. */
static void write_moncreate_item_mode(mu_binary_writer_t *w, opcua_uint16_t ns, opcua_uint32_t ident,
                                      opcua_uint32_t client_handle, opcua_double_t interval, opcua_uint32_t mode) {
    mu_nodeid_t n = {ns, MU_NODEID_NUMERIC, {ident}};
    mu_binary_write_nodeid(w, &n);
    mu_binary_write_uint32(w, 13); /* attributeId = Value */
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint16(w, 0);
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint32(w, mode); /* monitoringMode: 1 Sampling, 2 Reporting */
    mu_binary_write_uint32(w, client_handle);
    mu_binary_write_double(w, interval);
    {
        mu_nodeid_t nul = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_extension_object_header(w, &nul, 0);
    }
    mu_binary_write_uint32(w, 1);     /* queueSize */
    mu_binary_write_boolean(w, true); /* discardOldest */
}

/* US1 — SetTriggering (OPC 10000-4 §5.13.5, §5.13.1.6 triggering model). A triggered item in
   Sampling mode does not report on its own; once linked to a triggering item, it reports its
   sampled value when the triggering item reports. RED until Codex implements SetTriggering
   dispatch (T018) + the triggering engine logic (T015). */
void test_set_triggering(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0, 1000, 1000);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    s_mon_val = 10;
    mu_server_t *server = make_server(&mock, storage, sizeof(storage), &config, &samp_space);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* CreateMonitoredItems (seq 5): item A reporting (handle 7), item B sampling (handle 8),
       both on ns=1;i=5000, sampling 100 ms. */
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 5);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 2); /* two items */
    write_moncreate_item_mode(&w, 1, 5000, 7, 100.0, 2 /*Reporting*/);
    write_moncreate_item_mode(&w, 1, 5000, 8, 100.0, 1 /*Sampling*/);
    clen = build_msg(chunk, sizeof(chunk), 5, 5, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t ncreate;
    mu_binary_read_int32(&body, &ncreate);
    TEST_ASSERT_EQUAL(2, ncreate);
    opcua_uint32_t a_id, b_id;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_moncreate_result(&body, &a_id));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_moncreate_result(&body, &b_id));

    /* Publish #1 (seq 6) at t=300, BEFORE linking: item A (Reporting) reports its initial
       value; B (Sampling, unlinked) does not report on its own. */
    opcua_uint32_t sid;
    opcua_uint32_t hs[8];
    mu_datavalue_t dvs[8];
    enqueue_publish(&mock, 6);
    mu_server_poll(server);
    tick_to(server, 300);
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t n1 = parse_publish_notifications(&body, &sid, hs, dvs, 8);
    TEST_ASSERT_EQUAL(1, n1); /* only A reports; B is Sampling and unlinked */
    TEST_ASSERT_EQUAL(7, hs[0]);

    /* SetTriggering (seq 7): triggeringItem = A, linksToAdd = [B], linksToRemove = []. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_SETTRIGGERINGREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 7);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, a_id); /* triggeringItemId */
    mu_binary_write_int32(&w, 1);     /* linksToAdd count */
    mu_binary_write_uint32(&w, b_id);
    mu_binary_write_int32(&w, 0); /* linksToRemove count */
    clen = build_msg(chunk, sizeof(chunk), 7, 7, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SETTRIGGERINGRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_int32_t nadd;
    mu_binary_read_int32(&body, &nadd);
    TEST_ASSERT_EQUAL(1, nadd);
    opcua_statuscode_t add0;
    mu_binary_read_statuscode(&body, &add0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, add0);

    /* Change to 20: A reports the change; the trigger link makes B report its sample too. */
    s_mon_val = 20;
    enqueue_publish(&mock, 8);
    mu_server_poll(server);
    tick_to(server, 700);
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t n2 = parse_publish_notifications(&body, &sid, hs, dvs, 8);
    TEST_ASSERT_EQUAL(2, n2); /* A and triggered B both report */
    /* Both notifications carry value 20 (handles 7 and 8 in some order). */
    TEST_ASSERT_TRUE((hs[0] == 7 && hs[1] == 8) || (hs[0] == 8 && hs[1] == 7));
    TEST_ASSERT_EQUAL_INT32(20, dvs[0].value.value.i32);
    TEST_ASSERT_EQUAL_INT32(20, dvs[1].value.value.i32);
}

/* US1 — Standard-facet error paths (OPC 10000-4 §7.22.2, §5.13.5): percent deadband is
   rejected (Data Access Facet, not this profile); SetTriggering with an unknown link id
   yields a per-link Bad_MonitoredItemIdInvalid; an empty SetTriggering is Bad_NothingToDo. */
void test_standard_facet_errors(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    opcua_uint32_t sub_id, item_id;
    s_mon_val = 10;
    mu_server_t *server =
        setup_sub_with_item(&mock, storage, sizeof(storage), &config, 200.0, 1000, 1000, 100.0, &sub_id, &item_id);
    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;

    /* (1) CreateMonitoredItems with Percent deadband (seq 6) -> per-item
       Bad_MonitoredItemFilterUnsupported (0x80440000). */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 6);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_moncreate_item_filter(&w, 1, 5000, 9, 100.0, 1 /*StatusValue*/, 2 /*Percent*/, 10.0, 1, true);
    clen = build_msg(chunk, sizeof(chunk), 6, 6, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t nc;
    mu_binary_read_int32(&body, &nc);
    TEST_ASSERT_EQUAL(1, nc);
    opcua_uint32_t dummy;
    opcua_statuscode_t pst = read_moncreate_result(&body, &dummy);
    TEST_ASSERT_EQUAL_HEX32(0x80440000u, pst);

    /* (2) SetTriggering with a valid triggering item but unknown link id (seq 7) → per-link
       Bad_MonitoredItemIdInvalid. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_SETTRIGGERINGREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 7);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, item_id); /* valid triggering item */
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, 0xDEADu); /* unknown link */
    mu_binary_write_int32(&w, 0);
    clen = build_msg(chunk, sizeof(chunk), 7, 7, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SETTRIGGERINGRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_int32_t na;
    mu_binary_read_int32(&body, &na);
    TEST_ASSERT_EQUAL(1, na);
    opcua_statuscode_t a0;
    mu_binary_read_statuscode(&body, &a0);
    TEST_ASSERT_EQUAL_HEX32(STATUS_BAD_MONITOREDITEMIDINVALID, a0);

    /* (3) SetTriggering with empty add+remove (seq 8) → service fault Bad_NothingToDo. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_SETTRIGGERINGREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FIRST_AUTH_TOKEN, 8);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, item_id);
    mu_binary_write_int32(&w, 0);
    mu_binary_write_int32(&w, 0);
    clen = build_msg(chunk, sizeof(chunk), 8, 8, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_SERVICEFAULT, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(0x800F0000u, sr); /* Bad_NothingToDo */
}

#endif /* MICRO_OPCUA_SUBSCRIPTIONS_STANDARD */

int main(void) {
    UNITY_BEGIN();
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    RUN_TEST(test_monitored_item_absolute_deadband);
#if MU_MONITORED_QUEUE_DEPTH >= 2
    RUN_TEST(test_monitored_item_queue_overflow);
#endif
    RUN_TEST(test_set_triggering);
    RUN_TEST(test_standard_facet_errors);
#endif
    RUN_TEST(test_create_subscription);
    RUN_TEST(test_create_subscription_too_many);
    RUN_TEST(test_delete_subscriptions);
    RUN_TEST(test_create_monitored_items);
    RUN_TEST(test_create_monitored_items_too_many);
    RUN_TEST(test_delete_monitored_items);
    RUN_TEST(test_sampling_detects_change);
    RUN_TEST(test_publish_delivers_data_change);
    RUN_TEST(test_publish_keep_alive);
    RUN_TEST(test_publish_acknowledgement_unknown_sequence_returns_result);
    RUN_TEST(test_republish_and_acknowledge);
    RUN_TEST(test_republish_unknown_sequence_returns_bad_message_not_available);
    RUN_TEST(test_modify_subscription);
    RUN_TEST(test_set_publishing_mode);
    RUN_TEST(test_modify_monitored_items);
    RUN_TEST(test_set_monitoring_mode);
    RUN_TEST(test_two_sessions);
    RUN_TEST(test_subscription_session_isolation);
    return UNITY_END();
}
