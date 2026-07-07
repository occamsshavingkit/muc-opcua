/* src/address_space/base_nodes.c */
#include "base_nodes.h"
#include "muc_opcua/opcua_ids.h"
#include <stddef.h>

#ifdef MUC_OPCUA_BASE_NODES

/* Pooled strings to save flash */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif
static const opcua_byte_t s_str_Aggregates[] = "Aggregates";
static const opcua_byte_t s_str_BaseDataType[] = "BaseDataType";
static const opcua_byte_t s_str_BaseDataVariableType[] = "BaseDataVariableType";
static const opcua_byte_t s_str_BaseObjectType[] = "BaseObjectType";
static const opcua_byte_t s_str_BaseVariableType[] = "BaseVariableType";
static const opcua_byte_t s_str_Boolean[] = "Boolean";
static const opcua_byte_t s_str_Byte[] = "Byte";
static const opcua_byte_t s_str_DataTypes[] = "DataTypes";
static const opcua_byte_t s_str_DateTime[] = "DateTime";
static const opcua_byte_t s_str_Double[] = "Double";
static const opcua_byte_t s_str_Float[] = "Float";
static const opcua_byte_t s_str_FolderType[] = "FolderType";
static const opcua_byte_t s_str_GetMonitoredItems[] = "GetMonitoredItems";
static const opcua_byte_t s_str_HasChild[] = "HasChild";
static const opcua_byte_t s_str_HasComponent[] = "HasComponent";
static const opcua_byte_t s_str_HasProperty[] = "HasProperty";
static const opcua_byte_t s_str_HasSubtype[] = "HasSubtype";
static const opcua_byte_t s_str_HasTypeDefinition[] = "HasTypeDefinition";
static const opcua_byte_t s_str_HierarchicalReferences[] = "HierarchicalReferences";
static const opcua_byte_t s_str_InputArguments[] = "InputArguments";
static const opcua_byte_t s_str_Int16[] = "Int16";
static const opcua_byte_t s_str_Int32[] = "Int32";
static const opcua_byte_t s_str_Int64[] = "Int64";
static const opcua_byte_t s_str_LocaleIdArray[] = "LocaleIdArray";
static const opcua_byte_t s_str_LocalizedText[] = "LocalizedText";
static const opcua_byte_t s_str_MaxNodesPerBrowse[] = "MaxNodesPerBrowse";
static const opcua_byte_t s_str_MaxNodesPerRead[] = "MaxNodesPerRead";
static const opcua_byte_t s_str_NamespaceArray[] = "NamespaceArray";
static const opcua_byte_t s_str_NodeId[] = "NodeId";
static const opcua_byte_t s_str_NonHierarchicalReferences[] = "NonHierarchicalReferences";
static const opcua_byte_t s_str_ObjectTypes[] = "ObjectTypes";
static const opcua_byte_t s_str_Objects[] = "Objects";
static const opcua_byte_t s_str_OperationLimits[] = "OperationLimits";
static const opcua_byte_t s_str_Organizes[] = "Organizes";
static const opcua_byte_t s_str_OutputArguments[] = "OutputArguments";
static const opcua_byte_t s_str_PropertyType[] = "PropertyType";
static const opcua_byte_t s_str_QualifiedName[] = "QualifiedName";
static const opcua_byte_t s_str_ReferenceTypes[] = "ReferenceTypes";
static const opcua_byte_t s_str_References[] = "References";
static const opcua_byte_t s_str_ResendData[] = "ResendData";
static const opcua_byte_t s_str_Root[] = "Root";
static const opcua_byte_t s_str_SByte[] = "SByte";
static const opcua_byte_t s_str_Server[] = "Server";
static const opcua_byte_t s_str_ServerArray[] = "ServerArray";
static const opcua_byte_t s_str_ServerCapabilities[] = "ServerCapabilities";
static const opcua_byte_t s_str_ServerProfileArray[] = "ServerProfileArray";
static const opcua_byte_t s_str_ServerStatus[] = "ServerStatus";
static const opcua_byte_t s_str_ServerType[] = "ServerType";
static const opcua_byte_t s_str_State[] = "State";
static const opcua_byte_t s_str_StatusCode[] = "StatusCode";
static const opcua_byte_t s_str_String[] = "String";
static const opcua_byte_t s_str_Types[] = "Types";
static const opcua_byte_t s_str_UInt16[] = "UInt16";
static const opcua_byte_t s_str_UInt32[] = "UInt32";
static const opcua_byte_t s_str_UInt64[] = "UInt64";
static const opcua_byte_t s_str_VariableTypes[] = "VariableTypes";
static const opcua_byte_t s_str_Views[] = "Views";
static const opcua_byte_t s_str_ServerRedundancy[] = "ServerRedundancy";
static const opcua_byte_t s_str_Namespaces[] = "Namespaces";
static const opcua_byte_t s_str_RedundancySupport[] = "RedundancySupport";
static const opcua_byte_t s_str_ServiceLevel[] = "ServiceLevel";
static const opcua_byte_t s_str_en[] = "en";
static const opcua_byte_t s_str_http___opcfoundation_org_UA_Profile_Server_EmbeddedUA2017[] =
    "http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2017";
