/* tests/unit/test_node_management.c */
#define MUC_OPCUA_SERVICE_NODEMANAGEMENT 1
#include "../../src/core/server_internal.h"
#include "../../src/services/node_management.h"
#include "muc_opcua/server.h"
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
    (void)memset(buffer, 0xAA, length);
    return MU_STATUS_GOOD;
}

void setUp(void) {
    (void)memset(&config, 0, sizeof(config));
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
    (void)memcpy(&server, out_server, sizeof(mu_server_t));
}

void tearDown(void) {}

/* Encode AddNodesRequest manually, then decode and check */
void test_AddNodesRequest_Decode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    /* Array of AddNodesItem, count 1 */
    mu_binary_write_int32(&w, 1);

    /* parentNodeId (Numeric 85, ObjectsFolder) */
    mu_nodeid_t pid = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &pid);

    /* referenceTypeId (Numeric 35, Organizes) */
    mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
    mu_binary_write_nodeid(&w, &rid);

    /* requestedNewNodeId (ExpandedNodeId) */
    mu_expanded_nodeid_t newid = {{1, MU_NODEID_NUMERIC, {1000}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &newid);

    /* browseName */
    mu_qualified_name_t bn = {1, {6, (const opcua_byte_t *)"MyNode"}};
    mu_binary_write_qualified_name(&w, &bn);

    /* nodeClass */
    mu_binary_write_uint32(&w, 1); /* Object */

    /* nodeAttributes ExtensionObject (stub: 0 length body) */
    mu_nodeid_t attr_type = {0, MU_NODEID_NUMERIC, {251}}; /* ObjectAttributes_Encoding_DefaultBinary */
    mu_binary_write_nodeid(&w, &attr_type);
    mu_binary_write_byte(&w, 1);  /* Encoding: ByteString (Length + body) */
    mu_binary_write_int32(&w, 0); /* length 0 */

    /* typeDefinition */
    mu_expanded_nodeid_t tdef = {{0, MU_NODEID_NUMERIC, {58}}, {-1, NULL}, 0}; /* BaseObjectType */
    mu_binary_write_expanded_nodeid(&w, &tdef);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    mu_add_nodes_item_t items[2];
    size_t count = 0;
    opcua_statuscode_t s = mu_add_nodes_request_decode(&r, items, 2, &count);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, items[0].parent_node_id.identifier_type);
    TEST_ASSERT_EQUAL(85, items[0].parent_node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, items[0].reference_type_id.identifier_type);
    TEST_ASSERT_EQUAL(35, items[0].reference_type_id.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, items[0].requested_new_node_id.node_id.identifier_type);
    TEST_ASSERT_EQUAL(1000, items[0].requested_new_node_id.node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(1, items[0].node_class);
    TEST_ASSERT_EQUAL(251, items[0].node_attributes_type_id.identifier.numeric);
    TEST_ASSERT_EQUAL(0, items[0].node_attributes_len);
}

void test_AddNodesResponse_Encode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    mu_add_nodes_result_t res = {MU_STATUS_GOOD, {1, MU_NODEID_NUMERIC, {1000}}};
    opcua_statuscode_t s = mu_add_nodes_response_encode(&w, &res, 1);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    opcua_int32_t count = 0;
    mu_binary_read_int32(&r, &count);
    TEST_ASSERT_EQUAL(1, count);

    opcua_uint32_t status_code;
    mu_binary_read_uint32(&r, &status_code);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status_code);

    mu_nodeid_t nid;
    mu_binary_read_nodeid(&r, &nid);
    TEST_ASSERT_EQUAL(1, nid.namespace_index);
    TEST_ASSERT_EQUAL(1000, nid.identifier.numeric);
}

void test_DeleteNodesRequest_Decode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    /* count 1 */
    mu_binary_write_int32(&w, 1);

    /* nodeId */
    mu_nodeid_t nid = {1, MU_NODEID_NUMERIC, {1000}};
    mu_binary_write_nodeid(&w, &nid);

    /* deleteTargetReferences */
    mu_binary_write_byte(&w, 1);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    mu_delete_nodes_item_t items[2];
    size_t count = 0;
    opcua_statuscode_t s = mu_delete_nodes_request_decode(&r, items, 2, &count);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, items[0].node_id.identifier_type);
    TEST_ASSERT_EQUAL(1, items[0].node_id.namespace_index);
    TEST_ASSERT_EQUAL(1000, items[0].node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(true, items[0].delete_target_references);
}

