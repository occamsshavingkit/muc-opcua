/* tests/unit/test_reverse_connect.c
 *
 * Server-initiated TCP connections per OPC-10000-6 §7.5.
 */

#include "muc_opcua/platform.h"
#include "muc_opcua/server.h"
#include "muc_opcua/status.h"
#include <stddef.h>
#include <string.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static opcua_datetime_t test_get_time(void *ctx) {
    (void)ctx;
    return 1000;
}

static opcua_uint64_t test_get_tick(void *ctx) {
    (void)ctx;
    return 1000;
}

static opcua_statuscode_t test_random(void *ctx, opcua_byte_t *buf, size_t len) {
    (void)ctx;
    (void)memset(buf, 0x42, len);
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_REVERSE_CONNECT

/* ---- mock tracking ---- */
static int g_connect_called;
static int g_listen_called;
static const char *g_connect_url;
static int g_sentinel;

static opcua_statuscode_t mock_listen_track(void *ctx, const char *url) {
    (void)ctx;
    (void)url;
    g_listen_called++;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_connect_track(void *ctx, const char *url, void **handle) {
    (void)ctx;
    (void)url;
    g_connect_called++;
    g_connect_url = url;
    *handle = &g_sentinel;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_accept(void *ctx, void **handle) {
    (void)ctx;
    *handle = NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_read(void *ctx, void *h, opcua_byte_t *buf, size_t cap, size_t *n) {
    (void)ctx;
    (void)h;
    (void)buf;
    (void)cap;
    *n = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_write(void *ctx, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    (void)ctx;
    (void)h;
    (void)buf;
    *n = len;
    return MU_STATUS_GOOD;
}

static void mock_close(void *ctx, void *h) {
    (void)ctx;
    (void)h;
}

static void mock_shutdown(void *ctx) {
    (void)ctx;
}

static void build_tcp_adapter(mu_tcp_adapter_t *a) {
    (void)memset(a, 0, sizeof(*a));
    a->listen = mock_listen_track;
    a->connect = mock_connect_track;
    a->accept = mock_accept;
    a->read = mock_read;
    a->write = mock_write;
    a->close_connection = mock_close;
    a->shutdown = mock_shutdown;
}

static void build_valid_config(mu_server_config_t *cfg, opcua_byte_t *rx, size_t rx_sz, opcua_byte_t *tx,
                               size_t tx_sz) {
    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->endpoint_url = "opc.tcp://localhost:4840";
    cfg->receive_buffer = rx;
    cfg->receive_buffer_size = rx_sz;
    cfg->send_buffer = tx;
    cfg->send_buffer_size = tx_sz;
    cfg->max_sessions = 2;
    cfg->max_secure_channels = 1;
    cfg->max_chunk_count = 1;
    cfg->max_message_size = MU_MIN_CHUNK_SIZE;
    cfg->time_adapter.get_time = test_get_time;
    cfg->time_adapter.get_tick_ms = test_get_tick;
    cfg->entropy_adapter.generate_random = test_random;
}

/* ---- tests ---- */

void test_reverse_connect_url_calls_connect_not_listen(void) {
    opcua_byte_t rx[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx[MU_MIN_CHUNK_SIZE];
    mu_server_config_t cfg;
    build_valid_config(&cfg, rx, sizeof(rx), tx, sizeof(tx));
    build_tcp_adapter(&cfg.tcp_adapter);
    cfg.reverse_connect_url = "opc.tcp://192.168.1.100:4840";

    g_connect_called = 0;
    g_listen_called = 0;
    g_connect_url = NULL;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    opcua_statuscode_t s = mu_server_init(storage, sizeof(storage), &cfg, &server);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_NOT_NULL(server);
    TEST_ASSERT_EQUAL_INT(1, g_connect_called);
    TEST_ASSERT_EQUAL_INT(0, g_listen_called);
    TEST_ASSERT_EQUAL_STRING("opc.tcp://192.168.1.100:4840", g_connect_url);

    mu_server_close(server);
}

void test_null_reverse_connect_url_calls_listen(void) {
    opcua_byte_t rx[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx[MU_MIN_CHUNK_SIZE];
    mu_server_config_t cfg;
    build_valid_config(&cfg, rx, sizeof(rx), tx, sizeof(tx));
    build_tcp_adapter(&cfg.tcp_adapter);
    cfg.reverse_connect_url = NULL;

    g_connect_called = 0;
    g_listen_called = 0;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    opcua_statuscode_t s = mu_server_init(storage, sizeof(storage), &cfg, &server);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_NOT_NULL(server);
    TEST_ASSERT_EQUAL_INT(0, g_connect_called);
    TEST_ASSERT_EQUAL_INT(1, g_listen_called);

    mu_server_close(server);
}

void test_reverse_connect_url_requires_connect_callback(void) {
    opcua_byte_t rx[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx[MU_MIN_CHUNK_SIZE];
    mu_server_config_t cfg;
    build_valid_config(&cfg, rx, sizeof(rx), tx, sizeof(tx));
    build_tcp_adapter(&cfg.tcp_adapter);
    cfg.tcp_adapter.connect = NULL;
    cfg.reverse_connect_url = "opc.tcp://192.168.1.100:4840";

    opcua_statuscode_t s = mu_server_config_validate(&cfg);

    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, s);
}

#else /* !MUC_OPCUA_REVERSE_CONNECT */

void test_reverse_connect_requires_build_flag(void) {
    TEST_PASS_MESSAGE("Reverse Connect gated behind MUC_OPCUA_REVERSE_CONNECT");
}

#endif /* MUC_OPCUA_REVERSE_CONNECT */

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_REVERSE_CONNECT
    RUN_TEST(test_reverse_connect_url_calls_connect_not_listen);
    RUN_TEST(test_null_reverse_connect_url_calls_listen);
    RUN_TEST(test_reverse_connect_url_requires_connect_callback);
#else
    RUN_TEST(test_reverse_connect_requires_build_flag);
#endif
    return UNITY_END();
}
