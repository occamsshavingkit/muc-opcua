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
#include "../../src/services/read/common.h"
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

/* CU 5801 general regression (spec 090): no exposed Variable node may declare
   a DataType that is absent from the address space, or whose NodeId resolves
   to something other than a DataType node. Runs unconditionally (any
   profile/build) by walking the raw node table directly instead of relying
   on the BASE_TYPE_SYSTEM-gated helpers below. */
static void test_no_variable_has_dangling_datatype(void) {
    const mu_address_space_t *address_space = mu_base_address_space();
    for (size_t i = 0u; i < address_space->node_count; ++i) {
        const mu_node_t *node = &address_space->nodes[i];
        if (node->node_class != MU_NODECLASS_VARIABLE || node->data_type == 0u) {
            continue;
        }
        const mu_node_t *data_type_node = base_node(node->data_type);
        TEST_ASSERT_NOT_NULL_MESSAGE(data_type_node, "Variable references a DataType node absent from the "
                                                     "address space (CU 5801 dangling reference)");
        TEST_ASSERT_EQUAL_MESSAGE(MU_NODECLASS_DATATYPE, data_type_node->node_class,
                                  "Variable's declared DataType id resolves to a non-DataType node");
    }
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
    /* ObjectTypes under BaseObjectType(58). NOTE (spec 090, CU 5801 Part A):
       FileType(11575)/NamespaceMetadataType(11616)/NamespacesType(11645) (and
       AddressSpaceFileType(11595), a FileType subtype) moved off this bare
       SERVERTYPE gate onto their true (full-only) owner CU_NAMESPACES -- see
       test_namespace_file_types_gated_by_cu_namespaces below. */
    const uint32_t obj_bo[] = {2013u, 2020u, 2026u, 2029u, 2033u, 2034u};
    for (size_t i = 0; i < sizeof(obj_bo) / sizeof(obj_bo[0]); ++i) {
        TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, obj_bo[i]));
    }
    TEST_ASSERT_TRUE(has_forward_ref(2034u, 45u, 2036u));  /* Transparent */
    TEST_ASSERT_TRUE(has_forward_ref(2034u, 45u, 2039u));  /* NonTransparent */
    TEST_ASSERT_TRUE(has_forward_ref(2039u, 45u, 11945u)); /* NonTransparentNetwork */
    TEST_ASSERT_TRUE(has_forward_ref(61u, 45u, 11564u));   /* OperationLimits->Folder */

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

/* spec 090 (CU 5801 Part A): FileType(11575)/AddressSpaceFileType(11595)/
   NamespaceMetadataType(11616)/NamespacesType(11645) are auto-scoped to their
   true owner CU_NAMESPACES (full-only), not the bare SERVERTYPE gate -- so
   embedded/standard (which enable SERVERTYPE but not CU_NAMESPACES) no longer
   pay CU 5801 completeness for types they never expose, while full (which
   enables both) still compiles them. NamespaceMetadataType(11616) is properly
   owned by CU_NAMESPACE_METADATA(3545) (also full-only); it is grouped under
   CU_NAMESPACES here as a follow-up refinement since both CUs are full-only
   for now. ServerType.Namespaces(11527), the Optional InstanceDeclaration
   whose TypeDefinition is NamespacesType, moves with it. */
static void test_namespace_file_types_gated_by_cu_namespaces(void) {
#if MUC_OPCUA_CU_NAMESPACES
    assert_node(11575u, MU_NODECLASS_OBJECTTYPE, "FileType");
    assert_node(11595u, MU_NODECLASS_OBJECTTYPE, "AddressSpaceFileType");
    assert_node(11616u, MU_NODECLASS_OBJECTTYPE, "NamespaceMetadataType");
    assert_node(11645u, MU_NODECLASS_OBJECTTYPE, "NamespacesType");
    TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, 11575u));    /* BaseObjectType->FileType */
    TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, 11616u));    /* BaseObjectType->NamespaceMetadataType */
    TEST_ASSERT_TRUE(has_forward_ref(58u, 45u, 11645u));    /* BaseObjectType->NamespacesType */
    TEST_ASSERT_TRUE(has_forward_ref(11575u, 45u, 11595u)); /* FileType->AddressSpaceFileType */
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    assert_node(11527u, MU_NODECLASS_OBJECT, "Namespaces");
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 47u, 11527u)); /* ServerType->Namespaces */
#endif
#else
    /* embedded/standard: not present, and no dangling HasSubtype ref left
       pointing at a node that isn't compiled in. */
    TEST_ASSERT_NULL(base_node(11575u));
    TEST_ASSERT_NULL(base_node(11595u));
    TEST_ASSERT_NULL(base_node(11616u));
    TEST_ASSERT_NULL(base_node(11645u));
    TEST_ASSERT_FALSE(has_forward_ref(58u, 45u, 11575u));
    TEST_ASSERT_FALSE(has_forward_ref(58u, 45u, 11616u));
    TEST_ASSERT_FALSE(has_forward_ref(58u, 45u, 11645u));
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    TEST_ASSERT_NULL(base_node(11527u));
    TEST_ASSERT_FALSE(has_forward_ref(2004u, 47u, 11527u));
