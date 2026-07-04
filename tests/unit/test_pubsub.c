#define MUC_OPCUA_PUBSUB 1
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/pubsub.h"
#include "muc_opcua/server.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

static opcua_uint64_t mock_time_ms = 0;
static size_t udp_send_called = 0;
static char udp_last_address[32];
static uint16_t udp_last_port = 0;
static size_t udp_last_size = 0;
static opcua_byte_t udp_last_payload[64];

static opcua_uint64_t mock_get_tick_ms(void *context) {
    (void)context;
    return mock_time_ms;
}

static opcua_datetime_t mock_get_time(void *context) {
    (void)context;
    return 0;
}

static opcua_statuscode_t mock_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    (void)buffer;
    (void)length;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_udp_send(void *context, const opcua_byte_t *buffer, size_t buffer_size,
                                        const char *address, uint16_t port) {
    (void)context;
    udp_send_called++;
    udp_last_port = port;
    udp_last_size = buffer_size;
    if (address) {
        strncpy(udp_last_address, address, sizeof(udp_last_address) - 1u);
        udp_last_address[sizeof(udp_last_address) - 1u] = '\0';
    }
    if (buffer && buffer_size <= sizeof(udp_last_payload)) {
        (void)memcpy(udp_last_payload, buffer, buffer_size);
    }
    return MU_STATUS_GOOD;
}

void setUp(void) {
    mock_time_ms = 0;
    udp_send_called = 0;
    udp_last_address[0] = '\0';
    udp_last_port = 0;
    udp_last_size = 0;
    (void)memset(udp_last_payload, 0, sizeof(udp_last_payload));
}
void tearDown(void) {}

void test_pubsub_poll_timing(void) {
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES + 2048];
        struct mu_server align_server;
    } storage;
    mu_server_config_t config = {0};
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.time_adapter.get_tick_ms = mock_get_tick_ms;
    config.time_adapter.get_time = mock_get_time; // dummy
    config.udp_adapter.send = mock_udp_send;
    config.pubsub.enabled = true;
    config.pubsub.port = 4840;
    config.pubsub.publisher_id = 1234;
    config.pubsub.address = "239.0.0.1";

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

    if (config.endpoint_url == NULL || strncmp(config.endpoint_url, "opc.tcp://", 10) != 0) {
        (void)printf("FAIL endpoint\n");
    }
    if (config.receive_buffer == NULL || config.receive_buffer_size < MU_MIN_CHUNK_SIZE) {
        printf("FAIL recv\n");
    }
    if (config.send_buffer == NULL || config.send_buffer_size < MU_MIN_CHUNK_SIZE) {
        (void)printf("FAIL send\n");
    }
    if (config.max_sessions == 0 || config.max_secure_channels == 0) {
        printf("FAIL max\n");
    }
    if (config.max_chunk_count == 0 || config.max_message_size < MU_MIN_CHUNK_SIZE) {
        (void)printf("FAIL max chunk\n");
    }
    if (config.tcp_adapter.listen == NULL || config.tcp_adapter.accept == NULL || config.tcp_adapter.read == NULL ||
        config.tcp_adapter.write == NULL || config.tcp_adapter.close_connection == NULL ||
        config.tcp_adapter.shutdown == NULL) {
        printf("FAIL tcp\n");
    }
    if (config.time_adapter.get_time == NULL || config.time_adapter.get_tick_ms == NULL) {
        printf("FAIL time\n");
    }
    if (config.entropy_adapter.generate_random == NULL) {
        printf("FAIL entropy\n");
    }

    mu_server_t *server = NULL;
    opcua_statuscode_t status = mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);

    static const mu_pubsub_field_t fields[] = {
        {.value = {MU_TYPE_UINT32, {.ui32 = 42u}}},
    };
    mu_pubsub_writer_group_t wg = {.writer_group_id = 1,
                                   .publishing_interval_ms = 1000,
                                   .dataset_writer = {.data_set_writer_id = 1, .fields = fields, .field_count = 1}};

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
    TEST_ASSERT_EQUAL_STRING("239.0.0.1", udp_last_address);
    TEST_ASSERT_EQUAL_UINT16(4840, udp_last_port);
    TEST_ASSERT_EQUAL_UINT32(17, udp_last_size);
    TEST_ASSERT_EQUAL_HEX8(0xD1, udp_last_payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, udp_last_payload[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, udp_last_payload[6]);
    TEST_ASSERT_EQUAL_HEX8(0x06, udp_last_payload[9]);
    TEST_ASSERT_EQUAL_HEX8(0x01, udp_last_payload[11]);
    TEST_ASSERT_EQUAL_HEX8(0x07, udp_last_payload[12]);
}

void test_server_poll_drives_pubsub_without_tcp_client(void) {
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES + 2048];
        struct mu_server align_server;
    } storage;
    mu_server_config_t config = {0};
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.time_adapter.get_tick_ms = mock_get_tick_ms;
    config.time_adapter.get_time = mock_get_time;
    config.udp_adapter.send = mock_udp_send;
    config.pubsub.enabled = true;
    config.pubsub.port = 4840;
    config.pubsub.publisher_id = 1234;
    config.pubsub.address = "239.0.0.1";

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
    config.udp_adapter.send = mock_udp_send;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    static const mu_pubsub_field_t fields[] = {
        {.value = {MU_TYPE_UINT32, {.ui32 = 42u}}},
    };
    mu_pubsub_writer_group_t wg = {.writer_group_id = 1,
                                   .publishing_interval_ms = 1000,
                                   .dataset_writer = {.data_set_writer_id = 1, .fields = fields, .field_count = 1}};
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_add_writer_group(server, &wg));

    mock_time_ms = 1000;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_poll(server));

    /* OPC-10000-14 section 5.4.6.2.2 and section 7.3.2.1 describe
       broker-less UDP publishing; it must not depend on an active TCP Session. */
    TEST_ASSERT_EQUAL_UINT32(1, udp_send_called);
}

void test_pubsub_poll_uses_wrapped_32bit_tick_interval(void) {
    union {
        _Alignas(8) opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES + 2048];
        struct mu_server align_server;
    } storage;
    mu_server_config_t config = {0};
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.time_adapter.get_tick_ms = mock_get_tick_ms;
    config.time_adapter.get_time = mock_get_time;
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
    config.udp_adapter.send = mock_udp_send;

    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &server));

    static const mu_pubsub_field_t fields[] = {
        {.value = {MU_TYPE_UINT32, {.ui32 = 42u}}},
    };
    mu_pubsub_writer_group_t wg = {.writer_group_id = 1,
                                   .publishing_interval_ms = 1000,
                                   .dataset_writer = {.data_set_writer_id = 1, .fields = fields, .field_count = 1}};
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_add_writer_group(server, &wg));

    mock_time_ms = ((opcua_uint64_t)UINT32_MAX) + 1001u;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_pubsub_poll(server));
    TEST_ASSERT_EQUAL_UINT32(1, udp_send_called);

    mock_time_ms += 100u;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_pubsub_poll(server));
    TEST_ASSERT_EQUAL_UINT32(1, udp_send_called);

    mock_time_ms += 900u;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_pubsub_poll(server));
    TEST_ASSERT_EQUAL_UINT32(2, udp_send_called);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pubsub_poll_timing);
    RUN_TEST(test_server_poll_drives_pubsub_without_tcp_client);
    RUN_TEST(test_pubsub_poll_uses_wrapped_32bit_tick_interval);
    return UNITY_END();
}
