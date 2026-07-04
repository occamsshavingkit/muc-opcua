/* tests/unit/test_server_config.c */

#include "fake_platform.h"
#include "muc_opcua/server.h"
#include "muc_opcua/status.h"
#include <string.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static opcua_statuscode_t stub_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_accept(void *context, void **handle) {
    (void)context;
    *handle = NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_read(void *context, void *handle, opcua_byte_t *buffer, size_t capacity,
                                    size_t *bytes_read) {
    (void)context;
    (void)handle;
    (void)buffer;
    (void)capacity;
    *bytes_read = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_write(void *context, void *handle, const opcua_byte_t *buffer, size_t length,
                                     size_t *bytes_written) {
    (void)context;
    (void)handle;
    (void)buffer;
    *bytes_written = length;
    return MU_STATUS_GOOD;
}

static void stub_close(void *context, void *handle) {
    (void)context;
    (void)handle;
}

static void stub_shutdown(void *context) {
    (void)context;
}

/* Builds a config that mu_server_config_validate() accepts as-is; each test
   perturbs exactly the field(s) under test. */
static void build_valid_config(mu_server_config_t *config, opcua_byte_t *rx_buf, size_t rx_buf_size,
                               opcua_byte_t *tx_buf, size_t tx_buf_size) {
    (void)memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://localhost:4840";
    config->receive_buffer = rx_buf;
    config->receive_buffer_size = rx_buf_size;
    config->send_buffer = tx_buf;
    config->send_buffer_size = tx_buf_size;
    config->max_chunk_count = MU_DEFAULT_MAX_CHUNK_COUNT;
    config->max_message_size = MU_DEFAULT_MAX_MESSAGE_SIZE;
    config->max_sessions = MU_MAX_SESSIONS;
    config->max_secure_channels = MU_MAX_SECURE_CHANNELS;
    config->tcp_adapter.listen = stub_listen;
    config->tcp_adapter.accept = stub_accept;
    config->tcp_adapter.read = stub_read;
    config->tcp_adapter.write = stub_write;
    config->tcp_adapter.close_connection = stub_close;
    config->tcp_adapter.shutdown = stub_shutdown;
    fake_platform_init(NULL, &config->time_adapter, &config->entropy_adapter);
}

void test_server_config_null_buffers(void) {
    mu_server_config_t config;
    opcua_byte_t rx_buf[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx_buf[MU_MIN_CHUNK_SIZE];
    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    config.receive_buffer = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, mu_server_config_validate(&config));

    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));
    config.send_buffer = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, mu_server_config_validate(&config));
}

void test_server_config_invalid_endpoint(void) {
    mu_server_config_t config;
    opcua_byte_t rx_buf[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx_buf[MU_MIN_CHUNK_SIZE];
    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    config.endpoint_url = "http://localhost:4840"; /* wrong scheme */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPENDPOINTURLINVALID, mu_server_config_validate(&config));

    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));
    config.endpoint_url = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPENDPOINTURLINVALID, mu_server_config_validate(&config));
}

/* Regression coverage for the dead-config-field bug: raising config.max_sessions
   above the compiled MU_MAX_SESSIONS ceiling must be rejected, not silently
   accepted and then silently ignored by CreateSession. At the compiled ceiling
   itself, the request must still succeed (this is not a stricter-than-before
   rejection of valid configs, only a new rejection of previously-silently-
   ignored ones). */
void test_server_config_accepts_max_sessions_at_compiled_limit(void) {
    mu_server_config_t config;
    opcua_byte_t rx_buf[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx_buf[MU_MIN_CHUNK_SIZE];
    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    config.max_sessions = MU_MAX_SESSIONS;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_config_validate(&config));
}

void test_server_config_rejects_max_sessions_exceeding_compiled_limit(void) {
    mu_server_config_t config;
    opcua_byte_t rx_buf[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx_buf[MU_MIN_CHUNK_SIZE];
    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    config.max_sessions = MU_MAX_SESSIONS + 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, mu_server_config_validate(&config));
}

/* Same regression coverage for max_secure_channels vs. MU_MAX_CONNECTIONS (the
   structural ceiling: each connection owns exactly one secure channel). */
void test_server_config_accepts_max_secure_channels_at_compiled_limit(void) {
    mu_server_config_t config;
    opcua_byte_t rx_buf[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx_buf[MU_MIN_CHUNK_SIZE];
    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    config.max_secure_channels = MU_MAX_CONNECTIONS;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_config_validate(&config));
}

void test_server_config_rejects_max_secure_channels_exceeding_compiled_limit(void) {
    mu_server_config_t config;
    opcua_byte_t rx_buf[MU_MIN_CHUNK_SIZE];
    opcua_byte_t tx_buf[MU_MIN_CHUNK_SIZE];
    build_valid_config(&config, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    config.max_secure_channels = MU_MAX_CONNECTIONS + 1;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, mu_server_config_validate(&config));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_server_config_null_buffers);
    RUN_TEST(test_server_config_invalid_endpoint);
    RUN_TEST(test_server_config_accepts_max_sessions_at_compiled_limit);
    RUN_TEST(test_server_config_rejects_max_sessions_exceeding_compiled_limit);
    RUN_TEST(test_server_config_accepts_max_secure_channels_at_compiled_limit);
    RUN_TEST(test_server_config_rejects_max_secure_channels_exceeding_compiled_limit);
    return UNITY_END();
}
