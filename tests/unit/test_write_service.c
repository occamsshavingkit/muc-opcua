/* tests/unit/test_write_service.c */
#include "core/server_internal.h"
#include "core/service_dispatch.h"
#include "micro_opcua/encoding.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static mu_nodeid_t g_last_write_node;
static mu_variant_t g_last_write_val;
static int g_write_count = 0;

static opcua_statuscode_t mock_write_handler(void *handle, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                             const mu_variant_t *value) {
    (void)handle;
    (void)attribute_id;
    g_last_write_node = *node_id;
    g_last_write_val = *value;
    g_write_count++;
    return MU_STATUS_GOOD;
}

#ifdef MICRO_OPCUA_SERVICE_WRITE
void test_write_service_basic(void) {
    /* Initialize mock server structure */
    mu_server_t server;
    memset(&server, 0, sizeof(server));

    /* 1. Setup address space node */
    mu_node_t node;
    node.node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 5001}};
    node.node_class = MU_NODECLASS_VARIABLE;
    node.browse_name = (mu_string_t){4, (const opcua_byte_t *)"Test"};
    node.display_name = (mu_string_t){4, (const opcua_byte_t *)"Test"};
    node.references = NULL;
    node.reference_count = 0;

    mu_value_source_t val_src;
    val_src.type = MU_VALUESOURCE_STATIC;
    val_src.data.static_value.type = MU_TYPE_INT32;
    val_src.data.static_value.is_array = false;
    val_src.data.static_value.value.i32 = 10;
    node.value = &val_src;

    mu_address_space_t address_space = {&node, 1};
    server.config.address_space = &address_space;
    server.config.write_handler = mock_write_handler;

    /* Initialize user address space index */
    memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    /* 2. Encode WriteRequest */
    opcua_byte_t req_buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, req_buffer, sizeof(req_buffer));

    /* RequestHeader */
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_nodeid(&writer, &auth_token);
    mu_binary_write_int64(&writer, 0);   // timestamp
    mu_binary_write_uint32(&writer, 42); // requestHandle
    mu_binary_write_uint32(&writer, 0);  // returnDiagnostics
    mu_string_t audit_id = {-1, NULL};
    mu_binary_write_string(&writer, &audit_id);
    mu_binary_write_uint32(&writer, 10000); // timeoutHint
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_extension_object_header(&writer, &null_id, 0);

    /* nodesToWrite size */
    mu_binary_write_int32(&writer, 1);

    /* WriteValue */
    mu_binary_write_nodeid(&writer, &node.node_id);
    mu_binary_write_int32(&writer, 13);         // attributeId_Value
    mu_binary_write_string(&writer, &audit_id); // indexRange (null)

    mu_datavalue_t dv;
    dv.has_value = true;
    dv.has_status = false;
    dv.has_source_timestamp = false;
    dv.has_server_timestamp = false;
    dv.value.type = MU_TYPE_INT32;
    dv.value.is_array = false;
    dv.value.value.i32 = 42;
    mu_binary_write_datavalue(&writer, &dv);

    /* 3. Call handle_write */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[256];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    g_write_count = 0;

    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(1, g_write_count);
    TEST_ASSERT_EQUAL(5001, g_last_write_node.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, g_last_write_val.type);
    TEST_ASSERT_EQUAL(42, g_last_write_val.value.i32);

    /* Verify response */
    mu_binary_reader_t resp_reader;
    mu_binary_reader_init(&resp_reader, resp_buffer, resp_len);

    mu_nodeid_t resp_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp_reader, &resp_type));
    TEST_ASSERT_EQUAL(MU_ID_WRITERESPONSE, resp_type.identifier.numeric);

    opcua_int64_t timestamp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&resp_reader, &timestamp));
    opcua_uint32_t request_handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&resp_reader, &request_handle));
    TEST_ASSERT_EQUAL(42, request_handle);

    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);

    /* Diagnostics info & string table */
    opcua_byte_t diag_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&resp_reader, &diag_mask));
    TEST_ASSERT_EQUAL(0, diag_mask);
    opcua_int32_t str_table_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &str_table_len));
    TEST_ASSERT_EQUAL(-1, str_table_len);

    /* skipped extension object */
    mu_nodeid_t ext_id;
    size_t ext_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&resp_reader, &ext_id, &ext_len));
    TEST_ASSERT_EQUAL(0, ext_id.identifier.numeric);

    /* results array */
    opcua_int32_t results_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &results_len));
    TEST_ASSERT_EQUAL(1, results_len);

    opcua_statuscode_t item_status;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, item_status);
}

