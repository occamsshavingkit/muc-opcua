/* examples/minimal_server/main.c */
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include "micro_opcua/micro_opcua.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "static_address_space.h"

#include "../../src/platform/host_tcp_adapter.h"
#ifdef MICRO_OPCUA_HAVE_OPENSSL
#include "../../src/platform/host_crypto_adapter.h"
#endif

#define STORAGE_BYTES 4096
static opcua_byte_t g_server_storage[STORAGE_BYTES];

static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];

static volatile int g_running = 1;

static void sigint_handler(int sig) {
    (void)sig;
    g_running = 0;
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

    signal(SIGINT, sigint_handler);

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

    status = mu_host_tcp_adapter_init(&config.tcp_adapter);
    if (status != MU_STATUS_GOOD) {
        printf("Failed to init TCP adapter\n");
        return 1;
    }

    config.time_adapter.get_time = stub_get_time;
    config.time_adapter.get_tick_ms = stub_get_tick_ms;

    config.entropy_adapter.generate_random = stub_generate_random;

#ifdef MICRO_OPCUA_HAVE_OPENSSL
    /* Enable SecurityPolicy Basic256Sha256 by supplying a crypto adapter. The host
       adapter generates a self-signed instance certificate; the server then also
       advertises Sign and SignAndEncrypt endpoints alongside None. */
    static mu_crypto_adapter_t crypto;
    if (mu_host_crypto_adapter_init(&crypto) == MU_STATUS_GOOD) {
        config.crypto_adapter = &crypto;
        printf("SecurityPolicy Basic256Sha256 enabled (self-signed certificate)\n");
    }
#endif

    config.address_space = &g_minimal_address_space;

    printf("Initializing Micro OPC UA Server...\n");
    status = mu_server_init(g_server_storage, sizeof(g_server_storage), &config, &server);
    if (status != MU_STATUS_GOOD) {
        printf("Failed to initialize server: %s\n", mu_status_name(status));
        return 1;
    }

    printf("Server initialized successfully. Listening on %s\n", config.endpoint_url);

    /* Main loop */
    while (g_running) {
        mu_server_poll(server);
        usleep(10000); /* 10ms sleep to avoid 100% CPU on non-blocking sockets */
    }
    
    printf("\nShutting down...\n");
    mu_server_close(server);
    return 0;
}
