/* include/micro_opcua/server.h */
#ifndef MICRO_OPCUA_SERVER_H
#define MICRO_OPCUA_SERVER_H

#include "micro_opcua/platform.h"
#include "micro_opcua/config.h"
#include "micro_opcua/status.h"
#include "micro_opcua/address_space.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque Server State
 * Allocated by the user (usually statically) to avoid heap usage.
 */
typedef struct mu_server mu_server_t;

/* Server Configuration */
typedef struct {
    /* Identity and Discovery */
    const char *endpoint_url;
    const char *application_uri;
    const char *product_uri;
    const char *application_name;

    /* Buffers for networking (caller-owned to avoid heap) */
    opcua_byte_t *receive_buffer;
    size_t receive_buffer_size;
    opcua_byte_t *send_buffer;
    size_t send_buffer_size;

    /* Limits */
    opcua_uint32_t max_chunk_count;
    opcua_uint32_t max_message_size;
    opcua_uint32_t max_sessions;
    opcua_uint32_t max_secure_channels;

    /* Platform Adapters */
    mu_tcp_adapter_t tcp_adapter;
    mu_time_adapter_t time_adapter;
    mu_entropy_adapter_t entropy_adapter;
    
    /* Optional Adapters */
    mu_persistence_adapter_t *persistence_adapter; /* NULL if unsupported */
    mu_crypto_adapter_t *crypto_adapter;           /* NULL if unsupported */
    
    /* Static Address Space (optional) */
    const mu_address_space_t *address_space;
    
} mu_server_config_t;

/*
 * Initialize the server with the given config and static storage.
 * Storage must be at least MU_SERVER_STORAGE_BYTES bytes.
 */
opcua_statuscode_t mu_server_init(void *storage, size_t storage_size, const mu_server_config_t *config, mu_server_t **out_server);

/*
 * Run a single non-blocking iteration of the server.
 * This handles accepting connections, reading, parsing, dispatching, and writing.
 */
opcua_statuscode_t mu_server_poll(mu_server_t *server);

/*
 * Cleanly close all connections and shutdown the server.
 */
void mu_server_close(mu_server_t *server);

/*
 * Validation API for configuration.
 */
opcua_statuscode_t mu_server_config_validate(const mu_server_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* MICRO_OPCUA_SERVER_H */
