/* tests/integration/test_connection_robustness.c
 * Liveness + I/O robustness for the single connection slot:
 *  - a peer that connects but never opens a secure channel is timed out and the
 *    slot is reclaimed (uses a controllable monotonic tick);
 *  - a failed/short TCP write closes the connection rather than wedging it. */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ---- controllable mock transport + clock ---- */
static opcua_uint64_t g_tick_ms;
static int g_close_count;
static int g_write_mode; /* 0 = full ok, 1 = short write, 2 = error */

typedef struct {
    int accept_count;
    const opcua_byte_t *next_read;
    size_t next_read_len;
    int active;            /* a client is connected */
} mock_t;

static opcua_uint64_t mock_tick(void *c) { (void)c; return g_tick_ms; }
static opcua_datetime_t mock_time(void *c) { (void)c; return 0; }
static opcua_statuscode_t mock_listen(void *c, const char *u) { (void)c; (void)u; return MU_STATUS_GOOD; }
static void mock_shutdown(void *c) { (void)c; }
static opcua_statuscode_t mock_entropy(void *c, opcua_byte_t *b, size_t n) { (void)c; if (b) memset(b, 0, n); return MU_STATUS_GOOD; }

static void mock_close(void *c, void *h) { (void)h; ((mock_t *)c)->active = 0; g_close_count++; }

static opcua_statuscode_t mock_accept(void *c, void **handle) {
    mock_t *m = (mock_t *)c;
    if (!m->active && m->accept_count == 0) { m->accept_count++; m->active = 1; *handle = (void *)1; }
    else { *handle = NULL; }
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_read(void *c, void *h, opcua_byte_t *buf, size_t cap, size_t *n) {
    mock_t *m = (mock_t *)c; (void)h;
    if (m->next_read_len == 0 || m->next_read_len > cap) { *n = 0; return MU_STATUS_GOOD; }
    memcpy(buf, m->next_read, m->next_read_len);
    *n = m->next_read_len;
    m->next_read_len = 0; /* consumed */
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    (void)c; (void)h; (void)buf;
    if (g_write_mode == 2) { *n = 0; return MU_STATUS_BAD_INTERNALERROR; }
    if (g_write_mode == 1) { *n = len > 1 ? len - 1 : 0; return MU_STATUS_GOOD; } /* short */
    *n = len; return MU_STATUS_GOOD;
}

static void make_config(mu_server_config_t *config, mock_t *mock,
                        opcua_byte_t *rx, size_t rxn, opcua_byte_t *tx, size_t txn) {
    memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri = "urn:t"; config->product_uri = "urn:t"; config->application_name = "t";
    config->receive_buffer = rx; config->receive_buffer_size = rxn;
    config->send_buffer = tx; config->send_buffer_size = txn;
    config->max_chunk_count = 1; config->max_message_size = (opcua_uint32_t)txn;
    config->max_sessions = 1; config->max_secure_channels = 1;
    config->tcp_adapter.context = mock;
    config->tcp_adapter.listen = mock_listen; config->tcp_adapter.accept = mock_accept;
    config->tcp_adapter.read = mock_read; config->tcp_adapter.write = mock_write;
    config->tcp_adapter.close_connection = mock_close; config->tcp_adapter.shutdown = mock_shutdown;
    config->time_adapter.get_time = mock_time; config->time_adapter.get_tick_ms = mock_tick;
    config->entropy_adapter.generate_random = mock_entropy;
}

/* Build a minimal HELLO message into buf, returns length. */
static size_t build_hello(opcua_byte_t *buf, size_t cap) {
    mu_binary_writer_t w; mu_binary_writer_init(&w, buf, cap);
    buf[0]='H'; buf[1]='E'; buf[2]='L'; buf[3]='F'; w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    { mu_string_t url = { 19, (const opcua_byte_t *)"opc.tcp://host:4840" }; mu_binary_write_string(&w, &url); }
    { mu_binary_writer_t hs; mu_binary_writer_init(&hs, buf, cap); hs.position = 4; mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position); }
    return w.position;
}

