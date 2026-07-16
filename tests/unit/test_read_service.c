/* tests/unit/test_read_service.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/read.h"
#include "muc_opcua/encoding.h"

void test_read_service_request_decode(void) {
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    mu_binary_write_double(&writer, 0.0); /* MaxAge */
    mu_binary_write_uint32(&writer, MU_TIMESTAMPS_TO_RETURN_BOTH);
    mu_binary_write_int32(&writer, 1); /* Array length */

    mu_nodeid_t test_node = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 100};
    mu_binary_write_nodeid(&writer, &test_node);
    mu_binary_write_uint32(&writer, MU_ATTRIBUTEID_VALUE);

    mu_string_t empty_str = {.length = 0, .data = NULL};
    mu_binary_write_string(&writer, &empty_str); /* IndexRange */
    mu_binary_write_uint16(&writer, 0);          /* DataEncoding.NamespaceIndex */
    mu_binary_write_string(&writer, &empty_str); /* DataEncoding.Name */

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);

    mu_read_request_t req;
    mu_read_value_id_t nodes[2];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_request_decode(&reader, &req, nodes, 2));
    TEST_ASSERT_EQUAL(1, req.num_nodes_to_read);
    TEST_ASSERT_EQUAL(MU_TIMESTAMPS_TO_RETURN_BOTH, req.timestamps_to_return);
    TEST_ASSERT_EQUAL(100, req.nodes_to_read[0].node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_ATTRIBUTEID_VALUE, req.nodes_to_read[0].attribute_id);
}

void test_read_service_scalar_values(void) {
    mu_node_t node;
    node.node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 2253};
    node.node_class = 2; /* Variable */
    node.reference_count = 0;

    mu_value_source_t value_source;
    value_source.type = MU_VALUESOURCE_STATIC;
    value_source.data.static_value.type = MU_TYPE_INT32;
    value_source.data.static_value.is_array = false;
    value_source.data.static_value.value.i32 = 42;
    node.value = &value_source;

    mu_address_space_t address_space;
    address_space.nodes = &node;
    address_space.node_count = 1;

    mu_read_request_t req;
    req.max_age = 0;
    req.timestamps_to_return = MU_TIMESTAMPS_TO_RETURN_NEITHER;

    mu_read_value_id_t node_to_read = {.node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_VALUE};
    req.nodes_to_read = &node_to_read;
    req.num_nodes_to_read = 1;

    mu_read_response_t resp;
    mu_datavalue_t result_dv;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_process(&address_space, NULL, &req, 0, &resp, &result_dv, 1, NULL));
    TEST_ASSERT_EQUAL(1, resp.num_results);
    TEST_ASSERT_TRUE(resp.results[0].has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, resp.results[0].value.type);
    TEST_ASSERT_EQUAL(42, resp.results[0].value.value.i32);

    /* Encode and verify against independent binary fixture
     * OPC-10000-4 §7.38.1 / OPC-10000-6 §5.2.2.17 DataValue, §5.2.2.16 Variant */
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_response_encode(&writer, &resp));

    /* ReadResponse with one DataValue (Int32=42, has_value only):
     *   Int32 NoOfResults=1, DataValue mask=0x01,
     *   Variant mask=0x06 (BuiltInType Int32, scalar), Int32=42,
     *   Int32 DiagnosticInfos[]=empty (0) */
    const opcua_byte_t expected[] = {
        0x01, 0x00, 0x00, 0x00, /* Int32: num_results=1 */
        0x01,                   /* DataValue encoding mask: has_value */
        0x06,                   /* Variant mask: BuiltInType Int32, scalar */
        0x2A, 0x00, 0x00, 0x00, /* Int32: value=42 (LE) */
        0x00, 0x00, 0x00, 0x00, /* Int32: diagnosticInfos=0 (empty) */
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), writer.position);
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));
}

