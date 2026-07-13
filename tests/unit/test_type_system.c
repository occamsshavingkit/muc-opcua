/* tests/unit/test_type_system.c
 *
 * Feature 005 US2: Base Info Type System exposure. The expanded namespace-0
 * type-system checks run only when MUC_OPCUA_BASE_TYPE_SYSTEM is enabled.
 * The default build keeps a regression that the older nano/micro base-node
 * surface is unchanged.
 *
 * OPC-10000-3 7.7: HasTypeDefinition references.
 * OPC-10000-5: standard ReferenceType, DataType, ObjectType, and VariableType nodes.
 */
#include "unity.h"

#include "../../src/address_space/base_nodes.h"
#include "muc_opcua/address_space.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static mu_nodeid_t ns0(opcua_uint32_t id) {
    mu_nodeid_t node_id = {0u, MU_NODEID_NUMERIC, {id}};
    return node_id;
}

static const mu_node_t *base_node(opcua_uint32_t id) {
    mu_nodeid_t node_id = ns0(id);
    return mu_address_space_find_node(mu_base_address_space(), &node_id);
}

static bool string_equals(const mu_string_t *text, const char *expected) {
    size_t length = strlen(expected);
    return text->length == (opcua_int32_t)length && text->data != NULL && memcmp(text->data, expected, length) == 0;
}

static bool has_forward_ref(opcua_uint32_t source_id, opcua_uint32_t ref_type_id, opcua_uint32_t target_id) {
    const mu_node_t *source = base_node(source_id);
    TEST_ASSERT_NOT_NULL(source);

    for (size_t i = 0u; i < source->reference_count; ++i) {
        const mu_reference_t *ref = &source->references[i];
        if (ref->is_forward && ref->reference_type_id.identifier_type == MU_NODEID_NUMERIC &&
            ref->reference_type_id.namespace_index == 0u && ref->reference_type_id.identifier.numeric == ref_type_id &&
            ref->target_id.identifier_type == MU_NODEID_NUMERIC && ref->target_id.namespace_index == 0u &&
            ref->target_id.identifier.numeric == target_id) {
            return true;
        }
    }
    return false;
}

#if MUC_OPCUA_BASE_TYPE_SYSTEM

static void assert_node(opcua_uint32_t id, mu_node_class_t node_class, const char *browse_name) {
    const mu_node_t *node = base_node(id);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(node_class, node->node_class);
    TEST_ASSERT_TRUE_MESSAGE(string_equals(&node->browse_name, browse_name), browse_name);
}

void test_namespace0_type_system_nodes_are_present(void) {
    assert_node(91u, MU_NODECLASS_OBJECT, "ReferenceTypes");
    assert_node(31u, MU_NODECLASS_REFERENCETYPE, "References");
    assert_node(33u, MU_NODECLASS_REFERENCETYPE, "HierarchicalReferences");
    assert_node(35u, MU_NODECLASS_REFERENCETYPE, "Organizes");
    assert_node(40u, MU_NODECLASS_REFERENCETYPE, "HasTypeDefinition");
    assert_node(45u, MU_NODECLASS_REFERENCETYPE, "HasSubtype");
    assert_node(46u, MU_NODECLASS_REFERENCETYPE, "HasProperty");
    assert_node(47u, MU_NODECLASS_REFERENCETYPE, "HasComponent");

    assert_node(90u, MU_NODECLASS_OBJECT, "DataTypes");
    assert_node(24u, MU_NODECLASS_DATATYPE, "BaseDataType");
    assert_node(6u, MU_NODECLASS_DATATYPE, "Int32");
    assert_node(7u, MU_NODECLASS_DATATYPE, "UInt32");
    assert_node(12u, MU_NODECLASS_DATATYPE, "String");
    assert_node(13u, MU_NODECLASS_DATATYPE, "DateTime");

    assert_node(88u, MU_NODECLASS_OBJECT, "ObjectTypes");
    assert_node(58u, MU_NODECLASS_OBJECTTYPE, "BaseObjectType");
    assert_node(61u, MU_NODECLASS_OBJECTTYPE, "FolderType");
    assert_node(2004u, MU_NODECLASS_OBJECTTYPE, "ServerType");

    assert_node(89u, MU_NODECLASS_OBJECT, "VariableTypes");
    assert_node(62u, MU_NODECLASS_VARIABLETYPE, "BaseVariableType");
    assert_node(63u, MU_NODECLASS_VARIABLETYPE, "BaseDataVariableType");
    assert_node(68u, MU_NODECLASS_VARIABLETYPE, "PropertyType");
}

