#include "unity.h"
#include "micro_opcua/server.h"
#include "micro_opcua/pubsub.h"
#include "fake_platform.h"
#include "../../src/core/server_internal.h"
#include <string.h>
#include <stdio.h>

static opcua_uint64_t mock_time_ms = 0;
static size_t udp_send_called = 0;

static opcua_uint64_t mock_get_tick_ms(void *context) {
    (void)context;
    return mock_time_ms;
}

static opcua_statuscode_t mock_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context; (void)buffer; (void)length; return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_udp_send(void *context, const opcua_byte_t *buffer, size_t buffer_size, const char *address, uint16_t port) {
    (void)context;
    (void)buffer;
    (void)buffer_size;
    (void)address;
    (void)port;
    udp_send_called++;
    return MU_STATUS_GOOD;
}

void setUp(void) {
    mock_time_ms = 0;
    udp_send_called = 0;
}
void tearDown(void) {}

void test_pubsub_poll_timing(void) {
    union {
        opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES + 2048];
        struct mu_server align_server;
    } storage;
    mu_server_config_t config = {0};
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.time_adapter.get_tick_ms = mock_get_tick_ms;
    config.time_adapter.get_time = mock_get_tick_ms; // dummy
    config.udp_adapter.send = mock_udp_send;
    config.pubsub.enabled = true;
    config.pubsub.port = 4840;
    config.pubsub.publisher_id = 1234;
    
    opcua_byte_t recv_buf[8192];
    config.receive_buffer = recv_buf;
    config.receive_buffer_size = sizeof(recv_buf);
    opcua_byte_t send_buf[8192];
    config.send_buffer = send_buf;
    config.send_buffer_size = sizeof(send_buf);

    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;

    fake_platform_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);
    
    config.time_adapter.get_tick_ms = mock_get_tick_ms;

    if (config.endpoint_url == NULL || strncmp(config.endpoint_url, "opc.tcp://", 10) != 0) printf("FAIL endpoint\n");
    if (config.receive_buffer == NULL || config.receive_buffer_size < MU_MIN_CHUNK_SIZE) printf("FAIL recv\n");
    if (config.send_buffer == NULL || config.send_buffer_size < MU_MIN_CHUNK_SIZE) printf("FAIL send\n");
    if (config.max_sessions == 0 || config.max_secure_channels == 0) printf("FAIL max\n");
    if (config.max_chunk_count == 0 || config.max_message_size < MU_MIN_CHUNK_SIZE) printf("FAIL max chunk\n");
    if (config.tcp_adapter.listen == NULL || config.tcp_adapter.accept == NULL ||
        config.tcp_adapter.read == NULL || config.tcp_adapter.write == NULL ||
        config.tcp_adapter.close_connection == NULL || config.tcp_adapter.shutdown == NULL) printf("FAIL tcp\n");
    if (config.time_adapter.get_time == NULL || config.time_adapter.get_tick_ms == NULL) printf("FAIL time\n");
    if (config.entropy_adapter.generate_random == NULL) printf("FAIL entropy\n");
    
    mu_server_t *server = NULL;
    opcua_statuscode_t status = mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);

    mu_pubsub_writer_group_t wg = {
        .writer_group_id = 1,
        .publishing_interval_ms = 1000,
        .dataset_writer = { .data_set_writer_id = 1 }
    };
    
    status = mu_server_add_writer_group(server, &wg);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);

    // Initial poll: should NOT trigger since 0 - 0 < 1000
    mu_pubsub_poll(server);
    TEST_ASSERT_EQUAL_UINT32(0, udp_send_called);

    // Poll again at 500ms, should not trigger
    mock_time_ms = 500;
    mu_pubsub_poll(server);
    TEST_ASSERT_EQUAL_UINT32(0, udp_send_called);

    // Poll at 1000ms, should trigger
    mock_time_ms = 1000;
    mu_pubsub_poll(server);
    TEST_ASSERT_EQUAL_UINT32(1, udp_send_called);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pubsub_poll_timing);
    return UNITY_END();
}
