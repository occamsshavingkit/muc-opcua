/* tests/unit/test_time_sync.c
 *
 * OPC-10000-4 §A.2 (TimestampsToReturn) — ServerTimestamp in response headers.
 * Tests that response headers carry the server's UTC time when MUC_OPCUA_TIME_SYNC
 * is enabled, and zero when it is disabled.
 *
 * Strategy: directly test write_response_prefix and mu_write_service_fault with
 * a fully initialized server config.
 */
#include "core/service_dispatch.h"
#include "core/service_dispatch/common.h"
#include "fake_platform.h"
#include "mock_transport.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Custom time adapter that returns a known non-zero timestamp.
 * OPC UA datetime is 100-ns intervals since 1601-01-01.
 * 132537600000000000 = 2021-01-01 00:00:00 UTC in 100ns ticks. */
static opcua_datetime_t test_known_time(void *context) {
    (void)context;
    return 132537600000000000LL;
}

static opcua_uint64_t test_tick_ms(void *context) {
    (void)context;
    return 1000;
}

/* Parses the ServerTimestamp (int64_t after the response type NodeId)
 * from a response body. Returns the timestamp or -1 on error. */
static opcua_int64_t parse_response_timestamp(const opcua_byte_t *buf, size_t len) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    mu_nodeid_t type;
    if (mu_binary_read_nodeid(&r, &type) != MU_STATUS_GOOD) {
        return -1;
    }
    opcua_int64_t ts = 0;
    if (mu_binary_read_int64(&r, &ts) != MU_STATUS_GOOD) {
        return -1;
    }
    return ts;
}

/* Sets up a fully populated server config using the mock transport and
 * the custom time adapter. */
static void setup_server_config(mu_server_config_t *config, mock_t *mock) {
    (void)memset(config, 0, sizeof(*config));
    config->endpoint_url = "opc.tcp://host:4840";
    config->application_uri = "urn:test";
    config->product_uri = "urn:test";
    config->application_name = "test";
    static opcua_byte_t rx[8192], tx[8192];
    config->receive_buffer = rx;
    config->receive_buffer_size = sizeof(rx);
    config->send_buffer = tx;
    config->send_buffer_size = sizeof(tx);
    config->max_chunk_count = 1;
    config->max_message_size = 8192;
    config->max_sessions = 1;
    config->max_secure_channels = 1;

    config->time_adapter.get_time = test_known_time;
    config->time_adapter.get_tick_ms = test_tick_ms;
    fake_platform_init(&config->tcp_adapter, NULL, &config->entropy_adapter);

    (void)memset(mock, 0, sizeof(*mock));
    config->tcp_adapter.context = mock;
    config->tcp_adapter.listen = mock_listen;
    config->tcp_adapter.accept = mock_accept;
    config->tcp_adapter.read = mock_read;
    config->tcp_adapter.write = mock_write;
    config->tcp_adapter.close_connection = mock_close;
    config->tcp_adapter.shutdown = mock_shutdown;
}

static void test_time_sync_write_response_prefix_populated(void) {
    mock_t mock;
    mu_server_config_t config;
    setup_server_config(&config, &mock);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    opcua_statuscode_t s = mu_server_init(storage, sizeof(storage), &config, &server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_NOT_NULL(server);

    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    s = write_response_prefix(&w, MU_ID_READRESPONSE, 42, MU_STATUS_GOOD, server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    opcua_int64_t ts = parse_response_timestamp(buf, w.position);

#if MUC_OPCUA_TIME_SYNC
    TEST_ASSERT_EQUAL_INT64_MESSAGE(test_known_time(NULL), ts,
                                    "ServerTimestamp should match time adapter when TIME_SYNC is on");
#else
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, ts, "ServerTimestamp should be zero when TIME_SYNC is off");
#endif

    mu_server_close(server);
}

static void test_time_sync_service_fault_populated(void) {
    mock_t mock;
    mu_server_config_t config;
    setup_server_config(&config, &mock);

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    opcua_statuscode_t s = mu_server_init(storage, sizeof(storage), &config, &server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_NOT_NULL(server);

    opcua_byte_t buf[256];
    size_t len = sizeof(buf);

    s = mu_write_service_fault(buf, &len, 42, MU_STATUS_BAD_SERVICEUNSUPPORTED, server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_GREATER_THAN(0, len);

    opcua_int64_t ts = parse_response_timestamp(buf, len);

#if MUC_OPCUA_TIME_SYNC
    TEST_ASSERT_EQUAL_INT64_MESSAGE(test_known_time(NULL), ts,
                                    "ServerTimestamp should match time adapter in ServiceFault");
#else
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, ts, "ServerTimestamp should be zero in ServiceFault when TIME_SYNC is off");
#endif

    mu_server_close(server);
}

static void test_time_sync_null_server_produces_zero_timestamp(void) {
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    opcua_statuscode_t s = write_response_prefix(&w, MU_ID_READRESPONSE, 42, MU_STATUS_GOOD, NULL);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    opcua_int64_t ts = parse_response_timestamp(buf, w.position);
    TEST_ASSERT_EQUAL_INT64(0, ts);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_time_sync_write_response_prefix_populated);
    RUN_TEST(test_time_sync_service_fault_populated);
    RUN_TEST(test_time_sync_null_server_produces_zero_timestamp);
    return UNITY_END();
}
