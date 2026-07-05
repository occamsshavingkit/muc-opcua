/* tests/unit/test_write_errors.c */
/* Normative References:
 * - OPC-10000-4 section 5.11.4.2 (WriteValue parameters)
 * - OPC-10000-4 section 5.11.4.3 (Write Service level results)
 * - OPC-10000-4 section 5.11.4.4 (Write operation-level results)
 * - OPC-10000-4 section 7.38.2 (Common StatusCodes)
 */

#include "core/server_internal.h"
#include "core/service_dispatch.h"
#include "muc_opcua/encoding.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MUC_OPCUA_SERVICE_WRITE
static opcua_statuscode_t dummy_write_handler(void *handle, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                              const mu_datavalue_t *value) {
    (void)handle;
    (void)node_id;
    (void)attribute_id;
    (void)value;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t counting_write_handler(void *handle, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                                 const mu_datavalue_t *value) {
    int *callback_count = (int *)handle;
    (void)node_id;
    (void)attribute_id;
    (void)value;
    if (callback_count) {
        (*callback_count)++;
    }
    return MU_STATUS_GOOD;
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle) {
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t audit_id = {-1, NULL};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(writer, &auth_token));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(writer, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(writer, request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(writer, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(writer, &audit_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(writer, 10000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_extension_object_header(writer, &null_id, 0));
}

static void write_int32_value_writevalue(mu_binary_writer_t *writer, const mu_nodeid_t *node_id,
                                         opcua_int32_t attribute_id, opcua_int32_t value) {
    mu_string_t null_string = {-1, NULL};
    mu_datavalue_t dv;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(writer, node_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(writer, attribute_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(writer, &null_string));

    (void)memset(&dv, 0, sizeof(dv));
    dv.has_value = true;
    dv.value.type = MU_TYPE_INT32;
    dv.value.is_array = false;
    dv.value.value.i32 = value;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_datavalue(writer, &dv));
}

static void assert_single_write_result(const opcua_byte_t *resp_buffer, size_t resp_len,
                                       opcua_statuscode_t expected_service_result,
                                       opcua_statuscode_t expected_item_status) {
    mu_binary_reader_t resp_reader;
    mu_binary_reader_init(&resp_reader, resp_buffer, resp_len);

    mu_nodeid_t resp_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp_reader, &resp_type));
    TEST_ASSERT_EQUAL(MU_ID_WRITERESPONSE, resp_type.identifier.numeric);

    opcua_int64_t ts;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&resp_reader, &ts));
    opcua_uint32_t handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&resp_reader, &handle));

    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &service_result));
    TEST_ASSERT_EQUAL(expected_service_result, service_result);

    opcua_byte_t diag;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&resp_reader, &diag));
    opcua_int32_t str_tbl;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &str_tbl));
    mu_nodeid_t ext_id;
    size_t ext_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&resp_reader, &ext_id, &ext_len));

    opcua_int32_t results_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &results_len));
    TEST_ASSERT_EQUAL(1, results_len);

    opcua_statuscode_t item_status;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status));
    TEST_ASSERT_EQUAL(expected_item_status, item_status);
}

void test_write_service_empty_array(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.config.write_handler = dummy_write_handler;

    opcua_byte_t req_buffer[128];
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

    /* nodesToWrite size = 0 */
    mu_binary_write_int32(&writer, 0);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[128];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    /* Citing OPC-10000-4 section 5.11.4.3: empty nodesToWrite returns Bad_NothingToDo */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTHINGTODO, status);
}