void test_write_service_type_mismatch(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));

    mu_node_t node;
    node.node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 5001}};
    node.node_class = MU_NODECLASS_VARIABLE;
    node.browse_name = (mu_string_t){4, (const opcua_byte_t *)"Test"};
    node.display_name = (mu_string_t){4, (const opcua_byte_t *)"Test"};
    node.references = NULL;
    node.reference_count = 0;

    mu_value_source_t val_src;
    val_src.type = MU_VALUESOURCE_STATIC;
    val_src.data.static_value.type = MU_TYPE_INT32;
    val_src.data.static_value.is_array = false;
    val_src.data.static_value.value.i32 = 10;
    node.value = &val_src;

    mu_address_space_t address_space = {&node, 1};
    server.config.address_space = &address_space;
    server.config.write_handler = mock_write_handler;

    memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    opcua_byte_t req_buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, req_buffer, sizeof(req_buffer));

    /* RequestHeader */
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_nodeid(&writer, &auth_token);
    mu_binary_write_int64(&writer, 0);
    mu_binary_write_uint32(&writer, 42);
    mu_binary_write_uint32(&writer, 0);
    mu_string_t audit_id = {-1, NULL};
    mu_binary_write_string(&writer, &audit_id);
    mu_binary_write_uint32(&writer, 10000);
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_extension_object_header(&writer, &null_id, 0);

    /* nodesToWrite size */
    mu_binary_write_int32(&writer, 1);

    /* WriteValue with FLOAT type (mismatch) */
    mu_binary_write_nodeid(&writer, &node.node_id);
    mu_binary_write_int32(&writer, 13);
    mu_binary_write_string(&writer, &audit_id);

    mu_datavalue_t dv;
    dv.has_value = true;
    dv.has_status = false;
    dv.has_source_timestamp = false;
    dv.has_server_timestamp = false;
    dv.value.type = MU_TYPE_FLOAT;
    dv.value.is_array = false;
    dv.value.value.f = 4.2f;
    mu_binary_write_datavalue(&writer, &dv);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[256];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    g_write_count = 0;

    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(0, g_write_count); // Callback not invoked

    /* Verify response */
    mu_binary_reader_t resp_reader;
    mu_binary_reader_init(&resp_reader, resp_buffer, resp_len);

    mu_nodeid_t resp_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp_reader, &resp_type));
    opcua_int64_t timestamp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&resp_reader, &timestamp));
    opcua_uint32_t request_handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&resp_reader, &request_handle));
    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);

    /* Diagnostics info & string table & skipped ext obj */
    opcua_byte_t diag_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&resp_reader, &diag_mask));
    opcua_int32_t str_table_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &str_table_len));
    mu_nodeid_t ext_id;
    size_t ext_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&resp_reader, &ext_id, &ext_len));

    /* results array */
    opcua_int32_t results_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &results_len));
    TEST_ASSERT_EQUAL(1, results_len);

    opcua_statuscode_t item_status;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TYPEMISMATCH, item_status);
}

