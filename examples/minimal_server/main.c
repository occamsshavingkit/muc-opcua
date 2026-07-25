/* examples/minimal_server/main.c */
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include "static_address_space.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../src/platform/host_tcp_adapter.h"
#ifdef MUC_OPCUA_HAVE_OPENSSL
#include "../../src/platform/host_crypto_adapter.h"
#endif
#include "muc_opcua/services/alarms_conditions.h"

/* Size the no-heap storage block from the library's required size so it tracks the
 * compiled feature set (e.g. MUC_OPCUA_SECURITY adds the server-owned secure
 * scratch region). Using a hardcoded literal here silently under-sizes the block
 * when features are enabled and makes mu_server_init() return Bad_OutOfMemory. */
#define STORAGE_BYTES MU_SERVER_STORAGE_BYTES
static opcua_byte_t g_server_storage[STORAGE_BYTES];

static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];

static volatile int g_running = 1;

static void sigint_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static opcua_datetime_t stub_get_time(void *context) {
    (void)context;
    return 0;
}
static opcua_uint64_t stub_get_tick_ms(void *context) {
    (void)context;
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        return 0u;
    }
    return (opcua_uint64_t)timestamp.tv_sec * 1000u + (opcua_uint64_t)timestamp.tv_nsec / 1000000u;
}
static opcua_statuscode_t stub_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer) {
        (void)memset(buffer, 0xAA, length);
    }
    return MU_STATUS_GOOD;
}

int main(void) {
    mu_server_config_t config;
    mu_server_t *server = NULL;
    opcua_statuscode_t status;

    signal(SIGINT, sigint_handler);

    (void)memset(&config, 0, sizeof(config));

    config.endpoint_url = "opc.tcp://localhost:4840";
    config.application_uri = "urn:localhost:muc_opcua:minimal_server";
    config.product_uri = "urn:muc_opcua:minimal_server";
    config.application_name = "Minimal muc-opcua Server";

    config.receive_buffer = g_recv_buffer;
    config.receive_buffer_size = sizeof(g_recv_buffer);
    config.send_buffer = g_send_buffer;
    config.send_buffer_size = sizeof(g_send_buffer);

    config.max_chunk_count = MU_DEFAULT_MAX_CHUNK_COUNT;
    config.max_message_size = MU_DEFAULT_MAX_MESSAGE_SIZE;
    config.max_sessions = MU_INTERN_MAX_SESSIONS;
    config.max_secure_channels = MU_INTERN_MAX_SECURE_CHANNELS;

    status = mu_host_tcp_adapter_init(&config.tcp_adapter);
    if (status != MU_STATUS_GOOD) {
        (void)printf("Failed to init TCP adapter\n");
        return 1;
    }

    config.time_adapter.get_time = stub_get_time;
    config.time_adapter.get_tick_ms = stub_get_tick_ms;

    config.entropy_adapter.generate_random = stub_generate_random;

#ifdef MUC_OPCUA_HAVE_OPENSSL
    /* Enable SecurityPolicy Basic256Sha256 by supplying a crypto adapter. The host
       adapter generates a self-signed instance certificate; the server then also
       advertises Sign and SignAndEncrypt endpoints alongside None. */
    static mu_crypto_adapter_t crypto;
    if (mu_host_crypto_adapter_init(&crypto) == MU_STATUS_GOOD) {
        config.crypto_adapter = &crypto;
        /* Demo/interop only: accept clients whose freshly generated self-signed
           certificate is not in a trust list. A production server MUST instead
           supply config.trust_list and leave this false so unknown certificates
           are rejected (certificate validity is enforced either way). */
        config.allow_untrusted_clients = 1;
        (void)printf("SecurityPolicy Basic256Sha256 enabled (self-signed certificate)\n");
        (void)printf("WARNING: allow_untrusted_clients=1 (demo) — application authentication is disabled\n");
    }
#endif
    config.address_space = &g_minimal_address_space;

    (void)printf("Initializing muc-opcua Server...\n");
    status = mu_server_init(g_server_storage, sizeof(g_server_storage), &config, &server);
    if (status != MU_STATUS_GOOD) {
#ifdef MUC_OPCUA_STATUS_STRINGS
        (void)printf("Failed to initialize server: %s\n", mu_status_name(status));
#else
        (void)printf("Failed to initialize server: 0x%08X\n", (unsigned)status);
#endif
        return 1;
    }

    (void)printf("Server initialized successfully. Listening on %s\n", config.endpoint_url);

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
    {
        mu_condition_id_t cid;
        cid.node_id = (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 12345};
        if (mu_alarms_trigger_dialog(server, &cid, 0x03) == MU_STATUS_GOOD) {
            (void)printf("Triggered test dialog condition (NodeId 1:12345)\n");
        }
    }
#endif

    /* Main loop */
    while (g_running) {
        mu_server_poll(server);
        usleep(10000); /* 10ms sleep to avoid 100% CPU on non-blocking sockets */
    }

    (void)printf("\nShutting down...\n");
    mu_server_close(server);
    return 0;
}
