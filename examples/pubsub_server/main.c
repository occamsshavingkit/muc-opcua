#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include "../../src/platform/host_tcp_adapter.h"
#include "muc_opcua/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern opcua_statuscode_t mu_host_udp_init(void *context, uint16_t port);
extern opcua_statuscode_t mu_host_udp_send(void *context, const opcua_byte_t *buffer, size_t buffer_size,
                                           const char *address, uint16_t port);
extern void mu_host_udp_shutdown(void *context);

static opcua_datetime_t stub_get_time(void *context) {
    (void)context;
    return 0;
}

#include <time.h>
static opcua_uint64_t stub_get_tick_ms(void *context) {
    (void)context;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (opcua_uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static opcua_statuscode_t stub_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer) {
        (void)memset(buffer, 0xAA, length);
    }
    return MU_STATUS_GOOD;
}

#define STORAGE_SIZE MU_SERVER_STORAGE_BYTES
static opcua_byte_t server_storage[STORAGE_SIZE] __attribute__((aligned(8)));
static opcua_byte_t receive_buffer[8192];
static opcua_byte_t send_buffer[8192];

int main(void) {
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));

    config.endpoint_url = "opc.tcp://localhost:4840";
    config.receive_buffer = receive_buffer;
    config.receive_buffer_size = sizeof(receive_buffer);
    config.send_buffer = send_buffer;
    config.send_buffer_size = sizeof(send_buffer);

    config.max_sessions = MU_MAX_SESSIONS;
    config.max_secure_channels = MU_MAX_SECURE_CHANNELS;
    config.max_chunk_count = MU_DEFAULT_MAX_CHUNK_COUNT;
    config.max_message_size = MU_DEFAULT_MAX_MESSAGE_SIZE;

    mu_host_tcp_adapter_init(&config.tcp_adapter);

    config.time_adapter.get_time = stub_get_time;
    config.time_adapter.get_tick_ms = stub_get_tick_ms;
    config.entropy_adapter.generate_random = stub_generate_random;

    int udp_fd = -1;
    mu_host_udp_init(&udp_fd, 4840);

    config.pubsub.enabled = true;
    config.pubsub.port = 4840;
    config.pubsub.publisher_id = 0x12345678;
    config.pubsub.address = "255.255.255.255";

    config.udp_adapter.context = &udp_fd;
    config.udp_adapter.init = mu_host_udp_init;
    config.udp_adapter.send = mu_host_udp_send;
    config.udp_adapter.shutdown = mu_host_udp_shutdown;

    mu_server_t *server = NULL;
    opcua_statuscode_t status = mu_server_init(server_storage, sizeof(server_storage), &config, &server);
    if (status != MU_STATUS_GOOD) {
        (void)printf("Failed to init server: %x\n", status);
        return 1;
    }

    static mu_pubsub_field_t fields[] = {
        {.value = {MU_TYPE_DOUBLE, {.d = 23.5}}},
    };
    mu_pubsub_writer_group_t wg = {.writer_group_id = 1,
                                   .publishing_interval_ms = 1000,
                                   .dataset_writer = {.data_set_writer_id = 1, .fields = fields, .field_count = 1}};
    mu_server_add_writer_group(server, &wg);

    (void)printf("PubSub server started. Broadcasting UADP packets every 1 second...\n");

    while (true) {
        mu_server_poll(server);
        usleep(10000); // 10ms tick
    }

    mu_server_close(server);
    return 0;
}
