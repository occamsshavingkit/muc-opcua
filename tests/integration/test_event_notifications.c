/* tests/integration/test_event_notifications.c */
#include "../../src/core/server_internal.h"
#include "../../src/services/subscription.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "service_builders.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define ID_CREATESUBSCRIPTIONREQUEST 787
#define ID_CREATESUBSCRIPTIONRESPONSE 790
#define ID_CREATEMONITOREDITEMSREQUEST 751
#define ID_CREATEMONITOREDITEMSRESPONSE 754
#define ID_PUBLISHREQUEST 826
#define ID_PUBLISHRESPONSE 829
#define ID_EVENTNOTIFICATIONLIST 914

#define MAX_INBOUND 8
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

static void enqueue_connect(mock_t *mock) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
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
    enqueue(mock, tmp, w.position);

    /* OPN */
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
    enqueue(mock, chunk, w.position);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_create_session_body(&w, 2, 60000.0);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(mock, chunk, clen);

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
    mu_binary_write_int32(&w, 0);
    mu_binary_write_int32(&w, 0);
    {
        mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {321}};
        mu_binary_write_extension_object_header(&w, &token_type, 20);
        mu_string_t policy = {16, (const opcua_byte_t *)"anonymous_policy"};
        mu_binary_write_string(&w, &policy);
    }
    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(mock, chunk, clen);
}

static void run_connect(mu_server_t *server, mock_t *mock) {
    mu_server_poll(server); /* process accept */
    mu_server_poll(server); /* process HEL */
    mu_server_poll(server); /* process OPN */
    s_channel_id = read_opn_channel_id(mock->last_write, mock->last_write_len);
    patch_msg_channel_ids(mock, s_channel_id);
    mu_server_poll(server); /* process CreateSession */
    mu_server_poll(server); /* process ActivateSession */
}

static opcua_uint32_t parse_response(const opcua_byte_t *write_buf, size_t write_len, mu_binary_reader_t *body,
                                     opcua_statuscode_t *status) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, write_buf, write_len);
    r.position = 24;
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);

    /* responseHeader */
    opcua_datetime_t ts;
    mu_binary_read_int64(&r, &ts);
    opcua_uint32_t handle;
    mu_binary_read_uint32(&r, &handle);
    mu_binary_read_statuscode(&r, status);

    opcua_byte_t diag_mask;
    mu_binary_read_byte(&r, &diag_mask);
    opcua_int32_t string_table_len;
    mu_binary_read_int32(&r, &string_table_len);

    mu_nodeid_t dummy;
    size_t dummy_len;
    mu_binary_read_extension_object_header(&r, &dummy, &dummy_len);

    *body = r;
    return type.identifier.numeric;
}

static void enqueue_create_subscription(mock_t *mock, opcua_uint32_t seq, opcua_double_t interval) {
    opcua_byte_t tmp[512], chunk[512];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATESUBSCRIPTIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_double(&w, interval);
    mu_binary_write_uint32(&w, 100);
    mu_binary_write_uint32(&w, 10);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(mock, chunk, clen);
}

static void write_event_filter(mu_binary_writer_t *w) {
    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {727}};
    opcua_byte_t filter_buf[512];
    mu_binary_writer_t f_w;
    mu_binary_writer_init(&f_w, filter_buf, sizeof(filter_buf));

    mu_binary_write_int32(&f_w, 5);
    const char *names[] = {"EventId", "EventType", "Time", "Message", "Severity"};
    for (int i = 0; i < 5; ++i) {
        mu_nodeid_t tid = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&f_w, &tid);
        mu_binary_write_int32(&f_w, 1);
        mu_binary_write_uint16(&f_w, 0);
        mu_string_t field_name = {(opcua_int32_t)strlen(names[i]), (const opcua_byte_t *)names[i]};
        mu_binary_write_string(&f_w, &field_name);
        mu_binary_write_uint32(&f_w, 13);
        mu_string_t idx_range = {-1, NULL};
        mu_binary_write_string(&f_w, &idx_range);
    }
    mu_binary_write_int32(&f_w, 0);

    mu_binary_write_extension_object_header(w, &filter_type, f_w.position);
    (void)memcpy(w->buffer + w->position, filter_buf, f_w.position);
    w->position += f_w.position;
}

