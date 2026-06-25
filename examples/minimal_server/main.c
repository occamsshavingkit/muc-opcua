/* examples/minimal_server/main.c */
#include "micro_opcua/micro_opcua.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "static_address_space.h"

#define STORAGE_BYTES 4096
static opcua_byte_t g_server_storage[STORAGE_BYTES];

static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];

/* Stub platform adapters for host example without network behavior */
static opcua_statuscode_t stub_tcp_listen(void *context, const char *endpoint_url) {
    (void)context;
    printf("Listening on %s...\n", endpoint_url);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_tcp_accept(void *context, void **connection_handle) {
    (void)context;
    (void)connection_handle;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_tcp_read(void *context, void *connection_handle, opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read) {
    (void)context; (void)connection_handle; (void)buffer; (void)buffer_size;
    if (bytes_read) *bytes_read = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t stub_tcp_write(void *context, void *connection_handle, const opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_written) {
    (void)context; (void)connection_handle; (void)buffer;
    if (bytes_written) *bytes_written = buffer_size;
    return MU_STATUS_GOOD;
}

static void stub_tcp_close_connection(void *context, void *connection_handle) {
    (void)context; (void)connection_handle;
}

static void stub_tcp_shutdown(void *context) {
    (void)context;
}

static opcua_datetime_t stub_get_time(void *context) {
    (void)context; return 0;
}
static opcua_uint64_t stub_get_tick_ms(void *context) {
    (void)context; return 0;
}
static opcua_statuscode_t stub_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer) memset(buffer, 0xAA, length);
    return MU_STATUS_GOOD;
}

int main(void)
{
    mu_server_config_t config;
    mu_server_t *server = NULL;
    opcua_statuscode_t status;

    memset(&config, 0, sizeof(config));

    config.endpoint_url = "opc.tcp://localhost:4840";
    config.application_uri = "urn:localhost:micro_opcua:minimal_server";
    config.product_uri = "urn:micro_opcua:minimal_server";
    config.application_name = "Minimal Micro OPC UA Server";

    config.receive_buffer = g_recv_buffer;
    config.receive_buffer_size = sizeof(g_recv_buffer);
    config.send_buffer = g_send_buffer;
    config.send_buffer_size = sizeof(g_send_buffer);

    config.max_chunk_count = MU_DEFAULT_MAX_CHUNK_COUNT;
    config.max_message_size = MU_DEFAULT_MAX_MESSAGE_SIZE;
    config.max_sessions = MU_MAX_SESSIONS;
    config.max_secure_channels = MU_MAX_SECURE_CHANNELS;

    config.tcp_adapter.listen = stub_tcp_listen;
    config.tcp_adapter.accept = stub_tcp_accept;
    config.tcp_adapter.read = stub_tcp_read;
    config.tcp_adapter.write = stub_tcp_write;
    config.tcp_adapter.close_connection = stub_tcp_close_connection;
    config.tcp_adapter.shutdown = stub_tcp_shutdown;

    config.time_adapter.get_time = stub_get_time;
    config.time_adapter.get_tick_ms = stub_get_tick_ms;

    config.entropy_adapter.generate_random = stub_generate_random;

    config.address_space = &g_minimal_address_space;

    printf("Initializing Micro OPC UA Server...\n");
    status = mu_server_init(g_server_storage, sizeof(g_server_storage), &config, &server);
    if (status != MU_STATUS_GOOD) {
        printf("Failed to initialize server: %s\n", mu_status_name(status));
        return 1;
    }

    printf("Server initialized successfully.\n");

    /* Main loop (mock) */
    printf("Entering main loop. Polling once...\n");
    mu_server_poll(server);
    
    printf("Shutting down...\n");
    mu_server_close(server);
    return 0;
}