static const opcua_byte_t s_str_http___opcfoundation_org_UA_Profile_Server_NanoEmbeddedDevice2017[] =
    "http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017";
static const opcua_byte_t s_str_http___opcfoundation_org_UA_Profile_Server_StandardUA2017[] =
    "http://opcfoundation.org/UA-Profile/Server/StandardUA2017";
static const opcua_byte_t s_str_http___opcfoundation_org_UA_[] = "http://opcfoundation.org/UA/";
static const opcua_byte_t s_str_urn_muc_opcua_server[] = "urn:muc-opcua:server";
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static const mu_reference_t s_root_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, true},
                                             {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {86}}, true},
                                             {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {87}}, true},
#if MUC_OPCUA_BASE_TYPE_SYSTEM
                                             {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}
#endif
};

static const mu_reference_t s_objects_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {2253}}, true},
#if MUC_OPCUA_BASE_TYPE_SYSTEM
                                                {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}
#endif
};

#if MUC_OPCUA_BASE_TYPE_SYSTEM
static const mu_reference_t s_views_refs[] = {{{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_types_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {88}}, true},
                                              {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {89}}, true},
                                              {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {90}}, true},
                                              {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {91}}, true},
                                              {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_object_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {58}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_variable_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {62}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_data_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {24}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_reference_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {31}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_references_refs[] = {{{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {32}}, true},
                                                   {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {33}}, true}};

static const mu_reference_t s_nonhierarchical_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {40}}, true}};

static const mu_reference_t s_hierarchical_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {34}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {35}}, true}};

static const mu_reference_t s_has_child_refs[] = {{{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {44}}, true},
                                                  {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {45}}, true}};

static const mu_reference_t s_aggregates_refs[] = {{{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {46}}, true},
                                                   {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {47}}, true}};

static const mu_reference_t s_base_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {1}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {3}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {4}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {5}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {6}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {7}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {8}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {9}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {10}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {12}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {13}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {17}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {19}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {20}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {21}}, true}};

static const mu_reference_t s_base_object_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {61}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2004}}, true}};

static const mu_reference_t s_base_variable_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {63}}, true}};

static const mu_reference_t s_base_data_variable_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {68}}, true}};

static const mu_reference_t s_property_type_ref[] = {
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {68}}, true}};

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
static const mu_reference_t s_get_monitored_items_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11493}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11494}}, true}};

static const mu_reference_t s_resend_data_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12874}}, true}};
#endif

#endif

static const mu_reference_t s_server_refs[] = {{{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2254}}, true},
                                               {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2255}}, true},
                                               {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2256}}, true},
                                               {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2268}}, true},
#if MUC_OPCUA_BASE_TYPE_SYSTEM && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
                                               {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11492}}, true},
                                               {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {12873}}, true},
#endif
#if MUC_OPCUA_BASE_TYPE_SYSTEM
                                               {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2004}}, true}
#endif
};

static const mu_reference_t s_server_status_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2259}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2258}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2257}}, true},
#if MUC_OPCUA_BASE_TYPE_SYSTEM
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {63}}, true}
#endif
};