void test_read_service_rejects_invalid_timestamps_to_return(void) {
    mu_node_t node;
    node.node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 2253};
    node.node_class = MU_NODECLASS_VARIABLE;
    node.reference_count = 0;

    mu_value_source_t value_source;
    value_source.type = MU_VALUESOURCE_STATIC;
    value_source.data.static_value.type = MU_TYPE_INT32;
    value_source.data.static_value.is_array = false;
    value_source.data.static_value.value.i32 = 42;
    node.value = &value_source;

    mu_address_space_t address_space = {.nodes = &node, .node_count = 1};
    mu_read_value_id_t node_to_read = {.node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_VALUE};

    /* OPC-10000-4 5.11.2.3 defines Read parameters, 7.39 limits
       TimestampsToReturn request values, and 7.38.2 defines
       Bad_TimestampsToReturnInvalid for invalid enum values. */
    mu_read_request_t req = {.max_age = 0,
                             .timestamps_to_return = MU_TIMESTAMPS_TO_RETURN_INVALID,
                             .nodes_to_read = &node_to_read,
                             .num_nodes_to_read = 1};

    mu_read_response_t resp;
    mu_datavalue_t result_dv;

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID,
                      mu_read_process(&address_space, NULL, &req, 0, &resp, &result_dv, 1, NULL));
}

/* BrowseName and DisplayName are mandatory readable Attributes of every Node
   (OPC 10000-3 5.2.4/5.2.5), returned as QualifiedName and LocalizedText. Reading
   Value on a non-Variable Node is Bad_AttributeIdInvalid. */
void test_read_service_browsename_displayname(void) {
    mu_node_t node;
    node.node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 1000};
    node.node_class = MU_NODECLASS_OBJECT;
    node.browse_name = (mu_string_t){6, (const opcua_byte_t *)"MyVar1"};
    node.display_name = (mu_string_t){6, (const opcua_byte_t *)"MyVar1"};
    node.reference_count = 0;
    node.value = NULL;

    mu_address_space_t address_space = {.nodes = &node, .node_count = 1};

    mu_read_value_id_t reads[3] = {{.node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_BROWSENAME},
                                   {.node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_DISPLAYNAME},
                                   {.node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_VALUE}};
    mu_read_request_t req = {.max_age = 0,
                             .timestamps_to_return = MU_TIMESTAMPS_TO_RETURN_NEITHER,
                             .nodes_to_read = reads,
                             .num_nodes_to_read = 3};

    mu_read_response_t resp;
    mu_datavalue_t results[3];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_process(&address_space, NULL, &req, 0, &resp, results, 3, NULL));

    /* BrowseName -> QualifiedName */
    TEST_ASSERT_TRUE(resp.results[0].has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_QUALIFIEDNAME, resp.results[0].value.type);
    TEST_ASSERT_EQUAL(1, resp.results[0].value.value.qualified_name.namespace_index);
    TEST_ASSERT_EQUAL_MEMORY("MyVar1", resp.results[0].value.value.qualified_name.name.data, 6);

    /* DisplayName -> LocalizedText */
    TEST_ASSERT_TRUE(resp.results[1].has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_LOCALIZEDTEXT, resp.results[1].value.type);
    TEST_ASSERT_EQUAL_MEMORY("MyVar1", resp.results[1].value.value.localized_text.text.data, 6);

    /* Value on an Object -> Bad_AttributeIdInvalid */
    TEST_ASSERT_FALSE(resp.results[2].has_value);
    TEST_ASSERT_TRUE(resp.results[2].has_status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ATTRIBUTEIDINVALID, resp.results[2].status);
}

