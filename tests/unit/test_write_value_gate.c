/* tests/unit/test_write_value_gate.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/session.h"

typedef struct {
    mu_node_t *node;
    mu_value_source_t *value;
    int calls;
} write_gate_context_t;

void setUp(void) {}
void tearDown(void) {}

static opcua_statuscode_t gate_write_handler(void *context, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                             const mu_datavalue_t *value) {
    write_gate_context_t *gate = (write_gate_context_t *)context;
    if (!gate || !gate->node || !node_id || !value) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    gate->calls++;
    TEST_ASSERT_EQUAL(13u, attribute_id);
    TEST_ASSERT_EQUAL(1u, node_id->namespace_index);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, node_id->identifier_type);
    TEST_ASSERT_EQUAL(9101u, node_id->identifier.numeric);
    TEST_ASSERT_TRUE(value->has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value->value.type);
    TEST_ASSERT_FALSE(value->value.is_array);
    gate->value->data.static_value.value.i32 = value->value.value.i32;
    return MU_STATUS_GOOD;
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle) {
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {12345}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t audit_id = {-1, NULL};

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(writer, &auth_token));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int64(writer, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(writer, request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(writer, 0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(writer, &audit_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(writer, 10000));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_extension_object_header(writer, &null_id, 0));
}

static size_t build_write_value_request(opcua_byte_t *buffer, size_t capacity, const mu_nodeid_t *node_id,
                                        opcua_int32_t value) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    write_request_header(&writer, 2389);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 1));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, node_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 13));
    {
        mu_string_t null_range = {-1, NULL};
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_string(&writer, &null_range));
    }
    {
        mu_datavalue_t dv;
        (void)memset(&dv, 0, sizeof(dv));
        dv.has_value = true;
        dv.value.type = MU_TYPE_INT32;
        dv.value.is_array = false;
        dv.value.value.i32 = value;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_datavalue(&writer, &dv));
    }

    return writer.position;
}

static void init_server(mu_server_t *server, mu_node_t *node, mu_value_source_t *value, write_gate_context_t *gate) {
    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id;
    opcua_uint32_t auth_token;

    (void)memset(server, 0, sizeof(*server));
    node->node_id = (mu_nodeid_t){1, MU_NODEID_NUMERIC, {.numeric = 9101}};
    node->node_class = MU_NODECLASS_VARIABLE;
    node->browse_name = (mu_string_t){8, (const opcua_byte_t *)"Writable"};
    node->display_name = (mu_string_t){8, (const opcua_byte_t *)"Writable"};
    value->type = MU_VALUESOURCE_STATIC;
    value->data.static_value.type = MU_TYPE_INT32;
    value->data.static_value.is_array = false;
    value->data.static_value.value.i32 = 41;
    node->value = value;

    static mu_address_space_t address_space;
    address_space.nodes = node;
    address_space.node_count = 1;
    server->config.address_space = &address_space;
#ifdef MUC_OPCUA_SERVICE_WRITE
    server->config.write_handler = gate_write_handler;
    server->config.write_handler_handle = gate;
#else
    (void)gate;
#endif
    server->secure_channel.is_open = true;
    server->secure_channel.channel_id = 1u;

    mu_session_init(&server->sessions[0]);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_session_create(&server->sessions[0], 0, &revised_timeout, &session_id, &auth_token));
#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
    server->sessions[0].secure_channel_id = server->secure_channel.channel_id;
#endif
    TEST_ASSERT_EQUAL(12345u, auth_token);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token,
                                                          MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY));
}

static void decode_single_write_result(const opcua_byte_t *response, size_t response_len,
                                       opcua_statuscode_t *operation_result) {
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_int64_t timestamp;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_byte_t diagnostics_mask;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;
    opcua_int32_t results_count;
    opcua_int32_t diagnostics_count;

    mu_binary_reader_init(&reader, response, response_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_ID_WRITERESPONSE, response_type.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&reader, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &request_handle));
    TEST_ASSERT_EQUAL(2389u, request_handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, &service_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &diagnostics_mask));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &string_table_count));
    TEST_ASSERT_EQUAL(-1, string_table_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &additional_header_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &additional_header_encoding));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &results_count));
    TEST_ASSERT_EQUAL(1, results_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, operation_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &diagnostics_count));
    TEST_ASSERT_EQUAL(-1, diagnostics_count);
    TEST_ASSERT_EQUAL_size_t(response_len, reader.position);
}

#ifdef MUC_OPCUA_SERVICE_WRITE
void test_dedicated_write_value_gate_registers_and_mutates_with_legacy_aggregate_off(void) {
    mu_server_t server;
    mu_node_t node;
    mu_value_source_t value;
    write_gate_context_t gate;
    opcua_byte_t request[160];
    opcua_byte_t response[160];
    size_t request_len;
    size_t response_len = sizeof(response);
    opcua_statuscode_t operation_result = 0;

    (void)memset(&node, 0, sizeof(node));
    (void)memset(&value, 0, sizeof(value));
    gate.node = &node;
    gate.value = &value;
    gate.calls = 0;
    init_server(&server, &node, &value, &gate);
    request_len = build_write_value_request(request, sizeof(request), &node.node_id, 99);

    TEST_ASSERT_NOT_NULL(mu_get_service_handler(MU_ID_WRITEREQUEST));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_WRITEREQUEST, request, request_len,
                                                                response, &response_len));
    decode_single_write_result(response, response_len, &operation_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, operation_result);
    TEST_ASSERT_EQUAL(1, gate.calls);
    TEST_ASSERT_EQUAL(99, node.value->data.static_value.value.i32);
}
#else
void test_write_value_gate_off_rejects_before_write_mutation_or_response(void) {
    mu_server_t server;
    mu_node_t node;
    mu_value_source_t value;
    write_gate_context_t gate;
    opcua_byte_t request[160];
    opcua_byte_t response[64];
    size_t request_len;
    size_t response_len = sizeof(response);

    (void)memset(&node, 0, sizeof(node));
    (void)memset(&value, 0, sizeof(value));
    (void)memset(response, 0xA5, sizeof(response));
    gate.node = &node;
    gate.value = &value;
    gate.calls = 0;
    init_server(&server, &node, &value, &gate);
    request_len = build_write_value_request(request, sizeof(request), &node.node_id, 99);

    TEST_ASSERT_NULL(mu_get_service_handler(MU_ID_WRITEREQUEST));
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_SERVICEUNSUPPORTED,
        mu_service_dispatch(&server, MU_ID_WRITEREQUEST, request, request_len, response, &response_len));
    TEST_ASSERT_EQUAL(0, gate.calls);
    TEST_ASSERT_EQUAL(41, node.value->data.static_value.value.i32);
    TEST_ASSERT_EQUAL(sizeof(response), response_len);
    TEST_ASSERT_EQUAL_UINT8(0xA5, response[0]);
}
#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_SERVICE_WRITE
    RUN_TEST(test_dedicated_write_value_gate_registers_and_mutates_with_legacy_aggregate_off);
#else
    RUN_TEST(test_write_value_gate_off_rejects_before_write_mutation_or_response);
#endif
    return UNITY_END();
}