#endif
#endif
}
#endif

#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
/* spec 085: core ServerType-tree InstanceDeclarations (first slice toward CU 5801).
   Each type's own Mandatory/Optional child Variables/Objects, with HasComponent/
   HasProperty from the type + a HasModellingRule edge. NodeIds grounded against
   docs/superpowers/plans/2026-07-17-cu-5801-core-node-inventory.md (OPC-10000-5
   §7.6/§7.7/§6.3.2/§7.8/§6.3.1). */
static void test_servertype_instance_declarations(void) {
    /* ServerStatusType(2138) components (HasComponent 47) */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2139u)); /* StartTime */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2140u)); /* CurrentTime */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2141u)); /* State */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2142u)); /* BuildInfo */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2752u)); /* SecondsTillShutdown */
    TEST_ASSERT_TRUE(has_forward_ref(2138u, 47u, 2753u)); /* ShutdownReason */

    /* BuildInfoType(3051) components */
    TEST_ASSERT_TRUE(has_forward_ref(3051u, 47u, 3052u)); /* ProductUri */
    TEST_ASSERT_TRUE(has_forward_ref(3051u, 47u, 3057u)); /* BuildDate */

    /* ServerDiagnosticsSummaryType(2150) first component */
    TEST_ASSERT_TRUE(has_forward_ref(2150u, 47u, 2151u)); /* ServerViewCount */

    /* ServerCapabilitiesType(2013) first property (HasProperty 46) */
    TEST_ASSERT_TRUE(has_forward_ref(2013u, 46u, 2014u));  /* ServerProfileArray */
    TEST_ASSERT_TRUE(has_forward_ref(2013u, 46u, 19809u)); /* MaxLogObjectContinuationPoints */

    /* ServerType(2004) direct children -- from the inventory doc's ServerType table:
       ServerArray is a Property, ServerStatus/ServerCapabilities are Components. */
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 46u, 2005u)); /* ServerArray (HasProperty) */
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 47u, 2007u)); /* ServerStatus (HasComponent) */
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 47u, 2009u)); /* ServerCapabilities (HasComponent) */

    /* HasModellingRule(37): a Mandatory decl -> Mandatory(78) */
    TEST_ASSERT_TRUE(has_forward_ref(2139u, 37u, 78u)); /* StartTime is Mandatory */
}

/* Reads DATATYPE + VALUERANK off `node_id` and asserts they equal
   (expected_data_type, expected_value_rank). Shared helper for the
   representative spread of CU 5801 Variable declarations below. */
static void assert_datatype_and_valuerank(opcua_uint32_t node_id, opcua_uint32_t expected_data_type,
                                          opcua_int32_t expected_value_rank) {
    mu_variant_t v = {0};
    const mu_node_t *n = base_node(node_id);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_attribute(NULL, n, MU_ATTRIBUTEID_DATATYPE, &v));
    TEST_ASSERT_EQUAL_UINT32(expected_data_type, v.value.nodeid.identifier.numeric);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, read_attribute(NULL, n, MU_ATTRIBUTEID_VALUERANK, &v));
    TEST_ASSERT_EQUAL_INT32(expected_value_rank, v.value.i32);
}