static void write_event_moncreate_item(mu_binary_writer_t *w, opcua_uint32_t client_handle) {
    mu_nodeid_t n = {0, MU_NODEID_NUMERIC, {2253}};
    mu_binary_write_nodeid(w, &n);
    mu_binary_write_uint32(w, 12);
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint16(w, 0);
    {
        mu_string_t nul = {-1, NULL};
        mu_binary_write_string(w, &nul);
    }
    mu_binary_write_uint32(w, 2);
    mu_binary_write_uint32(w, client_handle);
    mu_binary_write_double(w, 0.0);
    write_event_filter(w);
    mu_binary_write_uint32(w, 8);
    mu_binary_write_boolean(w, true);
}

static void enqueue_create_monitored_item(mock_t *mock, opcua_uint32_t seq, opcua_uint32_t sub_id,
                                          opcua_uint32_t client_handle) {
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_event_moncreate_item(&w, client_handle);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(mock, chunk, clen);
}

static void enqueue_publish(mock_t *m, opcua_uint32_t seq) {
    opcua_byte_t tmp[256], chunk[256];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_PUBLISHREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_int32(&w, 0);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(m, chunk, clen);
}

static opcua_uint64_t s_tick = 0;
static opcua_uint64_t test_get_tick_ms(void *c) {
    (void)c;
    return s_tick;
}

void test_alarm_event_generation_and_publishing(void) {
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_EVENTS
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:t";
    config.product_uri = "urn:t";
    config.application_name = "t";

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
    s_tick = 0;
    config.time_adapter.get_tick_ms = test_get_tick_ms;
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server, &mock);
    mu_server_poll(server); /* CreateSubscription */
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    /* Create MonitoredItem on EventNotifier */
    enqueue_create_monitored_item(&mock, 5, sub_id, 99);
    mu_server_poll(server);
    opcua_uint32_t resp_id = parse_response(mock.last_write, mock.last_write_len, &body, &sr);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE, resp_id);

    /* Trigger Alarm Event on server */
    mu_event_notification_t ev;
    (void)memset(&ev, 0, sizeof(ev));
    ev.event_type.namespace_index = 0;
    ev.event_type.identifier_type = MU_NODEID_NUMERIC;
    ev.event_type.identifier.numeric = 2782; /* ExclusiveLimitAlarmType */
    ev.event_id = (mu_bytestring_t){4, (const opcua_byte_t *)"EV01"};
    ev.severity = 500;
    ev.message = (mu_string_t){10, (const opcua_byte_t *)"LimitAlarm"};
    ev.time = 123456789;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_trigger_event(server, &ev));

    /* Enqueue Publish Request */
    enqueue_publish(&mock, 6);

    /* Tick past publishing interval to deliver notifications */
    s_tick = 250;
    mu_server_poll(server);
    mu_server_poll(server);

    /* Verify PublishResponse carries the EventNotificationList */
    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    opcua_uint32_t resp_sub_id;
    mu_binary_read_uint32(&body, &resp_sub_id);
    TEST_ASSERT_EQUAL(sub_id, resp_sub_id);

    opcua_int32_t navail;
    mu_binary_read_int32(&body, &navail);
    opcua_byte_t more;
    mu_binary_read_byte(&body, &more);
    opcua_uint32_t seq_num;
    mu_binary_read_uint32(&body, &seq_num);
    opcua_int64_t pub_time;
    mu_binary_read_int64(&body, &pub_time);

    opcua_int32_t ndata;
    mu_binary_read_int32(&body, &ndata);
    TEST_ASSERT_EQUAL(1, ndata);

    mu_nodeid_t ext_type;
    size_t ext_len;
    mu_binary_read_extension_object_header(&body, &ext_type, &ext_len);
    TEST_ASSERT_EQUAL(ID_EVENTNOTIFICATIONLIST, ext_type.identifier.numeric);

    opcua_int32_t total_events;
    mu_binary_read_int32(&body, &total_events);
    TEST_ASSERT_EQUAL(1, total_events);

    opcua_uint32_t client_handle;
    mu_binary_read_uint32(&body, &client_handle);
    TEST_ASSERT_EQUAL(99, client_handle);

    opcua_int32_t field_count;
    mu_binary_read_int32(&body, &field_count);
    TEST_ASSERT_EQUAL(5, field_count);

    /* EventId: Variant ByteString */
    mu_variant_t var_id;
    mu_binary_read_variant(&body, &var_id);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTESTRING, var_id.type);
    TEST_ASSERT_EQUAL(4, var_id.value.bytestr.length);
    TEST_ASSERT_EQUAL_MEMORY("EV01", var_id.value.bytestr.data, 4);

    /* EventType: Variant NodeId */
    mu_variant_t var_type;
    mu_binary_read_variant(&body, &var_type);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, var_type.type);
    TEST_ASSERT_EQUAL(2782, var_type.value.nodeid.identifier.numeric);

    /* Time: Variant DateTime */
    mu_variant_t var_time;
    mu_binary_read_variant(&body, &var_time);
    TEST_ASSERT_EQUAL(MU_TYPE_DATETIME, var_time.type);
    TEST_ASSERT_EQUAL_INT64(123456789, var_time.value.dt);

    /* Message: Variant LocalizedText */
    mu_variant_t var_msg;
    mu_binary_read_variant(&body, &var_msg);
    TEST_ASSERT_EQUAL(MU_TYPE_LOCALIZEDTEXT, var_msg.type);
    TEST_ASSERT_EQUAL(10, var_msg.value.localized_text.text.length);
    TEST_ASSERT_EQUAL_MEMORY("LimitAlarm", var_msg.value.localized_text.text.data, 10);

    /* Severity: Variant UInt16 */
    mu_variant_t var_sev;
    mu_binary_read_variant(&body, &var_sev);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT16, var_sev.type);
    TEST_ASSERT_EQUAL(500, var_sev.value.ui16);

    mu_server_close(server);
