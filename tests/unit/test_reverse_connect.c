/* tests/unit/test_reverse_connect.c
 *
 * Server-initiated TCP connections + ReverseHello (RHE) per OPC-10000-6 §7.1.3 / §7.1.2.6.
 */

#include "muc_opcua/encoding.h"
#include "muc_opcua/platform.h"
#include "muc_opcua/server.h"
#include "muc_opcua/status.h"
#include <stddef.h>
#include <string.h>
#include <unity.h>

#ifdef MUC_OPCUA_REVERSE_CONNECT
#include "../../src/core/tcp_connection.h"
#endif

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

/* Capture the first message written to the outbound connection (the ReverseHello). */
static opcua_byte_t g_write_capture[512];
static size_t g_write_capture_len;
static int g_write_called;

static opcua_statuscode_t mock_write(void *ctx, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    (void)ctx;
    (void)h;
    if (g_write_called == 0 && len <= sizeof(g_write_capture)) {
        (void)memcpy(g_write_capture, buf, len);
        g_write_capture_len = len;
    }
    g_write_called++;
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
    cfg->application_uri = "urn:muc-opcua:server";
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

/* Reverse connect needs a ServerUri (ApplicationUri) for the ReverseHello. */
void test_reverse_connect_requires_application_uri(void) {
    opcua_byte_t rx[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx[MU_MIN_CHUNK_SIZE];
    mu_server_config_t cfg;
    build_valid_config(&cfg, rx, sizeof(rx), tx, sizeof(tx));
    build_tcp_adapter(&cfg.tcp_adapter);
    cfg.reverse_connect_url = "opc.tcp://192.168.1.100:4840";
    cfg.application_uri = NULL;

    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, mu_server_config_validate(&cfg));
}

/* FR-1: the ReverseHello encoder produces a spec RHEF message (OPC-10000-6 §7.1.2.6):
   'R','H','E','F', declared MessageSize == actual length, then ServerUri + EndpointUrl. */
void test_reverse_hello_encoder_produces_valid_rhe(void) {
    mu_server_config_t cfg;
    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.application_uri = "urn:muc-opcua:server";
    cfg.endpoint_url = "opc.tcp://localhost:4840";

    opcua_byte_t buf[256];
    size_t len = sizeof(buf);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_tcp_create_reverse_hello(&cfg, buf, &len));

    TEST_ASSERT_EQUAL_UINT8('R', buf[0]);
    TEST_ASSERT_EQUAL_UINT8('H', buf[1]);
    TEST_ASSERT_EQUAL_UINT8('E', buf[2]);
    TEST_ASSERT_EQUAL_UINT8('F', buf[3]);
    opcua_uint32_t declared = (opcua_uint32_t)buf[4] | ((opcua_uint32_t)buf[5] << 8) | ((opcua_uint32_t)buf[6] << 16) |
                              ((opcua_uint32_t)buf[7] << 24);
    TEST_ASSERT_EQUAL_UINT32((opcua_uint32_t)len, declared);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf + 8, len - 8);
    mu_string_t server_uri, endpoint;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&r, &server_uri));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint));
    TEST_ASSERT_EQUAL_INT(20, server_uri.length); /* "urn:muc-opcua:server" */
    TEST_ASSERT_EQUAL_MEMORY("urn:muc-opcua:server", server_uri.data, 20);
    TEST_ASSERT_EQUAL_INT(24, endpoint.length); /* "opc.tcp://localhost:4840" */
    TEST_ASSERT_EQUAL_MEMORY("opc.tcp://localhost:4840", endpoint.data, 24);
    TEST_ASSERT_EQUAL_size_t(len - 8, r.position); /* body fully consumed */
}

/* FR-2: a server-initiated connection sends the ReverseHello as its FIRST message
   (OPC-10000-6 §7.1.3) — not the previous broken behaviour of connecting then waiting. */
void test_reverse_connect_emits_reverse_hello_first(void) {
    opcua_byte_t rx[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx[MU_MIN_CHUNK_SIZE];
    mu_server_config_t cfg;
    build_valid_config(&cfg, rx, sizeof(rx), tx, sizeof(tx));
    build_tcp_adapter(&cfg.tcp_adapter);
    cfg.reverse_connect_url = "opc.tcp://192.168.1.100:4840";

    g_connect_called = 0;
    g_write_called = 0;
    g_write_capture_len = 0;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &cfg, &server));

    TEST_ASSERT_EQUAL_INT(1, g_connect_called);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, g_write_capture_len);
    /* First bytes on the wire are an RHEF ReverseHello. */
    TEST_ASSERT_EQUAL_UINT8('R', g_write_capture[0]);
    TEST_ASSERT_EQUAL_UINT8('H', g_write_capture[1]);
    TEST_ASSERT_EQUAL_UINT8('E', g_write_capture[2]);
    TEST_ASSERT_EQUAL_UINT8('F', g_write_capture[3]);

    mu_server_close(server);
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
    RUN_TEST(test_reverse_connect_requires_application_uri);
    RUN_TEST(test_reverse_hello_encoder_produces_valid_rhe);
    RUN_TEST(test_reverse_connect_emits_reverse_hello_first);
#else
    RUN_TEST(test_reverse_connect_requires_build_flag);
#endif
    return UNITY_END();
}
