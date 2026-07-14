/* src/address_space/base_nodes.c */
#include "base_nodes.h"
#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/services/method.h"
#include <stddef.h>
#include <string.h>

#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER

/* Pooled strings to save flash */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif
static const opcua_byte_t s_str_Aggregates[] = "Aggregates";
static const opcua_byte_t s_str_BaseDataType[] = "BaseDataType";
static const opcua_byte_t s_str_BaseDataVariableType[] = "BaseDataVariableType";
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
static const opcua_byte_t s_str_BaseInterfaceType[] = "BaseInterfaceType";
#endif
static const opcua_byte_t s_str_BaseObjectType[] = "BaseObjectType";
static const opcua_byte_t s_str_BaseVariableType[] = "BaseVariableType";
static const opcua_byte_t s_str_Boolean[] = "Boolean";
static const opcua_byte_t s_str_Byte[] = "Byte";
#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY
static const opcua_byte_t s_str_CurrencyUnit[] = "CurrencyUnit";
static const opcua_byte_t s_str_CurrencyUnitType[] = "CurrencyUnitType";
#endif
static const opcua_byte_t s_str_DataTypes[] = "DataTypes";
static const opcua_byte_t s_str_DateTime[] = "DateTime";
#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME
static const opcua_byte_t s_str_DefaultInstanceBrowseName[] = "DefaultInstanceBrowseName";
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME || defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY)
static const opcua_byte_t s_str_Default_Binary[] = "Default Binary";
#endif
static const opcua_byte_t s_str_Double[] = "Double";
#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS
static const opcua_byte_t s_str_EUInformation[] = "EUInformation";
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME
static const opcua_byte_t s_str_EstimatedReturnTime[] = "EstimatedReturnTime";
#endif
static const opcua_byte_t s_str_Float[] = "Float";
static const opcua_byte_t s_str_FolderType[] = "FolderType";
static const opcua_byte_t s_str_GetMonitoredItems[] = "GetMonitoredItems";
#if MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE
/* OPC-10000-5 §11.21, OPC-10000-7 CU 2446: HasAddIn ReferenceType BrowseName. */
static const opcua_byte_t s_str_HasAddIn[] = "HasAddIn";
#endif
static const opcua_byte_t s_str_HasChild[] = "HasChild";
static const opcua_byte_t s_str_HasComponent[] = "HasComponent";
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
static const opcua_byte_t s_str_HasInterface[] = "HasInterface";
#endif
static const opcua_byte_t s_str_HasProperty[] = "HasProperty";
static const opcua_byte_t s_str_HasSubtype[] = "HasSubtype";
static const opcua_byte_t s_str_HasTypeDefinition[] = "HasTypeDefinition";
static const opcua_byte_t s_str_HierarchicalReferences[] = "HierarchicalReferences";
static const opcua_byte_t s_str_InputArguments[] = "InputArguments";
static const opcua_byte_t s_str_Int16[] = "Int16";
static const opcua_byte_t s_str_Int32[] = "Int32";
static const opcua_byte_t s_str_Int64[] = "Int64";
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
static const opcua_byte_t s_str_InterfaceTypes[] = "InterfaceTypes";
#endif
static const opcua_byte_t s_str_LocaleIdArray[] = "LocaleIdArray";
#ifdef MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT
static const opcua_byte_t s_str_Locations[] = "Locations";
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
static const opcua_byte_t s_str_LocalTime[] = "LocalTime";
#endif
static const opcua_byte_t s_str_LocalizedText[] = "LocalizedText";
static const opcua_byte_t s_str_MaxArrayLength[] = "MaxArrayLength";
static const opcua_byte_t s_str_MaxMonitoredItemsPerCall[] = "MaxMonitoredItemsPerCall";
static const opcua_byte_t s_str_MaxNodesPerBrowse[] = "MaxNodesPerBrowse";
static const opcua_byte_t s_str_MaxNodesPerRead[] = "MaxNodesPerRead";
static const opcua_byte_t s_str_MaxNodesPerWrite[] = "MaxNodesPerWrite";
static const opcua_byte_t s_str_MaxStringLength[] = "MaxStringLength";
static const opcua_byte_t s_str_NamespaceArray[] = "NamespaceArray";
static const opcua_byte_t s_str_NodeId[] = "NodeId";
static const opcua_byte_t s_str_NonHierarchicalReferences[] = "NonHierarchicalReferences";
static const opcua_byte_t s_str_ObjectTypes[] = "ObjectTypes";
static const opcua_byte_t s_str_Objects[] = "Objects";
static const opcua_byte_t s_str_OperationLimits[] = "OperationLimits";
#if MUC_OPCUA_CU_BASE_INFO_OPTIONSET
static const opcua_byte_t s_str_OptionSetType[] = "OptionSetType";
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST
static const opcua_byte_t s_str_SelectionListType[] = "SelectionListType";
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME || defined(MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS) ||                           \
    defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY)
static const opcua_byte_t s_str_Structure[] = "Structure";
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
static const opcua_byte_t s_str_TimeZoneDataType[] = "TimeZoneDataType";
#endif
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
#if MUC_OPCUA_CU_DATA_ACCESS
/* Spec 060: Data Access type-system BrowseNames (OPC-10000-8 §5.3). */
static const opcua_byte_t s_str_AnalogItemType[] = "AnalogItemType";
static const opcua_byte_t s_str_AnalogUnitType[] = "AnalogUnitType";
static const opcua_byte_t s_str_BaseAnalogType[] = "BaseAnalogType";
static const opcua_byte_t s_str_DataItemType[] = "DataItemType";
static const opcua_byte_t s_str_Definition[] = "Definition";
static const opcua_byte_t s_str_DiscreteItemType[] = "DiscreteItemType";
static const opcua_byte_t s_str_EURange[] = "EURange";
static const opcua_byte_t s_str_EngineeringUnits[] = "EngineeringUnits";
static const opcua_byte_t s_str_EnumStrings[] = "EnumStrings";
static const opcua_byte_t s_str_EnumValues[] = "EnumValues";
static const opcua_byte_t s_str_FalseState[] = "FalseState";
static const opcua_byte_t s_str_InstrumentRange[] = "InstrumentRange";
static const opcua_byte_t s_str_Mandatory[] = "Mandatory";
static const opcua_byte_t s_str_MultiStateDiscreteType[] = "MultiStateDiscreteType";
static const opcua_byte_t s_str_MultiStateValueDiscreteType[] = "MultiStateValueDiscreteType";
static const opcua_byte_t s_str_Optional[] = "Optional";
static const opcua_byte_t s_str_TrueState[] = "TrueState";
static const opcua_byte_t s_str_TwoStateDiscreteType[] = "TwoStateDiscreteType";
static const opcua_byte_t s_str_ValuePrecision[] = "ValuePrecision";
#endif
#if MUC_OPCUA_CU_DATA_ACCESS || defined(MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT)
static const opcua_byte_t s_str_ValueAsText[] = "ValueAsText";
#endif
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
#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
                                             {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}