#endif
}

#if MUC_OPCUA_EVENT_FILTER_WHERE
/* SimpleAttributeOperand(BaseEventType, /<field>, Value) ExtensionObject. */
static void write_simple_attr_operand(mu_binary_writer_t *w, const char *field) {
    opcua_byte_t buf[128];
    mu_binary_writer_t b;
    mu_binary_writer_init(&b, buf, sizeof(buf));
    mu_nodeid_t typedef_id = {0, MU_NODEID_NUMERIC, {2041}}; /* BaseEventType */
    mu_binary_write_nodeid(&b, &typedef_id);
    mu_binary_write_int32(&b, 1); /* browsePath length */
    mu_binary_write_uint16(&b, 0);
    mu_string_t name = {(opcua_int32_t)strlen(field), (const opcua_byte_t *)field};
    mu_binary_write_string(&b, &name);
    mu_binary_write_uint32(&b, 13); /* attributeId = Value */
    mu_string_t idx = {-1, NULL};
    mu_binary_write_string(&b, &idx);

    mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {603}}; /* SimpleAttributeOperand_Encoding_DefaultBinary */
    mu_binary_write_extension_object_header(w, &t, b.position);
    (void)memcpy(w->buffer + w->position, buf, b.position);
    w->position += b.position;
}

/* LiteralOperand(UInt16) ExtensionObject. */
static void write_literal_u16(mu_binary_writer_t *w, opcua_uint16_t val) {
    opcua_byte_t buf[32];
    mu_binary_writer_t b;
    mu_binary_writer_init(&b, buf, sizeof(buf));
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_UINT16;
    v.value.ui16 = val;
    mu_binary_write_variant(&b, &v);

    mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {597}}; /* LiteralOperand_Encoding_DefaultBinary */
    mu_binary_write_extension_object_header(w, &t, b.position);
    (void)memcpy(w->buffer + w->position, buf, b.position);
    w->position += b.position;
}

