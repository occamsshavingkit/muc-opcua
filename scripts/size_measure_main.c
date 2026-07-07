/* scripts/size_measure_main.c — minimal main() that calls mu_server_init()
 * to force the linker to pull in library symbols. --gc-sections strips
 * unreferenced code, so the measured ELF reflects actual linked size. */

#include "muc_opcua/server.h"
#include "muc_opcua/types.h"
#include <stddef.h>
#include <string.h>

static opcua_byte_t rx_buf[256];
static opcua_byte_t tx_buf[256];

static opcua_statuscode_t fake_listen(void *ctx, const char *url) {
    (void)ctx;
    (void)url;
    return 0;
}
static opcua_statuscode_t fake_accept(void *ctx, void **ch) {
    (void)ctx;
    (void)ch;
    return 0;
}
static opcua_statuscode_t fake_read(void *ctx, void *ch, opcua_byte_t *b, size_t len, size_t *n) {
    (void)ctx;
    (void)ch;
    (void)b;
    (void)len;
    *n = 0;
    return 0;
}
static opcua_statuscode_t fake_write(void *ctx, void *ch, const opcua_byte_t *b, size_t len, size_t *n) {
    (void)ctx;
    (void)ch;
    (void)b;
    (void)len;
    *n = 0;
    return 0;
}
static void fake_close(void *ctx, void *ch) {
    (void)ctx;
    (void)ch;
}
static void fake_shutdown(void *ctx) {
    (void)ctx;
}
static opcua_statuscode_t fake_connect(void *ctx, const char *url, void **ch) {
    (void)ctx;
    (void)url;
    (void)ch;
    return 0;
}
static opcua_uint64_t fake_time(void *ctx) {
    (void)ctx;
    return 0;
}
static opcua_uint64_t fake_tick(void *ctx) {
    (void)ctx;
    return 0;
}
static opcua_statuscode_t fake_random(void *ctx, opcua_byte_t *buf, size_t len) {
    (void)ctx;
    (void)buf;
    (void)len;
    return 0;
}

int main(void) {
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.endpoint_url = "opc.tcp://localhost:4840";
    cfg.receive_buffer = rx_buf;
    cfg.receive_buffer_size = sizeof(rx_buf);
    cfg.send_buffer = tx_buf;
    cfg.send_buffer_size = sizeof(tx_buf);
    cfg.max_sessions = 1;
    cfg.max_secure_channels = 1;
    cfg.max_chunk_count = 1;
    cfg.max_message_size = 256;
    cfg.tcp_adapter.listen = fake_listen;
    cfg.tcp_adapter.accept = fake_accept;
    cfg.tcp_adapter.read = fake_read;
    cfg.tcp_adapter.write = fake_write;
    cfg.tcp_adapter.close_connection = fake_close;
    cfg.tcp_adapter.shutdown = fake_shutdown;
    cfg.tcp_adapter.connect = (opcua_statuscode_t(*)(void *, const char *, void **))fake_connect;
    cfg.time_adapter.get_time = (opcua_datetime_t(*)(void *))fake_time;
    cfg.time_adapter.get_tick_ms = fake_tick;
    cfg.entropy_adapter.generate_random = fake_random;
    mu_server_t *srv = NULL;
    mu_server_init(storage, sizeof(storage), &cfg, &srv);
    (void)srv;
    return 0;
}