/* spec 086 Task 1/2: Variable nodes must report their true DataType/ValueRank
   attributes instead of the legacy type_definition fallback (which returns the
   TypeDefinition NodeId, e.g. PropertyType/BaseDataVariableType -- never a real
   DataType).

   ServerStatusType_StartTime(2139): OPC-10000-5 §7.6 Table 8 and the official
   OPC Foundation NodeSet2.xml (UAVariable i=2139) both give DataType=UtcTime
   (294) -- a DateTime subtype, not the erroneous DateTime(13) originally
   assumed by Task 1's placeholder assertion (that section number, §6.3.2, was
   also wrong: §6.3.2 is ServerCapabilitiesType, not ServerStatusType). Grounded
   against opc-ua-reference (search_text) and cross-checked against the raw
   NodeSet2.xml (grep 'NodeId="i=2139"'), per the "never trust an anchor list
   blindly" rule -- ValueRank stays Scalar(-1) in both readings. */
static void test_variable_reports_true_datatype_and_valuerank(void) {
    /* ServerStatusType(2138) own declarations (OPC-10000-5 §7.6). */
    assert_datatype_and_valuerank(2139u, 294u, -1); /* StartTime: UtcTime */
    assert_datatype_and_valuerank(2141u, 852u, -1); /* State: ServerState */
    assert_datatype_and_valuerank(2142u, 338u, -1); /* BuildInfo: BuildInfo */
    /* BuildInfoType(3051) own declarations (OPC-10000-5 §7.7 Table 77). */
    assert_datatype_and_valuerank(3052u, 12u, -1); /* ProductUri: String */
    /* ServerDiagnosticsSummaryType(2150) own declarations (OPC-10000-5 §7.8). */
    assert_datatype_and_valuerank(2151u, 7u, -1); /* ServerViewCount: UInt32 */
    /* ServerType(2004) direct children (OPC-10000-5 §6.3.1). */
    assert_datatype_and_valuerank(2005u, 12u, 1);   /* ServerArray: String[] */
    assert_datatype_and_valuerank(2007u, 862u, -1); /* ServerStatus: ServerStatusDataType */
}

/* spec 091 (CU 5801): one row of a VariableType's own InstanceDeclaration
   table -- reusable across every "type X HasComponent/HasProperty -> its own
   Mandatory/Optional child Variables" completeness test (OperationLimitsType,
   SamplingIntervalDiagnosticsType, and later diagnostics-type slices all
   share this exact shape). */
typedef struct {
    opcua_uint32_t node_id;
    const char *browse_name;
    opcua_uint32_t data_type;
    opcua_int32_t value_rank;
} mu_type_decl_t;

/* Asserts that `type_id` carries a `ref_kind` (HasComponent 47/HasProperty 46)
   forward ref to every decl in `decls`, and that each decl node is a Variable
   with the given BrowseName/DataType/ValueRank, HasModellingRule(37) ->
   `modelling_rule` (Mandatory 78/Optional 80), and HasTypeDefinition(40) ->
   `type_definition`. */
static void assert_type_decls(opcua_uint32_t type_id, opcua_uint32_t ref_kind, opcua_uint32_t modelling_rule,
                              opcua_uint32_t type_definition, const mu_type_decl_t *decls, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(has_forward_ref(type_id, ref_kind, decls[i].node_id), decls[i].browse_name);
        assert_node(decls[i].node_id, MU_NODECLASS_VARIABLE, decls[i].browse_name);
        assert_datatype_and_valuerank(decls[i].node_id, decls[i].data_type, decls[i].value_rank);
        TEST_ASSERT_TRUE_MESSAGE(has_forward_ref(decls[i].node_id, 37u, modelling_rule), decls[i].browse_name);
        TEST_ASSERT_TRUE_MESSAGE(has_forward_ref(decls[i].node_id, 40u, type_definition), decls[i].browse_name);
    }
}

/* spec 090 (CU 5801 Part B): OperationLimitsType(11564)'s 12 own Optional
   Property InstanceDeclarations, formally defined in OPC-10000-5 §6.3.11
   Table 20. NodeIds grounded against the official OPC Foundation NodeIds.csv
   (files.opcfoundation.org/schemas/1.05/NodeIds.csv) -- distinct from the
   runtime OperationLimits(11704) object's own property NodeIds
   (11705/11707/11710/11714/...) and from ServerCapabilitiesType's/ServerType's
   own OperationLimits-property copies (e.g. 12157-12160). Table 20 gives every
   row as HasProperty/UInt32/PropertyType/Optional; that matches this test's
   expectations exactly (no deviation from the spec found). */
