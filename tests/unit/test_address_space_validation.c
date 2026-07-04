/* tests/unit/test_address_space_validation.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

/*
 * These tests assume mu_address_space_t and mu_node_t APIs
 * which will be implemented in subsequent tasks.
 */

void setUp(void) {}
void tearDown(void) {}

void test_duplicate_nodeids_fail_validation(void) {
    mu_node_t nodes[2];
    mu_address_space_t space;

    (void)memset(nodes, 0, sizeof(nodes));
    nodes[0].node_id.identifier_type = MU_NODEID_NUMERIC;
    nodes[0].node_id.identifier.numeric = 1000;

    nodes[1].node_id.identifier_type = MU_NODEID_NUMERIC;
    nodes[1].node_id.identifier.numeric = 1000;

    space.nodes = nodes;
    space.node_count = 2;

    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, mu_address_space_validate(&space));
}

void test_unresolved_references_fail_validation(void) {
    mu_node_t nodes[1];
    mu_reference_t refs[1];
    mu_address_space_t space;

    memset(nodes, 0, sizeof(nodes));
    (void)memset(refs, 0, sizeof(refs));

    refs[0].target_id.identifier_type = MU_NODEID_NUMERIC;
    refs[0].target_id.identifier.numeric = 9999; /* Does not exist */

    nodes[0].node_id.identifier_type = MU_NODEID_NUMERIC;
    nodes[0].node_id.identifier.numeric = 1000;
    nodes[0].references = refs;
    nodes[0].reference_count = 1;

    space.nodes = nodes;
    space.node_count = 1;

    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, mu_address_space_validate(&space));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_duplicate_nodeids_fail_validation);
    RUN_TEST(test_unresolved_references_fail_validation);
    return UNITY_END();
}