static const mu_reference_t s_server_capabilities_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2269}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2271}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11704}}, true},
#if MUC_OPCUA_BASE_TYPE_SYSTEM
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {58}}, true}
#endif
};

static const mu_reference_t s_operation_limits_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11705}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11710}}, true},
#if MUC_OPCUA_BASE_TYPE_SYSTEM
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {58}}, true}
#endif
};

static const mu_string_t s_server_array[] = {{20, s_str_urn_muc_opcua_server}};

static const mu_value_source_t s_server_array_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {.type = MU_TYPE_STRING, .value.array = s_server_array, .is_array = true, .array_length = 1}}};

static const mu_string_t s_namespace_array[] = {{28, s_str_http___opcfoundation_org_UA_}};

static const mu_value_source_t s_namespace_array_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {.type = MU_TYPE_STRING, .value.array = s_namespace_array, .is_array = true, .array_length = 1}}};

static const mu_value_source_t s_server_status_state_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_INT32, .value.i32 = 0}}};

static const mu_string_t s_server_profile_array[] = {
#if MUC_OPCUA_STANDARD_PROFILE
    {57, s_str_http___opcfoundation_org_UA_Profile_Server_StandardUA2017}
#elif MUC_OPCUA_BASE_TYPE_SYSTEM
    {57, s_str_http___opcfoundation_org_UA_Profile_Server_EmbeddedUA2017}
#else
    {65, s_str_http___opcfoundation_org_UA_Profile_Server_NanoEmbeddedDevice2017}
#endif
};

static const mu_value_source_t s_server_profile_array_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {
         .type = MU_TYPE_STRING, .value.array = s_server_profile_array, .is_array = true, .array_length = 1}}};

static const mu_string_t s_locale_id_array[] = {{2, s_str_en}};

static const mu_value_source_t s_locale_id_array_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {.type = MU_TYPE_STRING, .value.array = s_locale_id_array, .is_array = true, .array_length = 1}}};

static const mu_value_source_t s_max_nodes_per_read_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = 32}}};

static const mu_value_source_t s_max_nodes_per_browse_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = 8}}};