/* EventFilter: 5 select clauses + WhereClause (Severity >= threshold). */
static void write_event_filter_where(mu_binary_writer_t *w, opcua_uint32_t op, opcua_uint16_t threshold) {
    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {727}};
    opcua_byte_t filter_buf[1024];
    mu_binary_writer_t f_w;
    mu_binary_writer_init(&f_w, filter_buf, sizeof(filter_buf));

    /* SelectClauses are raw SimpleAttributeOperand structures (not ExtensionObjects). */
    mu_binary_write_int32(&f_w, 5);
    const char *names[] = {"EventId", "EventType", "Time", "Message", "Severity"};
    for (int i = 0; i < 5; ++i) {
        mu_nodeid_t tid = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&f_w, &tid);
        mu_binary_write_int32(&f_w, 1);
        mu_binary_write_uint16(&f_w, 0);
        mu_string_t nm = {(opcua_int32_t)strlen(names[i]), (const opcua_byte_t *)names[i]};
        mu_binary_write_string(&f_w, &nm);
        mu_binary_write_uint32(&f_w, 13);
        mu_string_t idx = {-1, NULL};
        mu_binary_write_string(&f_w, &idx);
    }
    /* WhereClause FilterOperands ARE ExtensionObjects. 1 element:
       GreaterThanOrEqual(SimpleAttr(Severity), Literal(threshold)). */
    mu_binary_write_int32(&f_w, 1);
    mu_binary_write_uint32(&f_w, op);
    mu_binary_write_int32(&f_w, 2);
    write_simple_attr_operand(&f_w, "Severity");
    write_literal_u16(&f_w, threshold);

    mu_binary_write_extension_object_header(w, &filter_type, f_w.position);
    (void)memcpy(w->buffer + w->position, filter_buf, f_w.position);
    w->position += f_w.position;
}

static void write_event_moncreate_item_where(mu_binary_writer_t *w, opcua_uint32_t op, opcua_uint32_t client_handle) {
    mu_nodeid_t n = {0, MU_NODEID_NUMERIC, {2253}};
    mu_binary_write_nodeid(w, &n);
    mu_binary_write_uint32(w, 12);
    mu_string_t nul = {-1, NULL};
    mu_binary_write_string(w, &nul);
    mu_binary_write_uint16(w, 0);
    mu_binary_write_string(w, &nul);
    mu_binary_write_uint32(w, 2);
    mu_binary_write_uint32(w, client_handle);
    mu_binary_write_double(w, 0.0);
    write_event_filter_where(w, op, 500);
    mu_binary_write_uint32(w, 8);
    mu_binary_write_boolean(w, true);
}

static void enqueue_create_monitored_item_where(mock_t *mock, opcua_uint32_t seq, opcua_uint32_t sub_id,
                                                opcua_uint32_t op, opcua_uint32_t client_handle) {
    opcua_byte_t tmp[2048], chunk[2048];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
    mu_binary_write_nodeid(&w, &t);
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    write_event_moncreate_item_where(&w, op, client_handle);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(mock, chunk, clen);
}

static void trigger_severity_event(mu_server_t *server, opcua_uint16_t severity, const char *id) {
    mu_event_notification_t ev;
    (void)memset(&ev, 0, sizeof(ev));
    ev.event_type.identifier_type = MU_NODEID_NUMERIC;
    ev.event_type.identifier.numeric = 2782;
    ev.event_id = (mu_bytestring_t){4, (const opcua_byte_t *)id};
    ev.severity = severity;
    ev.message = (mu_string_t){3, (const opcua_byte_t *)"msg"};
    ev.time = 100;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_trigger_event(server, &ev));
}

/* The WhereClause (Severity >= 500) must drop the low-severity event, and the
   EventFieldList count must reflect the filtered total (regression for the
   pre-filter over-count that desynced the Publish stream). */
