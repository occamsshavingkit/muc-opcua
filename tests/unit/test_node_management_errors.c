/* tests/unit/test_node_management_errors.c */
#include "../../src/core/server_internal.h"
#include "../../src/services/node_management.h"
#include "micro_opcua/server.h"
#include "unity.h"
#include <string.h>

static mu_server_t server;
static union {
    _Alignas(8) opcua_byte_t bytes[sizeof(mu_server_t) + 128];
} storage;
static opcua_byte_t rx_buf[8192];
static opcua_byte_t tx_buf[8192];
static mu_server_config_t config;

static opcua_statuscode_t mock_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_accept(void *context, void **client_handle) {
    (void)context;
    (void)client_handle;
    return MU_STATUS_BAD_NOTSUPPORTED;
}
static opcua_statuscode_t mock_read(void *context, void *client_handle, opcua_byte_t *buffer, size_t size,
                                    size_t *bytes_read) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    (void)size;
    (void)bytes_read;
    return MU_STATUS_BAD_NOTSUPPORTED;
}
static opcua_statuscode_t mock_write(void *context, void *client_handle, const opcua_byte_t *buffer, size_t size,
                                     size_t *bytes_written) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    (void)size;
    (void)bytes_written;
    return MU_STATUS_BAD_NOTSUPPORTED;
}
static void mock_close(void *context, void *client_handle) {
    (void)context;
    (void)client_handle;
}
static void mock_shutdown(void *context) {
    (void)context;
}
static opcua_datetime_t mock_get_time(void *context) {
    (void)context;
    return 133400000000000000ULL;
}
static opcua_uint64_t mock_get_tick(void *context) {
    (void)context;
    return 1000;
}
static opcua_statuscode_t mock_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    memset(buffer, 0xAA, length);
    return MU_STATUS_GOOD;
}

void setUp(void) {
    memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    config.max_sessions = 2;
    config.max_secure_channels = 2;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.allow_node_management = true;

    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    config.time_adapter.get_time = mock_get_time;
    config.time_adapter.get_tick_ms = mock_get_tick;

    config.entropy_adapter.generate_random = mock_generate_random;

    mu_server_t *out_server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &out_server));
    memcpy(&server, out_server, sizeof(mu_server_t));
}

void tearDown(void) {}

void test_AddNodes_OOM(void) {
    /* Fill address space */
    server.dynamic_address_space.nodes_count = MU_MAX_DYNAMIC_NODES;

    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    mu_binary_write_int32(&w, 1); /* Count = 1 */

    mu_nodeid_t pid = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &pid);
    mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
    mu_binary_write_nodeid(&w, &rid);
    mu_expanded_nodeid_t newid = {{1, MU_NODEID_NUMERIC, {1000}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &newid);
    mu_qualified_name_t bn = {1, {6, (const opcua_byte_t *)"MyNode"}};
    mu_binary_write_qualified_name(&w, &bn);
    mu_binary_write_uint32(&w, 1); /* Object */

    mu_nodeid_t attr_type = {0, MU_NODEID_NUMERIC, {251}};
    mu_binary_write_nodeid(&w, &attr_type);
    mu_binary_write_byte(&w, 1);
    mu_binary_write_int32(&w, 0);

    mu_expanded_nodeid_t tdef = {{0, MU_NODEID_NUMERIC, {58}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &tdef);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    opcua_byte_t out_buf[512];
    mu_binary_writer_t w_out;
    mu_binary_writer_init(&w_out, out_buf, sizeof(out_buf));

    opcua_statuscode_t s = mu_add_nodes_process(&server, &r, &w_out);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    mu_binary_reader_t r_out;
    mu_binary_reader_init(&r_out, out_buf, w_out.position);
    opcua_int32_t count = 0;
    mu_binary_read_int32(&r_out, &count);
    TEST_ASSERT_EQUAL(1, count);

    opcua_statuscode_t res_status;
    mu_binary_read_uint32(&r_out, &res_status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_OUTOFMEMORY, res_status);
}

void test_AddNodes_InvalidNodeClass(void) {
    server.dynamic_address_space.nodes_count = 0;

    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    mu_binary_write_int32(&w, 1); /* Count = 1 */

    mu_nodeid_t pid = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &pid);
    mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
    mu_binary_write_nodeid(&w, &rid);
    mu_expanded_nodeid_t newid = {{1, MU_NODEID_NUMERIC, {1000}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &newid);
    mu_qualified_name_t bn = {1, {6, (const opcua_byte_t *)"MyNode"}};
    mu_binary_write_qualified_name(&w, &bn);
    mu_binary_write_uint32(&w, 0); /* INVALID NODE CLASS */

    mu_nodeid_t attr_type = {0, MU_NODEID_NUMERIC, {251}};
    mu_binary_write_nodeid(&w, &attr_type);
    mu_binary_write_byte(&w, 1);
    mu_binary_write_int32(&w, 0);

    mu_expanded_nodeid_t tdef = {{0, MU_NODEID_NUMERIC, {58}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &tdef);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    opcua_byte_t out_buf[512];
    mu_binary_writer_t w_out;
    mu_binary_writer_init(&w_out, out_buf, sizeof(out_buf));

    opcua_statuscode_t s = mu_add_nodes_process(&server, &r, &w_out);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    mu_binary_reader_t r_out;
    mu_binary_reader_init(&r_out, out_buf, w_out.position);
    opcua_int32_t count = 0;
    mu_binary_read_int32(&r_out, &count);
    TEST_ASSERT_EQUAL(1, count);

    opcua_statuscode_t res_status;
    mu_binary_read_uint32(&r_out, &res_status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NODECLASSINVALID, res_status);
}

void test_AddReferences_OOM(void) {
    server.dynamic_address_space.references_count = MU_MAX_DYNAMIC_REFERENCES;

    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    mu_binary_write_int32(&w, 1); /* Count = 1 */
    mu_nodeid_t sid = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &sid);
    mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
    mu_binary_write_nodeid(&w, &rid);
    mu_binary_write_byte(&w, 1);
    mu_string_t srv = {-1, NULL};
    mu_binary_write_string(&w, &srv);
    mu_expanded_nodeid_t tid = {{1, MU_NODEID_NUMERIC, {1000}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &tid);
    mu_binary_write_uint32(&w, 1); /* targetNodeClass */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    opcua_byte_t out_buf[512];
    mu_binary_writer_t w_out;
    mu_binary_writer_init(&w_out, out_buf, sizeof(out_buf));

    opcua_statuscode_t s = mu_add_references_process(&server, &r, &w_out);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    mu_binary_reader_t r_out;
    mu_binary_reader_init(&r_out, out_buf, w_out.position);
    opcua_int32_t count = 0;
    mu_binary_read_int32(&r_out, &count);
    TEST_ASSERT_EQUAL(1, count);

    opcua_statuscode_t res_status;
    mu_binary_read_uint32(&r_out, &res_status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_OUTOFMEMORY, res_status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_AddNodes_OOM);
    RUN_TEST(test_AddNodes_InvalidNodeClass);
    RUN_TEST(test_AddReferences_OOM);
    return UNITY_END();
}