#if MUC_OPCUA_BASE_TYPE_SYSTEM
static const mu_node_t s_base_nodes[] = {
    {{0, MU_NODEID_NUMERIC, {1}},
     MU_NODECLASS_DATATYPE,
     {7, s_str_Boolean},
     {7, s_str_Boolean},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2}},
     MU_NODECLASS_DATATYPE,
     {5, s_str_SByte},
     {5, s_str_SByte},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {3}},
     MU_NODECLASS_DATATYPE,
     {4, s_str_Byte},
     {4, s_str_Byte},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {4}},
     MU_NODECLASS_DATATYPE,
     {5, s_str_Int16},
     {5, s_str_Int16},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {5}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_UInt16},
     {6, s_str_UInt16},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {6}},
     MU_NODECLASS_DATATYPE,
     {5, s_str_Int32},
     {5, s_str_Int32},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {7}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_UInt32},
     {6, s_str_UInt32},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {8}},
     MU_NODECLASS_DATATYPE,
     {5, s_str_Int64},
     {5, s_str_Int64},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {9}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_UInt64},
     {6, s_str_UInt64},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {10}},
     MU_NODECLASS_DATATYPE,
     {5, s_str_Float},
     {5, s_str_Float},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_Double},
     {6, s_str_Double},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {12}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_String},
     {6, s_str_String},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {13}},
     MU_NODECLASS_DATATYPE,
     {8, s_str_DateTime},
     {8, s_str_DateTime},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {17}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_NodeId},
     {6, s_str_NodeId},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {19}},
     MU_NODECLASS_DATATYPE,
     {10, s_str_StatusCode},
     {10, s_str_StatusCode},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {20}},
     MU_NODECLASS_DATATYPE,
     {13, s_str_QualifiedName},
     {13, s_str_QualifiedName},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {21}},
     MU_NODECLASS_DATATYPE,
     {13, s_str_LocalizedText},
     {13, s_str_LocalizedText},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {24}},
     MU_NODECLASS_DATATYPE,
     {12, s_str_BaseDataType},
     {12, s_str_BaseDataType},
     s_base_data_type_refs,
     sizeof(s_base_data_type_refs) / sizeof(s_base_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {31}},
     MU_NODECLASS_REFERENCETYPE,
     {10, s_str_References},
     {10, s_str_References},
     s_references_refs,
     sizeof(s_references_refs) / sizeof(s_references_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {32}},
     MU_NODECLASS_REFERENCETYPE,
     {25, s_str_NonHierarchicalReferences},
     {25, s_str_NonHierarchicalReferences},
     s_nonhierarchical_refs,
     sizeof(s_nonhierarchical_refs) / sizeof(s_nonhierarchical_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {33}},
     MU_NODECLASS_REFERENCETYPE,
     {22, s_str_HierarchicalReferences},
     {22, s_str_HierarchicalReferences},
     s_hierarchical_refs,
     sizeof(s_hierarchical_refs) / sizeof(s_hierarchical_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {34}},
     MU_NODECLASS_REFERENCETYPE,
     {8, s_str_HasChild},
     {8, s_str_HasChild},
     s_has_child_refs,
     sizeof(s_has_child_refs) / sizeof(s_has_child_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {35}},
     MU_NODECLASS_REFERENCETYPE,
     {9, s_str_Organizes},
     {9, s_str_Organizes},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {40}},
     MU_NODECLASS_REFERENCETYPE,
     {17, s_str_HasTypeDefinition},
     {17, s_str_HasTypeDefinition},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {44}},
     MU_NODECLASS_REFERENCETYPE,
     {10, s_str_Aggregates},
     {10, s_str_Aggregates},
     s_aggregates_refs,
     sizeof(s_aggregates_refs) / sizeof(s_aggregates_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {45}},
     MU_NODECLASS_REFERENCETYPE,
     {10, s_str_HasSubtype},
     {10, s_str_HasSubtype},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {46}},
     MU_NODECLASS_REFERENCETYPE,
     {11, s_str_HasProperty},
     {11, s_str_HasProperty},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {47}},
     MU_NODECLASS_REFERENCETYPE,
     {12, s_str_HasComponent},
     {12, s_str_HasComponent},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {58}},
     MU_NODECLASS_OBJECTTYPE,
     {14, s_str_BaseObjectType},
     {14, s_str_BaseObjectType},
     s_base_object_type_refs,
     sizeof(s_base_object_type_refs) / sizeof(s_base_object_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {61}},
     MU_NODECLASS_OBJECTTYPE,
     {10, s_str_FolderType},
     {10, s_str_FolderType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {62}},
     MU_NODECLASS_VARIABLETYPE,
     {16, s_str_BaseVariableType},
     {16, s_str_BaseVariableType},
     s_base_variable_type_refs,
     sizeof(s_base_variable_type_refs) / sizeof(s_base_variable_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {63}},
     MU_NODECLASS_VARIABLETYPE,
     {20, s_str_BaseDataVariableType},
     {20, s_str_BaseDataVariableType},
     s_base_data_variable_type_refs,
     sizeof(s_base_data_variable_type_refs) / sizeof(s_base_data_variable_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {68}},
     MU_NODECLASS_VARIABLETYPE,
     {12, s_str_PropertyType},
     {12, s_str_PropertyType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {84}},
     MU_NODECLASS_OBJECT,
     {4, s_str_Root},
     {4, s_str_Root},
     s_root_refs,
     sizeof(s_root_refs) / sizeof(s_root_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {85}},
     MU_NODECLASS_OBJECT,
     {7, s_str_Objects},
     {7, s_str_Objects},
     s_objects_refs,
     sizeof(s_objects_refs) / sizeof(s_objects_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {86}},
     MU_NODECLASS_OBJECT,
     {5, s_str_Types},
     {5, s_str_Types},
     s_types_refs,
     sizeof(s_types_refs) / sizeof(s_types_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {87}},
     MU_NODECLASS_OBJECT,
     {5, s_str_Views},
     {5, s_str_Views},
     s_views_refs,
     sizeof(s_views_refs) / sizeof(s_views_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {88}},
     MU_NODECLASS_OBJECT,
     {11, s_str_ObjectTypes},
     {11, s_str_ObjectTypes},
     s_type_folder_object_refs,
     sizeof(s_type_folder_object_refs) / sizeof(s_type_folder_object_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {89}},
     MU_NODECLASS_OBJECT,
     {13, s_str_VariableTypes},
     {13, s_str_VariableTypes},
     s_type_folder_variable_refs,
     sizeof(s_type_folder_variable_refs) / sizeof(s_type_folder_variable_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {90}},
     MU_NODECLASS_OBJECT,
     {9, s_str_DataTypes},
     {9, s_str_DataTypes},
     s_type_folder_data_refs,
     sizeof(s_type_folder_data_refs) / sizeof(s_type_folder_data_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {91}},
     MU_NODECLASS_OBJECT,
     {14, s_str_ReferenceTypes},
     {14, s_str_ReferenceTypes},
     s_type_folder_reference_refs,
     sizeof(s_type_folder_reference_refs) / sizeof(s_type_folder_reference_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
    {{0, MU_NODEID_NUMERIC, {2004}},
     MU_NODECLASS_OBJECTTYPE,
     {10, s_str_ServerType},
     {10, s_str_ServerType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2253}},
     MU_NODECLASS_OBJECT,
     {6, s_str_Server},
     {6, s_str_Server},
     s_server_refs,
     sizeof(s_server_refs) / sizeof(s_server_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2004}}},
    {{0, MU_NODEID_NUMERIC, {2254}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ServerArray},
     {11, s_str_ServerArray},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_server_array_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2255}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_NamespaceArray},
     {14, s_str_NamespaceArray},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_namespace_array_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2256}},
     MU_NODECLASS_VARIABLE,
     {12, s_str_ServerStatus},
     {12, s_str_ServerStatus},
     s_server_status_refs,
     sizeof(s_server_status_refs) / sizeof(s_server_status_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}}},
    {{0, MU_NODEID_NUMERIC, {2259}},
     MU_NODECLASS_VARIABLE,
     {5, s_str_State},
     {5, s_str_State},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_server_status_state_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2268}},
     MU_NODECLASS_OBJECT,
     {18, s_str_ServerCapabilities},
     {18, s_str_ServerCapabilities},
     s_server_capabilities_refs,
     sizeof(s_server_capabilities_refs) / sizeof(s_server_capabilities_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {58}}},
    {{0, MU_NODEID_NUMERIC, {2269}},
     MU_NODECLASS_VARIABLE,
     {18, s_str_ServerProfileArray},
     {18, s_str_ServerProfileArray},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_server_profile_array_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2271}},
     MU_NODECLASS_VARIABLE,
     {13, s_str_LocaleIdArray},
     {13, s_str_LocaleIdArray},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_locale_id_array_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    {{0, MU_NODEID_NUMERIC, {11492}},
     MU_NODECLASS_METHOD,
     {17, s_str_GetMonitoredItems},
     {17, s_str_GetMonitoredItems},
     s_get_monitored_items_refs,
     sizeof(s_get_monitored_items_refs) / sizeof(s_get_monitored_items_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11493}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_InputArguments},
     {14, s_str_InputArguments},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {11494}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_OutputArguments},
     {15, s_str_OutputArguments},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
    {{0, MU_NODEID_NUMERIC, {11704}},
     MU_NODECLASS_OBJECT,
     {15, s_str_OperationLimits},
     {15, s_str_OperationLimits},
     s_operation_limits_refs,
     sizeof(s_operation_limits_refs) / sizeof(s_operation_limits_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {58}}},
    {{0, MU_NODEID_NUMERIC, {11705}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_MaxNodesPerRead},
     {15, s_str_MaxNodesPerRead},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_nodes_per_read_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {11710}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxNodesPerBrowse},
     {17, s_str_MaxNodesPerBrowse},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_nodes_per_browse_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    {{0, MU_NODEID_NUMERIC, {12873}},
     MU_NODECLASS_METHOD,
     {10, s_str_ResendData},
     {10, s_str_ResendData},
     s_resend_data_refs,
     sizeof(s_resend_data_refs) / sizeof(s_resend_data_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {12874}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_InputArguments},
     {14, s_str_InputArguments},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
};
#else
static const mu_node_t s_base_nodes[] = {
    {{0, MU_NODEID_NUMERIC, {84}},
     MU_NODECLASS_OBJECT,
     {4, s_str_Root},
     {4, s_str_Root},
     s_root_refs,
     sizeof(s_root_refs) / sizeof(s_root_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {85}},
     MU_NODECLASS_OBJECT,
     {7, s_str_Objects},
     {7, s_str_Objects},
     s_objects_refs,
     sizeof(s_objects_refs) / sizeof(s_objects_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {86}},
     MU_NODECLASS_OBJECT,
     {5, s_str_Types},
     {5, s_str_Types},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {87}},
     MU_NODECLASS_OBJECT,
     {5, s_str_Views},
     {5, s_str_Views},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2253}},
     MU_NODECLASS_OBJECT,
     {6, s_str_Server},
     {6, s_str_Server},
     s_server_refs,
     sizeof(s_server_refs) / sizeof(s_server_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2254}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ServerArray},
     {11, s_str_ServerArray},
     NULL,
     0,
     &s_server_array_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2255}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_NamespaceArray},
     {14, s_str_NamespaceArray},
     NULL,
     0,
     &s_namespace_array_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2256}},
     MU_NODECLASS_VARIABLE,
     {12, s_str_ServerStatus},
     {12, s_str_ServerStatus},
     s_server_status_refs,
     sizeof(s_server_status_refs) / sizeof(s_server_status_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2259}},
     MU_NODECLASS_VARIABLE,
     {5, s_str_State},
     {5, s_str_State},
     NULL,
     0,
     &s_server_status_state_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2268}},
     MU_NODECLASS_OBJECT,
     {18, s_str_ServerCapabilities},
     {18, s_str_ServerCapabilities},
     s_server_capabilities_refs,
     sizeof(s_server_capabilities_refs) / sizeof(s_server_capabilities_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2269}},
     MU_NODECLASS_VARIABLE,
     {18, s_str_ServerProfileArray},
     {18, s_str_ServerProfileArray},
     NULL,
     0,
     &s_server_profile_array_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2271}},
     MU_NODECLASS_VARIABLE,
     {13, s_str_LocaleIdArray},
     {13, s_str_LocaleIdArray},
     NULL,
     0,
     &s_locale_id_array_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11704}},
     MU_NODECLASS_OBJECT,
     {15, s_str_OperationLimits},
     {15, s_str_OperationLimits},
     s_operation_limits_refs,
     sizeof(s_operation_limits_refs) / sizeof(s_operation_limits_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11705}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_MaxNodesPerRead},
     {15, s_str_MaxNodesPerRead},
     NULL,
     0,
     &s_max_nodes_per_read_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11710}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxNodesPerBrowse},
     {17, s_str_MaxNodesPerBrowse},
     NULL,
     0,
     &s_max_nodes_per_browse_value,
     .type_definition = {0}},