void test_event_where_clause_filters_and_counts(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:t";
    config.product_uri = "urn:t";
    config.application_name = "t";
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
    s_tick = 0;
    config.time_adapter.get_tick_ms = test_get_tick_ms;
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server, &mock);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    enqueue_create_monitored_item_where(&mock, 5, sub_id, 4u, 77);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    /* One passing (>=500) and one failing (<500) event. */
    trigger_severity_event(server, 800, "HI01");
    trigger_severity_event(server, 200, "LO01");

    enqueue_publish(&mock, 6);
    s_tick = 250;
    mu_server_poll(server);
    mu_server_poll(server);

    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    opcua_uint32_t resp_sub_id;
    mu_binary_read_uint32(&body, &resp_sub_id);
    opcua_int32_t navail;
    mu_binary_read_int32(&body, &navail);
    opcua_byte_t more;
    mu_binary_read_byte(&body, &more);
    opcua_uint32_t seq_num;
    mu_binary_read_uint32(&body, &seq_num);
    opcua_int64_t pub_time;
    mu_binary_read_int64(&body, &pub_time);
    opcua_int32_t ndata;
    mu_binary_read_int32(&body, &ndata);
    TEST_ASSERT_EQUAL(1, ndata);

    mu_nodeid_t ext_type;
    size_t ext_len;
    mu_binary_read_extension_object_header(&body, &ext_type, &ext_len);
    TEST_ASSERT_EQUAL(ID_EVENTNOTIFICATIONLIST, ext_type.identifier.numeric);

    /* Only the severity-800 event survives the WhereClause → count is exactly 1. */
    opcua_int32_t total_events;
    mu_binary_read_int32(&body, &total_events);
    TEST_ASSERT_EQUAL(1, total_events);

    opcua_uint32_t client_handle;
    mu_binary_read_uint32(&body, &client_handle);
    TEST_ASSERT_EQUAL(77, client_handle);

    opcua_int32_t field_count;
    mu_binary_read_int32(&body, &field_count);
    TEST_ASSERT_EQUAL(5, field_count);

    mu_variant_t var_id;
    mu_binary_read_variant(&body, &var_id);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTESTRING, var_id.type);
    TEST_ASSERT_EQUAL_MEMORY("HI01", var_id.value.bytestr.data, 4);

    mu_server_close(server);
}

/* A WhereClause using an unsupported FilterOperator (Cast) must fail the item at
   CreateMonitoredItems with Bad_FilterOperatorUnsupported (OPC-10000-4 §7.7.3),
   not be silently accepted. */
void test_event_where_unsupported_operator_rejected(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:t";
    config.product_uri = "urn:t";
    config.application_name = "t";
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
    s_tick = 0;
    config.time_adapter.get_tick_ms = test_get_tick_ms;
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server, &mock);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    enqueue_create_monitored_item_where(&mock, 5, sub_id, 12u /* Cast: unsupported */, 88);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr); /* service ok; item-level failure */

    opcua_int32_t results_count;
    mu_binary_read_int32(&body, &results_count);
    TEST_ASSERT_EQUAL(1, results_count);
    opcua_statuscode_t item_status;
    mu_binary_read_statuscode(&body, &item_status);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_FILTEROPERATORUNSUPPORTED, item_status);

    mu_server_close(server);
}
#endif /* MUC_OPCUA_EVENT_FILTER_WHERE */

/* Issue #288 repro attempt: asyncua starts its Publish loop the instant
   CreateSubscription returns — so the FIRST Publish arrives before any
   CreateMonitoredItems. handle_publish must find the subscription (sub_count>0)
   and park the request, NOT return Bad_NoSubscription. */
void test_publish_immediately_after_create_subscription(void) {
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 500.0);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:t";
    config.product_uri = "urn:t";
    config.application_name = "t";
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
    s_tick = 0;
    config.time_adapter.get_tick_ms = test_get_tick_ms;
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_binary_reader_t body;
    opcua_statuscode_t sr;
    run_connect(server, &mock);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    /* First Publish — no monitored items exist yet (asyncua's timeline). The
       subscription DOES exist, so handle_publish must find it (sub_count>0) and
       PARK the request: no immediate response, and definitely not a ServiceFault
       with Bad_NoSubscription (the #288 symptom). */
    mock.last_write_len = 0;
    enqueue_publish(&mock, 6);
    mu_server_poll(server);
    if (mock.last_write_len > 0) {
        /* Any response here would be a ServiceFault — the bug. */
        parse_response(mock.last_write, mock.last_write_len, &body, &sr);
        TEST_ASSERT_NOT_EQUAL_HEX32(MU_STATUS_BAD_NOSUBSCRIPTION, sr);
        TEST_FAIL_MESSAGE("Immediate Publish produced a response (expected parked, no ServiceFault)");
    }

    /* Prove the parked Publish is honoured: add a monitored item, change the
       value, tick, and expect a DataChangeNotification — not a fault. */
    enqueue_publish(&mock, 7);
    mu_server_poll(server);
    s_tick = 1200;
    mu_server_poll(server);
    mu_server_poll(server);
    if (mock.last_write_len > 0) {
        opcua_uint32_t rid = parse_response(mock.last_write, mock.last_write_len, &body, &sr);
        TEST_ASSERT_NOT_EQUAL_HEX32(MU_STATUS_BAD_NOSUBSCRIPTION, sr);
        TEST_ASSERT_NOT_EQUAL(MU_ID_SERVICEFAULT, rid);
    }

    mu_server_close(server);
