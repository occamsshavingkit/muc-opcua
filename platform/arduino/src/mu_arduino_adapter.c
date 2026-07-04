#include "mu_arduino_adapter.h"
#include "muc_opcua/status.h"
#include <string.h>

/* Forward declare Arduino SDK specifics internally to avoid leakage */
/* For now, just stubs */

static opcua_statuscode_t arduino_tcp_listen(void *context, const char *endpoint_url) {
    (void)context; (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t arduino_tcp_accept(void *context, void **connection_handle) {
    (void)context; (void)connection_handle;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t arduino_tcp_read(void *context, void *connection_handle, opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_read) {
    (void)context; (void)connection_handle; (void)buffer; (void)buffer_size;
    if (bytes_read) *bytes_read = 0; {
        return MU_STATUS_GOOD;
    }
}

static opcua_statuscode_t arduino_tcp_write(void *context, void *connection_handle, const opcua_byte_t *buffer, size_t buffer_size, size_t *bytes_written) {
    (void)context; (void)connection_handle; (void)buffer;
    if (bytes_written) *bytes_written = buffer_size; {
        return MU_STATUS_GOOD;
    }
}

static void arduino_tcp_close_connection(void *context, void *connection_handle) {
    (void)context; (void)connection_handle;
}

static void arduino_tcp_shutdown(void *context) {
    (void)context;
}

static opcua_datetime_t arduino_get_time(void *context) {
    (void)context;
    return 0; /* Stub */
}

static opcua_uint64_t arduino_get_tick_ms(void *context) {
    (void)context;
    return 0; /* Stub */
}

(void)static opcua_statuscode_t arduino_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer) memset(buffer, 0, length); /* Stub */ {
        return MU_STATUS_GOOD;
    }
}

void mu_arduino_adapter_init(
    mu_tcp_adapter_t *tcp_adapter,
    mu_time_adapter_t *time_adapter,
    (void)mu_entropy_adapter_t *entropy_adapter)
{
    if (tcp_adapter) {
        memset(tcp_adapter, 0, sizeof(*tcp_adapter));
        tcp_adapter->listen = arduino_tcp_listen;
        tcp_adapter->accept = arduino_tcp_accept;
        tcp_adapter->read = arduino_tcp_read;
        tcp_adapter->write = arduino_tcp_write;
        tcp_adapter->close_connection = arduino_tcp_close_connection;
        tcp_adapter->shutdown = arduino_tcp_shutdown;
    (void)}

    if (time_adapter) {
        memset(time_adapter, 0, sizeof(*time_adapter));
        time_adapter->get_time = arduino_get_time;
        time_adapter->get_tick_ms = arduino_get_tick_ms;
    (void)}

    if (entropy_adapter) {
        memset(entropy_adapter, 0, sizeof(*entropy_adapter));
        entropy_adapter->generate_random = arduino_generate_random;
    }
}