#endif
};

static const mu_reference_t s_objects_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {2253}}, true},
#ifdef MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT
                                                {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {31915}}, true},
#endif
#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
                                                {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}
#endif
};

#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
static const mu_reference_t s_views_refs[] = {{{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_types_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {88}}, true},
                                              {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {89}}, true},
                                              {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {90}}, true},
                                              {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {91}}, true},
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
                                              {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {17708}}, true},
#endif
                                              {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_object_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {58}}, true},
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {17602}}, true},
#endif
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_variable_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {62}}, true},
#if MUC_OPCUA_CU_BASE_INFO_OPTIONSET
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_OPTIONSETTYPE}}, true},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_SELECTIONLISTTYPE}}, true},
#endif
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_data_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {24}}, true},
#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {MU_ID_EUINFORMATION_DATATYPE}}, true},
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_TIMEZONEDATATYPE}}, true},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_CURRENCYUNITTYPE}}, true},
#endif
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_type_folder_reference_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {31}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

static const mu_reference_t s_references_refs[] = {{{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {32}}, true},
                                                   {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {33}}, true}};

static const mu_reference_t s_nonhierarchical_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {40}}, true},
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {17603}}, true},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE
    /* OPC-10000-5 §11.21, OPC-10000-7 CU 2446: HasAddIn (17604) subtype. */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {17604}}, true}
#endif
};

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
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {21}}, true},
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME || defined(MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS) ||                           \
    defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY)
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {22}}, true}
#endif
};

#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME || defined(MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS) ||                           \
    defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY)
static const mu_reference_t s_structure_refs[] = {
#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_ID_EUINFORMATION_DATATYPE}}, true},
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_TIMEZONEDATATYPE}}, true},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_CURRENCYUNITTYPE}}, true},
#endif
};
#endif

#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
static const mu_reference_t s_time_zone_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {8917}}, true}};
#endif

#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY
static const mu_reference_t s_currency_unit_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {23507}}, true}};
#endif

static const mu_reference_t s_base_object_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {61}}, true},
#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17605}}, true},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {17602}}, true},
#endif
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2004}}, true}};

#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
static const mu_reference_t s_base_interface_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {88}}, false},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {58}}, false}};

static const mu_reference_t s_has_interface_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {32}}, false}};

static const mu_reference_t s_interface_types_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {86}}, false},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};
#endif

static const mu_reference_t s_base_variable_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {63}}, true}};

static const mu_reference_t s_base_data_variable_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {68}}, true},
#if MUC_OPCUA_CU_BASE_INFO_OPTIONSET
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_OPTIONSETTYPE}}, true},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_SELECTIONLISTTYPE}}, true},
#endif
#if MUC_OPCUA_CU_DATA_ACCESS
    /* Spec 060: BaseDataVariableType HasSubtype-> DataItemType (2365). */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2365}}, true}
#endif
};

static const mu_reference_t s_property_type_ref[] = {
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {68}}, true}};

#if MUC_OPCUA_CU_DATA_ACCESS
/* Spec 060 (OPC-10000-8 §5.3): Data Access VariableType subtype/property graph.
 * HasSubtype(45) forward refs live on the PARENT type node (matching the
 * BaseVariableType(62)->BaseDataVariableType(63) convention above); each type's
 * HasProperty(46) forward refs point at its property instance-declarations. */

/* Property instance-declarations carry HasTypeDefinition(40)->PropertyType(68)
 * plus HasModellingRule(37)->Mandatory(78)/Optional(80). Shared by rule. */
static const mu_reference_t s_da_prop_mandatory_refs[] = {
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {68}}, true},
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true}};

static const mu_reference_t s_da_prop_optional_refs[] = {
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {68}}, true},
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {80}}, true}};

/* DataItemType (2365): HasSubtype-> DiscreteItemType(2372), BaseAnalogType(15318);
 * optional props Definition(2366), ValuePrecision(2367). */
static const mu_reference_t s_da_data_item_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2372}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {15318}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2366}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2367}}, true}};

/* AnalogItemType (2368): mandatory EURange(2369); optional InstrumentRange(2370),
 * EngineeringUnits(2371). */
static const mu_reference_t s_da_analog_item_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2369}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2370}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2371}}, true}};

/* DiscreteItemType (2372, abstract): HasSubtype-> TwoStateDiscreteType(2373),
 * MultiStateDiscreteType(2376), MultiStateValueDiscreteType(11238). */
static const mu_reference_t s_da_discrete_item_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2373}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2376}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11238}}, true}};

/* TwoStateDiscreteType (2373): mandatory FalseState(2374), TrueState(2375). */
static const mu_reference_t s_da_two_state_discrete_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2374}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2375}}, true}};

/* MultiStateDiscreteType (2376): mandatory EnumStrings(2377). */
static const mu_reference_t s_da_multi_state_discrete_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2377}}, true}};

/* MultiStateValueDiscreteType (11238): mandatory EnumValues(11241), ValueAsText(11461). */
static const mu_reference_t s_da_multi_state_value_discrete_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11241}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11461}}, true}};

/* BaseAnalogType (15318): HasSubtype-> AnalogItemType(2368), AnalogUnitType(17497);
 * optional props InstrumentRange(17567), EURange(17568), EngineeringUnits(17569). */
