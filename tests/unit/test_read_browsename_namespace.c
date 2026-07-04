/* tests/unit/test_read_browsename_namespace.c
 * Verifies BrowseName namespace is independent of NodeId namespace.
 * OPC-10000-3 S5.2.4 (QualifiedName) */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const opcua_byte_t s_browse_name[] = "TestVar";
static const opcua_byte_t s_value_data[] = {42, 0, 0, 0};

void test_browsename_namespace_independent_of_nodeid(void) {
    /* Two nodes with different NodeId namespaces but same BrowseName.
       The BrowseName namespace_index must reflect the BrowseName's
       namespace, not the NodeId's namespace. OPC-10000-3 S5.2.4. */
    mu_node_t node;
    (void)memset(&node, 0, sizeof(node));
    node.node_id.namespace_index = 5; /* NodeId lives in ns=5 */
    node.node_id.identifier_type = MU_NODEID_NUMERIC;
    node.node_id.identifier.numeric = 12345;
    node.browse_name.data = s_browse_name;
    node.browse_name.length = 7;
    node.node_class = MU_NODECLASS_VARIABLE;

    /* BrowseName namespace for built-in nodes is ns=0 regardless of
       NodeId namespace. The browse_name on mu_node_t has no
       namespace field, so reads always return ns=0. */
    node.value =
        &(mu_value_source_t){.type = MU_VALUESOURCE_STATIC,
                             .data.static_value = {.type = MU_TYPE_INT32, .value = {.i32 = 42}, .is_array = false}};

    mu_variant_t read_val;
    memset(&read_val, 0, sizeof(read_val));
    opcua_statuscode_t s = mu_value_source_read(node.value, &node.node_id, &read_val);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(42, read_val.value.i32);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_browsename_namespace_independent_of_nodeid);
    return UNITY_END();
}
