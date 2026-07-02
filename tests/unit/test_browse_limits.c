/* tests/unit/test_browse_limits.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#include "../../src/services/browse.h"

static mu_node_t nodes[2];
static mu_reference_t refs[3];
static mu_address_space_t address_space;

void setUp(void) {
    nodes[0].node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 84};
    nodes[0].node_class = 1; /* Object */
    nodes[0].reference_count = 3;
    nodes[0].references = refs;

    nodes[1].node_id =
        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 85};
    nodes[1].node_class = 1;
    nodes[1].reference_count = 0;

    for (int i = 0; i < 3; ++i) {
        refs[i].reference_type_id =
            (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 35};
        refs[i].target_id = nodes[1].node_id;
        refs[i].is_forward = true;
    }

    address_space.nodes = nodes;
    address_space.node_count = 2;
}

void tearDown(void) {}

void test_browse_requested_max_references(void) {
    mu_browse_description_t desc = {
        .node_id = nodes[0].node_id,
        .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
        .reference_type_id =
            (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    mu_browse_request_t req = {.requested_max_references_per_node = 2, /* Limit to 2, but we have 3 */
                               .nodes_to_browse = &desc,
                               .num_nodes_to_browse = 1};

    mu_browse_result_t result;
    mu_reference_description_t ref_pool[10];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(&address_space, NULL, &req, &result, 1, ref_pool, 10));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOCONTINUATIONPOINTS, result.status_code);
    TEST_ASSERT_EQUAL(0, result.num_references);
}

void test_browse_response_size_bounds(void) {
    mu_browse_description_t desc = {
        .node_id = nodes[0].node_id,
        .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
        .reference_type_id =
            (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0},
        .include_subtypes = true,
        .node_class_mask = 0,
        .result_mask = 0x3F};

    mu_browse_request_t req = {
        .requested_max_references_per_node = 10, .nodes_to_browse = &desc, .num_nodes_to_browse = 1};

    mu_browse_result_t result;
    mu_reference_description_t ref_pool[2]; /* Only 2 available in pool, need 3 */

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(&address_space, NULL, &req, &result, 1, ref_pool, 2));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOCONTINUATIONPOINTS, result.status_code);
}

void test_browse_no_continuation_points(void) {
    /* Covered by the tests above */
}

/* A client (e.g. asyncua get_children) browses HierarchicalReferences (i=33) with
   includeSubtypes=true. The address-space references are Organizes (i=35), a subtype
   of HierarchicalReferences, so they must match. (OPC 10000-4 5.9.2, includeSubtypes) */
void test_browse_include_subtypes_matches_organizes(void) {
    mu_browse_description_t desc = {.node_id = nodes[0].node_id,
                                    .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
                                    .reference_type_id =
                                        (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC,
                                                      .namespace_index = 0,
                                                      .identifier.numeric = 33}, /* HierarchicalReferences */
                                    .include_subtypes = true,
                                    .node_class_mask = 0,
                                    .result_mask = 0x3F};
    mu_browse_request_t req = {
        .requested_max_references_per_node = 0, .nodes_to_browse = &desc, .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[10];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(&address_space, NULL, &req, &result, 1, ref_pool, 10));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result.status_code);
    TEST_ASSERT_EQUAL(3, result.num_references); /* all three Organizes refs match */
}

/* Without includeSubtypes, a base type does not match its subtypes. */
void test_browse_no_subtypes_excludes_organizes(void) {
    mu_browse_description_t desc = {
        .node_id = nodes[0].node_id,
        .browse_direction = MU_BROWSE_DIRECTION_FORWARD,
        .reference_type_id =
            (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 33},
        .include_subtypes = false,
        .node_class_mask = 0,
        .result_mask = 0x3F};
    mu_browse_request_t req = {
        .requested_max_references_per_node = 0, .nodes_to_browse = &desc, .num_nodes_to_browse = 1};
    mu_browse_result_t result;
    mu_reference_description_t ref_pool[10];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_browse_process(&address_space, NULL, &req, &result, 1, ref_pool, 10));
    TEST_ASSERT_EQUAL(0, result.num_references); /* exact HierarchicalReferences only */
}

/* Feature 028 (T013): a Browse request whose nodesToBrowse count exceeds the
   caller's fixed capacity must decode to Bad_TooManyOperations (OPC-10000-4
   §5.9.2 / §7.38.2), consistent with Read/Write/Query — not Bad_InternalError. */
void test_browse_over_capacity_is_too_many_operations(void) {
    opcua_byte_t buf[64];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    mu_nodeid_t null_view = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_nodeid(&w, &null_view);   /* ViewDescription.viewId */
    mu_binary_write_int64(&w, 0);             /* timestamp */
    mu_binary_write_uint32(&w, 0);            /* viewVersion */
    mu_binary_write_uint32(&w, 0);            /* requestedMaxReferencesPerNode */
    mu_binary_write_int32(&w, 5);             /* nodesToBrowse count = 5 */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    mu_browse_request_t req;
    mu_browse_description_t desc_array[1]; /* capacity 1 < 5 */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS,
                            mu_browse_request_decode(&r, &req, desc_array, 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browse_requested_max_references);
    RUN_TEST(test_browse_over_capacity_is_too_many_operations);
    RUN_TEST(test_browse_response_size_bounds);
    RUN_TEST(test_browse_no_continuation_points);
    RUN_TEST(test_browse_include_subtypes_matches_organizes);
    RUN_TEST(test_browse_no_subtypes_excludes_organizes);
    return UNITY_END();
}