/* A peer that connects and then sends nothing must be timed out and the slot freed. */
void test_idle_connection_times_out(void) {
    g_tick_ms = 0; g_close_count = 0; g_write_mode = 0;
    mock_t mock; memset(&mock, 0, sizeof(mock));
    mu_server_config_t config; static opcua_byte_t rx[8192], tx[8192];
    make_config(&config, &mock, rx, sizeof(rx), tx, sizeof(tx));

    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server);                 /* accept; last_activity stamped at t=0 */
    TEST_ASSERT_NOT_NULL(server->client_handle);

    g_tick_ms = 5000;                        /* within the connect timeout */
    mu_server_poll(server);                  /* no data, still connected */
    TEST_ASSERT_NOT_NULL(server->client_handle);
    TEST_ASSERT_EQUAL(0, g_close_count);

    g_tick_ms = 60000;                       /* past the connect timeout */
    mu_server_poll(server);                  /* should reclaim the slot */
    TEST_ASSERT_NULL(server->client_handle);
    TEST_ASSERT_EQUAL(1, g_close_count);
}

/* Inbound traffic resets the idle timer (a busy client is not dropped). */
void test_activity_resets_idle_timer(void) {
    g_tick_ms = 0; g_close_count = 0; g_write_mode = 0;
    mock_t mock; memset(&mock, 0, sizeof(mock));
    mu_server_config_t config; static opcua_byte_t rx[8192], tx[8192];
    make_config(&config, &mock, rx, sizeof(rx), tx, sizeof(tx));
    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server);                  /* accept at t=0 */

    opcua_byte_t hello[128]; size_t hlen = build_hello(hello, sizeof(hello));
    g_tick_ms = 20000;
    mock.next_read = hello; mock.next_read_len = hlen;
    mu_server_poll(server);                  /* HELLO at t=20000 -> activity, ACK sent */
    TEST_ASSERT_NOT_NULL(server->client_handle);

    g_tick_ms = 40000;                       /* 20s since last activity: under the limit */
    mu_server_poll(server);
    TEST_ASSERT_NOT_NULL(server->client_handle);
    TEST_ASSERT_EQUAL(0, g_close_count);
}

/* A failed TCP write closes the connection instead of wedging it. */
void test_write_failure_closes_connection(void) {
    g_tick_ms = 0; g_close_count = 0; g_write_mode = 2; /* write errors */
    mock_t mock; memset(&mock, 0, sizeof(mock));
    mu_server_config_t config; static opcua_byte_t rx[8192], tx[8192];
    make_config(&config, &mock, rx, sizeof(rx), tx, sizeof(tx));
    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server);                  /* accept */
    opcua_byte_t hello[128]; size_t hlen = build_hello(hello, sizeof(hello));
    mock.next_read = hello; mock.next_read_len = hlen;
    mu_server_poll(server);                  /* HELLO -> ACK write fails -> close */
    TEST_ASSERT_NULL(server->client_handle);
    TEST_ASSERT_EQUAL(1, g_close_count);
}

/* A short (partial) TCP write also closes the connection. */
void test_short_write_closes_connection(void) {
    g_tick_ms = 0; g_close_count = 0; g_write_mode = 1; /* short write */
    mock_t mock; memset(&mock, 0, sizeof(mock));
    mu_server_config_t config; static opcua_byte_t rx[8192], tx[8192];
    make_config(&config, &mock, rx, sizeof(rx), tx, sizeof(tx));
    opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server);
    opcua_byte_t hello[128]; size_t hlen = build_hello(hello, sizeof(hello));
    mock.next_read = hello; mock.next_read_len = hlen;
    mu_server_poll(server);
    TEST_ASSERT_NULL(server->client_handle);
    TEST_ASSERT_EQUAL(1, g_close_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_connection_times_out);
    RUN_TEST(test_activity_resets_idle_timer);
    RUN_TEST(test_write_failure_closes_connection);
    RUN_TEST(test_short_write_closes_connection);
    return UNITY_END();
}