void test_read_service_batch_preserves_per_operation_results(void) {
    mu_value_source_t variable_value;
    variable_value.type = MU_VALUESOURCE_STATIC;
    variable_value.data.static_value.type = MU_TYPE_INT32;
    variable_value.data.static_value.is_array = false;
    variable_value.data.static_value.value.i32 = 77;

    mu_node_t nodes[2];
    nodes[0].node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 1001};
    nodes[0].node_class = MU_NODECLASS_VARIABLE;
    nodes[0].browse_name = (mu_string_t){8, (const opcua_byte_t *)"BatchVar"};
    nodes[0].display_name = (mu_string_t){8, (const opcua_byte_t *)"BatchVar"};
    nodes[0].references = NULL;
    nodes[0].reference_count = 0;
    nodes[0].value = &variable_value;

    nodes[1].node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 1002};
    nodes[1].node_class = MU_NODECLASS_OBJECT;
    nodes[1].browse_name = (mu_string_t){8, (const opcua_byte_t *)"BatchObj"};
    nodes[1].display_name = (mu_string_t){8, (const opcua_byte_t *)"BatchObj"};
    nodes[1].references = NULL;
    nodes[1].reference_count = 0;
    nodes[1].value = NULL;

    mu_address_space_t address_space = {.nodes = nodes, .node_count = 2};

    mu_read_value_id_t reads[5] = {
        {.node_id = nodes[0].node_id, .attribute_id = MU_ATTRIBUTEID_VALUE},
        {.node_id = nodes[0].node_id, .attribute_id = MU_ATTRIBUTEID_DISPLAYNAME},
        {.node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 9999},
         .attribute_id = MU_ATTRIBUTEID_VALUE},
        {.node_id = nodes[1].node_id, .attribute_id = MU_ATTRIBUTEID_VALUE},
        {.node_id = nodes[0].node_id, .attribute_id = MU_ATTRIBUTEID_EVENTNOTIFIER},
    };

    mu_read_request_t batch_req = {.max_age = 0,
                                   .timestamps_to_return = MU_TIMESTAMPS_TO_RETURN_NEITHER,
                                   .nodes_to_read = reads,
                                   .num_nodes_to_read = 5};
    mu_read_response_t batch_resp;
    mu_datavalue_t batch_results[5];

    /* OPC-10000-4 5.11.2.2 returns a StatusCode for each nodesToRead entry.
       OPC-10000-4 5.11.2.4 and 7.38.2 define those operation-level statuses,
       so mixed per-operation failures must not change the service-level result. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_read_process(&address_space, NULL, &batch_req, 0, &batch_resp, batch_results, 5, NULL));
    TEST_ASSERT_EQUAL(5, batch_resp.num_results);

    for (size_t i = 0; i < batch_req.num_nodes_to_read; ++i) {
        mu_read_request_t single_req = {.max_age = 0,
                                        .timestamps_to_return = MU_TIMESTAMPS_TO_RETURN_NEITHER,
                                        .nodes_to_read = &reads[i],
                                        .num_nodes_to_read = 1};
        mu_read_response_t single_resp;
        mu_datavalue_t single_result;

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                          mu_read_process(&address_space, NULL, &single_req, 0, &single_resp, &single_result, 1, NULL));
        TEST_ASSERT_EQUAL(1, single_resp.num_results);
        TEST_ASSERT_EQUAL(single_resp.results[0].has_value, batch_resp.results[i].has_value);
        TEST_ASSERT_EQUAL(single_resp.results[0].has_status, batch_resp.results[i].has_status);

        if (single_resp.results[0].has_status) {
            TEST_ASSERT_EQUAL(single_resp.results[0].status, batch_resp.results[i].status);
        }
        if (single_resp.results[0].has_value) {
            TEST_ASSERT_EQUAL(single_resp.results[0].value.type, batch_resp.results[i].value.type);
            TEST_ASSERT_EQUAL(single_resp.results[0].value.is_array, batch_resp.results[i].value.is_array);

            if (batch_resp.results[i].value.type == MU_TYPE_INT32) {
                TEST_ASSERT_EQUAL(single_resp.results[0].value.value.i32, batch_resp.results[i].value.value.i32);
            } else if (batch_resp.results[i].value.type == MU_TYPE_LOCALIZEDTEXT) {
                TEST_ASSERT_EQUAL(single_resp.results[0].value.value.localized_text.text.length,
                                  batch_resp.results[i].value.value.localized_text.text.length);
                TEST_ASSERT_EQUAL_MEMORY(single_resp.results[0].value.value.localized_text.text.data,
                                         batch_resp.results[i].value.value.localized_text.text.data,
                                         (size_t)batch_resp.results[i].value.value.localized_text.text.length);
            }
        }
    }

    TEST_ASSERT_TRUE(batch_resp.results[0].has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, batch_resp.results[0].value.type);
    TEST_ASSERT_EQUAL(77, batch_resp.results[0].value.value.i32);
    TEST_ASSERT_TRUE(batch_resp.results[1].has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_LOCALIZEDTEXT, batch_resp.results[1].value.type);
    TEST_ASSERT_TRUE(batch_resp.results[2].has_status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NODEIDUNKNOWN, batch_resp.results[2].status);
    TEST_ASSERT_TRUE(batch_resp.results[3].has_status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ATTRIBUTEIDINVALID, batch_resp.results[3].status);
    TEST_ASSERT_TRUE(batch_resp.results[4].has_status);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ATTRIBUTEIDINVALID, batch_resp.results[4].status);
}

/* Feature 028 (T036): a Read request whose nodesToRead count exceeds the caller's
   fixed capacity (MU_DISPATCH_MAX_READ_NODES at the dispatch boundary) must decode
   to Bad_TooManyOperations, matching Browse/Write/Query. OPC-10000-4 §5.11.2 / §7.38.2. */
