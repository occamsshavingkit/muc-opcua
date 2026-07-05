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

    /* Independent binary fixture for BrowseResponse encoding
     * OPC-10000-4 §5.9.2.2 / OPC-10000-6 §5.2.2:
     *   BrowseResult: StatusCode GOOD, ContinuationPoint null (-1),
     *   1 ReferenceDescription (Organizes ns=0;i=35, forward,
     *   ObjectsFolder ns=0;i=85, BrowseName="Objects" ns=0,
     *   DisplayName="Objects", NodeClass Object=1,
     *   TypeDefinition BaseObjectType ns=0;i=61) */
    const opcua_byte_t expected[] = {
        0x01, 0x00, 0x00, 0x00,  /* Int32: num_results=1 */
        0x00, 0x00, 0x00, 0x00,  /* StatusCode: GOOD */
        0xFF, 0xFF, 0xFF, 0xFF,  /* ContinuationPoint: null (-1) */
        0x01, 0x00, 0x00, 0x00,  /* Int32: num_references=1 */
        0x00, 0x23,                 /* NodeId TwoByte: ns=0, id=35 (Organizes) */
        0x01,                       /* Boolean: is_forward=true */
        0x00, 0x55,                 /* NodeId TwoByte: ns=0, id=85 (ObjectsFolder) */
        0x00, 0x00,                 /* UInt16: browse_name_namespace_index=0 */
        0x07, 0x00, 0x00, 0x00,  /* Int32: string length=7 */
        'O', 'b', 'j', 'e', 'c', 't', 's',  /* "Objects" */
        0x02,                       /* LocalizedText mask: text only */
        0x07, 0x00, 0x00, 0x00,  /* Int32: string length=7 */
        'O', 'b', 'j', 'e', 'c', 't', 's',  /* "Objects" */
        0x01, 0x00, 0x00, 0x00,  /* UInt32: node_class=Object(1) */
        0x00, 0x3D,                 /* NodeId TwoByte: ns=0, id=61 (BaseObjectType) */
        0x00, 0x00, 0x00, 0x00,  /* Int32: diagnosticInfos=0 (empty) */
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), writer.position);
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(expected));
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

/* T015: OPC-10000-4 §5.9.2.2 Table 34 — a non-null ViewDescription.viewId must
   restrict results to references defined for that View. This micro profile does
   not implement Views, so a non-null viewId is rejected with Bad_ViewIdUnknown
   instead of being silently ignored. A null viewId (the common case) browses
   normally. */
void test_browse_service_rejects_non_null_view_id(void) {
    mu_browse_description_t desc = {
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84},
        .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    mu_browse_request_t req = {.view_id = {.identifier_type = MU_NODEID_NUMERIC,
                                           .namespace_index = 0,
                                           .identifier.numeric = 12345}, /* non-null View */
                               .timestamp = 0,
                               .view_version = 0,
                               .requested_max_references_per_node = 0,
                               .nodes_to_browse = &desc,
                               .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[4];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(NULL, NULL, &req, &result, 1, ref_pool, 4));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_VIEWIDUNKNOWN, result.status_code);
    TEST_ASSERT_EQUAL(0, result.num_references);
}

void test_browse_service_accepts_null_view_id(void) {
    mu_browse_description_t desc = {
        .node_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84},
        .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
        .reference_type_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    /* null viewId: ns=0, numeric 0 -> browse normally (no Bad_ViewIdUnknown) */
    mu_browse_request_t req = {
        .view_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .timestamp = 0,
        .view_version = 0,
        .requested_max_references_per_node = 0,
        .nodes_to_browse = &desc,
        .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[4];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(NULL, NULL, &req, &result, 1, ref_pool, 4));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_BAD_VIEWIDUNKNOWN, result.status_code);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browse_service_static_references);
    RUN_TEST(test_browse_service_response_encode);
    RUN_TEST(test_browse_service_rejects_invalid_browse_direction);
    RUN_TEST(test_browse_service_rejects_non_null_view_id);
    RUN_TEST(test_browse_service_accepts_null_view_id);
    return UNITY_END();
}
