/* tests/unit/test_address_space_dynamic.c */
#include "../../src/core/server_internal.h"
#include "../../src/services/browse.h"
#include "micro_opcua/server.h"
#include "unity.h"
#include <string.h>

static mu_server_t server;
static union {
    _Alignas(8) opcua_byte_t bytes[sizeof(mu_server_t) + 128];
} storage;
static mu_server_config_t config;

static opcua_statuscode_t mock_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_accept(void *context, void **client_handle) {
    (void)context;
    (void)client_handle;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_read(void *context, void *client_handle, opcua_byte_t *buffer, size_t size,
                                    size_t *bytes_read) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    (void)size;
    (void)bytes_read;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_write(void *context, void *client_handle, const opcua_byte_t *buffer, size_t size,
                                     size_t *bytes_written) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    (void)size;
    (void)bytes_written;
    return MU_STATUS_GOOD;
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
    return 0;
}
static opcua_uint64_t mock_get_tick(void *context) {
    (void)context;
    return 0;
}
static opcua_statuscode_t mock_generate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    memset(buffer, 0, length);
    return MU_STATUS_GOOD;
}

static opcua_byte_t rx_buf[8192];
static opcua_byte_t tx_buf[8192];

static mu_node_t static_nodes[1];
static mu_address_space_t static_space;

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
    config.entropy_adapter.generate_random = mock_generate_random;

    /* Static node */
    memset(&static_nodes[0], 0, sizeof(mu_node_t));
    static_nodes[0].node_id.identifier_type = MU_NODEID_NUMERIC;
    static_nodes[0].node_id.namespace_index = 1;
    static_nodes[0].node_id.identifier.numeric = 42;
    static_nodes[0].node_class = MU_NODECLASS_VARIABLE;

    static_space.nodes = static_nodes;
    static_space.node_count = 1;
    config.address_space = &static_space;

    mu_server_t *out_server;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage.bytes, sizeof(storage.bytes), &config, &out_server));
    memcpy(&server, out_server, sizeof(mu_server_t));
}

void tearDown(void) {}

void test_ResolveNode_DynamicFallback(void) {
    /* Setup dynamic node */
    server.dynamic_address_space.nodes_count = 1;
    mu_node_t *dyn_node = &server.dynamic_address_space.nodes[0];
    memset(dyn_node, 0, sizeof(mu_node_t));
    dyn_node->node_id.identifier_type = MU_NODEID_NUMERIC;
    dyn_node->node_id.namespace_index = 1;
    dyn_node->node_id.identifier.numeric = 99;
    dyn_node->node_class = MU_NODECLASS_OBJECT;

    /* Create address space view for dynamic */
    mu_address_space_t dyn_space = {.nodes = server.dynamic_address_space.nodes,
                                    .node_count = server.dynamic_address_space.nodes_count};

    mu_nodeid_t id_static = {1, MU_NODEID_NUMERIC, {42}};
    mu_nodeid_t id_dyn = {1, MU_NODEID_NUMERIC, {99}};
    mu_nodeid_t id_base = {0, MU_NODEID_NUMERIC, {85}}; /* ObjectsFolder */
    mu_nodeid_t id_missing = {1, MU_NODEID_NUMERIC, {1234}};

    const mu_node_t *res;
    mu_address_space_index_t user_idx = {0};

    /* 1. Resolve static */
    res = mu_resolve_node(server.config.address_space, &user_idx, &dyn_space, &id_static);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_EQUAL(42, res->node_id.identifier.numeric);

    /* 2. Resolve dynamic */
    res = mu_resolve_node(server.config.address_space, &user_idx, &dyn_space, &id_dyn);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_EQUAL(99, res->node_id.identifier.numeric);

    /* 3. Resolve base */
    res = mu_resolve_node(server.config.address_space, &user_idx, &dyn_space, &id_base);
    TEST_ASSERT_NOT_NULL(res);
    TEST_ASSERT_EQUAL(85, res->node_id.identifier.numeric);

    /* 4. Missing */
    res = mu_resolve_node(server.config.address_space, &user_idx, &dyn_space, &id_missing);
    TEST_ASSERT_NULL(res);
}

void test_Browse_DynamicReferences(void) {
    /* Add a dynamic reference */
    server.dynamic_address_space.references_count = 1;
    mu_dynamic_reference_t *dyn = &server.dynamic_address_space.references[0];

    dyn->source_node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {42}};        /* From static node */
    dyn->ref.reference_type_id = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {35}}; /* Organizes */
    dyn->ref.is_forward = true;
    dyn->ref.target_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {99}}; /* To dynamic node */

    /* Add dynamic node 99 and 42 */
    server.dynamic_address_space.nodes_count = 2;
    mu_node_t *dyn_node = &server.dynamic_address_space.nodes[0];
    memset(dyn_node, 0, sizeof(mu_node_t));
    dyn_node->node_id.identifier_type = MU_NODEID_NUMERIC;
    dyn_node->node_id.namespace_index = 1;
    dyn_node->node_id.identifier.numeric = 99;
    dyn_node->node_class = MU_NODECLASS_OBJECT;

    mu_node_t *dyn_node2 = &server.dynamic_address_space.nodes[1];
    memset(dyn_node2, 0, sizeof(mu_node_t));
    dyn_node2->node_id.identifier_type = MU_NODEID_NUMERIC;
    dyn_node2->node_id.namespace_index = 1;
    dyn_node2->node_id.identifier.numeric = 42;
    dyn_node2->node_class = MU_NODECLASS_OBJECT;

    mu_address_space_t dyn_space = {.nodes = server.dynamic_address_space.nodes,
                                    .node_count = server.dynamic_address_space.nodes_count};

    mu_browse_description_t req_desc;
    req_desc.node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {42}};
    req_desc.browse_direction = MU_BROWSE_DIRECTION_FORWARD;
    req_desc.reference_type_id = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {33}}; /* HierarchicalReferences */
    req_desc.include_subtypes = true;
    req_desc.node_class_mask = 0xFFFFFFFF;
    req_desc.result_mask = 0x3F;

    mu_browse_request_t req;
    req.requested_max_references_per_node = 0;
    req.num_nodes_to_browse = 1;
    req.nodes_to_browse = &req_desc;

    mu_reference_description_t refs[10];
    mu_browse_result_t res[1];

    extern opcua_statuscode_t mu_browse_process_with_user_index(
        const mu_address_space_t *address_space, const mu_address_space_index_t *user_index,
        const mu_address_space_t *dynamic,
#ifdef MICRO_OPCUA_SERVICE_NODEMANAGEMENT
        const mu_dynamic_reference_t *dyn_refs, size_t dyn_refs_count,
#endif
        const mu_browse_request_t *req, mu_browse_result_t *results, size_t max_results,
        mu_reference_description_t *ref_pool, size_t max_total_refs);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process_with_user_index(NULL, NULL, &dyn_space,
#ifdef MICRO_OPCUA_SERVICE_NODEMANAGEMENT
                                                                        server.dynamic_address_space.references,
                                                                        server.dynamic_address_space.references_count,
#endif
                                                                        &req, res, 1, refs, 10));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, res[0].status_code);
    TEST_ASSERT_EQUAL(1, res[0].num_references);
    TEST_ASSERT_EQUAL(99, res[0].references[0].node_id.identifier.numeric);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ResolveNode_DynamicFallback);
    RUN_TEST(test_Browse_DynamicReferences);
    return UNITY_END();
}
