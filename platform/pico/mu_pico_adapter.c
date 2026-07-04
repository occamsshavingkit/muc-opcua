#include "mu_pico_adapter.h"
#include "muc_opcua/status.h"
#include <string.h>

/*
 * Pico SDK specifics are included here (never in the public muc_opcua headers)
 * so the portable core stays platform-agnostic.
 *   - pico/time.h        : time_us_64(), to_ms_since_boot(), get_absolute_time()
 *   - hardware/structs/rosc.h : rosc_hw->randombit, the ring-oscillator TRNG bit
 */
#include "hardware/structs/rosc.h"
#include "pico/time.h"

static opcua_statuscode_t pico_tcp_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t pico_tcp_accept(void *context, void **connection_handle) {
    (void)context;
    (void)connection_handle;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t pico_tcp_read(void *context, void *connection_handle, opcua_byte_t *buffer,
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

static opcua_statuscode_t pico_tcp_write(void *context, void *connection_handle, const opcua_byte_t *buffer,
                                         size_t buffer_size, size_t *bytes_written) {
    (void)context;
    (void)connection_handle;
    (void)buffer;
    if (bytes_written) {
        *bytes_written = buffer_size;
    }
    return MU_STATUS_GOOD;
}

static void pico_tcp_close_connection(void *context, void *connection_handle) {
    (void)context;
    (void)connection_handle;
}

static void pico_tcp_shutdown(void *context) {
    (void)context;
}

/* OPC UA DateTime is a 64-bit count of 100 ns intervals. The RP2040 has no RTC
   with a wall-clock battery, so this is monotonic time since boot expressed in
   100 ns units (1 us = 10 ticks). It is non-zero and strictly increasing, which
   is what the server's timestamps and timeout logic require; integrators that
   need true UTC must supply their own get_time backed by an RTC/NTP source. */
static opcua_datetime_t pico_get_time(void *context) {
    (void)context;
    return (opcua_datetime_t)(time_us_64() * 10u);
}

/* Monotonic milliseconds since boot; drives idle/session timeouts. */
static opcua_uint64_t pico_get_tick_ms(void *context) {
    (void)context;
    return (opcua_uint64_t)to_ms_since_boot(get_absolute_time());
}

/* Hardware entropy from the RP2040 ring oscillator (ROSC). rosc_hw->randombit
   yields one physically-random bit per read; sampling with a short delay between
   reads decorrelates successive bits. This replaces the previous all-zero stub,
   which made every nonce and derived session key predictable. Only reached at
   handshake (nonce/key generation), so the per-byte sampling cost is acceptable.
   NOTE: ROSC is a raw TRNG; a production build may prefer to seed a health-tested
   DRBG from it, but raw ROSC is vastly stronger than the constant it replaces. */
static opcua_statuscode_t pico_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    for (size_t i = 0; i < length; ++i) {
        opcua_byte_t b = 0;
        for (int bit = 0; bit < 8; ++bit) {
            b = (opcua_byte_t)((b << 1) | (rosc_hw->randombit & 1u));
            /* Short portable delay so successive ROSC samples decorrelate; a
               volatile counter is not optimized away and needs no SDK symbol. */
            for (volatile int d = 0; d < 20; ++d) {
            }
        }
        buffer[i] = b;
    }
    return MU_STATUS_GOOD;
}

void mu_pico_adapter_init(mu_tcp_adapter_t *tcp_adapter, mu_time_adapter_t *time_adapter,
                          (void)mu_entropy_adapter_t *entropy_adapter) {
    if (tcp_adapter) {
        (void)memset(tcp_adapter, 0, sizeof(*tcp_adapter));
        tcp_adapter->listen = pico_tcp_listen;
        tcp_adapter->accept = pico_tcp_accept;
        tcp_adapter->read = pico_tcp_read;
        tcp_adapter->write = pico_tcp_write;
        tcp_adapter->close_connection = pico_tcp_close_connection;
        tcp_adapter->shutdown = pico_tcp_shutdown;
    }

(void)    if (time_adapter) {
        (void)memset(time_adapter, 0, sizeof(*time_adapter));
        time_adapter->get_time = pico_get_time;
        time_adapter->get_tick_ms = pico_get_tick_ms;
    }

(void)    if (entropy_adapter) {
        (void)memset(entropy_adapter, 0, sizeof(*entropy_adapter));
        entropy_adapter->generate_random = pico_generate_random;
    }
}