void test_type_hierarchies_have_subtype_references(void) {
    TEST_ASSERT_TRUE(has_forward_ref(86u, 35u, 88u));
    TEST_ASSERT_TRUE(has_forward_ref(86u, 35u, 89u));
    TEST_ASSERT_TRUE(has_forward_ref(86u, 35u, 90u));
    TEST_ASSERT_TRUE(has_forward_ref(86u, 35u, 91u));

    TEST_ASSERT_TRUE(has_forward_ref(31u, 45u, 33u));
    TEST_ASSERT_TRUE(has_forward_ref(33u, 45u, 34u));
    TEST_ASSERT_TRUE(has_forward_ref(34u, 45u, 45u));
    TEST_ASSERT_TRUE(has_forward_ref(44u, 45u, 46u));
    TEST_ASSERT_TRUE(has_forward_ref(44u, 45u, 47u));

    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 6u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 7u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 12u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 13u));

    TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, 61u));
    TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, 2004u));
    TEST_ASSERT_TRUE(has_forward_ref(62u, 45u, 63u));
    TEST_ASSERT_TRUE(has_forward_ref(63u, 45u, 68u));
}

void test_instances_have_type_definition_references(void) {
    TEST_ASSERT_TRUE(has_forward_ref(2253u, 40u, 2004u));
    TEST_ASSERT_TRUE(has_forward_ref(85u, 40u, 61u));
    TEST_ASSERT_TRUE(has_forward_ref(86u, 40u, 61u));
    TEST_ASSERT_TRUE(has_forward_ref(2269u, 40u, 68u));
    TEST_ASSERT_TRUE(has_forward_ref(11705u, 40u, 68u));
}

void test_server_profile_array_advertises_embedded_profile(void) {
#if MUC_OPCUA_MARKER_STANDARD_PROFILE
    static const char embedded_profile[] = "http://opcfoundation.org/UA-Profile/Server/StandardUA2017";
#else
    static const char embedded_profile[] = "http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2017";
#endif
    const mu_node_t *node = base_node(2269u);
    mu_variant_t value;

    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_NOT_NULL(node->value);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_value_source_read(node->value, &node->node_id, &value));
    TEST_ASSERT_EQUAL_INT(MU_TYPE_STRING, value.type);
    TEST_ASSERT_TRUE(value.is_array);
    TEST_ASSERT_EQUAL_INT32(1, value.array_length);

    const mu_string_t *profiles = (const mu_string_t *)value.value.array;
    TEST_ASSERT_TRUE(string_equals(&profiles[0], embedded_profile));
}

#elif MUC_OPCUA_BASE_NODES
/* Node 86 (the Types folder) and 2269 (ServerProfileArray) only exist at all
   when MUC_OPCUA_BASE_NODES is on (src/address_space/base_nodes.c's whole
   s_base_nodes[] table is #ifdef MUC_OPCUA_BASE_NODES; mu_base_address_space()
   returns {NULL, 0} otherwise). Nano/Micro leave BASE_NODES off entirely (the
   application supplies its own address space instead, e.g.
   examples/minimal_server/static_address_space.c), so this "present but
   unexpanded" check only applies to the BASE_NODES-on, BASE_TYPE_SYSTEM-off
   combination -- not achieved by any named profile, but a valid custom build. */

void test_default_build_keeps_types_folder_unexpanded(void) {
    const mu_node_t *types = base_node(86u);
    const mu_node_t *server_profile_array = base_node(2269u);
    mu_variant_t value;

    TEST_ASSERT_NOT_NULL(types);
    TEST_ASSERT_EQUAL_UINT(0u, types->reference_count);
    TEST_ASSERT_NULL(base_node(91u));
    TEST_ASSERT_NULL(base_node(31u));

    TEST_ASSERT_NOT_NULL(server_profile_array);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_value_source_read(server_profile_array->value, &server_profile_array->node_id, &value));
    TEST_ASSERT_EQUAL_INT(MU_TYPE_STRING, value.type);
    TEST_ASSERT_TRUE(value.is_array);
    TEST_ASSERT_EQUAL_INT32(1, value.array_length);
    {
        const mu_string_t *profiles = (const mu_string_t *)value.value.array;
        TEST_ASSERT_TRUE(
            string_equals(&profiles[0], "http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017"));
    }
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_BASE_TYPE_SYSTEM
    RUN_TEST(test_namespace0_type_system_nodes_are_present);
    RUN_TEST(test_type_hierarchies_have_subtype_references);
    RUN_TEST(test_instances_have_type_definition_references);
    RUN_TEST(test_server_profile_array_advertises_embedded_profile);
#elif MUC_OPCUA_BASE_NODES
    RUN_TEST(test_default_build_keeps_types_folder_unexpanded);
#endif
    return UNITY_END();
}
