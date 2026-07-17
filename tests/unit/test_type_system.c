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

#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* spec 080b re-parents the numeric primitives under Integer/UInteger so the
       advertised DataType tree matches OPC-10000-3: Int32/UInt32 are no longer
       direct BaseDataType children. */
    TEST_ASSERT_TRUE(has_forward_ref(27u, 45u, 6u));  /* Integer -> Int32 */
    TEST_ASSERT_TRUE(has_forward_ref(28u, 45u, 7u));  /* UInteger -> UInt32 */
    TEST_ASSERT_FALSE(has_forward_ref(24u, 45u, 6u)); /* no longer direct */
    TEST_ASSERT_FALSE(has_forward_ref(24u, 45u, 7u));
#else
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 6u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 7u));
#endif
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 12u)); /* String stays direct */
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 13u)); /* DateTime stays direct */

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

#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
/* spec 079: specialized DataType nodes and their supertype closure (CU 4426 Decimal,
   CU 2483 Date DataTypes). */
void test_specialized_datatype_nodes_and_supertypes(void) {
    assert_node(26u, MU_NODECLASS_DATATYPE, "Number");
    assert_node(50u, MU_NODECLASS_DATATYPE, "Decimal");
    assert_node(12879u, MU_NODECLASS_DATATYPE, "DurationString");
    assert_node(12880u, MU_NODECLASS_DATATYPE, "TimeString");
    assert_node(12881u, MU_NODECLASS_DATATYPE, "DateString");

    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 26u));    /* BaseDataType -> Number */
    TEST_ASSERT_TRUE(has_forward_ref(26u, 45u, 50u));    /* Number -> Decimal */
    TEST_ASSERT_TRUE(has_forward_ref(12u, 45u, 12879u)); /* String -> DurationString */
    TEST_ASSERT_TRUE(has_forward_ref(12u, 45u, 12880u)); /* String -> TimeString */
    TEST_ASSERT_TRUE(has_forward_ref(12u, 45u, 12881u)); /* String -> DateString */
}
#endif

#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE
/* spec 080a: Argument DataType + its Encoding Objects and the shared encoding infra
   (CU 3641). */
void test_argument_datatype_and_encodings(void) {
    assert_node(296u, MU_NODECLASS_DATATYPE, "Argument");
    assert_node(76u, MU_NODECLASS_OBJECTTYPE, "DataTypeEncodingType");
    assert_node(38u, MU_NODECLASS_REFERENCETYPE, "HasEncoding");

    TEST_ASSERT_TRUE(has_forward_ref(22u, 45u, 296u));  /* Structure -> Argument */
    TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, 76u));   /* BaseObjectType -> DataTypeEncodingType */
    TEST_ASSERT_TRUE(has_forward_ref(296u, 38u, 297u)); /* Argument -HasEncoding-> Default XML */
    TEST_ASSERT_TRUE(has_forward_ref(296u, 38u, 298u)); /* Argument -HasEncoding-> Default Binary */
}
#endif

#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
/* spec 080b: the remaining base type-system nodes (CU 3188 Base Types) and the
   core type folders (CU 3185). */
static void assert_type_definition(opcua_uint32_t id, opcua_uint32_t expected) {
    const mu_node_t *node = base_node(id);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(MU_NODEID_NUMERIC, node->type_definition.identifier_type);
    TEST_ASSERT_EQUAL_UINT(0u, node->type_definition.namespace_index);
    TEST_ASSERT_EQUAL_UINT32(expected, node->type_definition.identifier.numeric);
}