void test_write_datavalue_without_value_returns_type_mismatch_without_callback(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));

    mu_node_t node;
    (void)memset(&node, 0, sizeof(node));
    node.node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 5001}};
    node.node_class = MU_NODECLASS_VARIABLE;
    node.browse_name = (mu_string_t){4, (const opcua_byte_t *)"Test"};
    node.display_name = (mu_string_t){4, (const opcua_byte_t *)"Test"};

    mu_address_space_t address_space = {&node, 1};
    int callback_count = 0;
    server.config.address_space = &address_space;
    server.config.write_handler = counting_write_handler;
    server.config.write_handler_handle = &callback_count;
    (void)memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    opcua_byte_t req_buffer[128];
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

    /* nodesToWrite size = 1 */
    mu_binary_write_int32(&writer, 1);

    /* OPC-10000-4 section 5.11.4.2 requires WriteValue.value for the Value Attribute. */
    mu_binary_write_nodeid(&writer, &node.node_id);
    mu_binary_write_int32(&writer, 13);
    mu_binary_write_string(&writer, &audit_id);

    /* DataValue encoding mask with no Value bit present. */
    mu_binary_write_byte(&writer, 0);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[128];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(0, callback_count);

    mu_binary_reader_t resp_reader;
    mu_binary_reader_init(&resp_reader, resp_buffer, resp_len);

    mu_nodeid_t resp_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp_reader, &resp_type));
    opcua_int64_t ts;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&resp_reader, &ts));
    opcua_uint32_t handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&resp_reader, &handle));
    opcua_statuscode_t s_res;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &s_res));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s_res);

    opcua_byte_t diag;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&resp_reader, &diag));
    opcua_int32_t str_tbl;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &str_tbl));
    mu_nodeid_t ext_id;
    size_t ext_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&resp_reader, &ext_id, &ext_len));

    opcua_int32_t results_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &results_len));
    TEST_ASSERT_EQUAL(1, results_len);

    opcua_statuscode_t item_status;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status));
    /* OPC-10000-4 section 5.11.4.4 reports operation-level failures in results[]. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TYPEMISMATCH, item_status);
}

void test_write_service_non_value_attribute(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));

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
    server.config.write_handler = dummy_write_handler;
    (void)memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    opcua_byte_t req_buffer[128];
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

    /* nodesToWrite size = 1 */
    mu_binary_write_int32(&writer, 1);

    /* WriteValue with AttributeId = 3 (BrowseName) */
    mu_binary_write_nodeid(&writer, &node.node_id);
    mu_binary_write_int32(&writer, 3); // BrowseName
    mu_binary_write_string(&writer, &audit_id);

    mu_datavalue_t dv;
    (void)memset(&dv, 0, sizeof(dv));
    dv.has_value = true;
    dv.has_status = false;
    dv.has_source_timestamp = false;
    dv.has_server_timestamp = false;
    dv.value.type = MU_TYPE_INT32;
    dv.value.is_array = false;
    dv.value.value.i32 = 42;
    mu_binary_write_datavalue(&writer, &dv);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[128];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    mu_binary_reader_t resp_reader;
    mu_binary_reader_init(&resp_reader, resp_buffer, resp_len);

    mu_nodeid_t resp_type;
    mu_binary_read_nodeid(&resp_reader, &resp_type);
    opcua_int64_t ts;
    mu_binary_read_int64(&resp_reader, &ts);
    opcua_uint32_t handle;
    mu_binary_read_uint32(&resp_reader, &handle);
    opcua_statuscode_t s_res;
    mu_binary_read_statuscode(&resp_reader, &s_res);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s_res);

    opcua_byte_t diag;
    mu_binary_read_byte(&resp_reader, &diag);
    opcua_int32_t str_tbl;
    mu_binary_read_int32(&resp_reader, &str_tbl);
    mu_nodeid_t ext_id;
    size_t ext_len;
    mu_binary_read_extension_object_header(&resp_reader, &ext_id, &ext_len);

    opcua_int32_t results_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &results_len));
    TEST_ASSERT_EQUAL(1, results_len);

    opcua_statuscode_t item_status;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTWRITABLE, item_status);
}

void test_write_service_index_range(void) {
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
    server.config.write_handler = dummy_write_handler;
    (void)memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    opcua_byte_t req_buffer[128];
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

    /* nodesToWrite size = 1 */
    mu_binary_write_int32(&writer, 1);

    /* WriteValue with AttributeId = 13 and non-empty indexRange */
    mu_binary_write_nodeid(&writer, &node.node_id);
    mu_binary_write_int32(&writer, 13);

    mu_string_t index_range = {3, (const opcua_byte_t *)"1:2"};
    mu_binary_write_string(&writer, &index_range);

    mu_datavalue_t dv;
    (void)memset(&dv, 0, sizeof(dv));
    dv.has_value = true;
    dv.has_status = false;
    dv.has_source_timestamp = false;
    dv.has_server_timestamp = false;
    dv.value.type = MU_TYPE_INT32;
    dv.value.is_array = false;
    dv.value.value.i32 = 42;
    mu_binary_write_datavalue(&writer, &dv);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[128];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    mu_binary_reader_t resp_reader;
    mu_binary_reader_init(&resp_reader, resp_buffer, resp_len);

    mu_nodeid_t resp_type;
    mu_binary_read_nodeid(&resp_reader, &resp_type);
    opcua_int64_t ts;
    mu_binary_read_int64(&resp_reader, &ts);
    opcua_uint32_t handle;
    mu_binary_read_uint32(&resp_reader, &handle);
    opcua_statuscode_t s_res;
    mu_binary_read_statuscode(&resp_reader, &s_res);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s_res);

    opcua_byte_t diag;
    mu_binary_read_byte(&resp_reader, &diag);
    opcua_int32_t str_tbl;
    mu_binary_read_int32(&resp_reader, &str_tbl);
    mu_nodeid_t ext_id;
    size_t ext_len;
    mu_binary_read_extension_object_header(&resp_reader, &ext_id, &ext_len);

    opcua_int32_t results_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&resp_reader, &results_len));
    TEST_ASSERT_EQUAL(1, results_len);

    opcua_statuscode_t item_status;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&resp_reader, &item_status));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_WRITENOTSUPPORTED, item_status);
}