#if MUC_OPCUA_REDUNDANCY || MUC_OPCUA_STANDARD_PROFILE
    {{0, MU_NODEID_NUMERIC, {2296}},
     MU_NODECLASS_OBJECT,
     {16, s_str_ServerRedundancy},
     {16, s_str_ServerRedundancy},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2034u}}},
#endif

};
#endif

static const mu_address_space_t s_base_space = {s_base_nodes, sizeof(s_base_nodes) / sizeof(s_base_nodes[0])};
#else
static const mu_address_space_t s_base_space = {NULL, 0};
#endif /* MUC_OPCUA_BASE_NODES */

const mu_address_space_t *mu_base_address_space(void) {
    return &s_base_space;
}

#ifdef MUC_OPCUA_BASE_NODES
static opcua_statuscode_t base_status_time_read(void *ctx, const mu_nodeid_t *id, mu_variant_t *v) {
    const mu_base_runtime_t *rt = (const mu_base_runtime_t *)ctx;

    *v = (mu_variant_t){0};
    v->type = MU_TYPE_DATETIME;
    if (id->identifier.numeric == 2257u) {
        v->value.dt = rt->start_time;
    } else {
        v->value.dt = (rt->time && rt->time->get_time) ? rt->time->get_time(rt->time->context) : 0;
    }
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_BASE_NODES */

void mu_base_runtime_init(mu_base_runtime_nodes_t *s, const mu_time_adapter_t *time, opcua_datetime_t start_time) {
    s->rt.time = time;
    s->rt.start_time = start_time;

#ifdef MUC_OPCUA_BASE_NODES
    s->values[0].type = MU_VALUESOURCE_CALLBACK;
    s->values[0].data.callback.read = base_status_time_read;
    s->values[0].data.callback.context = &s->rt;
    s->values[1].type = MU_VALUESOURCE_CALLBACK;
    s->values[1].data.callback.read = base_status_time_read;
    s->values[1].data.callback.context = &s->rt;

#if MUC_OPCUA_BASE_TYPE_SYSTEM
    const mu_reference_t *const type_ref = s_property_type_ref;
    const size_t type_ref_cnt = sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]);
    const mu_nodeid_t type_def = {0, MU_NODEID_NUMERIC, {.numeric = 68}};