void test_DeleteNodesResponse_Encode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    opcua_statuscode_t res = MU_STATUS_GOOD;
    opcua_statuscode_t s = mu_delete_nodes_response_encode(&w, &res, 1);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    opcua_int32_t count = 0;
    mu_binary_read_int32(&r, &count);
    TEST_ASSERT_EQUAL(1, count);

    opcua_uint32_t status_code;
    mu_binary_read_uint32(&r, &status_code);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status_code);
}

/* Encode AddReferencesRequest manually, then decode and check */
void test_AddReferencesRequest_Decode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    mu_binary_write_int32(&w, 1);

    mu_nodeid_t sid = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &sid);

    mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
    mu_binary_write_nodeid(&w, &rid);

    mu_binary_write_byte(&w, 1); /* isForward */

    mu_string_t srv = {-1, NULL};
    mu_binary_write_string(&w, &srv);

    mu_expanded_nodeid_t tid = {{1, MU_NODEID_NUMERIC, {1000}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &tid);

    mu_binary_write_uint32(&w, 1); /* targetNodeClass */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    mu_add_references_item_t items[2];
    size_t count = 0;
    opcua_statuscode_t s = mu_add_references_request_decode(&r, items, 2, &count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(85, items[0].source_node_id.identifier.numeric);
    TEST_ASSERT_TRUE(items[0].is_forward);
    TEST_ASSERT_EQUAL(1000, items[0].target_node_id.node_id.identifier.numeric);
}

void test_AddReferencesResponse_Encode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    mu_add_references_result_t results[1];
    results[0].status_code = MU_STATUS_GOOD;

    opcua_statuscode_t s = mu_add_references_response_encode(&w, results, 1);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    opcua_int32_t count = 0;
    mu_binary_read_int32(&r, &count);
    TEST_ASSERT_EQUAL(1, count);

    opcua_uint32_t status_code;
    mu_binary_read_uint32(&r, &status_code);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status_code);
}

void test_DeleteReferencesRequest_Decode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    mu_binary_write_int32(&w, 1);

    mu_nodeid_t sid = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &sid);

    mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
    mu_binary_write_nodeid(&w, &rid);

    mu_binary_write_byte(&w, 1); /* isForward */

    mu_expanded_nodeid_t tid = {{1, MU_NODEID_NUMERIC, {1000}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(&w, &tid);

    mu_binary_write_byte(&w, 1); /* deleteBidirectional */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    mu_delete_references_item_t items[2];
    size_t count = 0;
    opcua_statuscode_t s = mu_delete_references_request_decode(&r, items, 2, &count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(85, items[0].source_node_id.identifier.numeric);
    TEST_ASSERT_TRUE(items[0].is_forward);
    TEST_ASSERT_TRUE(items[0].delete_bidirectional);
}

void test_DeleteReferencesResponse_Encode(void) {
    opcua_byte_t buffer[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, sizeof(buffer));

    opcua_statuscode_t results[1];
    results[0] = MU_STATUS_GOOD;

    opcua_statuscode_t s = mu_delete_references_response_encode(&w, results, 1);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buffer, w.position);

    opcua_int32_t count = 0;
    mu_binary_read_int32(&r, &count);
    TEST_ASSERT_EQUAL(1, count);

    opcua_uint32_t status_code;
    mu_binary_read_uint32(&r, &status_code);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status_code);
}

