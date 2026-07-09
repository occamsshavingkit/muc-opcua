/* tests/integration/test_single_client_limit.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/server_internal.h"
#include <string.h>

typedef struct {
    int accept_calls;
    void *client_handle;
    void *second_handle;
    bool read_called;
    bool write_called;
    bool close_called;
    opcua_byte_t written_data[256];
    size_t written_len;
} mock_tcp_t;

static opcua_statuscode_t mock_accept(void *context, void **handle) {
    mock_tcp_t *mock = (mock_tcp_t *)context;
    mock->accept_calls++;
    if (mock->accept_calls == 1) {
        *handle = mock->client_handle;
        return MU_STATUS_GOOD;
    } else if (mock->accept_calls == 2) {
        *handle = mock->second_handle;
        return MU_STATUS_GOOD;
    }
    return MU_STATUS_BAD_INTERNALERROR; /* no more connections */
}

static opcua_statuscode_t mock_read(void *context, void *handle, opcua_byte_t *buffer, size_t length,
                                    size_t *bytes_read) {
    mock_tcp_t *mock = (mock_tcp_t *)context;
    mock->read_called = true;
    if (handle == mock->second_handle) {
        if (length >= 8) {
            buffer[0] = 'H';
            buffer[1] = 'E';
            buffer[2] = 'L';
            buffer[3] = 'F';
            buffer[4] = 32;
            buffer[5] = 0;
            buffer[6] = 0;
            buffer[7] = 0;
            *bytes_read = 8;
            return MU_STATUS_GOOD;
        }
    } else if (handle == mock->client_handle) {
        *bytes_read = 0;
        return MU_STATUS_GOOD;
    }
    return MU_STATUS_BAD_INTERNALERROR; /* Stop polling */
}

static opcua_statuscode_t mock_write(void *context, void *handle, const opcua_byte_t *buffer, size_t length,
                                     size_t *bytes_written) {
    mock_tcp_t *mock = (mock_tcp_t *)context;
    mock->write_called = true;
    if (handle == mock->second_handle) {
        if (length < sizeof(mock->written_data)) {
            (void)memcpy(mock->written_data, buffer, length);
            mock->written_len = length;
            *bytes_written = length;
        }
    }
    return MU_STATUS_GOOD;
}

#include "fake_platform.h"

static void mock_close(void *context, void *handle) {
    mock_tcp_t *mock = (mock_tcp_t *)context;
    if (handle == mock->second_handle) {
        mock->close_called = true;
    }
}

static opcua_statuscode_t mock_listen(void *context, const char *url) {
    (void)context;
    (void)url;
    return MU_STATUS_GOOD;
}

static void mock_shutdown(void *context) {
    (void)context;
}

void test_single_client_limit_second_connection(void) {
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    /* Under multiple connections build, connection limit is MU_INTERN_MAX_CONNECTIONS (4), not 1 */
    return;
#endif
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));

    mock_tcp_t mock_ctx;
    (void)memset(&mock_ctx, 0, sizeof(mock_ctx));
    mock_ctx.client_handle = (void *)1;
    mock_ctx.second_handle = (void *)2;

    config.endpoint_url = "opc.tcp://localhost:4840";
    opcua_byte_t rx_buf[8192];
    opcua_byte_t tx_buf[8192];
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;

    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);

    config.tcp_adapter.context = &mock_ctx;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    mu_server_init(storage, sizeof(storage), &config, &server);

    /* First poll: accepts the first connection */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL_PTR(mock_ctx.client_handle, server->client_handle);
    TEST_ASSERT_EQUAL(1, mock_ctx.accept_calls);

    /* Second poll: first connection is active, so it will check for a second connection and reject it */
    mu_server_poll(server);
    TEST_ASSERT_EQUAL(2, mock_ctx.accept_calls);
    TEST_ASSERT_TRUE(mock_ctx.read_called);
    TEST_ASSERT_TRUE(mock_ctx.write_called);
    TEST_ASSERT_TRUE(mock_ctx.close_called);

    /* The server should remain connected to the first client */
    TEST_ASSERT_EQUAL_PTR(mock_ctx.client_handle, server->client_handle);

    /* The second client should have received an ERR message */
    TEST_ASSERT_GREATER_THAN(8, mock_ctx.written_len);
    TEST_ASSERT_EQUAL('E', mock_ctx.written_data[0]);
    TEST_ASSERT_EQUAL('R', mock_ctx.written_data[1]);
    TEST_ASSERT_EQUAL('R', mock_ctx.written_data[2]);
    TEST_ASSERT_EQUAL('F', mock_ctx.written_data[3]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_single_client_limit_second_connection);
    return UNITY_END();
}
