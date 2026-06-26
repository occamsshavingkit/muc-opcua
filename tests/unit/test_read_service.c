/* tests/unit/test_read_service.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/read.h"
#include "micro_opcua/encoding.h"

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
    mu_binary_write_uint16(&writer, 0); /* DataEncoding.NamespaceIndex */
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
    node.node_id = (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 2253};
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
    
    mu_read_value_id_t node_to_read = {
        .node_id = node.node_id,
        .attribute_id = MU_ATTRIBUTEID_VALUE
    };
    req.nodes_to_read = &node_to_read;
    req.num_nodes_to_read = 1;
    
    mu_read_response_t resp;
    mu_datavalue_t result_dv;
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_process(&address_space, &req, &resp, &result_dv, 1));
    TEST_ASSERT_EQUAL(1, resp.num_results);
    TEST_ASSERT_TRUE(resp.results[0].has_value);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, resp.results[0].value.type);
    TEST_ASSERT_EQUAL(42, resp.results[0].value.value.i32);
    
    /* Encode */
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_response_encode(&writer, &resp));
    
    /* Decode to verify */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);
    
    opcua_int32_t num_results;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &num_results));
    TEST_ASSERT_EQUAL(1, num_results);
    
    /* DataValue Encoding Mask */
    opcua_byte_t mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &mask));
    TEST_ASSERT_EQUAL(1, mask); /* has_value = true, everything else false */
    
    /* Variant */
    opcua_byte_t type_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &type_id));
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, type_id);
    
    opcua_int32_t val;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &val));
    TEST_ASSERT_EQUAL(42, val);
}

/* BrowseName and DisplayName are mandatory readable Attributes of every Node
   (OPC 10000-3 5.2.4/5.2.5), returned as QualifiedName and LocalizedText. Reading
   Value on a non-Variable Node is Bad_AttributeIdInvalid. */
void test_read_service_browsename_displayname(void) {
    mu_node_t node;
    node.node_id = (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 1, .identifier.numeric = 1000};
    node.node_class = MU_NODECLASS_OBJECT;
    node.browse_name = (mu_string_t){ 6, (const opcua_byte_t *)"MyVar1" };
    node.display_name = (mu_string_t){ 6, (const opcua_byte_t *)"MyVar1" };
    node.reference_count = 0;
    node.value = NULL;

    mu_address_space_t address_space = { .nodes = &node, .node_count = 1 };

    mu_read_value_id_t reads[3] = {
        { .node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_BROWSENAME },
        { .node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_DISPLAYNAME },
        { .node_id = node.node_id, .attribute_id = MU_ATTRIBUTEID_VALUE }
    };
    mu_read_request_t req = { .max_age = 0, .timestamps_to_return = MU_TIMESTAMPS_TO_RETURN_NEITHER,
                              .nodes_to_read = reads, .num_nodes_to_read = 3 };

    mu_read_response_t resp;
    mu_datavalue_t results[3];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_read_process(&address_space, &req, &resp, results, 3));

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_service_request_decode);
    RUN_TEST(test_read_service_scalar_values);
    RUN_TEST(test_read_service_browsename_displayname);
    return UNITY_END();
}
