/* platform/pico/pico_minimal_server.c
 *
 * Minimal RP2040/Pico example: brings up the muc-opcua server with the Pico
 * platform adapters and a tiny static address space.
 *
 * Adapter status (see mu_pico_adapter.c):
 *   - Entropy: REAL. Backed by the RP2040 ROSC hardware TRNG, so nonces and
 *     derived session keys are unpredictable. (Previously an all-zero stub —
 *     never deploy the old stub with security enabled.)
 *   - Time: REAL, monotonic since boot (no RTC, so not wall-clock UTC).
 *   - TCP: STILL A SKELETON. The network primitives are no-ops; a real TCP/IP
 *     stack (lwIP, e.g. pico_cyw43_arch_lwip_* on a Pico W) must be wired before
 *     this server can accept a client. Until then this example validates the
 *     embedded cross-compile and server lifecycle only.
 *
 * DEPLOYMENT NOTES:
 *   - Because TCP is not yet wired, this example cannot complete a real
 *     handshake; when a transport is added, start with SecurityPolicy None for
 *     bring-up, then enable a secured policy once a trust list is configured
 *     (the core now fails closed on secured policies without one).
 *   - The secured handshake path (CreateSession/ActivateSession) uses several
 *     hundred bytes to ~2 KB of stack per call. The Pico SDK default
 *     PICO_STACK_SIZE is 2 KB; build secured configurations with at least
 *     `-DPICO_STACK_SIZE=0x4000` (16 KB) to leave headroom.
 */
#include "mu_pico_adapter.h"
#include "muc_opcua/muc_opcua.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

/* Tiny static address space: Objects (ns=0;i=85) organizes an Int32 variable
 * MyVar1 (ns=1;i=1000). Every reference target resolves, so validation passes. */
static const mu_reference_t s_objects_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true}};
static const mu_reference_t s_myvar1_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
static const mu_value_source_t s_myvar1_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
static const mu_node_t s_nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                     MU_NODECLASS_OBJECT,
                                     {7, (const opcua_byte_t *)"Objects"},
                                     {7, (const opcua_byte_t *)"Objects"},
                                     s_objects_refs,
                                     1,
                                     NULL},
                                    {{1, MU_NODEID_NUMERIC, {1000}},
                                     MU_NODECLASS_VARIABLE,
                                     {6, (const opcua_byte_t *)"MyVar1"},
                                     {6, (const opcua_byte_t *)"MyVar1"},
                                     s_myvar1_refs,
                                     1,
                                     &s_myvar1_value}};
static const mu_address_space_t g_pico_address_space = {s_nodes, 2};

static opcua_byte_t g_storage[MU_SERVER_STORAGE_BYTES];
static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];

int main(void) {
    stdio_init_all();

    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));

    config.endpoint_url = "opc.tcp://0.0.0.0:4840";
    config.application_uri = "urn:pico:muc_opcua:minimal_server";
    config.product_uri = "urn:muc_opcua:minimal_server";
    config.application_name = "muc-opcua Pico Server";

    config.receive_buffer = g_recv_buffer;
    config.receive_buffer_size = sizeof(g_recv_buffer);
    config.send_buffer = g_send_buffer;
    config.send_buffer_size = sizeof(g_send_buffer);

    config.max_chunk_count = MU_DEFAULT_MAX_CHUNK_COUNT;
    config.max_message_size = MU_DEFAULT_MAX_MESSAGE_SIZE;
    config.max_sessions = MU_MAX_SESSIONS;
    config.max_secure_channels = MU_MAX_SECURE_CHANNELS;

    mu_pico_adapter_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);
    config.address_space = &g_pico_address_space;

    mu_server_t *server = NULL;
    if (mu_server_init(g_storage, sizeof(g_storage), &config, &server) != MU_STATUS_GOOD) {
        (void)printf("Failed to initialize muc-opcua server\n");
        return 1;
    }

    (void)printf("muc-opcua Pico server initialized\n");

    while (true) {
        mu_server_poll(server);
        sleep_ms(10);
    }

    return 0;
}