static const mu_reference_t s_da_base_analog_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2368}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {17497}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17567}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17568}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17569}}, true}};

/* AnalogUnitType (17497): mandatory EngineeringUnits(17502); optional
 * InstrumentRange(17500), EURange(17501). */
static const mu_reference_t s_da_analog_unit_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17500}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17501}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17502}}, true}};
#endif /* MUC_OPCUA_CU_DATA_ACCESS */

#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
static const mu_reference_t s_get_monitored_items_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11493}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11494}}, true}};

static const mu_reference_t s_resend_data_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12874}}, true}};

/* Method Server Facet (spec 062): the built-in Server methods' InputArguments/
   OutputArguments Property Values, served as Argument[] so a client can browse the
   signature. GetMonitoredItems(subscriptionId:UInt32) -> (serverHandles:UInt32[],
   clientHandles:UInt32[]); ResendData(subscriptionId:UInt32). ns0 UInt32 = i=7. */
#if MUC_OPCUA_CU_METHOD_SERVER
static const opcua_byte_t s_arg_subscription_id[] = "SubscriptionId";
static const opcua_byte_t s_arg_server_handles[] = "ServerHandles";
static const opcua_byte_t s_arg_client_handles[] = "ClientHandles";

#define MU_ARG_UINT32_DT                                                                                               \
    {                                                                                                                  \
        0, MU_NODEID_NUMERIC, {                                                                                        \
            7                                                                                                          \
        }                                                                                                              \
    }
#define MU_ARG_NO_DESC                                                                                                 \
    {                                                                                                                  \
        {-1, NULL}, {                                                                                                  \
            -1, NULL                                                                                                   \
        }                                                                                                              \
    }

static const mu_argument_t s_gmi_input_args[] = {{{14, s_arg_subscription_id}, MU_ARG_UINT32_DT, -1, MU_ARG_NO_DESC}};
static const mu_argument_t s_gmi_output_args[] = {{{13, s_arg_server_handles}, MU_ARG_UINT32_DT, 1, MU_ARG_NO_DESC},
                                                  {{13, s_arg_client_handles}, MU_ARG_UINT32_DT, 1, MU_ARG_NO_DESC}};
static const mu_argument_t s_resend_input_args[] = {
    {{14, s_arg_subscription_id}, MU_ARG_UINT32_DT, -1, MU_ARG_NO_DESC}};

static const mu_value_source_t s_gmi_input_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_EXTENSIONOBJECT, {.array = s_gmi_input_args}, true, 1}}};
static const mu_value_source_t s_gmi_output_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_EXTENSIONOBJECT, {.array = s_gmi_output_args}, true, 2}}};
static const mu_value_source_t s_resend_input_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_EXTENSIONOBJECT, {.array = s_resend_input_args}, true, 1}}};

#define MU_GMI_INPUT_VALUE &s_gmi_input_value
#define MU_GMI_OUTPUT_VALUE &s_gmi_output_value
#define MU_RESEND_INPUT_VALUE &s_resend_input_value
#else
#define MU_GMI_INPUT_VALUE NULL
#define MU_GMI_OUTPUT_VALUE NULL
#define MU_RESEND_INPUT_VALUE NULL
#endif
#endif

#endif

static const mu_reference_t s_server_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2254}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2255}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2256}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2268}}, true},
#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER && MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11492}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {12873}}, true},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME
    /* OPC-10000-5 §6.3.1, OPC-10000-7 CU 3198: Server.EstimatedReturnTime Property. */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12885}}, true},
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    /* OPC-10000-5 §6.3.1: Server.LocalTime Property. */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17634}}, true},
#endif
#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2004}}, true}
#endif
};

static const mu_reference_t s_server_status_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2259}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2258}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2257}}, true},
#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {63}}, true}
#endif
};

static const mu_reference_t s_server_capabilities_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2269}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2271}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11702}}, true}, /* MaxArrayLength */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11703}}, true}, /* MaxStringLength */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11704}}, true},
#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {58}}, true}
#endif
};

static const mu_reference_t s_operation_limits_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11705}}, true},
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11707}}, true}, /* MaxNodesPerWrite */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11714}}, true}, /* MaxMonitoredItemsPerCall */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11710}}, true},
#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
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
#if MUC_OPCUA_MARKER_STANDARD_PROFILE
    {57, s_str_http___opcfoundation_org_UA_Profile_Server_StandardUA2017}
#elif MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
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

/* Spec 057: advertised OperationLimits/ServerCapabilities values are the SAME
 * macros the server enforces with (config.h / capacities.h). Drift is prevented
 * by _Static_asserts in the enforcing translation units (attribute_handler.c,
 * view_handler.c, subscription_helpers.c). */
static const mu_value_source_t s_max_nodes_per_read_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_MAX_NODES_PER_READ}}};

static const mu_value_source_t s_max_nodes_per_browse_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_MAX_NODES_PER_BROWSE}}};

static const mu_value_source_t s_max_nodes_per_write_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_MAX_NODES_PER_WRITE}}};

static const mu_value_source_t s_max_monitored_items_per_call_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_MAX_MONITORED_ITEMS_PER_CALL}}};

static const mu_value_source_t s_max_array_length_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_INTERN_MAX_ARRAY_LENGTH}}};

static const mu_value_source_t s_max_string_length_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_MAX_ENCODED_STRING_LENGTH}}};

#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
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
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME || defined(MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS) ||                           \
    defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY)
    {{0, MU_NODEID_NUMERIC, {22}},
     MU_NODECLASS_DATATYPE,
     {9, s_str_Structure},
     {9, s_str_Structure},
     s_structure_refs,
     sizeof(s_structure_refs) / sizeof(s_structure_refs[0]),
     NULL,
     .type_definition = {0}},