#else
    const mu_reference_t *const type_ref = NULL;
    const size_t type_ref_cnt = 0;
    const mu_nodeid_t type_def = {0};
#endif

    s->nodes[0] = (mu_node_t){
        .node_id = {.namespace_index = 0, .identifier_type = MU_NODEID_NUMERIC, .identifier.numeric = 2258u},
        .node_class = MU_NODECLASS_VARIABLE,
        .browse_name = {.length = (opcua_int32_t)11, .data = (const opcua_byte_t *)"CurrentTime"},
        .display_name = {.length = (opcua_int32_t)11, .data = (const opcua_byte_t *)"CurrentTime"},
        .references = type_ref,
        .reference_count = type_ref_cnt,
        .value = &s->values[0],
        .type_definition = type_def};
    s->nodes[1] = (mu_node_t){
        .node_id = {.namespace_index = 0, .identifier_type = MU_NODEID_NUMERIC, .identifier.numeric = 2257u},
        .node_class = MU_NODECLASS_VARIABLE,
        .browse_name = {.length = (opcua_int32_t)9, .data = (const opcua_byte_t *)"StartTime"},
        .display_name = {.length = (opcua_int32_t)9, .data = (const opcua_byte_t *)"StartTime"},
        .references = type_ref,
        .reference_count = type_ref_cnt,
        .value = &s->values[1],
        .type_definition = type_def};

    s->space.nodes = s->nodes;
    s->space.node_count = 2;
