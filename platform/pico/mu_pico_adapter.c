/* platform/pico/mu_pico_adapter.c */
#include "mu_pico_adapter.h"
#include <string.h>

/* 
 * Forward declare Pico SDK specifics internally to avoid leakage 
 * into the public micro_opcua headers.
 */

static opcua_statuscode_t pico_tcp_listen(void *context, const char *endpoint_url) {
    (void)context; (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t pico_tcp_accept(void *context, void **connection_handle) {
    (void)context; (void)connection_handle;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t pico_tcp_read(void *context, void *connection_handle, opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read) {
    (void)context; (void)connection_handle; (void)buffer; (void)buffer_size;
    if (bytes_read) *bytes_read = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t pico_tcp_write(void *context, void *connection_handle, const opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_written) {
    (void)context; (void)connection_handle; (void)buffer;
    if (bytes_written) *bytes_written = buffer_size;
    return MU_STATUS_GOOD;
}

static void pico_tcp_close_connection(void *context, void *connection_handle) {
    (void)context; (void)connection_handle;
}

static void pico_tcp_shutdown(void *context) {
    (void)context;
}

static opcua_datetime_t pico_get_time(void *context) {
    (void)context;
    return 0; /* Stub */
}

static opcua_uint64_t pico_get_tick_ms(void *context) {
    (void)context;
    return 0; /* Stub */
}

static opcua_statuscode_t pico_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer) memset(buffer, 0, length); /* Stub */
    return MU_STATUS_GOOD;
}

void mu_pico_adapter_init(
    mu_tcp_adapter_t *tcp_adapter,
    mu_time_adapter_t *time_adapter,
    mu_entropy_adapter_t *entropy_adapter)
{
    if (tcp_adapter) {
        memset(tcp_adapter, 0, sizeof(*tcp_adapter));
        tcp_adapter->listen = pico_tcp_listen;
        tcp_adapter->accept = pico_tcp_accept;
        tcp_adapter->read = pico_tcp_read;
        tcp_adapter->write = pico_tcp_write;
        tcp_adapter->close_connection = pico_tcp_close_connection;
        tcp_adapter->shutdown = pico_tcp_shutdown;
    }

    if (time_adapter) {
        memset(time_adapter, 0, sizeof(*time_adapter));
        time_adapter->get_time = pico_get_time;
        time_adapter->get_tick_ms = pico_get_tick_ms;
    }

    if (entropy_adapter) {
        memset(entropy_adapter, 0, sizeof(*entropy_adapter));
        entropy_adapter->generate_random = pico_generate_random;
    }
}