void test_base_types_and_modelling_rules(void) {
    /* Built-in / abstract DataTypes still missing before this slice. */
    assert_node(14u, MU_NODECLASS_DATATYPE, "Guid");
    assert_node(15u, MU_NODECLASS_DATATYPE, "ByteString");
    assert_node(16u, MU_NODECLASS_DATATYPE, "XmlElement");
    assert_node(18u, MU_NODECLASS_DATATYPE, "ExpandedNodeId");
    assert_node(23u, MU_NODECLASS_DATATYPE, "DataValue");
    assert_node(25u, MU_NODECLASS_DATATYPE, "DiagnosticInfo");
    assert_node(27u, MU_NODECLASS_DATATYPE, "Integer");
    assert_node(28u, MU_NODECLASS_DATATYPE, "UInteger");
    assert_node(29u, MU_NODECLASS_DATATYPE, "Enumeration");
    assert_node(290u, MU_NODECLASS_DATATYPE, "Duration");
    assert_node(291u, MU_NODECLASS_DATATYPE, "NumericRange");
    assert_node(294u, MU_NODECLASS_DATATYPE, "UtcTime");
    assert_node(7594u, MU_NODECLASS_DATATYPE, "EnumValueType");
    assert_node(12756u, MU_NODECLASS_DATATYPE, "Union");

    /* ReferenceType + ObjectType. */
    assert_node(37u, MU_NODECLASS_REFERENCETYPE, "HasModellingRule");
    assert_node(77u, MU_NODECLASS_OBJECTTYPE, "ModellingRuleType");

    /* ModellingRule Objects (typed by ModellingRuleType 77). */
    assert_node(78u, MU_NODECLASS_OBJECT, "Mandatory");
    assert_node(80u, MU_NODECLASS_OBJECT, "Optional");
    assert_node(83u, MU_NODECLASS_OBJECT, "ExposesItsArray");
    assert_node(11508u, MU_NODECLASS_OBJECT, "OptionalPlaceholder");
    assert_node(11510u, MU_NODECLASS_OBJECT, "MandatoryPlaceholder");
    assert_type_definition(78u, 77u);
    assert_type_definition(80u, 77u);
    assert_type_definition(83u, 77u);
    assert_type_definition(11508u, 77u);
    assert_type_definition(11510u, 77u);

    /* EnumValueType Encoding Objects (typed by DataTypeEncodingType 76). */
    assert_node(7616u, MU_NODECLASS_OBJECT, "Default XML");
    assert_node(8251u, MU_NODECLASS_OBJECT, "Default Binary");
    assert_type_definition(7616u, 76u);
    assert_type_definition(8251u, 76u);

    /* HasSubtype closure: direct BaseDataType children. */
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 14u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 15u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 16u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 18u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 23u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 25u));
    TEST_ASSERT_TRUE(has_forward_ref(24u, 45u, 29u));

    /* Number(26) -> Integer/UInteger/Float/Double (Decimal covered by DATATYPES). */
    TEST_ASSERT_TRUE(has_forward_ref(26u, 45u, 27u));
    TEST_ASSERT_TRUE(has_forward_ref(26u, 45u, 28u));
    TEST_ASSERT_TRUE(has_forward_ref(26u, 45u, 10u));
    TEST_ASSERT_TRUE(has_forward_ref(26u, 45u, 11u));

    /* Integer -> signed; UInteger -> unsigned. */
    TEST_ASSERT_TRUE(has_forward_ref(27u, 45u, 2u)); /* SByte */
    TEST_ASSERT_TRUE(has_forward_ref(27u, 45u, 4u)); /* Int16 */
    TEST_ASSERT_TRUE(has_forward_ref(27u, 45u, 8u)); /* Int64 */
    TEST_ASSERT_TRUE(has_forward_ref(28u, 45u, 3u)); /* Byte */
    TEST_ASSERT_TRUE(has_forward_ref(28u, 45u, 5u)); /* UInt16 */
    TEST_ASSERT_TRUE(has_forward_ref(28u, 45u, 9u)); /* UInt64 */

    /* Specialized leaf subtypes. */
    TEST_ASSERT_TRUE(has_forward_ref(11u, 45u, 290u));   /* Double -> Duration */
    TEST_ASSERT_TRUE(has_forward_ref(12u, 45u, 291u));   /* String -> NumericRange */
    TEST_ASSERT_TRUE(has_forward_ref(13u, 45u, 294u));   /* DateTime -> UtcTime */
    TEST_ASSERT_TRUE(has_forward_ref(22u, 45u, 7594u));  /* Structure -> EnumValueType */
    TEST_ASSERT_TRUE(has_forward_ref(22u, 45u, 12756u)); /* Structure -> Union */
    TEST_ASSERT_TRUE(has_forward_ref(32u, 45u, 37u));    /* NonHier -> HasModellingRule */
    TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, 77u));    /* BaseObjectType -> ModellingRuleType */

    /* EnumValueType HasEncoding -> its Encoding Objects. */
    TEST_ASSERT_TRUE(has_forward_ref(7594u, 38u, 7616u));
    TEST_ASSERT_TRUE(has_forward_ref(7594u, 38u, 8251u));

    /* CU 3185: the core type folders remain present (entry points). */
    assert_node(86u, MU_NODECLASS_OBJECT, "Types");
    assert_node(88u, MU_NODECLASS_OBJECT, "ObjectTypes");
    assert_node(89u, MU_NODECLASS_OBJECT, "VariableTypes");
    assert_node(90u, MU_NODECLASS_OBJECT, "DataTypes");
    assert_node(91u, MU_NODECLASS_OBJECT, "ReferenceTypes");
}
#endif

#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
/* spec 083: ServerType type tree (CU 3189) -- ObjectTypes/VariableTypes/DataTypes
   and their Encoding Objects that make up the Server object's type system. Nodes
   land in later tasks (3/4/5); this test only asserts the HasSubtype/HasEncoding
   reference closure. */