#endif
}

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_EVENTS && MUC_OPCUA_CU_AUDITING
/* EventFilter selecting base + AuditEvent fields (spec 074): EventType, Status,
   AttributeId. No WhereClause. */
static void write_event_filter_audit(mu_binary_writer_t *w) {
    mu_nodeid_t filter_type = {0, MU_NODEID_NUMERIC, {727}};
    opcua_byte_t filter_buf[512];
    mu_binary_writer_t f_w;
    mu_binary_writer_init(&f_w, filter_buf, sizeof(filter_buf));
    mu_binary_write_int32(&f_w, 3);
    const char *names[] = {"EventType", "Status", "AttributeId"};
    for (int i = 0; i < 3; ++i) {
        mu_nodeid_t tid = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&f_w, &tid);
        mu_binary_write_int32(&f_w, 1);
        mu_binary_write_uint16(&f_w, 0);
        mu_string_t nm = {(opcua_int32_t)strlen(names[i]), (const opcua_byte_t *)names[i]};
        mu_binary_write_string(&f_w, &nm);
        mu_binary_write_uint32(&f_w, 13);
        mu_string_t idx = {-1, NULL};
        mu_binary_write_string(&f_w, &idx);
    }
    mu_binary_write_int32(&f_w, 0);
    mu_binary_write_extension_object_header(w, &filter_type, f_w.position);
    (void)memcpy(w->buffer + w->position, filter_buf, f_w.position);
    w->position += f_w.position;
}

static void enqueue_create_monitored_item_audit(mock_t *mock, opcua_uint32_t seq, opcua_uint32_t sub_id,
                                                opcua_uint32_t client_handle) {
    opcua_byte_t tmp[1024], chunk[1024];
    mu_binary_writer_t w;
    size_t clen;
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {ID_CREATEMONITOREDITEMSREQUEST}};
    mu_binary_write_nodeid(&w, &t);
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, seq);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    /* MonitoredItem on Server EventNotifier (2253, attr 12) with the audit filter. */
    mu_nodeid_t n = {0, MU_NODEID_NUMERIC, {2253}};
    mu_binary_write_nodeid(&w, &n);
    mu_binary_write_uint32(&w, 12);
    mu_string_t nul = {-1, NULL};
    mu_binary_write_string(&w, &nul);
    mu_binary_write_uint16(&w, 0);
    mu_binary_write_string(&w, &nul);
    mu_binary_write_uint32(&w, 2);
    mu_binary_write_uint32(&w, client_handle);
    mu_binary_write_double(&w, 0.0);
    write_event_filter_audit(&w);
    mu_binary_write_uint32(&w, 8);
    mu_binary_write_boolean(&w, true);
    clen = build_msg(chunk, sizeof(chunk), seq, seq, tmp, w.position);
    enqueue(mock, chunk, clen);
}
#endif

/* spec 074 SC-001: a client subscribed to the Server EventNotifier receives an
   AuditWriteUpdateEvent (i=2100) when the server raises a WRITE_UPDATE audit,
   with the selected audit fields (Status, AttributeId) resolved from the pool. */