static void test_operation_limits_type_instance_declarations(void) {
    static const mu_type_decl_t decls[] = {
        {11565u, "MaxNodesPerRead", 7u, -1},
        {12161u, "MaxNodesPerHistoryReadData", 7u, -1},
        {12162u, "MaxNodesPerHistoryReadEvents", 7u, -1},
        {11567u, "MaxNodesPerWrite", 7u, -1},
        {12163u, "MaxNodesPerHistoryUpdateData", 7u, -1},
        {12164u, "MaxNodesPerHistoryUpdateEvents", 7u, -1},
        {11569u, "MaxNodesPerMethodCall", 7u, -1},
        {11570u, "MaxNodesPerBrowse", 7u, -1},
        {11571u, "MaxNodesPerRegisterNodes", 7u, -1},
        {11572u, "MaxNodesPerTranslateBrowsePathsToNodeIds", 7u, -1},
        {11573u, "MaxNodesPerNodeManagement", 7u, -1},
        {11574u, "MaxMonitoredItemsPerCall", 7u, -1},
    };

    /* OperationLimitsType -HasProperty(46)-> decl, Optional(80), PropertyType(68) */
    assert_type_decls(11564u, 46u, 80u, 68u, decls, sizeof(decls) / sizeof(decls[0]));
}

/* spec 091 (CU 5801): SamplingIntervalDiagnosticsType(2165)'s 4 own Mandatory
   HasComponent InstanceDeclarations (OPC-10000-5 §7.10 Table 80), all
   TypeDefinition BaseDataVariableType(63). Grounded against opc-ua-reference
   (search_nodes confirms 2166/11697/11698/11699 all resolve to OPC-10000-5
   §7.10; the Table 80 text itself -- fetched via WebFetch since search_text
   doesn't return full table bodies -- gives every row as
   HasComponent/Variable/BaseDataVariableType/Mandatory, matching exactly). */
static void test_sampling_interval_diagnostics_type_instance_declarations(void) {
    static const mu_type_decl_t decls[] = {
        {2166u, "SamplingInterval", 290u, -1},                   /* Duration */
        {11697u, "SampledMonitoredItemsCount", 7u, -1},          /* UInt32 */
        {11698u, "MaxSampledMonitoredItemsCount", 7u, -1},       /* UInt32 */
        {11699u, "DisabledMonitoredItemsSamplingCount", 7u, -1}, /* UInt32 */
    };

    /* SamplingIntervalDiagnosticsType -HasComponent(47)-> decl, Mandatory(78), BaseDataVariableType(63) */
    assert_type_decls(2165u, 47u, 78u, 63u, decls, sizeof(decls) / sizeof(decls[0]));
}

/* spec 091 (CU 5801): SessionSecurityDiagnosticsType(2244)'s 9 own Mandatory
   HasComponent InstanceDeclarations (OPC-10000-5 §7.16 Table 86), all
   TypeDefinition BaseDataVariableType(63). Grounded via opc-ua-reference
   (search_nodes confirms every child NodeId resolves to §7.16; Table 86
   itself -- fetched via WebFetch since search_text truncates the reference
   table body -- gives every row as HasComponent/Variable/
   BaseDataVariableType/Mandatory, matching exactly; no HasProperty rows).
   SecurityMode(2251)'s DataType is MessageSecurityMode(302), an Enumeration
   closure DataType added below (OPC-10000-4 §7.20 Table 139). Client
   Certificate(3058, ByteString) falls inside the 2365..11461 DataAccess band,
   so base_nodes.c dual-places it (DataAccess-on and -off copies); it must
   resolve identically here regardless of which copy is compiled in. */