void test_read_over_capacity_is_too_many_operations(void) {
    opcua_byte_t buffer[64];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    mu_binary_write_double(&writer, 0.0); /* MaxAge */
    mu_binary_write_uint32(&writer, MU_TIMESTAMPS_TO_RETURN_BOTH);
    mu_binary_write_int32(&writer, 3); /* nodesToRead count = 3 */

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);
    mu_read_request_t req;
    mu_read_value_id_t nodes[2]; /* capacity 2 < 3 */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS, mu_read_request_decode(&reader, &req, nodes, 2));
}

#if MUC_OPCUA_CU_EVENTS
void test_read_service_eventnotifier(void) {
    /* spec 074 / CU 3194 / OPC-10000-3 §5.4.6: EventNotifier is a readable
       attribute of Object/View nodes (the Server Object advertises
       SubscribeToEvents); other NodeClasses return Bad_AttributeIdInvalid. */
    mu_node_t nodes[2];
    (void)memset(nodes, 0, sizeof(nodes));
    nodes[0].node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 2253};
    nodes[0].node_class = MU_NODECLASS_OBJECT;
    nodes[0].browse_name = (mu_string_t){6, (const opcua_byte_t *)"Server"};
    nodes[0].display_name = (mu_string_t){6, (const opcua_byte_t *)"Server"};
    nodes[0].event_notifier = 0x01u;
    nodes[1].node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 1000};
    nodes[1].node_class = MU_NODECLASS_VARIABLE;
    nodes[1].browse_name = (mu_string_t){3, (const opcua_byte_t *)"Var"};
    nodes[1].display_name = (mu_string_t){3, (const opcua_byte_t *)"Var"};

    mu_address_space_t address_space = {.nodes = nodes, .node_count = 2};
    mu_read_value_id_t reads[2] = {{.node_id = nodes[0].node_id, .attribute_id = MU_ATTRIBUTEID_EVENTNOTIFIER},
                                   {.node_id = nodes[1].node_id, .attribute_id = MU_ATTRIBUTEID_EVENTNOTIFIER}};
    mu_read_request_t req = {.max_age = 0,
                             .timestamps_to_return = MU_TIMESTAMPS_TO_RETURN_NEITHER,
                             .nodes_to_read = reads,
                             .num_nodes_to_read = 2};
    mu_read_response_t resp;
    mu_datavalue_t results[2];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_process(&address_space, NULL, &req, 0, &resp, results, 2, NULL));

    /* Object -> Byte with SubscribeToEvents (bit 0) */
    TEST_ASSERT_TRUE(resp.results[0].has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTE, resp.results[0].value.type);
    TEST_ASSERT_EQUAL_HEX8(0x01u, resp.results[0].value.value.by);
    /* Variable has no EventNotifier attribute */
    TEST_ASSERT_FALSE(resp.results[1].has_value);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ATTRIBUTEIDINVALID, resp.results[1].status);
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_service_request_decode);
    RUN_TEST(test_read_over_capacity_is_too_many_operations);
    RUN_TEST(test_read_service_scalar_values);
    RUN_TEST(test_read_service_rejects_invalid_timestamps_to_return);
    RUN_TEST(test_read_service_browsename_displayname);
#if MUC_OPCUA_CU_EVENTS
    RUN_TEST(test_read_service_eventnotifier);
#endif
    RUN_TEST(test_read_service_batch_preserves_per_operation_results);
    return UNITY_END();
}
