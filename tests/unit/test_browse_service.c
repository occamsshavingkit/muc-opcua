/* tests/unit/test_browse_service.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/browse.h"

void test_browse_service_static_references(void) {
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    /* ViewDescription */
    mu_nodeid_t empty_nodeid = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};
    mu_binary_write_nodeid(&writer, &empty_nodeid); /* empty nodeid */
    mu_binary_write_int64(&writer, 0);              /* timestamp */
    mu_binary_write_uint32(&writer, 0);             /* view version */

    /* RequestedMaxReferencesPerNode */
    mu_binary_write_uint32(&writer, 10);

    /* NoOfNodesToBrowse */
    mu_binary_write_int32(&writer, 1);

    /* NodesToBrowse[0] */
    mu_nodeid_t root_folder = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84};
    mu_nodeid_t hier_ref = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 33};
    mu_binary_write_nodeid(&writer, &root_folder); /* RootFolder */
    mu_binary_write_uint32(&writer, MU_BROWSE_DIRECTION_FORWARD);
    mu_binary_write_nodeid(&writer, &hier_ref); /* HierarchicalReferences */
    mu_binary_write_boolean(&writer, true);
    mu_binary_write_uint32(&writer, 0xFFFFFFFF); /* All classes */
    mu_binary_write_uint32(&writer, 0x3F);       /* Result mask */

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);

    mu_browse_request_t req;
    mu_browse_description_t desc[2];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_request_decode(&reader, &req, desc, 2));
    TEST_ASSERT_EQUAL(10, req.requested_max_references_per_node);
    TEST_ASSERT_EQUAL(1, req.num_nodes_to_browse);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, req.nodes_to_browse[0].node_id.identifier_type);
    TEST_ASSERT_EQUAL(84, req.nodes_to_browse[0].node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_BROWSE_DIRECTION_FORWARD, req.nodes_to_browse[0].browse_direction);
    TEST_ASSERT_TRUE(req.nodes_to_browse[0].include_subtypes);
}

void test_browse_service_response_encode(void) {
    opcua_byte_t buffer[256];
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    mu_reference_description_t ref = {
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 35},
        .is_forward = true,
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85},
        .browse_name_namespace_index = 0,
        .browse_name = {.length = 7, .data = (opcua_byte_t *)"Objects"},
        .display_name = {.length = 7, .data = (opcua_byte_t *)"Objects"},
        .node_class = 1, /* Object */
        .type_definition = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 61}};

    mu_browse_result_t res = {.status_code = MU_STATUS_GOOD, .references = &ref, .num_references = 1};

    mu_browse_response_t resp = {.results = &res, .num_results = 1};

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_response_encode(&writer, &resp));

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, writer.position);

    opcua_int32_t num_results;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &num_results));
    TEST_ASSERT_EQUAL(1, num_results);

    opcua_statuscode_t status;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, &status));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    opcua_int32_t cp_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &cp_len));
    TEST_ASSERT_EQUAL(-1, cp_len);

    opcua_int32_t num_refs;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &num_refs));
    TEST_ASSERT_EQUAL(1, num_refs);

    mu_nodeid_t ref_type_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &ref_type_id));
    TEST_ASSERT_EQUAL(35, ref_type_id.identifier.numeric);

    opcua_boolean_t is_forward;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_boolean(&reader, &is_forward));
    TEST_ASSERT_TRUE(is_forward);

    mu_nodeid_t node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &node_id));
    TEST_ASSERT_EQUAL(85, node_id.identifier.numeric);

    opcua_uint16_t browse_name_ns;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint16(&reader, &browse_name_ns));
    TEST_ASSERT_EQUAL(0, browse_name_ns);

    mu_string_t browse_name;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &browse_name));
    TEST_ASSERT_EQUAL(7, browse_name.length);

    opcua_byte_t lt_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &lt_mask));
    TEST_ASSERT_EQUAL(2, lt_mask);

    mu_string_t disp_name;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &disp_name));
    TEST_ASSERT_EQUAL(7, disp_name.length);

    opcua_uint32_t node_class;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &node_class));
    TEST_ASSERT_EQUAL(1, node_class);

    mu_nodeid_t type_def;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &type_def));
    TEST_ASSERT_EQUAL(61, type_def.identifier.numeric);

    opcua_int32_t diag_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &diag_len));
    TEST_ASSERT_EQUAL(0, diag_len);

    TEST_ASSERT_EQUAL(writer.position, reader.position);
}

void test_browse_service_rejects_invalid_browse_direction(void) {
    mu_browse_description_t desc = {
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84},
        .browse_direction = 3,
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    /* OPC-10000-4 section 5.9.2.4 lists Bad_BrowseDirectionInvalid for Browse
       results, and section 7.38.2 defines it for invalid BrowseDirection values. */
    mu_browse_request_t req = {
        .requested_max_references_per_node = 0, .nodes_to_browse = &desc, .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[4];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(NULL, NULL, &req, &result, 1, ref_pool, 4));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_BROWSEDIRECTIONINVALID, result.status_code);
    TEST_ASSERT_EQUAL(0, result.num_references);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browse_service_static_references);
    RUN_TEST(test_browse_service_response_encode);
    RUN_TEST(test_browse_service_rejects_invalid_browse_direction);
    return UNITY_END();
}