static void test_session_security_diagnostics_type_instance_declarations(void) {
    static const mu_type_decl_t decls[] = {
        {2245u, "SessionId", 17u, -1},               /* NodeId */
        {2246u, "ClientUserIdOfSession", 12u, -1},   /* String */
        {2247u, "ClientUserIdHistory", 12u, 1},      /* String[] */
        {2248u, "AuthenticationMechanism", 12u, -1}, /* String */
        {2249u, "Encoding", 12u, -1},                /* String */
        {2250u, "TransportProtocol", 12u, -1},       /* String */
        {2251u, "SecurityMode", 302u, -1},           /* MessageSecurityMode */
        {2252u, "SecurityPolicyUri", 12u, -1},       /* String */
        {3058u, "ClientCertificate", 15u, -1},       /* ByteString */
    };

    /* SessionSecurityDiagnosticsType -HasComponent(47)-> decl, Mandatory(78), BaseDataVariableType(63) */
    assert_type_decls(2244u, 47u, 78u, 63u, decls, sizeof(decls) / sizeof(decls[0]));

    /* MessageSecurityMode(302): Enumeration DataType, subtype of
       Enumeration(29). Per the ServerState(852)/RedundancySupport(851)
       precedent, no EnumStrings child is added. */
    assert_node(302u, MU_NODECLASS_DATATYPE, "MessageSecurityMode");
    TEST_ASSERT_TRUE(has_forward_ref(29u, 45u, 302u)); /* Enumeration -HasSubtype-> MessageSecurityMode */
}

/* spec 092 (CU 5801): ServerDiagnosticsType(2020)'s 5 own InstanceDeclarations
   (OPC-10000-5 §6.3.3 Table 11), plus the further own-instance cascades under
   ServerDiagnosticsSummary(2021) and SessionsDiagnosticsSummary(2744).
   Grounded via opc-ua-reference (search_nodes confirms every NodeId below
   resolves to §6.3.3) and the official OPC Foundation NodeIds.csv (schemas
   1.05), which gives every row's exact BrowseName/NodeClass and confirms
   NodeId 3123 is an official numbering gap (not invented here). Table 11
   (fetched via WebFetch, since search_text truncates before the table body)
   gives every row's Reference kind/ModellingRule: HasComponent/Mandatory for
   ServerDiagnosticsSummary/SubscriptionDiagnosticsArray/
   SessionsDiagnosticsSummary, HasComponent/Optional for
   SamplingIntervalDiagnosticsArray, HasProperty/Mandatory for EnabledFlag.
   SessionsDiagnosticsSummary(2744) and the 3116-3130 cascade fall inside the
   2365..11461 DataAccess band, so base_nodes.c dual-places them; this test
   must resolve identically regardless of which copy (DataAccess-on/-off) is
   compiled in. Heterogeneous TypeDefinitions across siblings (2150/2164/2171/
   2026/68 for the top level; 2196/2243 for 2744's own children) mean those
   groups can't share a single assert_type_decls call, so they're asserted
   individually; only the 12 homogeneous ServerDiagnosticsSummary field
   children (all BaseDataVariableType) reuse the helper. */
