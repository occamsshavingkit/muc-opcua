/* tests/unit/test_server_address_space_config.c
 * Feature 025 (F6): the user address-space sort index is a fixed-size
 * order[MU_MAX_ADDRESS_SPACE_NODES] array. A space larger than the cap must be
 * rejected loudly at mu_server_init rather than silently degrading to an O(n)
 * linear scan (which also made Query return nothing). */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

static mu_server_config_t config;
static opcua_byte_t rx_buf[8192];
static opcua_byte_t tx_buf[8192];
static _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];

/* One more than the cap so the over-limit case is exercised. */
static mu_node_t s_nodes[MU_MAX_ADDRESS_SPACE_NODES + 1];
static mu_address_space_t s_space;

static opcua_statuscode_t mock_listen(void *c, const char *u) {
    (void)c;
    (void)u;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_accept(void *c, void **h) {
    (void)c;
    (void)h;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_read(void *c, void *h, opcua_byte_t *b, size_t n, size_t *r) {
    (void)c;
    (void)h;
    (void)b;
    (void)n;
    (void)r;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *b, size_t n, size_t *w) {
    (void)c;
    (void)h;
    (void)b;
    (void)n;
    (void)w;
    return MU_STATUS_GOOD;
}
static void mock_close(void *c, void *h) {
    (void)c;
    (void)h;
}
static void mock_shutdown(void *c) {
    (void)c;
}
static opcua_datetime_t mock_get_time(void *c) {
    (void)c;
    return 0;
}
static opcua_uint64_t mock_get_tick(void *c) {
    (void)c;
    return 0;
}
static opcua_statuscode_t mock_random(void *c, opcua_byte_t *b, size_t n) {
    (void)c;
    memset(b, 0, n);
    return MU_STATUS_GOOD;
}

void setUp(void) {
    memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;
    config.time_adapter.get_time = mock_get_time;
    config.time_adapter.get_tick_ms = mock_get_tick;
    config.entropy_adapter.generate_random = mock_random;

    /* Distinct numeric NodeIds, no references, so validation passes the
       duplicate and unresolved-reference checks and only the size cap matters. */
    memset(s_nodes, 0, sizeof(s_nodes));
    for (size_t i = 0; i < (MU_MAX_ADDRESS_SPACE_NODES + 1); ++i) {
        s_nodes[i].node_id.identifier_type = MU_NODEID_NUMERIC;
        s_nodes[i].node_id.namespace_index = 1;
        s_nodes[i].node_id.identifier.numeric = (opcua_uint32_t)(i + 1);
        s_nodes[i].node_class = MU_NODECLASS_VARIABLE;
    }
    s_space.nodes = s_nodes;
    config.address_space = &s_space;
}
void tearDown(void) {}

/* A space at exactly the cap is fully indexable and accepted. */
void test_server_init_accepts_max_nodes(void) {
    s_space.node_count = MU_MAX_ADDRESS_SPACE_NODES;
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));
}

/* One node over the cap must fail loudly, not silently fall back to linear scan. */
void test_server_init_rejects_too_many_nodes(void) {
    s_space.node_count = MU_MAX_ADDRESS_SPACE_NODES + 1;
    mu_server_t *server = NULL;
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_server_init_accepts_max_nodes);
    RUN_TEST(test_server_init_rejects_too_many_nodes);
    return UNITY_END();
}
