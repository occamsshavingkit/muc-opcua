#include "fake_platform.h"
#include "muc_opcua/status.h"
#include <string.h>

static opcua_statuscode_t fake_tcp_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t fake_tcp_accept(void *context, void **connection_handle) {
    (void)context;
    (void)connection_handle;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t fake_tcp_read(void *context, void *connection_handle, opcua_byte_t *buffer,
                                        size_t buffer_size, size_t *bytes_read) {
    (void)context;
    (void)connection_handle;
    (void)buffer;
    (void)buffer_size;
    if (bytes_read) {
        *bytes_read = 0;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t fake_tcp_write(void *context, void *connection_handle, const opcua_byte_t *buffer,
                                         size_t buffer_size, size_t *bytes_written) {
    (void)context;
    (void)connection_handle;
    (void)buffer;
    if (bytes_written) {
        *bytes_written = buffer_size;
    }
    return MU_STATUS_GOOD;
}

static void fake_tcp_close_connection(void *context, void *connection_handle) {
    (void)context;
    (void)connection_handle;
}

static void fake_tcp_shutdown(void *context) {
    (void)context;
}

static opcua_datetime_t fake_get_time(void *context) {
    (void)context;
    return 0; /* Return fixed time for tests */
}

static opcua_uint64_t fake_get_tick_ms(void *context) {
    (void)context;
    return 0; /* Return fixed tick for tests */
}

static opcua_statuscode_t fake_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer) {
        (void)memset(buffer, 0x42, length); /* Fixed randomness for tests */
    }
    return MU_STATUS_GOOD;
}

void fake_platform_init(mu_tcp_adapter_t *tcp_adapter, mu_time_adapter_t *time_adapter,
                        mu_entropy_adapter_t *entropy_adapter) {
    if (tcp_adapter) {
        (void)memset(tcp_adapter, 0, sizeof(*tcp_adapter));
        tcp_adapter->listen = fake_tcp_listen;
        tcp_adapter->accept = fake_tcp_accept;
        tcp_adapter->read = fake_tcp_read;
        tcp_adapter->write = fake_tcp_write;
        tcp_adapter->close_connection = fake_tcp_close_connection;
        tcp_adapter->shutdown = fake_tcp_shutdown;
    }

    if (time_adapter) {
        (void)memset(time_adapter, 0, sizeof(*time_adapter));
        time_adapter->get_time = fake_get_time;
        time_adapter->get_tick_ms = fake_get_tick_ms;
    }

    if (entropy_adapter) {
        (void)memset(entropy_adapter, 0, sizeof(*entropy_adapter));
        entropy_adapter->generate_random = fake_generate_random;
    }
}