static void test_server_diagnostics_type_instance_declarations(void) {
    TEST_ASSERT_TRUE(has_forward_ref(2020u, 47u, 2021u)); /* HasComponent -> ServerDiagnosticsSummary */
    assert_node(2021u, MU_NODECLASS_VARIABLE, "ServerDiagnosticsSummary");
    assert_datatype_and_valuerank(2021u, 859u, -1);       /* ServerDiagnosticsSummaryDataType */
    TEST_ASSERT_TRUE(has_forward_ref(2021u, 37u, 78u));   /* Mandatory */
    TEST_ASSERT_TRUE(has_forward_ref(2021u, 40u, 2150u)); /* TypeDef ServerDiagnosticsSummaryType */

    TEST_ASSERT_TRUE(has_forward_ref(2020u, 47u, 2022u)); /* HasComponent -> SamplingIntervalDiagnosticsArray */
    assert_node(2022u, MU_NODECLASS_VARIABLE, "SamplingIntervalDiagnosticsArray");
    assert_datatype_and_valuerank(2022u, 856u, 1);        /* SamplingIntervalDiagnosticsDataType[] */
    TEST_ASSERT_TRUE(has_forward_ref(2022u, 37u, 80u));   /* Optional */
    TEST_ASSERT_TRUE(has_forward_ref(2022u, 40u, 2164u)); /* TypeDef SamplingIntervalDiagnosticsArrayType */

    TEST_ASSERT_TRUE(has_forward_ref(2020u, 47u, 2023u)); /* HasComponent -> SubscriptionDiagnosticsArray */
    assert_node(2023u, MU_NODECLASS_VARIABLE, "SubscriptionDiagnosticsArray");
    assert_datatype_and_valuerank(2023u, 874u, 1);        /* SubscriptionDiagnosticsDataType[] */
    TEST_ASSERT_TRUE(has_forward_ref(2023u, 37u, 78u));   /* Mandatory */
    TEST_ASSERT_TRUE(has_forward_ref(2023u, 40u, 2171u)); /* TypeDef SubscriptionDiagnosticsArrayType */

    TEST_ASSERT_TRUE(has_forward_ref(2020u, 47u, 2744u)); /* HasComponent -> SessionsDiagnosticsSummary */
    assert_node(2744u, MU_NODECLASS_OBJECT, "SessionsDiagnosticsSummary");
    TEST_ASSERT_TRUE(has_forward_ref(2744u, 37u, 78u));   /* Mandatory */
    TEST_ASSERT_TRUE(has_forward_ref(2744u, 40u, 2026u)); /* TypeDef SessionsDiagnosticsSummaryType */

    TEST_ASSERT_TRUE(has_forward_ref(2020u, 46u, 2025u)); /* HasProperty -> EnabledFlag */
    assert_node(2025u, MU_NODECLASS_VARIABLE, "EnabledFlag");
    assert_datatype_and_valuerank(2025u, 1u, -1);       /* Boolean */
    TEST_ASSERT_TRUE(has_forward_ref(2025u, 37u, 78u)); /* Mandatory */
    TEST_ASSERT_TRUE(has_forward_ref(2025u, 40u, 68u)); /* TypeDef PropertyType */

    /* Cascade under ServerDiagnosticsSummary(2021): its own 12 materialized
       field children, all HasComponent/Mandatory/BaseDataVariableType. */
    static const mu_type_decl_t summary_decls[] = {
        {3116u, "ServerViewCount", 7u, -1},
        {3117u, "CurrentSessionCount", 7u, -1},
        {3118u, "CumulatedSessionCount", 7u, -1},
        {3119u, "SecurityRejectedSessionCount", 7u, -1},
        {3120u, "RejectedSessionCount", 7u, -1},
        {3121u, "SessionTimeoutCount", 7u, -1},
        {3122u, "SessionAbortCount", 7u, -1},
        {3124u, "PublishingIntervalCount", 7u, -1},
        {3125u, "CurrentSubscriptionCount", 7u, -1},
        {3126u, "CumulatedSubscriptionCount", 7u, -1},
        {3127u, "SecurityRejectedRequestsCount", 7u, -1},
        {3128u, "RejectedRequestsCount", 7u, -1},
    };
    assert_type_decls(2021u, 47u, 78u, 63u, summary_decls, sizeof(summary_decls) / sizeof(summary_decls[0]));
    TEST_ASSERT_NULL(base_node(3123u)); /* official numbering gap -- not invented */

    /* Cascade under SessionsDiagnosticsSummary(2744): its own 2 array
       children. Heterogeneous TypeDefinitions (2196/2243), so asserted
       individually rather than via assert_type_decls. */
    TEST_ASSERT_TRUE(has_forward_ref(2744u, 47u, 3129u));
    assert_node(3129u, MU_NODECLASS_VARIABLE, "SessionDiagnosticsArray");
    assert_datatype_and_valuerank(3129u, 865u, 1); /* SessionDiagnosticsDataType[] */
    TEST_ASSERT_TRUE(has_forward_ref(3129u, 37u, 78u));
    TEST_ASSERT_TRUE(has_forward_ref(3129u, 40u, 2196u));

    TEST_ASSERT_TRUE(has_forward_ref(2744u, 47u, 3130u));
    assert_node(3130u, MU_NODECLASS_VARIABLE, "SessionSecurityDiagnosticsArray");
    assert_datatype_and_valuerank(3130u, 868u, 1); /* SessionSecurityDiagnosticsDataType[] */
    TEST_ASSERT_TRUE(has_forward_ref(3130u, 37u, 78u));
    TEST_ASSERT_TRUE(has_forward_ref(3130u, 40u, 2243u));
}