void test_write_service_batch(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));

    mu_node_t nodes[2];
    memset(nodes, 0, sizeof(nodes));

    nodes[0].node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 1001}};
    nodes[0].node_class = MU_NODECLASS_VARIABLE;
    nodes[0].browse_name = (mu_string_t){4, (const opcua_byte_t *)"Var1"};
    nodes[0].display_name = (mu_string_t){4, (const opcua_byte_t *)"Var1"};

    mu_value_source_t val_src_0;
    val_src_0.type = MU_VALUESOURCE_STATIC;
    val_src_0.data.static_value.type = MU_TYPE_INT32;
    val_src_0.data.static_value.is_array = false;
    val_src_0.data.static_value.value.i32 = 10;
    nodes[0].value = &val_src_0;

    nodes[1].node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 1002}};
    nodes[1].node_class = MU_NODECLASS_VARIABLE;
    nodes[1].browse_name = (mu_string_t){4, (const opcua_byte_t *)"Var2"};
    nodes[1].display_name = (mu_string_t){4, (const opcua_byte_t *)"Var2"};

    mu_value_source_t val_src_1;
    val_src_1.type = MU_VALUESOURCE_STATIC;
    val_src_1.data.static_value.type = MU_TYPE_FLOAT;
    val_src_1.data.static_value.is_array = false;
    val_src_1.data.static_value.value.f = 1.0f;
    nodes[1].value = &val_src_1;

    mu_address_space_t address_space = {nodes, 2};
    server.config.address_space = &address_space;
    server.config.write_handler = mock_write_handler;

    memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    opcua_byte_t req_buffer[512];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, req_buffer, sizeof(req_buffer));

    /* RequestHeader */
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_nodeid(&writer, &auth_token);
    mu_binary_write_int64(&writer, 0);
    mu_binary_write_uint32(&writer, 42);
    mu_binary_write_uint32(&writer, 0);
    mu_string_t audit_id = {-1, NULL};
    mu_binary_write_string(&writer, &audit_id);
    mu_binary_write_uint32(&writer, 10000);
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_extension_object_header(&writer, &null_id, 0);

    /* nodesToWrite size = 2 */
    mu_binary_write_int32(&writer, 2);

    /* WriteValue 0: Success path (Int32 42) */
    mu_binary_write_nodeid(&writer, &nodes[0].node_id);
    mu_binary_write_int32(&writer, 13);
    mu_binary_write_string(&writer, &audit_id);
    mu_datavalue_t dv0;
    dv0.has_value = true;
    dv0.has_status = false;
    dv0.has_source_timestamp = false;
    dv0.has_server_timestamp = false;
    dv0.value.type = MU_TYPE_INT32;
    dv0.value.is_array = false;
    dv0.value.value.i32 = 42;
    mu_binary_write_datavalue(&writer, &dv0);

    /* WriteValue 1: Type mismatch path (Int32 written to Float node) */
    mu_binary_write_nodeid(&writer, &nodes[1].node_id);
    mu_binary_write_int32(&writer, 13);
    mu_binary_write_string(&writer, &audit_id);
    mu_datavalue_t dv1;
    dv1.has_value = true;
    dv1.has_status = false;
    dv1.has_source_timestamp = false;
    dv1.has_server_timestamp = false;
    dv1.value.type = MU_TYPE_INT32; // mismatch: node expects Float
    dv1.value.is_array = false;
    dv1.value.value.i32 = 99;
    mu_binary_write_datavalue(&writer, &dv1);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[512];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    g_write_count = 0;

    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(1, g_write_count); // Only the first one succeeded and triggered callback

    /* Verify response */
    mu_binary_reader_t resp_reader;
    mu_binary_reader_init(&resp_reader, resp_buffer, resp_len);

    mu_nodeid_t resp_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp_reader, &resp_type));
    opcua_int64_t timestamp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&resp_reader, &timestamp));
    opcua_uint32_t request_handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&resp_reader, &request_handle));
    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);

    /* Diagnostics info & string table & skipped ext obj */
    opcua_byte_t diag_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&resp_reader, &diag_mask));
    opcua_int32_t str_table_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &str_table_len));
    mu_nodeid_t ext_id;
    size_t ext_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&resp_reader, &ext_id, &ext_len));

    /* results array size = 2 */
    opcua_int32_t results_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &results_len));
    TEST_ASSERT_EQUAL(2, results_len);

    opcua_statuscode_t item_status0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, item_status0);

    opcua_statuscode_t item_status1;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status1));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TYPEMISMATCH, item_status1);
}
#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MICRO_OPCUA_SERVICE_WRITE
    RUN_TEST(test_write_service_basic);
    RUN_TEST(test_write_service_type_mismatch);
    RUN_TEST(test_write_service_batch);
#endif
    return UNITY_END();
}
