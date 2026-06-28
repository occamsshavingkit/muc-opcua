/* tests/unit/test_connection_multiplex.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "fake_platform.h"
#include <string.h>

#include "../../src/core/server_internal.h"

void setUp(void) {}
void tearDown(void) {}

typedef struct {
    void *handles[8];
    int handle_count;
    int next_handle;
    int close_count;
    void *closed_handles[8];
    int write_count;
    opcua_byte_t last_write_buf[256];
    size_t last_write_len;
} mock_tcp_t;

static opcua_statuscode_t mock_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static void mock_shutdown(void *context) {
    (void)context;
}

static void mock_close(void *context, void *handle) {
    mock_tcp_t *tcp = (mock_tcp_t *)context;
    if (tcp->close_count < 8) {
        tcp->closed_handles[tcp->close_count++] = handle;
    }
    for (int i = 0; i < tcp->handle_count; ++i) {
        if (tcp->handles[i] == handle) {
            tcp->handles[i] = NULL;
        }
    }
}

static opcua_statuscode_t mock_accept(void *context, void **handle) {
    mock_tcp_t *tcp = (mock_tcp_t *)context;
    if (tcp->next_handle < tcp->handle_count) {
        *handle = tcp->handles[tcp->next_handle++];
    } else {
        *handle = NULL;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_read(void *context, void *handle, opcua_byte_t *buffer, size_t capacity, size_t *bytes_read) {
    (void)context;
    (void)handle;
    (void)buffer;
    (void)capacity;
    *bytes_read = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_write(void *context, void *handle, const opcua_byte_t *buffer, size_t len, size_t *bytes_written) {
    mock_tcp_t *tcp = (mock_tcp_t *)context;
    (void)handle;
    tcp->write_count++;
    if (len <= sizeof(tcp->last_write_buf)) {
        memcpy(tcp->last_write_buf, buffer, len);
        tcp->last_write_len = len;
    }
    *bytes_written = len;
    return MU_STATUS_GOOD;
}

void test_connection_limits(void) {
#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
    mock_tcp_t tcp;
    memset(&tcp, 0, sizeof(tcp));
    
    tcp.handles[0] = (void *)0x10;
    tcp.handles[1] = (void *)0x20;
    tcp.handles[2] = (void *)0x30;
    tcp.handles[3] = (void *)0x40;
    tcp.handles[4] = (void *)0x50;
    tcp.handle_count = 5;
    tcp.next_handle = 0;

    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_sessions = 4;
    config.max_secure_channels = 4;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.endpoint_url = "opc.tcp://localhost:4840";
    static opcua_byte_t rx_buf[8192];
    static opcua_byte_t tx_buf[8192];
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = &tcp;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    union {
        opcua_byte_t bytes[MU_SERVER_STORAGE_BYTES + 2048];
    } storage;
    mu_server_t *server = NULL;
    opcua_statuscode_t status = mu_server_init(storage.bytes, sizeof(storage), &config, &server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    /* Poll to accept connection 1 (0x10) */
    status = mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_PTR((void *)0x10, server->conns[0].client_handle);

    /* Poll to accept connection 2 (0x20) */
    status = mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_PTR((void *)0x20, server->conns[1].client_handle);

    /* Poll to accept connection 3 (0x30) */
    status = mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_PTR((void *)0x30, server->conns[2].client_handle);

    /* Poll to accept connection 4 (0x40) */
    status = mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_PTR((void *)0x40, server->conns[3].client_handle);

    /* Now the server is full (MU_MAX_CONNECTIONS = 4).
       Next poll should reject connection 5 (0x50).
       Let's verify that close_connection is called for 0x50. */
    status = mu_server_poll(server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    int closed_50 = 0;
    for (int i = 0; i < tcp.close_count; ++i) {
        if (tcp.closed_handles[i] == (void *)0x50) {
            closed_50 = 1;
        }
    }
    TEST_ASSERT_EQUAL(1, closed_50);

    mu_server_close(server);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_connection_limits);
    return UNITY_END();
}
