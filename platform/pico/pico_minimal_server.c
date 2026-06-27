/* platform/pico/pico_minimal_server.c
 *
 * Minimal RP2040/Pico example: brings up the micro-opcua server with the Pico
 * platform adapters and a tiny static address space. The Pico adapter's network
 * primitives are skeletons (no TCP/IP stack wired yet), so this builds and runs
 * the server lifecycle to validate the embedded cross-compile path.
 */
#include "micro_opcua/micro_opcua.h"
#include "mu_pico_adapter.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

/* Tiny static address space: Objects (ns=0;i=85) organizes an Int32 variable
 * MyVar1 (ns=1;i=1000). Every reference target resolves, so validation passes. */
static const mu_reference_t s_objects_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 1, MU_NODEID_NUMERIC, { 1000 } }, true }
};
static const mu_reference_t s_myvar1_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 0, MU_NODEID_NUMERIC, { 85 } }, false }
};
static const mu_value_source_t s_myvar1_value = {
    MU_VALUESOURCE_STATIC, { .static_value = { MU_TYPE_INT32, { .i32 = 42 } } }
};
static const mu_node_t s_nodes[] = {
    { { 0, MU_NODEID_NUMERIC, { 85 } }, MU_NODECLASS_OBJECT,
      { 7, (const opcua_byte_t *)"Objects" }, { 7, (const opcua_byte_t *)"Objects" },
      s_objects_refs, 1, NULL },
    { { 1, MU_NODEID_NUMERIC, { 1000 } }, MU_NODECLASS_VARIABLE,
      { 6, (const opcua_byte_t *)"MyVar1" }, { 6, (const opcua_byte_t *)"MyVar1" },
      s_myvar1_refs, 1, &s_myvar1_value }
};
static const mu_address_space_t g_pico_address_space = { s_nodes, 2 };

static opcua_byte_t g_storage[MU_SERVER_STORAGE_BYTES];
static opcua_byte_t g_recv_buffer[MU_MIN_CHUNK_SIZE];
static opcua_byte_t g_send_buffer[MU_MIN_CHUNK_SIZE];

int main(void)
{
    stdio_init_all();

    mu_server_config_t config;
    memset(&config, 0, sizeof(config));

    config.endpoint_url = "opc.tcp://0.0.0.0:4840";
    config.application_uri = "urn:pico:micro_opcua:minimal_server";
    config.product_uri = "urn:micro_opcua:minimal_server";
    config.application_name = "Micro OPC UA Pico Server";

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
        printf("Failed to initialize Micro OPC UA server\n");
        return 1;
    }

    printf("Micro OPC UA Pico server initialized\n");

    while (true) {
        mu_server_poll(server);
        sleep_ms(10);
    }

    return 0;
}