/* spec 091 (CU 5801): SessionsDiagnosticsSummaryType(2026)'s 3 own
   InstanceDeclarations (OPC-10000-5 §6.3.4), all HasComponent. Grounded via
   opc-ua-reference (search_nodes: 2027/2028/12097 all resolve to §6.3.4;
   WebFetch of the section table confirms every row is HasComponent, with
   SessionDiagnosticsArray/SessionSecurityDiagnosticsArray Mandatory and
   <ClientName> an Optional placeholder). 2027/2028 are homogeneous
   (BaseDataVariableType, Mandatory) so share assert_type_decls; <ClientName>
   (12097) is an Object OptionalPlaceholder(11508) slot with TypeDefinition
   SessionDiagnosticsObjectType(2029) -- only the bare placeholder node itself
   is emitted here (matching the ServerCapabilitiesType.<VendorCapability>
   (11562) precedent). The full SessionDiagnosticsObjectType(2029) 56-node
   cascade under this placeholder is explicitly DEFERRED to a future slice;
   it is not built here. */
static void test_sessions_diagnostics_summary_type_instance_declarations(void) {
    static const mu_type_decl_t decls[] = {
        {2027u, "SessionDiagnosticsArray", 865u, 1},         /* SessionDiagnosticsDataType[] */
        {2028u, "SessionSecurityDiagnosticsArray", 868u, 1}, /* SessionSecurityDiagnosticsDataType[] */
    };
    /* SessionsDiagnosticsSummaryType -HasComponent(47)-> decl, Mandatory(78) */
    assert_type_decls(2026u, 47u, 78u, 2196u, &decls[0], 1); /* TypeDef SessionDiagnosticsArrayType */
    assert_type_decls(2026u, 47u, 78u, 2243u, &decls[1], 1); /* TypeDef SessionSecurityDiagnosticsArrayType */

    /* <ClientName>(12097): OptionalPlaceholder(11508), TypeDef
       SessionDiagnosticsObjectType(2029). Bare placeholder only -- no
       children (deferred, see comment above). */
    TEST_ASSERT_TRUE(has_forward_ref(2026u, 47u, 12097u));
    assert_node(12097u, MU_NODECLASS_OBJECT, "<ClientName>");
    TEST_ASSERT_TRUE(has_forward_ref(12097u, 37u, 11508u)); /* HasModellingRule -> OptionalPlaceholder */
    TEST_ASSERT_TRUE(has_forward_ref(12097u, 40u, 2029u));  /* HasTypeDefinition -> SessionDiagnosticsObjectType */
}

/* spec 090 (CU 5801 fix): the 4 previously-dangling DataType references found
   by a completeness sweep. See docs/superpowers/plans (090) for the grounded
   ModellingRule/owning-CU facts behind each fix:
   - LocaleIdArray(2016) Mandatory -> LocaleId(295) DataType ADDED (subtype of
     String(12), no encodings).
   - SoftwareCertificates(3049) Mandatory -> SignedSoftwareCertificate(344)
     DataType ADDED (subtype of Structure(22), with Default XML(345)/Default
     Binary(346) Encoding Objects).
   - ServerType.UrisVersion(15003) Optional, owned by CU 3994 (Session
     Sessionless Invocation / "Sessionless Server Facet", entirely
     unimplemented in this codebase -- docs/conformance/completion.md shows
     0/2). REMOVED: not exposed in any profile until that facet is
     implemented, rather than fabricating a claim for an unimplemented CU.
   - ServerType.LocalTime(17612) Optional, owned by CU 2476 (Base Info
     LocalTime, full-only). RE-GATED from the bare SERVERTYPE &&
     TYPE_INFORMATION condition onto MUC_OPCUA_CU_BASE_INFO_LOCALTIME, matching
     its DataType TimeZoneDataType(8912)'s existing gate. */
