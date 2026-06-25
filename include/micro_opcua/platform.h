/* include/micro_opcua/platform.h */
#ifndef MICRO_OPCUA_PLATFORM_H
#define MICRO_OPCUA_PLATFORM_H

#include "micro_opcua/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TCP Transport Adapter Interface
 * Provides non-blocking networking primitives.
 */
typedef struct mu_tcp_adapter {
    void *context;
    
    /* Initialize the listening endpoint */
    opcua_statuscode_t (*listen)(void *context, const char *endpoint_url);
    
    /* Accept a new connection */
    opcua_statuscode_t (*accept)(void *context, void **connection_handle);
    
    /* Read from connection (non-blocking) */
    opcua_statuscode_t (*read)(void *context, void *connection_handle, opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read);
    
    /* Write to connection (non-blocking) */
    opcua_statuscode_t (*write)(void *context, void *connection_handle, const opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_written);
    
    /* Close a connection */
    void (*close_connection)(void *context, void *connection_handle);
    
    /* Shut down the adapter */
    void (*shutdown)(void *context);
} mu_tcp_adapter_t;

/*
 * Time Adapter Interface
 * Provides UTC time and monotonic ticks.
 */
typedef struct mu_time_adapter {
    void *context;
    
    /* Get current UTC time (100-ns intervals since 1601-01-01) */
    opcua_datetime_t (*get_time)(void *context);
    
    /* Get monotonic millisecond tick count for timeouts */
    opcua_uint64_t (*get_tick_ms)(void *context);
} mu_time_adapter_t;

/*
 * Entropy Adapter Interface
 * Provides cryptographically secure random bytes (required for session nonces).
 */
typedef struct mu_entropy_adapter {
    void *context;
    
    /* Generate secure random bytes */
    opcua_statuscode_t (*generate_random)(void *context, opcua_byte_t *buffer, size_t length);
} mu_entropy_adapter_t;

/*
 * Persistence Adapter Interface (Optional)
 * For storing/loading certificates or keys.
 */
typedef struct mu_persistence_adapter {
    void *context;
    
    /* Read a file/object into buffer */
    opcua_statuscode_t (*read)(void *context, const char *key, opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read);
    
    /* Write buffer to file/object */
    opcua_statuscode_t (*write)(void *context, const char *key, const opcua_byte_t *buffer, size_t length);
} mu_persistence_adapter_t;

/*
 * Crypto Adapter Interface (Optional)
 * For cryptographic operations if SecurityPolicy > None.
 */
typedef struct mu_crypto_adapter {
    void *context;
    /* Minimal interface placeholder for future expansion */
    opcua_statuscode_t (*verify_signature)(void *context, const mu_bytestring_t *data, const mu_bytestring_t *signature, const mu_bytestring_t *public_key);
} mu_crypto_adapter_t;

#ifdef __cplusplus
}
#endif

#endif /* MICRO_OPCUA_PLATFORM_H */