static void test_servertype_type_tree_and_encodings(void) {
    /* ObjectTypes under BaseObjectType(58) */
    const uint32_t obj_bo[] = {2013u, 2020u, 2026u, 2029u, 2033u, 2034u, 11575u, 11616u, 11645u};
    for (size_t i = 0; i < sizeof(obj_bo) / sizeof(obj_bo[0]); ++i) {
        TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, obj_bo[i]));
    }
    TEST_ASSERT_TRUE(has_forward_ref(2034u, 45u, 2036u));   /* Transparent */
    TEST_ASSERT_TRUE(has_forward_ref(2034u, 45u, 2039u));   /* NonTransparent */
    TEST_ASSERT_TRUE(has_forward_ref(2039u, 45u, 11945u));  /* NonTransparentNetwork */
    TEST_ASSERT_TRUE(has_forward_ref(61u, 45u, 11564u));    /* OperationLimits->Folder */
    TEST_ASSERT_TRUE(has_forward_ref(11575u, 45u, 11595u)); /* AddressSpaceFile->File */

    /* VariableTypes under BaseDataVariableType(63) */
    const uint32_t vt[] = {2137u, 2138u, 2150u, 2164u, 2165u, 2171u, 2172u, 2196u, 2197u, 2243u, 2244u, 3051u};
    for (size_t i = 0; i < sizeof(vt) / sizeof(vt[0]); ++i) {
        TEST_ASSERT_TRUE(has_forward_ref(63u, 45u, vt[i]));
    }

    /* DataTypes: Structure(22) + Enumeration(29) subtypes */
    const uint32_t dt_struct[] = {338u, 853u, 856u, 859u, 862u, 865u, 868u, 871u, 874u, 11943u, 11944u};
    for (size_t i = 0; i < sizeof(dt_struct) / sizeof(dt_struct[0]); ++i) {
        TEST_ASSERT_TRUE(has_forward_ref(22u, 45u, dt_struct[i]));
    }
    TEST_ASSERT_TRUE(has_forward_ref(29u, 45u, 851u));
    TEST_ASSERT_TRUE(has_forward_ref(29u, 45u, 852u));

    /* Encoding Objects reachable via HasEncoding(38) from their DataType */
    TEST_ASSERT_TRUE(has_forward_ref(862u, 38u, 863u));     /* ServerStatus XML */
    TEST_ASSERT_TRUE(has_forward_ref(862u, 38u, 864u));     /* ServerStatus Binary */
    TEST_ASSERT_TRUE(has_forward_ref(338u, 38u, 339u));     /* BuildInfo XML */
    TEST_ASSERT_TRUE(has_forward_ref(338u, 38u, 340u));     /* BuildInfo Binary */
    TEST_ASSERT_TRUE(has_forward_ref(11944u, 38u, 11958u)); /* NetworkGroup Binary */
}
#endif

#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
/* spec 085: core ServerType-tree InstanceDeclarations (first slice toward CU 5801).
   Each type's own Mandatory/Optional child Variables/Objects, with HasComponent/
   HasProperty from the type + a HasModellingRule edge. NodeIds grounded against
   docs/superpowers/plans/2026-07-17-cu-5801-core-node-inventory.md (OPC-10000-5
   §7.6/§7.7/§6.3.2/§7.8/§6.3.1). */
static void test_servertype_instance_declarations(void) {
    /* ServerStatusType(2138) components (HasComponent 47) */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2139u));  /* StartTime */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2140u));  /* CurrentTime */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2141u));  /* State */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2142u));  /* BuildInfo */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2752u));  /* SecondsTillShutdown */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2753u));  /* ShutdownReason */

    /* BuildInfoType(3051) components */
    TEST_ASSERT_TRUE(has_forward_ref(3051u, 47u, 3052u));  /* ProductUri */
    TEST_ASSERT_TRUE(has_forward_ref(3051u, 47u, 3057u));  /* BuildDate */

    /* ServerDiagnosticsSummaryType(2150) first component */
    TEST_ASSERT_TRUE(has_forward_ref(2150u, 47u, 2151u));  /* ServerViewCount */

    /* ServerCapabilitiesType(2013) first property (HasProperty 46) */
    TEST_ASSERT_TRUE(has_forward_ref(2013u, 46u, 2014u));  /* ServerProfileArray */

    /* ServerType(2004) direct children -- from the inventory doc's ServerType table:
       ServerArray is a Property, ServerStatus/ServerCapabilities are Components. */
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 46u, 2005u));  /* ServerArray (HasProperty) */
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 47u, 2007u));  /* ServerStatus (HasComponent) */
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 47u, 2009u));  /* ServerCapabilities (HasComponent) */

    /* HasModellingRule(37): a Mandatory decl -> Mandatory(78) */
    TEST_ASSERT_TRUE(has_forward_ref(2139u, 37u, 78u));    /* StartTime is Mandatory */
}
#endif

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
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
    RUN_TEST(test_specialized_datatype_nodes_and_supertypes);
#endif
#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE
    RUN_TEST(test_argument_datatype_and_encodings);
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    RUN_TEST(test_base_types_and_modelling_rules);
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    RUN_TEST(test_servertype_type_tree_and_encodings);
#endif
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    RUN_TEST(test_servertype_instance_declarations);
#endif
#elif MUC_OPCUA_BASE_NODES
    RUN_TEST(test_default_build_keeps_types_folder_unexpanded);
#endif
    return UNITY_END();
}