static void test_servertype_datatype_refs_not_dangling(void) {
    /* LocaleIdArray(2016) -> LocaleId(295): Mandatory, always present here. */
    assert_node(295u, MU_NODECLASS_DATATYPE, "LocaleId");
    TEST_ASSERT_TRUE(has_forward_ref(12u, 45u, 295u)); /* String -HasSubtype-> LocaleId */
    assert_datatype_and_valuerank(2016u, 295u, 1);     /* LocaleIdArray: LocaleId[] */

    /* SoftwareCertificates(3049) -> SignedSoftwareCertificate(344): Mandatory,
       always present here, with its Structure encodings. */
    assert_node(344u, MU_NODECLASS_DATATYPE, "SignedSoftwareCertificate");
    assert_node(345u, MU_NODECLASS_OBJECT, "Default XML");
    assert_node(346u, MU_NODECLASS_OBJECT, "Default Binary");
    TEST_ASSERT_TRUE(has_forward_ref(22u, 45u, 344u));  /* Structure -HasSubtype-> SignedSoftwareCertificate */
    TEST_ASSERT_TRUE(has_forward_ref(344u, 38u, 345u)); /* -HasEncoding-> Default XML */
    TEST_ASSERT_TRUE(has_forward_ref(344u, 38u, 346u)); /* -HasEncoding-> Default Binary */
    assert_datatype_and_valuerank(3049u, 344u, 1);      /* SoftwareCertificates: SignedSoftwareCertificate[] */

    /* UrisVersion(15003)/VersionTime(20998): removed everywhere (CU 3994
       unimplemented). No dangling reference left behind. */
    TEST_ASSERT_NULL(base_node(15003u));
    TEST_ASSERT_NULL(base_node(20998u));
    TEST_ASSERT_FALSE(has_forward_ref(2004u, 46u, 15003u)); /* ServerType no longer -HasProperty-> UrisVersion */

    /* LocalTime(17612): present iff LOCALTIME (full-only); its DataType
       TimeZoneDataType(8912) shares that exact gate, so never dangling. */
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    assert_node(17612u, MU_NODECLASS_VARIABLE, "LocalTime");
    TEST_ASSERT_TRUE(has_forward_ref(2004u, 46u, 17612u)); /* ServerType -HasProperty-> LocalTime */
    assert_datatype_and_valuerank(17612u, 8912u, -1);      /* LocalTime: TimeZoneDataType */
    assert_node(8912u, MU_NODECLASS_DATATYPE, "TimeZoneDataType");
#else
    TEST_ASSERT_NULL(base_node(17612u));
    TEST_ASSERT_FALSE(has_forward_ref(2004u, 46u, 17612u));
    TEST_ASSERT_NULL(base_node(8912u));
#endif
}
#endif

#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION

/* spec 092 (CU 5801): ServerRedundancyType(2034)'s 2 own InstanceDeclarations. */
static void test_server_redundancy_type_instance_declarations(void) {
    TEST_ASSERT_TRUE(has_forward_ref(2034u, 46u, 2035u));
    assert_node(2035u, MU_NODECLASS_VARIABLE, "RedundancySupport");
    assert_datatype_and_valuerank(2035u, 851u, -1);
    TEST_ASSERT_TRUE(has_forward_ref(2035u, 37u, 78u));
    TEST_ASSERT_TRUE(has_forward_ref(2035u, 40u, 68u));

    TEST_ASSERT_TRUE(has_forward_ref(2034u, 46u, 32410u));
    assert_node(32410u, MU_NODECLASS_VARIABLE, "RedundantServerArray");
    assert_datatype_and_valuerank(32410u, 853u, 1);
    TEST_ASSERT_TRUE(has_forward_ref(32410u, 37u, 80u));
    TEST_ASSERT_TRUE(has_forward_ref(32410u, 40u, 68u));
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
    RUN_TEST(test_no_variable_has_dangling_datatype);
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
    RUN_TEST(test_namespace_file_types_gated_by_cu_namespaces);
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    RUN_TEST(test_servertype_instance_declarations);
    RUN_TEST(test_variable_reports_true_datatype_and_valuerank);
    RUN_TEST(test_operation_limits_type_instance_declarations);
    RUN_TEST(test_sampling_interval_diagnostics_type_instance_declarations);
    RUN_TEST(test_session_security_diagnostics_type_instance_declarations);
    RUN_TEST(test_server_diagnostics_type_instance_declarations);
    RUN_TEST(test_sessions_diagnostics_summary_type_instance_declarations);
    RUN_TEST(test_server_redundancy_type_instance_declarations);
    RUN_TEST(test_servertype_datatype_refs_not_dangling);
#endif
#elif MUC_OPCUA_BASE_NODES
    RUN_TEST(test_default_build_keeps_types_folder_unexpanded);
#endif
    return UNITY_END();
}
