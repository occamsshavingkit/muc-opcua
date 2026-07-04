/* tests/integration/test_server_framing.c
 * mu_server_poll must reassemble the TCP byte stream into messages: process every
 * complete message available (multiple per read), and wait for the rest of a
 * message split across reads. Real clients (e.g. the .NET stack with its keep-alive
 * timer) coalesce/split messages, so one read() != one message. */
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct {
    int accept_count;
    opcua_byte_t feed[1024]; /* bytes to deliver to the client connection */
    size_t feed_len;
    size_t fed;
    size_t chunk; /* max bytes to return per read() (0 = all available) */
    int write_count;
    opcua_byte_t writes[8][512];
    size_t write_len[8];
} mock_t;

static opcua_statuscode_t m_listen(void *c, const char *u) {
    (void)c;
    (void)u;
    return MU_STATUS_GOOD;
}
static void m_shutdown(void *c) {
    (void)c;
}
static void m_close(void *c, void *h) {
    (void)c;
    (void)h;
}
static opcua_statuscode_t m_accept(void *c, void **h) {
    mock_t *m = c;
    m->accept_count++;
    *h = (m->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t m_read(void *c, void *h, opcua_byte_t *buf, size_t cap, size_t *n) {
    mock_t *m = c;
    (void)h;
    size_t avail = m->feed_len - m->fed;
    size_t want = avail;
    if (m->chunk && want > m->chunk) {
        want = m->chunk;
    }
    if (want > cap) {
        want = cap;
    }
    memcpy(buf, m->feed + m->fed, want);
    m->fed += want;
    *n = want;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t m_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    mock_t *m = c;
    (void)h;
    if (m->write_count < 8) {
        memcpy(m->writes[m->write_count], buf, len < 512 ? len : 512);
        m->write_len[m->write_count] = len;
        m->write_count++;
    }
    *n = len;
    return MU_STATUS_GOOD;
}

static size_t build_hello(opcua_byte_t *out) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, 256);
    out[0] = 'H';
    out[1] = 'E';
    out[2] = 'L';
    out[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t url = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
    mu_binary_write_string(&w, &url);
    mu_binary_writer_t hs;
    mu_binary_writer_init(&hs, out, 256);
    hs.position = 4;
    mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    return w.position;
}

static size_t build_opn(opcua_byte_t *out) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, 256);
    out[0] = 'O';
    out[1] = 'P';
    out[2] = 'N';
    out[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
    mu_binary_write_string(&w, &pol);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
    mu_binary_write_nodeid(&w, &t);
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(&w, &null_id); /* RequestHeader */
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_extension_object_header(&w, &null_id, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 3600000);
    mu_binary_writer_t hs;
    mu_binary_writer_init(&hs, out, 256);
    hs.position = 4;
    mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    return w.position;
}

static void configure(mu_server_config_t *config, mock_t *mock, opcua_byte_t *rx, opcua_byte_t *tx) {
    memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri = "u";
    config->product_uri = "u";
    config->application_name = "t";
    config->receive_buffer = rx;
    config->receive_buffer_size = 8192;
    config->send_buffer = tx;
    config->send_buffer_size = 8192;
    config->max_chunk_count = 1;
    config->max_message_size = 8192;
    config->max_sessions = 1;
    config->max_secure_channels = 1;
    fake_platform_init(NULL, &config->time_adapter, &config->entropy_adapter);
    config->tcp_adapter.context = mock;
    config->tcp_adapter.listen = m_listen;
    config->tcp_adapter.accept = m_accept;
    config->tcp_adapter.read = m_read;
    config->tcp_adapter.write = m_write;
    config->tcp_adapter.close_connection = m_close;
    config->tcp_adapter.shutdown = m_shutdown;
}

/* Two messages (HEL + OPN) delivered in a single read() must both be answered. */
void test_coalesced_messages_both_processed(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    size_t h = build_hello(mock.feed);
    size_t o = build_opn(mock.feed + h);
    mock.feed_len = h + o;
    mock.chunk = 0; /* deliver everything in one read */

    mu_server_config_t config;
    opcua_byte_t rx[8192], tx[8192];
    configure(&config, &mock, rx, tx);
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* one read delivers HEL+OPN -> must produce ACK and OPN response */

    TEST_ASSERT_EQUAL(2, mock.write_count);
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.writes[0], 4);
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock.writes[1], 4);
}

/* A single message split across two reads must be reassembled and answered once. */
void test_split_message_reassembled(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));
    size_t h = build_hello(mock.feed);
    mock.feed_len = h;
    mock.chunk = 5; /* deliver 5 bytes per read -> HEL header spans multiple reads */

    mu_server_config_t config;
    opcua_byte_t rx[8192], tx[8192];
    configure(&config, &mock, rx, tx);
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    for (int i = 0; i < 20 && mock.write_count == 0; ++i) {
        mu_server_poll(server); /* accumulate 5 bytes at a time until HEL is complete */
    }

    TEST_ASSERT_EQUAL(1, mock.write_count);
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.writes[0], 4);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_coalesced_messages_both_processed);
    RUN_TEST(test_split_message_reassembled);
    return UNITY_END();
}