#else
    s->space.nodes = NULL;
    s->space.node_count = 0;
#endif /* MUC_OPCUA_BASE_NODES */
}

const mu_node_t *mu_resolve_node(const mu_address_space_t *user, mu_address_space_index_t *user_index,
                                 const mu_address_space_t *dynamic, const mu_nodeid_t *node_id) {
    if (user) {
        const mu_node_t *n = mu_address_space_find_node(user, user_index, node_id);
        if (n) {
            return n;
        }
    }
    if (dynamic) {
        const mu_node_t *n = mu_address_space_find_node(dynamic, NULL, node_id);
        if (n) {
            return n;
        }
    }
    return mu_address_space_find_node(mu_base_address_space(), NULL, node_id);
}

#if MUC_OPCUA_DATA_ACCESS
double mu_resolve_eurange_span(const struct mu_server *server, const mu_node_t *node) {
    (void)server;
    if (node == NULL || node->references == NULL) {
        return 0.0;
    }
    for (size_t i = 0; i < node->reference_count; ++i) {
        if (node->references[i].reference_type_id.identifier.numeric == 46u && /* HasProperty */
            node->references[i].target_id.identifier.numeric == MU_ID_EURANGE) {
            const mu_node_t *range_node = mu_resolve_node(NULL, NULL, NULL, &node->references[i].target_id);
            if (range_node != NULL) {
                if (range_node->value != NULL && range_node->value->type == MU_VALUESOURCE_STATIC) {
                    const mu_variant_t *sv = &range_node->value->data.static_value;
                    if (sv->type == MU_TYPE_DOUBLE && sv->value.array != NULL) {
                        const opcua_double_t *darr = (const opcua_double_t *)sv->value.array;
                        return darr[1] - darr[0]; /* High - Low */
                    }
                }
            }
        }
    }
    return 0.0;
}
#endif
