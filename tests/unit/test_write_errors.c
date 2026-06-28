/* tests/unit/test_write_errors.c */
/* Normative References:
 * - OPC-10000-4 §5.11.4.2 (WriteValue parameters)
 * - OPC-10000-4 §5.11.4.3 (Write Service level results)
 */

#include "core/server_internal.h"
#include "core/service_dispatch.h"
#include "micro_opcua/encoding.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MICRO_OPCUA_SERVICE_WRITE
static opcua_statuscode_t dummy_write_handler(void *handle, const mu_nodeid_t *node_id, const mu_variant_t *value) {
    (void)handle;
    (void)node_id;
    (void)value;
    return MU_STATUS_GOOD;
}

void test_write_service_empty_array(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
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

    /* Citing OPC-10000-4 §5.11.4.3: empty nodesToWrite returns Bad_NothingToDo */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTHINGTODO, status);
}

void test_write_service_non_value_attribute(void) {
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
    memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

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
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ATTRIBUTEIDINVALID, item_status);
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
    memset(&server.user_address_space_index, 0, sizeof(server.user_address_space_index));

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
#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MICRO_OPCUA_SERVICE_WRITE
    RUN_TEST(test_write_service_empty_array);
    RUN_TEST(test_write_service_non_value_attribute);
    RUN_TEST(test_write_service_index_range);
#endif
    return UNITY_END();
}