void test_audit_write_event_e2e(void) {
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_EVENTS && MUC_OPCUA_CU_AUDITING
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));
    enqueue_connect(&mock);
    enqueue_create_subscription(&mock, 4, 200.0);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:t";
    config.product_uri = "urn:t";
    config.application_name = "t";
    config.auditing_enabled = true;

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
    s_tick = 0;
    config.time_adapter.get_tick_ms = test_get_tick_ms;
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_binary_reader_t body;
    opcua_statuscode_t sr;

    run_connect(server, &mock);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATESUBSCRIPTIONRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);
    opcua_uint32_t sub_id;
    mu_binary_read_uint32(&body, &sub_id);

    enqueue_create_monitored_item_audit(&mock, 5, sub_id, 77);
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(ID_CREATEMONITOREDITEMSRESPONSE,
                      parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    /* Server raises a WRITE_UPDATE AuditEvent. */
    mu_audit_event_t ae;
    (void)memset(&ae, 0, sizeof(ae));
    ae.event_type = MU_AUDIT_EVENT_WRITE_UPDATE;
    ae.status = true;
    ae.action_timestamp = 123456789;
    ae.specific.write_update.node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 5001}};
    ae.specific.write_update.old_value.type = MU_TYPE_INT32;
    ae.specific.write_update.old_value.value.i32 = 10;
    ae.specific.write_update.new_value.type = MU_TYPE_INT32;
    ae.specific.write_update.new_value.value.i32 = 20;
    mu_raise_audit_event(server, &ae);

    enqueue_publish(&mock, 6);
    s_tick = 250;
    mu_server_poll(server);
    mu_server_poll(server);

    TEST_ASSERT_EQUAL(ID_PUBLISHRESPONSE, parse_response(mock.last_write, mock.last_write_len, &body, &sr));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sr);

    opcua_uint32_t resp_sub_id;
    mu_binary_read_uint32(&body, &resp_sub_id);
    TEST_ASSERT_EQUAL(sub_id, resp_sub_id);
    opcua_int32_t navail;
    mu_binary_read_int32(&body, &navail);
    opcua_byte_t more;
    mu_binary_read_byte(&body, &more);
    opcua_uint32_t seq_num;
    mu_binary_read_uint32(&body, &seq_num);
    opcua_int64_t pub_time;
    mu_binary_read_int64(&body, &pub_time);
    opcua_int32_t ndata;
    mu_binary_read_int32(&body, &ndata);
    TEST_ASSERT_EQUAL(1, ndata);

    mu_nodeid_t ext_type;
    size_t ext_len;
    mu_binary_read_extension_object_header(&body, &ext_type, &ext_len);
    TEST_ASSERT_EQUAL(ID_EVENTNOTIFICATIONLIST, ext_type.identifier.numeric);
    opcua_int32_t total_events;
    mu_binary_read_int32(&body, &total_events);
    TEST_ASSERT_EQUAL(1, total_events);
    opcua_uint32_t client_handle;
    mu_binary_read_uint32(&body, &client_handle);
    TEST_ASSERT_EQUAL(77, client_handle);
    opcua_int32_t field_count;
    mu_binary_read_int32(&body, &field_count);
    TEST_ASSERT_EQUAL(3, field_count);

    /* EventType -> AuditWriteUpdateEventType i=2100 */
    mu_variant_t v_type;
    mu_binary_read_variant(&body, &v_type);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, v_type.type);
    TEST_ASSERT_EQUAL(2100, v_type.value.nodeid.identifier.numeric);
    /* Status -> Boolean true */
    mu_variant_t v_status;
    mu_binary_read_variant(&body, &v_status);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, v_status.type);
    TEST_ASSERT_TRUE(v_status.value.b);
    /* AttributeId -> UInt32 13 (Value) */
    mu_variant_t v_attr;
    mu_binary_read_variant(&body, &v_attr);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, v_attr.type);
    TEST_ASSERT_EQUAL(13u, v_attr.value.ui32);

    mu_server_close(server);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_alarm_event_generation_and_publishing);
    RUN_TEST(test_publish_immediately_after_create_subscription);
#if MUC_OPCUA_EVENT_FILTER_WHERE
    RUN_TEST(test_event_where_clause_filters_and_counts);
    RUN_TEST(test_event_where_unsupported_operator_rejected);
#endif
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_EVENTS && MUC_OPCUA_CU_AUDITING
    RUN_TEST(test_audit_write_event_e2e);
#endif
    return UNITY_END();
}