void test_write_unknown_node_is_operation_level_bad_nodeidunknown(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.config.write_handler = counting_write_handler;

    int callback_count = 0;
    server.config.write_handler_handle = &callback_count;
    memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    opcua_byte_t req_buffer[128];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, req_buffer, sizeof(req_buffer));

    write_request_header(&writer, 42);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 1));

    mu_nodeid_t unknown_node = {1, MU_NODEID_NUMERIC, {.numeric = 9090}};
    write_int32_value_writevalue(&writer, &unknown_node, 13, 42);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[128];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    /* OPC-10000-4 sections 5.11.4.4 and 7.38.2: item errors stay in results[]. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(0, callback_count);
    assert_single_write_result(resp_buffer, resp_len, MU_STATUS_GOOD, MU_STATUS_BAD_NODEIDUNKNOWN);
}

void test_write_value_on_object_is_operation_level_bad_notwritable(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));

    mu_node_t node;
    (void)memset(&node, 0, sizeof(node));
    node.node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 5002}};
    node.node_class = MU_NODECLASS_OBJECT;
    node.browse_name = (mu_string_t){6, (const opcua_byte_t *)"Object"};
    node.display_name = (mu_string_t){6, (const opcua_byte_t *)"Object"};

    mu_address_space_t address_space = {&node, 1};
    int callback_count = 0;
    server.config.address_space = &address_space;
    server.config.write_handler = counting_write_handler;
    server.config.write_handler_handle = &callback_count;
    memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

    opcua_byte_t req_buffer[128];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, req_buffer, sizeof(req_buffer));

    write_request_header(&writer, 42);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 1));
    write_int32_value_writevalue(&writer, &node.node_id, 13, 42);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[128];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    /* Bad_NotWritable is a common StatusCode reported per WriteValue result. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(0, callback_count);
    assert_single_write_result(resp_buffer, resp_len, MU_STATUS_GOOD, MU_STATUS_BAD_NOTWRITABLE);
}

void test_write_too_many_operations_is_service_level_bad_toomanyoperations(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.config.write_handler = counting_write_handler;

    int callback_count = 0;
    server.config.write_handler_handle = &callback_count;

    opcua_byte_t req_buffer[128];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, req_buffer, sizeof(req_buffer));

    write_request_header(&writer, 42);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 33));

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, req_buffer, writer.position);

    opcua_byte_t resp_buffer[128];
    mu_binary_writer_t resp_writer;
    mu_binary_writer_init(&resp_writer, resp_buffer, sizeof(resp_buffer));

    size_t resp_len = 0;
    opcua_statuscode_t status = handle_write(&server, &reader, &resp_writer, &resp_len);

    /* OPC-10000-4 section 7.38.2: invalid request cardinality rejects the Service. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TOOMANYOPERATIONS, status);
    TEST_ASSERT_EQUAL(0, callback_count);
    TEST_ASSERT_EQUAL(0, resp_len);
}
#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_SERVICE_WRITE
    RUN_TEST(test_write_service_empty_array);
    RUN_TEST(test_write_datavalue_without_value_returns_type_mismatch_without_callback);
    RUN_TEST(test_write_service_non_value_attribute);
    RUN_TEST(test_write_service_index_range);
    RUN_TEST(test_write_unknown_node_is_operation_level_bad_nodeidunknown);
    RUN_TEST(test_write_value_on_object_is_operation_level_bad_notwritable);
    RUN_TEST(test_write_too_many_operations_is_service_level_bad_toomanyoperations);
#endif
    return UNITY_END();
}