void test_DeleteReferences_MissingReference_IsBadNotFound(void) {
    opcua_byte_t request[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, request, sizeof(request));

    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t sid = {0, MU_NODEID_NUMERIC, {85}};
        mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
        mu_expanded_nodeid_t tid = {{1, MU_NODEID_NUMERIC, {1000}}, {-1, NULL}, 0};

        mu_binary_write_nodeid(&w, &sid);
        mu_binary_write_nodeid(&w, &rid);
        mu_binary_write_byte(&w, 1); /* isForward */
        mu_binary_write_expanded_nodeid(&w, &tid);
        mu_binary_write_byte(&w, 1); /* deleteBidirectional */
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    opcua_byte_t response[512];
    mu_binary_writer_t rw;
    mu_binary_writer_init(&rw, response, sizeof(response));
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, request, w.position);

    /* OPC-10000-4 section 5.8 DeleteReferences: a syntactically valid request
       for a reference that is not present returns per-item Bad_NotFound. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_delete_references_process(&server, &r, &rw));

    mu_binary_reader_t rr;
    mu_binary_reader_init(&rr, response, rw.position);
    opcua_int32_t result_count = 0;
    opcua_statuscode_t status_code = 0u;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&rr, &result_count));
    TEST_ASSERT_EQUAL(1, result_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&rr, &status_code));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTFOUND, status_code);
}

/* Feature 028 (T020/T021): AddNodes with a requestedNewNodeId that already exists
   must return Bad_NodeIdExists per-result (OPC-10000-4 §5.8 AddNodes / §7.38.2),
   not silently create a duplicate node. */
static void encode_add_nodes_one(mu_binary_writer_t *w, opcua_uint32_t new_numeric) {
    mu_binary_write_int32(w, 1); /* one AddNodesItem */
    mu_nodeid_t pid = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(w, &pid);
    mu_nodeid_t rid = {0, MU_NODEID_NUMERIC, {35}};
    mu_binary_write_nodeid(w, &rid);
    mu_expanded_nodeid_t newid = {{1, MU_NODEID_NUMERIC, {0}}, {-1, NULL}, 0};
    newid.node_id.identifier.numeric = new_numeric;
    mu_binary_write_expanded_nodeid(w, &newid);
    mu_qualified_name_t bn = {1, {6, (const opcua_byte_t *)"MyNode"}};
    mu_binary_write_qualified_name(w, &bn);
    mu_binary_write_uint32(w, 1); /* nodeClass Object */
    mu_nodeid_t attr_type = {0, MU_NODEID_NUMERIC, {251}};
    mu_binary_write_nodeid(w, &attr_type);
    mu_binary_write_byte(w, 1);  /* ExtensionObject encoding: ByteString */
    mu_binary_write_int32(w, 0); /* empty attributes body */
    mu_expanded_nodeid_t tdef = {{0, MU_NODEID_NUMERIC, {58}}, {-1, NULL}, 0};
    mu_binary_write_expanded_nodeid(w, &tdef);
}

static opcua_statuscode_t add_nodes_first_result_status(opcua_uint32_t new_numeric) {
    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    encode_add_nodes_one(&w, new_numeric);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    opcua_byte_t resp[512];
    mu_binary_writer_t rw;
    mu_binary_writer_init(&rw, resp, sizeof(resp));
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, req, w.position);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_add_nodes_process(&server, &r, &rw));

    mu_binary_reader_t rr;
    mu_binary_reader_init(&rr, resp, rw.position);
    opcua_int32_t rescount = 0;
    mu_binary_read_int32(&rr, &rescount);
    TEST_ASSERT_EQUAL(1, rescount);
    opcua_statuscode_t st = 0;
    mu_binary_read_statuscode(&rr, &st);
    return st;
}

void test_AddNodes_DuplicateNodeId_IsBadNodeIdExists(void) {
    /* First insertion with an explicit NodeId succeeds. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, add_nodes_first_result_status(5000));
    /* Re-adding the same NodeId is rejected per-result. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NODEIDEXISTS, add_nodes_first_result_status(5000));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_AddNodesRequest_Decode);
    RUN_TEST(test_AddNodes_DuplicateNodeId_IsBadNodeIdExists);
    RUN_TEST(test_AddNodesResponse_Encode);
    RUN_TEST(test_DeleteNodesRequest_Decode);
    RUN_TEST(test_DeleteNodesResponse_Encode);
    RUN_TEST(test_AddReferencesRequest_Decode);
    RUN_TEST(test_AddReferencesResponse_Encode);
    RUN_TEST(test_DeleteReferencesRequest_Decode);
    RUN_TEST(test_DeleteReferencesResponse_Encode);
    RUN_TEST(test_DeleteReferences_MissingReference_IsBadNotFound);
    return UNITY_END();
}