#endif
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
#if MUC_OPCUA_CU_DATA_ACCESS
    /* Spec 060: ModellingRule targets so a property's HasModellingRule resolves. */
    {{0, MU_NODEID_NUMERIC, {78}},
     MU_NODECLASS_OBJECT,
     {9, s_str_Mandatory},
     {9, s_str_Mandatory},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {80}},
     MU_NODECLASS_OBJECT,
     {8, s_str_Optional},
     {8, s_str_Optional},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
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
#if MUC_OPCUA_CU_DATA_ACCESS
    /* Spec 060: Data Access types + property instance-declarations (2365..11461). */
    {{0, MU_NODEID_NUMERIC, {2365}},
     MU_NODECLASS_VARIABLETYPE,
     {12, s_str_DataItemType},
     {12, s_str_DataItemType},
     s_da_data_item_type_refs,
     sizeof(s_da_data_item_type_refs) / sizeof(s_da_data_item_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2366}},
     MU_NODECLASS_VARIABLE,
     {10, s_str_Definition},
     {10, s_str_Definition},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2367}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_ValuePrecision},
     {14, s_str_ValuePrecision},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2368}},
     MU_NODECLASS_VARIABLETYPE,
     {14, s_str_AnalogItemType},
     {14, s_str_AnalogItemType},
     s_da_analog_item_type_refs,
     sizeof(s_da_analog_item_type_refs) / sizeof(s_da_analog_item_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2369}},
     MU_NODECLASS_VARIABLE,
     {7, s_str_EURange},
     {7, s_str_EURange},
     s_da_prop_mandatory_refs,
     sizeof(s_da_prop_mandatory_refs) / sizeof(s_da_prop_mandatory_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2370}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_InstrumentRange},
     {15, s_str_InstrumentRange},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2371}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_EngineeringUnits},
     {16, s_str_EngineeringUnits},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2372}},
     MU_NODECLASS_VARIABLETYPE,
     {16, s_str_DiscreteItemType},
     {16, s_str_DiscreteItemType},
     s_da_discrete_item_type_refs,
     sizeof(s_da_discrete_item_type_refs) / sizeof(s_da_discrete_item_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2373}},
     MU_NODECLASS_VARIABLETYPE,
     {20, s_str_TwoStateDiscreteType},
     {20, s_str_TwoStateDiscreteType},
     s_da_two_state_discrete_type_refs,
     sizeof(s_da_two_state_discrete_type_refs) / sizeof(s_da_two_state_discrete_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2374}},
     MU_NODECLASS_VARIABLE,
     {10, s_str_FalseState},
     {10, s_str_FalseState},
     s_da_prop_mandatory_refs,
     sizeof(s_da_prop_mandatory_refs) / sizeof(s_da_prop_mandatory_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2375}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_TrueState},
     {9, s_str_TrueState},
     s_da_prop_mandatory_refs,
     sizeof(s_da_prop_mandatory_refs) / sizeof(s_da_prop_mandatory_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {2376}},
     MU_NODECLASS_VARIABLETYPE,
     {22, s_str_MultiStateDiscreteType},
     {22, s_str_MultiStateDiscreteType},
     s_da_multi_state_discrete_type_refs,
     sizeof(s_da_multi_state_discrete_type_refs) / sizeof(s_da_multi_state_discrete_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2377}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_EnumStrings},
     {11, s_str_EnumStrings},
     s_da_prop_mandatory_refs,
     sizeof(s_da_prop_mandatory_refs) / sizeof(s_da_prop_mandatory_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    /* OPC-10000-5 §12.2.12.11, OPC-10000-7 CU 2476: TimeZoneDataType (8912),
     * subtype of Structure (22), with its Default Binary encoding Object (8917). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_TIMEZONEDATATYPE}},
     MU_NODECLASS_DATATYPE,
     {16, s_str_TimeZoneDataType},
     {16, s_str_TimeZoneDataType},
     s_time_zone_data_type_refs,
     sizeof(s_time_zone_data_type_refs) / sizeof(s_time_zone_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {8917}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
    {{0, MU_NODEID_NUMERIC, {11238}},
     MU_NODECLASS_VARIABLETYPE,
     {27, s_str_MultiStateValueDiscreteType},
     {27, s_str_MultiStateValueDiscreteType},
     s_da_multi_state_value_discrete_type_refs,
     sizeof(s_da_multi_state_value_discrete_type_refs) / sizeof(s_da_multi_state_value_discrete_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11241}},
     MU_NODECLASS_VARIABLE,
     {10, s_str_EnumValues},
     {10, s_str_EnumValues},
     s_da_prop_mandatory_refs,
     sizeof(s_da_prop_mandatory_refs) / sizeof(s_da_prop_mandatory_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {11461}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ValueAsText},
     {11, s_str_ValueAsText},
     s_da_prop_mandatory_refs,
     sizeof(s_da_prop_mandatory_refs) / sizeof(s_da_prop_mandatory_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif /* MUC_OPCUA_CU_DATA_ACCESS */
#ifdef MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT
#ifndef MUC_OPCUA_CU_DATA_ACCESS
    /* OPC-10000-7 CU 2969: ValueAsText is a PropertyType instance declaration
     * for Variables with enumerated DataTypes. There is no single global
     * numeric NodeId; i=11461 is the standard declaration used by
     * MultiStateValueDiscreteType and is already provided by Data Access. */
    {{0, MU_NODEID_NUMERIC, {11461}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ValueAsText},
     {11, s_str_ValueAsText},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS
    /* OPC-10000-7 CU 5592: EUInformation DataType (887), subtype of Structure (22). */
    {{0, MU_NODEID_NUMERIC, {MU_ID_EUINFORMATION_DATATYPE}},
     MU_NODECLASS_DATATYPE,
     {13, s_str_EUInformation},
     {13, s_str_EUInformation},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME && !MUC_OPCUA_CU_DATA_ACCESS
    /* OPC-10000-5 §12.2.12.11, OPC-10000-7 CU 2476: TimeZoneDataType (8912),
     * subtype of Structure (22), with its Default Binary encoding Object (8917). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_TIMEZONEDATATYPE}},
     MU_NODECLASS_DATATYPE,
     {16, s_str_TimeZoneDataType},
     {16, s_str_TimeZoneDataType},
     s_time_zone_data_type_refs,
     sizeof(s_time_zone_data_type_refs) / sizeof(s_time_zone_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {8917}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_OPTIONSET
    /* OPC-10000-5 §7.17, OPC-10000-7 CU 3127: OptionSetType (11487),
     * subtype of BaseDataVariableType (63). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_OPTIONSETTYPE}},
     MU_NODECLASS_VARIABLETYPE,
     {13, s_str_OptionSetType},
     {13, s_str_OptionSetType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
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
     MU_GMI_INPUT_VALUE,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {11494}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_OutputArguments},
     {15, s_str_OutputArguments},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     MU_GMI_OUTPUT_VALUE,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
    /* Spec 057: MaxArrayLength/MaxStringLength — sorted slots before 11704. */
    {{0, MU_NODEID_NUMERIC, {11702}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_MaxArrayLength},
     {14, s_str_MaxArrayLength},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_array_length_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {11703}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_MaxStringLength},
     {15, s_str_MaxStringLength},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_string_length_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
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
    /* Spec 057: MaxNodesPerWrite — sorted slot between 11705 and 11710. */
    {{0, MU_NODEID_NUMERIC, {11707}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_MaxNodesPerWrite},
     {16, s_str_MaxNodesPerWrite},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_nodes_per_write_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {11710}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxNodesPerBrowse},
     {17, s_str_MaxNodesPerBrowse},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_nodes_per_browse_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    /* Spec 057: MaxMonitoredItemsPerCall — sorted slot after 11710. */
    {{0, MU_NODEID_NUMERIC, {11714}},
     MU_NODECLASS_VARIABLE,
     {24, s_str_MaxMonitoredItemsPerCall},
     {24, s_str_MaxMonitoredItemsPerCall},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_monitored_items_per_call_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
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
     MU_RESEND_INPUT_VALUE,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME
    /* OPC-10000-5 §6.3.1, OPC-10000-7 CU 3198: Server.EstimatedReturnTime
     * (i=12885) is a DateTime (i=13) Property using PropertyType (i=68). */
    {{0, MU_NODEID_NUMERIC, {12885}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_EstimatedReturnTime},
     {19, s_str_EstimatedReturnTime},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#if MUC_OPCUA_CU_DATA_ACCESS
    /* Spec 060: BaseAnalogType + AnalogUnitType and their property instances
     * (15318..17569), sorted after all preceding NodeIds. */
    {{0, MU_NODEID_NUMERIC, {15318}},
     MU_NODECLASS_VARIABLETYPE,
     {14, s_str_BaseAnalogType},
     {14, s_str_BaseAnalogType},
     s_da_base_analog_type_refs,
     sizeof(s_da_base_analog_type_refs) / sizeof(s_da_base_analog_type_refs[0]),
     NULL,
     .type_definition = {0}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST
    /* OPC-10000-5 §7.18, OPC-10000-7 CU 2711: SelectionListType (16309),
     * subtype of BaseDataVariableType (63). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_SELECTIONLISTTYPE}},
     MU_NODECLASS_VARIABLETYPE,
     {17, s_str_SelectionListType},
     {17, s_str_SelectionListType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_DATA_ACCESS
    {{0, MU_NODEID_NUMERIC, {17497}},
     MU_NODECLASS_VARIABLETYPE,
     {14, s_str_AnalogUnitType},
     {14, s_str_AnalogUnitType},
     s_da_analog_unit_type_refs,
     sizeof(s_da_analog_unit_type_refs) / sizeof(s_da_analog_unit_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {17500}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_InstrumentRange},
     {15, s_str_InstrumentRange},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {17501}},
     MU_NODECLASS_VARIABLE,
     {7, s_str_EURange},
     {7, s_str_EURange},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {17502}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_EngineeringUnits},
     {16, s_str_EngineeringUnits},
     s_da_prop_mandatory_refs,
     sizeof(s_da_prop_mandatory_refs) / sizeof(s_da_prop_mandatory_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {17567}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_InstrumentRange},
     {15, s_str_InstrumentRange},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {17568}},
     MU_NODECLASS_VARIABLE,
     {7, s_str_EURange},
     {7, s_str_EURange},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {17569}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_EngineeringUnits},
     {16, s_str_EngineeringUnits},
     s_da_prop_optional_refs,
     sizeof(s_da_prop_optional_refs) / sizeof(s_da_prop_optional_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif /* MUC_OPCUA_CU_DATA_ACCESS */
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    /* OPC-10000-5 §6.9, OPC-10000-7 CU 3560: BaseInterfaceType ObjectType,
     * subtype of BaseObjectType (58). */
    {{0, MU_NODEID_NUMERIC, {17602}},
     MU_NODECLASS_OBJECTTYPE,
     {17, s_str_BaseInterfaceType},
     {17, s_str_BaseInterfaceType},
     s_base_interface_type_refs,
     sizeof(s_base_interface_type_refs) / sizeof(s_base_interface_type_refs[0]),
     NULL,
     .type_definition = {0}},
    /* OPC-10000-5 §11.20, OPC-10000-7 CU 3560: HasInterface ReferenceType,
     * subtype of NonHierarchicalReferences (32). */
    {{0, MU_NODEID_NUMERIC, {17603}},
     MU_NODECLASS_REFERENCETYPE,
     {12, s_str_HasInterface},
     {12, s_str_HasInterface},
     s_has_interface_refs,
     sizeof(s_has_interface_refs) / sizeof(s_has_interface_refs[0]),
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE
    /* OPC-10000-5 §11.21, OPC-10000-7 CU 2446: HasAddIn ReferenceType (17604),
     * subtype of NonHierarchicalReferences (32). */
    {{0, MU_NODEID_NUMERIC, {17604}},
     MU_NODECLASS_REFERENCETYPE,
     {8, s_str_HasAddIn},
     {8, s_str_HasAddIn},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME
    /* OPC-10000-7 CU 2447: ObjectTypes may carry DefaultInstanceBrowseName
     * (i=17605) to define the default BrowseName for their instances. */
    {{0, MU_NODEID_NUMERIC, {17605}},
     MU_NODECLASS_VARIABLE,
     {25, s_str_DefaultInstanceBrowseName},
     {25, s_str_DefaultInstanceBrowseName},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    /* OPC-10000-5 §6.3.1: Server.LocalTime (i=17634) is a Property using
     * TimeZoneDataType (i=8912); this node model records PropertyType (i=68). */
    {{0, MU_NODEID_NUMERIC, {17634}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_LocalTime},
     {9, s_str_LocalTime},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    /* OPC UA NodeIds.csv: InterfaceTypes Folder (i=17708), organized by Types (86). */
    {{0, MU_NODEID_NUMERIC, {17708}},
     MU_NODECLASS_OBJECT,
     {14, s_str_InterfaceTypes},
     {14, s_str_InterfaceTypes},
     s_interface_types_refs,
     sizeof(s_interface_types_refs) / sizeof(s_interface_types_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT
    /* OPC-10000-5 §8.2.12, OPC-10000-7 CU 4053: Locations Folder (i=31915),
     * organized by Objects (85), with FolderType (61). */
    {{0, MU_NODEID_NUMERIC, {31915}},
     MU_NODECLASS_OBJECT,
     {9, s_str_Locations},
     {9, s_str_Locations},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY
    /* OPC-10000-5 §12.2.12.2, OPC-10000-7 CU 5240: CurrencyUnitType (23498),
     * subtype of Structure (22), with its Default Binary encoding Object (23507). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_CURRENCYUNITTYPE}},
     MU_NODECLASS_DATATYPE,
     {16, s_str_CurrencyUnitType},
     {16, s_str_CurrencyUnitType},
     s_currency_unit_type_refs,
     sizeof(s_currency_unit_type_refs) / sizeof(s_currency_unit_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {23507}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    /* OPC-10000-7 CU 5240: DataVariables representing currency may carry a
     * CurrencyUnit Property. OPC-10000-5 §12.2.12.2 defines it by BrowseName;
     * no single global numeric NodeId is assigned, so expose the declaration
     * with a stable string NodeId and PropertyType (68). */
    {{0, MU_NODEID_STRING, {.string = {12, s_str_CurrencyUnit}}},
     MU_NODECLASS_VARIABLE,
     {12, s_str_CurrencyUnit},
     {12, s_str_CurrencyUnit},
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
#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS
    /* OPC-10000-7 CU 5592: EUInformation DataType (887), subtype of Structure (22). */
    {{0, MU_NODEID_NUMERIC, {MU_ID_EUINFORMATION_DATATYPE}},
     MU_NODECLASS_DATATYPE,
     {13, s_str_EUInformation},
     {13, s_str_EUInformation},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    /* OPC-10000-5 §12.2.12.11, OPC-10000-7 CU 2476: TimeZoneDataType (8912),
     * subtype of Structure (22), with its Default Binary encoding Object (8917). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_TIMEZONEDATATYPE}},
     MU_NODECLASS_DATATYPE,
     {16, s_str_TimeZoneDataType},
     {16, s_str_TimeZoneDataType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {8917}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT
#ifndef MUC_OPCUA_CU_DATA_ACCESS
    /* OPC-10000-7 CU 2969: ValueAsText applies to Variables with enumerated
     * DataTypes. This minimal node model records PropertyType (i=68). */
    {{0, MU_NODEID_NUMERIC, {11461}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ValueAsText},
     {11, s_str_ValueAsText},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#endif
#if MUC_OPCUA_CU_BASE_INFO_OPTIONSET
    /* OPC-10000-5 §7.17, OPC-10000-7 CU 3127: OptionSetType (11487),
     * subtype of BaseDataVariableType (63). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_OPTIONSETTYPE}},
     MU_NODECLASS_VARIABLETYPE,
     {13, s_str_OptionSetType},
     {13, s_str_OptionSetType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
    /* Spec 057: MaxArrayLength/MaxStringLength — sorted slots before 11704. */
    {{0, MU_NODEID_NUMERIC, {11702}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_MaxArrayLength},
     {14, s_str_MaxArrayLength},
     NULL,
     0,
     &s_max_array_length_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11703}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_MaxStringLength},
     {15, s_str_MaxStringLength},
     NULL,
     0,
     &s_max_string_length_value,
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
    /* Spec 057: MaxNodesPerWrite — sorted slot between 11705 and 11710. */
    {{0, MU_NODEID_NUMERIC, {11707}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_MaxNodesPerWrite},
     {16, s_str_MaxNodesPerWrite},
     NULL,
     0,
     &s_max_nodes_per_write_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11710}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxNodesPerBrowse},
     {17, s_str_MaxNodesPerBrowse},
     NULL,
     0,
     &s_max_nodes_per_browse_value,
     .type_definition = {0}},
    /* Spec 057: MaxMonitoredItemsPerCall — sorted slot after 11710. */
    {{0, MU_NODEID_NUMERIC, {11714}},
     MU_NODECLASS_VARIABLE,
     {24, s_str_MaxMonitoredItemsPerCall},
     {24, s_str_MaxMonitoredItemsPerCall},
     NULL,
     0,
     &s_max_monitored_items_per_call_value,
     .type_definition = {0}},
#ifdef MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME
    /* OPC-10000-5 §6.3.1, OPC-10000-7 CU 3198: Server.EstimatedReturnTime
     * (i=12885) is a DateTime (i=13) Property using PropertyType (i=68). */
    {{0, MU_NODEID_NUMERIC, {12885}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_EstimatedReturnTime},
     {19, s_str_EstimatedReturnTime},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST
    /* OPC-10000-5 §7.18, OPC-10000-7 CU 2711: SelectionListType (16309),
     * subtype of BaseDataVariableType (63). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_SELECTIONLISTTYPE}},
     MU_NODECLASS_VARIABLETYPE,
     {17, s_str_SelectionListType},
     {17, s_str_SelectionListType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    /* OPC-10000-5 §6.9, OPC-10000-7 CU 3560: BaseInterfaceType ObjectType,
     * subtype of BaseObjectType (58). */
    {{0, MU_NODEID_NUMERIC, {17602}},
     MU_NODECLASS_OBJECTTYPE,
     {17, s_str_BaseInterfaceType},
     {17, s_str_BaseInterfaceType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    /* OPC-10000-5 §11.20, OPC-10000-7 CU 3560: HasInterface ReferenceType,
     * subtype of NonHierarchicalReferences (32). */
    {{0, MU_NODEID_NUMERIC, {17603}},
     MU_NODECLASS_REFERENCETYPE,
     {12, s_str_HasInterface},
     {12, s_str_HasInterface},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE
    /* OPC-10000-5 §11.21, OPC-10000-7 CU 2446: HasAddIn ReferenceType (17604),
     * subtype of NonHierarchicalReferences (32). */
    {{0, MU_NODEID_NUMERIC, {17604}},
     MU_NODECLASS_REFERENCETYPE,
     {8, s_str_HasAddIn},
     {8, s_str_HasAddIn},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME
    /* OPC-10000-7 CU 2447: ObjectTypes may carry DefaultInstanceBrowseName
     * (i=17605) to define the default BrowseName for their instances. */
    {{0, MU_NODEID_NUMERIC, {17605}},
     MU_NODECLASS_VARIABLE,
     {25, s_str_DefaultInstanceBrowseName},
     {25, s_str_DefaultInstanceBrowseName},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    /* OPC-10000-5 §6.3.1: Server.LocalTime (i=17634) is a Property using
     * TimeZoneDataType (i=8912); this node model records PropertyType (i=68). */
    {{0, MU_NODEID_NUMERIC, {17634}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_LocalTime},
     {9, s_str_LocalTime},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    /* OPC UA NodeIds.csv: InterfaceTypes Folder (i=17708), organized by Types (86). */
    {{0, MU_NODEID_NUMERIC, {17708}},
     MU_NODECLASS_OBJECT,
     {14, s_str_InterfaceTypes},
     {14, s_str_InterfaceTypes},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY
    /* OPC-10000-5 §12.2.12.2, OPC-10000-7 CU 5240: CurrencyUnitType (23498),
     * subtype of Structure (22), with its Default Binary encoding Object (23507). */
    {{0, MU_NODEID_NUMERIC, {MU_NODEID_CURRENCYUNITTYPE}},
     MU_NODECLASS_DATATYPE,
     {16, s_str_CurrencyUnitType},
     {16, s_str_CurrencyUnitType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {23507}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    /* OPC-10000-7 CU 5240: DataVariables representing currency may carry a
     * CurrencyUnit Property. OPC-10000-5 §12.2.12.2 defines it by BrowseName;
     * no single global numeric NodeId is assigned, so expose the declaration
     * with a stable string NodeId and PropertyType (68). */
    {{0, MU_NODEID_STRING, {.string = {12, s_str_CurrencyUnit}}},
     MU_NODECLASS_VARIABLE,
     {12, s_str_CurrencyUnit},
     {12, s_str_CurrencyUnit},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT
    /* OPC-10000-5 §8.2.12, OPC-10000-7 CU 4053: Locations Folder (i=31915),
     * organized by Objects (85), with FolderType (61). */
    {{0, MU_NODEID_NUMERIC, {31915}},
     MU_NODECLASS_OBJECT,
     {9, s_str_Locations},
     {9, s_str_Locations},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#if MUC_OPCUA_CU_REDUNDANCY || MUC_OPCUA_MARKER_STANDARD_PROFILE
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
#endif /* MUC_OPCUA_FACET_CORE_2022_SERVER */

const mu_address_space_t *mu_base_address_space(void) {
    return &s_base_space;
}

#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
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
#endif /* MUC_OPCUA_FACET_CORE_2022_SERVER */

#if defined(MUC_OPCUA_FACET_CORE_2022_SERVER) && MUC_OPCUA_CU_DIAGNOSTICS
/* Live ServerDiagnosticsSummary (2275): emit the caller-owned counter struct as a scalar
   ServerDiagnosticsSummaryDataType ExtensionObject. The encoder reads value.array as a
   const mu_diagnostics_summary_t* (12 x UInt32, OPC-10000-5 §12.9). */
static opcua_statuscode_t base_diagnostics_summary_read(void *context, const mu_nodeid_t *id, mu_variant_t *v) {
    const mu_base_runtime_t *rt = (const mu_base_runtime_t *)context;
    (void)id;
    *v = (mu_variant_t){0};
    v->type = MU_TYPE_EXTENSIONOBJECT;
    v->is_array = false;
    v->array_length = 0;
    v->value.array = rt->diag;
    return (rt->diag != NULL) ? MU_STATUS_GOOD : MU_STATUS_BAD_NOTREADABLE;
}

/* ServerDiagnostics.EnabledFlag (2294): diagnostics are always collected -> Boolean true. */
static opcua_statuscode_t base_diagnostics_enabled_read(void *context, const mu_nodeid_t *id, mu_variant_t *v) {
    (void)context;
    (void)id;
    *v = (mu_variant_t){0};
    v->type = MU_TYPE_BOOLEAN;
    v->value.b = true;
    return MU_STATUS_GOOD;
}
#endif

void mu_base_runtime_init(mu_base_runtime_nodes_t *s, const mu_time_adapter_t *time, opcua_datetime_t start_time
#if MUC_OPCUA_CU_DIAGNOSTICS
                          ,
                          const mu_diagnostics_summary_t *diag
#endif
) {
    s->rt.time = time;
    s->rt.start_time = start_time;
#if MUC_OPCUA_CU_DIAGNOSTICS
    s->rt.diag = diag;
#endif

#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
    s->values[0].type = MU_VALUESOURCE_CALLBACK;
    s->values[0].data.callback.read = base_status_time_read;
    s->values[0].data.callback.context = &s->rt;
    s->values[1].type = MU_VALUESOURCE_CALLBACK;
    s->values[1].data.callback.read = base_status_time_read;
    s->values[1].data.callback.context = &s->rt;

#if MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
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

#if MUC_OPCUA_CU_DIAGNOSTICS
    /* Base Server Behaviour facet (spec 064): the ServerDiagnostics object tree.
       ServerDiagnostics(2274) Object, ServerDiagnosticsSummary(2275) live Variable,
       EnabledFlag(2294) Variable. Read-addressable by NodeId (OPC-10000-5 §6.3.3). */
    s->values[2] = (mu_value_source_t){0}; /* Object has no Value */
    s->values[3].type = MU_VALUESOURCE_CALLBACK;
    s->values[3].data.callback.read = base_diagnostics_summary_read;
    s->values[3].data.callback.context = &s->rt;
    s->values[4].type = MU_VALUESOURCE_CALLBACK;
    s->values[4].data.callback.read = base_diagnostics_enabled_read;
    s->values[4].data.callback.context = &s->rt;

    s->nodes[2] = (mu_node_t){
        .node_id = {.namespace_index = 0, .identifier_type = MU_NODEID_NUMERIC, .identifier.numeric = 2274u},
        .node_class = MU_NODECLASS_OBJECT,
        .browse_name = {.length = (opcua_int32_t)17, .data = (const opcua_byte_t *)"ServerDiagnostics"},
        .display_name = {.length = (opcua_int32_t)17, .data = (const opcua_byte_t *)"ServerDiagnostics"},
        .references = NULL,
        .reference_count = 0,
        .value = NULL,
        .type_definition = {0, MU_NODEID_NUMERIC, {.numeric = 2020u}}}; /* ServerDiagnosticsType */
    s->nodes[3] = (mu_node_t){
        .node_id = {.namespace_index = 0, .identifier_type = MU_NODEID_NUMERIC, .identifier.numeric = 2275u},
        .node_class = MU_NODECLASS_VARIABLE,
        .browse_name = {.length = (opcua_int32_t)24, .data = (const opcua_byte_t *)"ServerDiagnosticsSummary"},
        .display_name = {.length = (opcua_int32_t)24, .data = (const opcua_byte_t *)"ServerDiagnosticsSummary"},
        .references = NULL,
        .reference_count = 0,
        .value = &s->values[3],
        .type_definition = {0, MU_NODEID_NUMERIC, {.numeric = 63u}}}; /* BaseDataVariableType */
    s->nodes[4] = (mu_node_t){
        .node_id = {.namespace_index = 0, .identifier_type = MU_NODEID_NUMERIC, .identifier.numeric = 2294u},
        .node_class = MU_NODECLASS_VARIABLE,
        .browse_name = {.length = (opcua_int32_t)11, .data = (const opcua_byte_t *)"EnabledFlag"},
        .display_name = {.length = (opcua_int32_t)11, .data = (const opcua_byte_t *)"EnabledFlag"},
        .references = NULL,
        .reference_count = 0,
        .value = &s->values[4],
        .type_definition = {0, MU_NODEID_NUMERIC, {.numeric = 68u}}}; /* PropertyType */
#endif

#if MUC_OPCUA_CU_REDUNDANCY
    /* Client Redundancy facet (spec 066): Server.ServerRedundancy.RedundancySupport
       (i=3709) advertises the redundancy mode. This server supports client redundancy
       via TransferSubscriptions without being itself redundant -> None (0), the
       RedundancySupport enum (OPC-10000-5 §12.5) encoded as Int32. */
    s->values[MU_BASE_RUNTIME_REDUNDANCY_INDEX].type = MU_VALUESOURCE_STATIC;
    s->values[MU_BASE_RUNTIME_REDUNDANCY_INDEX].data.static_value = (mu_variant_t){0};
    s->values[MU_BASE_RUNTIME_REDUNDANCY_INDEX].data.static_value.type = MU_TYPE_INT32;
    s->values[MU_BASE_RUNTIME_REDUNDANCY_INDEX].data.static_value.value.i32 = 0;
    s->nodes[MU_BASE_RUNTIME_REDUNDANCY_INDEX] = (mu_node_t){
        .node_id = {.namespace_index = 0, .identifier_type = MU_NODEID_NUMERIC, .identifier.numeric = 3709u},
        .node_class = MU_NODECLASS_VARIABLE,
        .browse_name = {.length = (opcua_int32_t)17, .data = (const opcua_byte_t *)"RedundancySupport"},
        .display_name = {.length = (opcua_int32_t)17, .data = (const opcua_byte_t *)"RedundancySupport"},
        .references = NULL,
        .reference_count = 0,
        .value = &s->values[MU_BASE_RUNTIME_REDUNDANCY_INDEX],
        .type_definition = {0, MU_NODEID_NUMERIC, {.numeric = 68u}}}; /* PropertyType */
#endif

    s->space.nodes = s->nodes;
    s->space.node_count = MU_BASE_RUNTIME_NODE_COUNT;
#else
    s->space.nodes = NULL;
    s->space.node_count = 0;
#endif /* MUC_OPCUA_FACET_CORE_2022_SERVER */
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

#if MUC_OPCUA_CU_DATA_ACCESS
bool mu_resolve_eurange_span(const mu_address_space_t *user, mu_address_space_index_t *user_index,
                             const mu_address_space_t *dynamic, const mu_node_t *node, double *out_span) {
    if (node == NULL || node->references == NULL || out_span == NULL) {
        return false;
    }
    /* The EURange Property is identified by its BrowseName "EURange" (OPC-10000-8
       §5.3.2), NOT a fixed NodeId: on an instance the Property node has an
       integrator-assigned NodeId. Its value is a Range (low, high) carried as a
       2-element Double array. */
    static const opcua_byte_t k_eurange[] = "EURange";
    for (size_t i = 0; i < node->reference_count; ++i) {
        if (node->references[i].reference_type_id.identifier.numeric != 46u) { /* HasProperty */
            continue;
        }
        const mu_node_t *prop = mu_resolve_node(user, user_index, dynamic, &node->references[i].target_id);
        if (prop == NULL || prop->browse_name.length != (opcua_int32_t)(sizeof(k_eurange) - 1u) ||
            memcmp(prop->browse_name.data, k_eurange, sizeof(k_eurange) - 1u) != 0) {
            continue;
        }
        if (prop->value != NULL && prop->value->type == MU_VALUESOURCE_STATIC) {
            const mu_variant_t *sv = &prop->value->data.static_value;
            if (sv->type == MU_TYPE_DOUBLE && sv->value.array != NULL) {
                const opcua_double_t *darr = (const opcua_double_t *)sv->value.array;
                *out_span = darr[1] - darr[0]; /* High - Low; may be 0.0 (valid) */
                return true;
            }
        }
        /* EURange Property found but its value is unreadable: treat as absent
           (cannot compute a percent threshold). */
    }
    return false;
}
#endif
