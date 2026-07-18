/* src/address_space/base_nodes.c */
#include "base_nodes.h"
#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/services/method.h"
#include <stddef.h>
#include <string.h>

/* Server Object EventNotifier attribute (OPC-10000-3 §5.4.6): bit 0 =
 * SubscribeToEvents. Advertised only when the event delivery surface is
 * compiled in, so a no-events profile does not claim to be an event source
 * (spec 074 / CU 3194). */
#if MUC_OPCUA_CU_EVENTS
#define MU_SERVER_EVENTNOTIFIER 0x01u
#else
#define MU_SERVER_EVENTNOTIFIER 0x00u
#endif

#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER

/* spec 080b (GPT-5 review, DESIGN DECISION 1): Structure(22) and its subtype /
 * Encoding-Object scaffolding must compile whenever ANY CU that introduces a
 * Structure subtype or an Encoding Object is enabled -- not just the historical
 * LocalTime / EngineeringUnits / Currency trio.  This decouples the Argument
 * (CU 3641) and base-types (CU 3188) claims from those unrelated CUs. */
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME || defined(MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS) ||                           \
    defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY) || MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE ||                                \
    MUC_OPCUA_CU_BASE_INFO_BASE_TYPES || MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
#define MU_HAVE_STRUCTURE_TYPE 1
#else
#define MU_HAVE_STRUCTURE_TYPE 0
#endif

/* Pooled strings to save flash */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif
static const opcua_byte_t s_str_AggregateFunctions[] = "AggregateFunctions";
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
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME || defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY) ||                                    \
    MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE || MUC_OPCUA_CU_BASE_INFO_BASE_TYPES || MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
static const opcua_byte_t s_str_Default_Binary[] = "Default Binary";
#endif
static const opcua_byte_t s_str_Double[] = "Double";
#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS
static const opcua_byte_t s_str_EUInformation[] = "EUInformation";
#endif
#if defined(MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME) ||                                                           \
    (MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION)
/* spec 085 (CU 5801) Task 4: also needed for ServerType.EstimatedReturnTime
   (12882), the ObjectType-level InstanceDeclaration, which per the CU 5801
   Kconfig help text must exist "even if ... not used in any instance of the
   Server" -- i.e. independent of whether ESTIMATED_RETURN_TIME's own
   Server-instance Variable(12885) is compiled in. */
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
/* spec 090 (CU 5801 fix): LocaleId(295) DataType BrowseName -- the Mandatory
   element DataType of ServerCapabilitiesType.LocaleIdArray(2016), a subtype
   of String(12) with no encodings (OPC-10000-5 §6.3.2 Table 10 /
   §12.2.11.1). Previously dangling: LocaleIdArray declared .data_type = 295
   but no such node existed in the address space. */
static const opcua_byte_t s_str_LocaleId[] = "LocaleId";
/* spec 091 (CU 5801): MessageSecurityMode(302) DataType BrowseName -- the
   closure Enumeration DataType for SessionSecurityDiagnosticsType.SecurityMode
   (2251), a subtype of Enumeration(29) with no EnumStrings child (mirrors the
   ServerState(852)/RedundancySupport(851) precedent, OPC-10000-4 §7.20
   Table 139). */
static const opcua_byte_t s_str_MessageSecurityMode[] = "MessageSecurityMode";
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT
static const opcua_byte_t s_str_Locations[] = "Locations";
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
/* spec 090 (CU 5801 fix): ServerType.LocalTime(17612), the ObjectType-level
   InstanceDeclaration, is re-gated onto its true owner
   CU_BASE_INFO_LOCALTIME (full-only) instead of the bare SERVERTYPE &&
   TYPE_INFORMATION condition -- see the node definition near NodeId 17612
   below. The old gate exposed the property at embedded/standard without its
   TimeZoneDataType(8912) DataType (itself LOCALTIME-gated), a dangling
   reference. */
static const opcua_byte_t s_str_LocalTime[] = "LocalTime";
#endif
static const opcua_byte_t s_str_LocalizedText[] = "LocalizedText";
static const opcua_byte_t s_str_MaxArrayLength[] = "MaxArrayLength";
static const opcua_byte_t s_str_MaxMonitoredItems[] = "MaxMonitoredItems";
static const opcua_byte_t s_str_MaxMonitoredItemsPerCall[] = "MaxMonitoredItemsPerCall";
static const opcua_byte_t s_str_MaxMonitoredItemsPerSubscription[] = "MaxMonitoredItemsPerSubscription";
static const opcua_byte_t s_str_MaxMonitoredItemsQueueSize[] = "MaxMonitoredItemsQueueSize";
static const opcua_byte_t s_str_MaxSubscriptions[] = "MaxSubscriptions";
static const opcua_byte_t s_str_MaxSubscriptionsPerSession[] = "MaxSubscriptionsPerSession";
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
static const opcua_byte_t s_str_Number[] = "Number";
static const opcua_byte_t s_str_Decimal[] = "Decimal";
static const opcua_byte_t s_str_DurationString[] = "DurationString";
static const opcua_byte_t s_str_TimeString[] = "TimeString";
static const opcua_byte_t s_str_DateString[] = "DateString";
static const opcua_byte_t s_str_Argument[] = "Argument";
static const opcua_byte_t s_str_HasEncoding[] = "HasEncoding";
static const opcua_byte_t s_str_DataTypeEncodingType[] = "DataTypeEncodingType";
static const opcua_byte_t s_str_Default_XML[] = "Default XML";
/* ModellingRule Object BrowseNames: shared by the Data Access property
 * declarations (spec 060) and the base type system (spec 080b, CU 3188). */
static const opcua_byte_t s_str_Mandatory[] = "Mandatory";
static const opcua_byte_t s_str_Optional[] = "Optional";
#if MU_HAVE_STRUCTURE_TYPE
static const opcua_byte_t s_str_Structure[] = "Structure";
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
/* spec 080b (CU 3188): BrowseNames for the remaining base type-system nodes. */
static const opcua_byte_t s_str_Guid[] = "Guid";
static const opcua_byte_t s_str_ByteString[] = "ByteString";
static const opcua_byte_t s_str_XmlElement[] = "XmlElement";
static const opcua_byte_t s_str_ExpandedNodeId[] = "ExpandedNodeId";
static const opcua_byte_t s_str_DataValue[] = "DataValue";
static const opcua_byte_t s_str_DiagnosticInfo[] = "DiagnosticInfo";
static const opcua_byte_t s_str_Integer[] = "Integer";
static const opcua_byte_t s_str_UInteger[] = "UInteger";
static const opcua_byte_t s_str_Enumeration[] = "Enumeration";
static const opcua_byte_t s_str_Duration[] = "Duration";
static const opcua_byte_t s_str_NumericRange[] = "NumericRange";
static const opcua_byte_t s_str_UtcTime[] = "UtcTime";
static const opcua_byte_t s_str_EnumValueType[] = "EnumValueType";
static const opcua_byte_t s_str_Union[] = "Union";
static const opcua_byte_t s_str_HasModellingRule[] = "HasModellingRule";
static const opcua_byte_t s_str_ModellingRuleType[] = "ModellingRuleType";
static const opcua_byte_t s_str_ExposesItsArray[] = "ExposesItsArray";
static const opcua_byte_t s_str_OptionalPlaceholder[] = "OptionalPlaceholder";
static const opcua_byte_t s_str_MandatoryPlaceholder[] = "MandatoryPlaceholder";
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
/* spec 083 (CU 3189): BrowseNames for the ServerType structured DataTypes + enums
   (ObjectType/VariableType BrowseNames follow below). */
static const opcua_byte_t s_str_BuildInfo[] = "BuildInfo";
static const opcua_byte_t s_str_RedundantServerDataType[] = "RedundantServerDataType";
static const opcua_byte_t s_str_SamplingIntervalDiagnosticsDataType[] = "SamplingIntervalDiagnosticsDataType";
static const opcua_byte_t s_str_ServerDiagnosticsSummaryDataType[] = "ServerDiagnosticsSummaryDataType";
static const opcua_byte_t s_str_ServerStatusDataType[] = "ServerStatusDataType";
static const opcua_byte_t s_str_SessionDiagnosticsDataType[] = "SessionDiagnosticsDataType";
static const opcua_byte_t s_str_SessionSecurityDiagnosticsDataType[] = "SessionSecurityDiagnosticsDataType";
static const opcua_byte_t s_str_ServiceCounterDataType[] = "ServiceCounterDataType";
static const opcua_byte_t s_str_SubscriptionDiagnosticsDataType[] = "SubscriptionDiagnosticsDataType";
static const opcua_byte_t s_str_EndpointUrlListDataType[] = "EndpointUrlListDataType";
static const opcua_byte_t s_str_NetworkGroupDataType[] = "NetworkGroupDataType";
static const opcua_byte_t s_str_ServerState[] = "ServerState";
/* spec 083 (CU 3189): BrowseNames for the ServerType-tree ObjectTypes. */
static const opcua_byte_t s_str_ServerCapabilitiesType[] = "ServerCapabilitiesType";
static const opcua_byte_t s_str_ServerDiagnosticsType[] = "ServerDiagnosticsType";
static const opcua_byte_t s_str_SessionsDiagnosticsSummaryType[] = "SessionsDiagnosticsSummaryType";
static const opcua_byte_t s_str_SessionDiagnosticsObjectType[] = "SessionDiagnosticsObjectType";
static const opcua_byte_t s_str_VendorServerInfoType[] = "VendorServerInfoType";
static const opcua_byte_t s_str_ServerRedundancyType[] = "ServerRedundancyType";
static const opcua_byte_t s_str_TransparentRedundancyType[] = "TransparentRedundancyType";
static const opcua_byte_t s_str_NonTransparentRedundancyType[] = "NonTransparentRedundancyType";
static const opcua_byte_t s_str_OperationLimitsType[] = "OperationLimitsType";
static const opcua_byte_t s_str_FileType[] = "FileType";
static const opcua_byte_t s_str_AddressSpaceFileType[] = "AddressSpaceFileType";
static const opcua_byte_t s_str_NamespaceMetadataType[] = "NamespaceMetadataType";
static const opcua_byte_t s_str_NamespacesType[] = "NamespacesType";
static const opcua_byte_t s_str_NonTransparentNetworkRedundancyType[] = "NonTransparentNetworkRedundancyType";
/* spec 083 (CU 3189): BrowseNames for the ServerType-tree VariableTypes. */
static const opcua_byte_t s_str_ServerVendorCapabilityType[] = "ServerVendorCapabilityType";
static const opcua_byte_t s_str_ServerStatusType[] = "ServerStatusType";
static const opcua_byte_t s_str_ServerDiagnosticsSummaryType[] = "ServerDiagnosticsSummaryType";
static const opcua_byte_t s_str_SamplingIntervalDiagnosticsArrayType[] = "SamplingIntervalDiagnosticsArrayType";
static const opcua_byte_t s_str_SamplingIntervalDiagnosticsType[] = "SamplingIntervalDiagnosticsType";
static const opcua_byte_t s_str_SubscriptionDiagnosticsArrayType[] = "SubscriptionDiagnosticsArrayType";
static const opcua_byte_t s_str_SubscriptionDiagnosticsType[] = "SubscriptionDiagnosticsType";
static const opcua_byte_t s_str_SessionDiagnosticsArrayType[] = "SessionDiagnosticsArrayType";
static const opcua_byte_t s_str_SessionDiagnosticsVariableType[] = "SessionDiagnosticsVariableType";
static const opcua_byte_t s_str_SessionSecurityDiagnosticsArrayType[] = "SessionSecurityDiagnosticsArrayType";
static const opcua_byte_t s_str_SessionSecurityDiagnosticsType[] = "SessionSecurityDiagnosticsType";
static const opcua_byte_t s_str_BuildInfoType[] = "BuildInfoType";
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
/* spec 085 (CU 5801): BrowseNames for the ServerStatusType/BuildInfoType/
   ServerDiagnosticsSummaryType InstanceDeclarations (State and BuildInfo are
   already declared above and reused). */
static const opcua_byte_t s_str_StartTime[] = "StartTime";
static const opcua_byte_t s_str_CurrentTime[] = "CurrentTime";
static const opcua_byte_t s_str_SecondsTillShutdown[] = "SecondsTillShutdown";
static const opcua_byte_t s_str_ShutdownReason[] = "ShutdownReason";
static const opcua_byte_t s_str_ProductUri[] = "ProductUri";
static const opcua_byte_t s_str_ManufacturerName[] = "ManufacturerName";
static const opcua_byte_t s_str_ProductName[] = "ProductName";
static const opcua_byte_t s_str_SoftwareVersion[] = "SoftwareVersion";
static const opcua_byte_t s_str_BuildNumber[] = "BuildNumber";
static const opcua_byte_t s_str_BuildDate[] = "BuildDate";
static const opcua_byte_t s_str_ServerViewCount[] = "ServerViewCount";
static const opcua_byte_t s_str_CurrentSessionCount[] = "CurrentSessionCount";
static const opcua_byte_t s_str_CumulatedSessionCount[] = "CumulatedSessionCount";
static const opcua_byte_t s_str_SecurityRejectedSessionCount[] = "SecurityRejectedSessionCount";
static const opcua_byte_t s_str_RejectedSessionCount[] = "RejectedSessionCount";
static const opcua_byte_t s_str_SessionTimeoutCount[] = "SessionTimeoutCount";
static const opcua_byte_t s_str_SessionAbortCount[] = "SessionAbortCount";
static const opcua_byte_t s_str_PublishingIntervalCount[] = "PublishingIntervalCount";
static const opcua_byte_t s_str_CurrentSubscriptionCount[] = "CurrentSubscriptionCount";
static const opcua_byte_t s_str_CumulatedSubscriptionCount[] = "CumulatedSubscriptionCount";
static const opcua_byte_t s_str_SecurityRejectedRequestsCount[] = "SecurityRejectedRequestsCount";
static const opcua_byte_t s_str_RejectedRequestsCount[] = "RejectedRequestsCount";
/* spec 085 (CU 5801) Task 4: BrowseNames for the ServerCapabilitiesType/
   ServerType own InstanceDeclarations not already declared above/elsewhere. */
static const opcua_byte_t s_str_MinSupportedSampleRate[] = "MinSupportedSampleRate";
static const opcua_byte_t s_str_MaxBrowseContinuationPoints[] = "MaxBrowseContinuationPoints";
static const opcua_byte_t s_str_MaxQueryContinuationPoints[] = "MaxQueryContinuationPoints";
static const opcua_byte_t s_str_MaxHistoryContinuationPoints[] = "MaxHistoryContinuationPoints";
static const opcua_byte_t s_str_MaxLogObjectContinuationPoints[] = "MaxLogObjectContinuationPoints";
static const opcua_byte_t s_str_SoftwareCertificates[] = "SoftwareCertificates";
/* spec 090 (CU 5801 fix): SignedSoftwareCertificate(344) DataType BrowseName --
   the Mandatory element DataType of ServerCapabilitiesType.SoftwareCertificates
   (3049), a subtype of Structure(22) (OPC-10000-4 §7.37, OPC-10000-5 §12.3.13).
   Previously dangling: SoftwareCertificates declared .data_type = 344 but no
   such node existed in the address space. */
static const opcua_byte_t s_str_SignedSoftwareCertificate[] = "SignedSoftwareCertificate";
static const opcua_byte_t s_str_MaxByteStringLength[] = "MaxByteStringLength";
static const opcua_byte_t s_str_ModellingRules[] = "ModellingRules";
static const opcua_byte_t s_str_MaxSessions[] = "MaxSessions";
static const opcua_byte_t s_str_MaxSelectClauseParameters[] = "MaxSelectClauseParameters";
static const opcua_byte_t s_str_MaxWhereClauseParameters[] = "MaxWhereClauseParameters";
static const opcua_byte_t s_str_VendorCapability_Placeholder[] = "<VendorCapability>";
static const opcua_byte_t s_str_ConformanceUnits[] = "ConformanceUnits";
/* spec 090 (CU 5801 fix): ServerType.UrisVersion(15003)/VersionTime(20998) --
   removed. UrisVersion is Optional and owned by CU 3994 (Session Sessionless
   Invocation, part of the "Sessionless Server Facet"), which is entirely
   unimplemented in this codebase (docs/conformance/completion.md: 0/2). The
   property was previously declared under the bare SERVERTYPE &&
   TYPE_INFORMATION gate with .data_type = 20998, but no VersionTime DataType
   node exists (nor should it be claimed without implementing CU 3994). Not
   exposed in any profile until that facet is implemented. */
static const opcua_byte_t s_str_Auditing[] = "Auditing";
static const opcua_byte_t s_str_ServerDiagnostics[] = "ServerDiagnostics";
static const opcua_byte_t s_str_VendorServerInfo[] = "VendorServerInfo";
static const opcua_byte_t s_str_SetSubscriptionDurable[] = "SetSubscriptionDurable";
static const opcua_byte_t s_str_RequestServerStateChange[] = "RequestServerStateChange";
/* spec 090 (CU 5801 Part B): BrowseNames for OperationLimitsType's own
   InstanceDeclarations not already declared above (MaxNodesPerRead/Write/
   Browse and MaxMonitoredItemsPerCall are shared with the runtime
   OperationLimits(11704) instance's properties and declared unconditionally
   further below). */
static const opcua_byte_t s_str_MaxNodesPerHistoryReadData[] = "MaxNodesPerHistoryReadData";
static const opcua_byte_t s_str_MaxNodesPerHistoryReadEvents[] = "MaxNodesPerHistoryReadEvents";
static const opcua_byte_t s_str_MaxNodesPerHistoryUpdateData[] = "MaxNodesPerHistoryUpdateData";
static const opcua_byte_t s_str_MaxNodesPerHistoryUpdateEvents[] = "MaxNodesPerHistoryUpdateEvents";
static const opcua_byte_t s_str_MaxNodesPerMethodCall[] = "MaxNodesPerMethodCall";
static const opcua_byte_t s_str_MaxNodesPerRegisterNodes[] = "MaxNodesPerRegisterNodes";
static const opcua_byte_t s_str_MaxNodesPerTranslateBrowsePathsToNodeIds[] = "MaxNodesPerTranslateBrowsePathsToNodeIds";
static const opcua_byte_t s_str_MaxNodesPerNodeManagement[] = "MaxNodesPerNodeManagement";
/* spec 091 (CU 5801): BrowseNames for SamplingIntervalDiagnosticsType's own
   InstanceDeclarations (OPC-10000-5 §7.10 Table 80). */
static const opcua_byte_t s_str_SamplingInterval[] = "SamplingInterval";
static const opcua_byte_t s_str_SampledMonitoredItemsCount[] = "SampledMonitoredItemsCount";
static const opcua_byte_t s_str_MaxSampledMonitoredItemsCount[] = "MaxSampledMonitoredItemsCount";
static const opcua_byte_t s_str_DisabledMonitoredItemsSamplingCount[] = "DisabledMonitoredItemsSamplingCount";
/* spec 091 (CU 5801): BrowseNames for SessionSecurityDiagnosticsType's own
   InstanceDeclarations (OPC-10000-5 §7.16 Table 86). */
static const opcua_byte_t s_str_SessionId[] = "SessionId";
static const opcua_byte_t s_str_ClientUserIdOfSession[] = "ClientUserIdOfSession";
static const opcua_byte_t s_str_ClientUserIdHistory[] = "ClientUserIdHistory";
static const opcua_byte_t s_str_AuthenticationMechanism[] = "AuthenticationMechanism";
static const opcua_byte_t s_str_Encoding[] = "Encoding";
static const opcua_byte_t s_str_TransportProtocol[] = "TransportProtocol";
static const opcua_byte_t s_str_SecurityMode[] = "SecurityMode";
static const opcua_byte_t s_str_SecurityPolicyUri[] = "SecurityPolicyUri";
static const opcua_byte_t s_str_ClientCertificate[] = "ClientCertificate";
#endif
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
static const opcua_byte_t s_str_MultiStateDiscreteType[] = "MultiStateDiscreteType";
static const opcua_byte_t s_str_MultiStateValueDiscreteType[] = "MultiStateValueDiscreteType";
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
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {37}}, true}, /* HasSubtype -> HasModellingRule (CU 3188) */
#endif
#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE || MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {38}}, true}, /* HasSubtype -> HasEncoding (CU 3641/3188) */
#endif
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
#if !MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* Numeric primitives are direct BaseDataType subtypes until the base type
       system (CU 3188, spec 080b) re-parents them under Integer(27)/UInteger(28)/
       Number(26) to match OPC-10000-3. */
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
#endif
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {12}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {13}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {17}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {19}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {20}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {21}}, true},
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {14}}, true}, /* Guid */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {15}}, true}, /* ByteString */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {16}}, true}, /* XmlElement */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {18}}, true}, /* ExpandedNodeId */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {23}}, true}, /* DataValue */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {25}}, true}, /* DiagnosticInfo */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {29}}, true}, /* Enumeration */
#endif
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {26}}, true}, /* HasSubtype -> Number (CU 4426) */
#endif
#if MU_HAVE_STRUCTURE_TYPE
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {22}}, true}, /* HasSubtype -> Structure */
#endif
};

#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
/* CU 4426: HasSubtype forward refs for the specialized Number DataType nodes. */
static const mu_reference_t s_number_subtype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {50}}, true}, /* Number -> Decimal (CU 4426) */
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {10}}, true}, /* Number -> Float */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11}}, true}, /* Number -> Double */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {27}}, true}, /* Number -> Integer */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {28}}, true}, /* Number -> UInteger */
#endif
};
#endif
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES || (MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION)
/* CU 2483 / spec 090 (CU 5801 fix): HasSubtype forward refs for String(12)'s
   specialized leaf DataTypes. LocaleId(295) is needed whenever
   ServerCapabilitiesType.LocaleIdArray(2016) is exposed (Mandatory property,
   gated by SERVERTYPE && TYPE_INFORMATION), independent of whether
   CU_BASE_INFO_DATATYPES is also enabled. */
static const mu_reference_t s_string_subtype_refs[] = {
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {12879}}, true}, /* String -> DurationString */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {12880}}, true}, /* String -> TimeString */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {12881}}, true}, /* String -> DateString */
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {291}}, true}, /* String -> NumericRange (CU 3188) */
#endif
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {295}}, true}, /* String -> LocaleId */
#endif
};
#endif

#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
/* CU 3188: HasSubtype closure for the re-parented numeric primitives and the
   new specialized leaf DataTypes, plus HasEncoding for EnumValueType. */
static const mu_reference_t s_integer_subtype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2}}, true}, /* Integer -> SByte */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {4}}, true}, /* Integer -> Int16 */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {6}}, true}, /* Integer -> Int32 */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {8}}, true}, /* Integer -> Int64 */
};
static const mu_reference_t s_uinteger_subtype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {3}}, true}, /* UInteger -> Byte */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {5}}, true}, /* UInteger -> UInt16 */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {7}}, true}, /* UInteger -> UInt32 */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {9}}, true}, /* UInteger -> UInt64 */
};
static const mu_reference_t s_double_subtype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {290}}, true}}; /* Double -> Duration */
static const mu_reference_t s_datetime_subtype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {294}}, true}}; /* DateTime -> UtcTime */
static const mu_reference_t s_enum_value_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {7616}}, true}, /* EnumValueType -HasEncoding-> Default XML */
    {{0, MU_NODEID_NUMERIC, {38}},
     {0, MU_NODEID_NUMERIC, {8251}},
     true}}; /* EnumValueType -HasEncoding-> Default Binary */
#endif

#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
/* spec 083 (CU 3189): Enumeration(29) HasSubtype closure for the ServerType enums,
   plus HasEncoding refs for each structured DataType's Default XML/Binary
   Encoding Objects (DataTypeEncodingType 76).
   spec 091 (CU 5801): when TYPE_INFORMATION is also on, Enumeration(29) gets a
   third HasSubtype -> MessageSecurityMode(302), the closure DataType for
   SessionSecurityDiagnosticsType.SecurityMode(2251). Kept as two mutually
   exclusive arrays (rather than one array unconditionally including 302) so
   the extra ref only compiles in when 302 itself exists -- otherwise a
   SERVERTYPE-only build with TYPE_INFORMATION off would carry a dangling
   forward reference. */
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
static const mu_reference_t s_enumeration_refs_with_message_security_mode[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {851}}, true}, /* Enumeration -> RedundancySupport */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {852}}, true}, /* Enumeration -> ServerState */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {302}}, true}, /* Enumeration -> MessageSecurityMode */
};
#else
static const mu_reference_t s_enumeration_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {851}}, true}, /* Enumeration -> RedundancySupport */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {852}}, true}, /* Enumeration -> ServerState */
};
#endif
static const mu_reference_t s_build_info_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {339}}, true}, /* BuildInfo -HasEncoding-> Default XML */
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {340}}, true}, /* BuildInfo -HasEncoding-> Default Binary */
};
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
/* spec 090 (CU 5801 fix): SignedSoftwareCertificate(344) HasEncoding refs --
   the Mandatory element DataType of ServerCapabilitiesType.SoftwareCertificates
   (3049). */
static const mu_reference_t s_signed_software_certificate_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}},
     {0, MU_NODEID_NUMERIC, {345}},
     true}, /* SignedSoftwareCertificate -HasEncoding-> Default XML */
    {{0, MU_NODEID_NUMERIC, {38}},
     {0, MU_NODEID_NUMERIC, {346}},
     true}, /* SignedSoftwareCertificate -HasEncoding-> Default Binary */
};
#endif
static const mu_reference_t s_redundant_server_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {854}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {855}}, true},
};
static const mu_reference_t s_sampling_interval_diagnostics_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {857}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {858}}, true},
};
static const mu_reference_t s_server_diagnostics_summary_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {860}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {861}}, true},
};
static const mu_reference_t s_server_status_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {863}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {864}}, true},
};
static const mu_reference_t s_session_diagnostics_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {866}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {867}}, true},
};
static const mu_reference_t s_session_security_diagnostics_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {869}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {870}}, true},
};
static const mu_reference_t s_service_counter_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {872}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {873}}, true},
};
static const mu_reference_t s_subscription_diagnostics_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {875}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {876}}, true},
};
static const mu_reference_t s_endpoint_url_list_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {11949}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {11957}}, true},
};
static const mu_reference_t s_network_group_data_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {11950}}, true},
    {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {11958}}, true},
};
#endif

#if MU_HAVE_STRUCTURE_TYPE
static const mu_reference_t s_structure_refs[] = {
#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {296}}, true}, /* HasSubtype -> Argument (CU 3641) */
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {7594}}, true},  /* HasSubtype -> EnumValueType (CU 3188) */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {12756}}, true}, /* HasSubtype -> Union (CU 3188) */
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_ID_EUINFORMATION_DATATYPE}}, true},
#endif
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_TIMEZONEDATATYPE}}, true},
#endif
#ifdef MUC_OPCUA_CU_BASE_INFO_CURRENCY
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {MU_NODEID_CURRENCYUNITTYPE}}, true},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* spec 083 (CU 3189): the ServerType structured DataTypes. */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {338}}, true}, /* HasSubtype -> BuildInfo */
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 090 (CU 5801 fix): SignedSoftwareCertificate is only needed when
       ServerCapabilitiesType.SoftwareCertificates(3049) is exposed. */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {344}}, true}, /* HasSubtype -> SignedSoftwareCertificate */
#endif
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {853}}, true}, /* HasSubtype -> RedundantServerDataType */
    {{0, MU_NODEID_NUMERIC, {45}},
     {0, MU_NODEID_NUMERIC, {856}},
     true}, /* HasSubtype -> SamplingIntervalDiagnosticsDataType */
    {{0, MU_NODEID_NUMERIC, {45}},
     {0, MU_NODEID_NUMERIC, {859}},
     true}, /* HasSubtype -> ServerDiagnosticsSummaryDataType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {862}}, true}, /* HasSubtype -> ServerStatusDataType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {865}}, true}, /* HasSubtype -> SessionDiagnosticsDataType */
    {{0, MU_NODEID_NUMERIC, {45}},
     {0, MU_NODEID_NUMERIC, {868}},
     true}, /* HasSubtype -> SessionSecurityDiagnosticsDataType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {871}}, true}, /* HasSubtype -> ServiceCounterDataType */
    {{0, MU_NODEID_NUMERIC, {45}},
     {0, MU_NODEID_NUMERIC, {874}},
     true}, /* HasSubtype -> SubscriptionDiagnosticsDataType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11943}}, true}, /* HasSubtype -> EndpointUrlListDataType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11944}}, true}, /* HasSubtype -> NetworkGroupDataType */
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
#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE || MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {76}}, true}, /* HasSubtype -> DataTypeEncodingType */
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {77}}, true}, /* HasSubtype -> ModellingRuleType (CU 3188) */
#endif
#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17605}}, true},
#endif
#if MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {17602}}, true},
#endif
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2004}}, true}
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    ,
    /* spec 083 (CU 3189): ServerType-tree ObjectType subtypes of BaseObjectType. */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2013}}, true}, /* HasSubtype -> ServerCapabilitiesType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2020}}, true}, /* HasSubtype -> ServerDiagnosticsType */
    {{0, MU_NODEID_NUMERIC, {45}},
     {0, MU_NODEID_NUMERIC, {2026}},
     true}, /* HasSubtype -> SessionsDiagnosticsSummaryType */
    {{0, MU_NODEID_NUMERIC, {45}},
     {0, MU_NODEID_NUMERIC, {2029}},
     true}, /* HasSubtype -> SessionDiagnosticsObjectType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2033}}, true}, /* HasSubtype -> VendorServerInfoType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2034}}, true}  /* HasSubtype -> ServerRedundancyType */
#if MUC_OPCUA_CU_NAMESPACES
    ,
    /* spec 090 (CU 5801 Part A): FileType/NamespaceMetadataType/NamespacesType
       moved here from the bare SERVERTYPE gate above -- their true owner is
       CU_NAMESPACES (full-only), so embedded/standard (SERVERTYPE on,
       NAMESPACES off) no longer expose types they never use for CU 5801
       completeness. NamespaceMetadataType(11616) is properly owned by
       CU_NAMESPACE_METADATA(3545) (also full-only); grouped under
       CU_NAMESPACES here as a follow-up refinement. */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11575}}, true}, /* HasSubtype -> FileType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11616}}, true}, /* HasSubtype -> NamespaceMetadataType */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11645}}, true}  /* HasSubtype -> NamespacesType */
#endif
#endif
};

#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
/* spec 083 (CU 3189): FolderType HasSubtype -> OperationLimitsType. */
static const mu_reference_t s_folder_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11564}}, true}};

/* spec 083 (CU 3189): ServerRedundancyType HasSubtype -> Transparent-/
   NonTransparentRedundancyType. */
static const mu_reference_t s_server_redundancy_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2036}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2039}}, true}};

/* spec 083 (CU 3189): NonTransparentRedundancyType HasSubtype ->
   NonTransparentNetworkRedundancyType. */
static const mu_reference_t s_non_transparent_redundancy_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11945}}, true}};

#if MUC_OPCUA_CU_NAMESPACES
/* spec 083 (CU 3189)/spec 090 (CU 5801 Part A): FileType HasSubtype ->
   AddressSpaceFileType. Both nodes moved to the CU_NAMESPACES gate (full-only)
   -- see s_base_object_type_refs above for the rationale -- so this array is
   only referenced (and only needs to compile) when CU_NAMESPACES is on. */
static const mu_reference_t s_file_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {11595}}, true}};
#endif
#endif

#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
/* spec 085 (CU 5801): shared HasModellingRule(37)->Mandatory(78) plus
   HasTypeDefinition(40) forward refs for the ServerStatusType/BuildInfoType/
   ServerDiagnosticsSummaryType own InstanceDeclarations. Mirrors the Data
   Access property-declaration convention (s_da_prop_mandatory_refs below),
   which also carries an explicit HasTypeDefinition ref so Browse(HasTypeDefinition)
   / Browse(all forward) on the declaration node itself returns it, in addition
   to the node's cached `type_definition` field used to fill in ReferenceDescription. */
static const mu_reference_t s_mandatory_bdv_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {63}}, true}};

/* ServerStatusType.BuildInfo(2142): Mandatory, TypeDefinition BuildInfoType(3051)
   rather than plain BaseDataVariableType(63). */
static const mu_reference_t s_mandatory_buildinfotype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {3051}}, true}};

/* ServerStatusType(2138) HasComponent(47) -> its 6 own declarations
   (OPC-10000-5 §7.6). */
static const mu_reference_t s_server_status_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2139}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2140}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2141}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2142}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2752}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2753}}, true}};

/* BuildInfoType(3051) HasComponent(47) -> its 6 own declarations
   (OPC-10000-5 §7.7 Table 77). Shared by both the DataAccess-on and
   DataAccess-off copies of the 3051 node entry. */
static const mu_reference_t s_build_info_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {3052}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {3053}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {3054}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {3055}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {3056}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {3057}}, true}};

/* ServerDiagnosticsSummaryType(2150) HasComponent(47) -> its 12 own
   declarations (OPC-10000-5 §7.8). NodeId 2158 is skipped in the official
   numbering -- not a gap here. */
static const mu_reference_t s_server_diagnostics_summary_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2151}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2152}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2153}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2154}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2155}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2156}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2157}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2159}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2160}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2161}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2162}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2163}}, true}};

/* spec 091 (CU 5801): SamplingIntervalDiagnosticsType(2165) HasComponent(47)
   -> its 4 own declarations (OPC-10000-5 §7.10 Table 80). Grounded via
   opc-ua-reference: search_nodes confirms 2166/11697/11698/11699 all resolve
   to OPC-10000-5 §7.10; Table 80 itself gives every row as
   HasComponent/Mandatory/BaseDataVariableType (fetched via WebFetch since
   search_text truncates before the reference table body). None of the four
   fall in the DataAccess band (2365-11461): 2166 < 2365, 11697-11699 > 11461,
   so this is a single (non-DataAccess-branched) declaration set. */
static const mu_reference_t s_sampling_interval_diagnostics_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2166}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11697}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11698}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11699}}, true}};

/* spec 091 (CU 5801): SessionSecurityDiagnosticsType(2244) HasComponent(47)
   -> its 9 own declarations (OPC-10000-5 §7.16 Table 86). Grounded via
   opc-ua-reference (search_nodes confirms every child NodeId resolves to
   §7.16; Table 86 itself -- fetched via WebFetch since search_text truncates
   the reference table body -- gives every row as HasComponent/Variable/
   BaseDataVariableType/Mandatory, matching exactly). ClientCertificate(3058)
   falls inside the DataAccess band (2365..11461), so it is dual-placed in
   base_nodes.c (see the DataAccess-on/-off copies near BuildInfoType's own
   declarations) -- this ref list still names it once, since exactly one copy
   of the 3058 node exists in any given build. */
static const mu_reference_t s_session_security_diagnostics_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2245}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2246}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2247}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2248}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2249}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2250}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2251}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2252}}, true},
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {3058}}, true}};

/* spec 085 (CU 5801) Task 4: shared HasModellingRule(37)->Mandatory(78)/
   Optional(80) plus HasTypeDefinition(40) forward refs for the
   ServerCapabilitiesType(2013)/ServerType(2004) own Property/Object/Method
   InstanceDeclarations (OPC-10000-5 §6.3.2, §6.3.1). One array per distinct
   (ModellingRule, TypeDefinition) combination, reused across both owner types
   where the combination matches. */
static const mu_reference_t s_mandatory_property_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {68}}, true}};

static const mu_reference_t s_optional_property_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {80}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {68}}, true}};

/* ServerCapabilitiesType.OperationLimits(11551): Optional, TypeDefinition
   OperationLimitsType(11564). */
static const mu_reference_t s_optional_operationlimitstype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {80}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {11564}}, true}};

/* spec 090 (CU 5801 Part B): OperationLimitsType(11564) HasProperty(46) -> its
   12 own Optional Property InstanceDeclarations (OPC-10000-5 §6.3.11 Table
   20), grounded against the official OPC Foundation NodeIds.csv (schemas
   1.05). Order follows the spec table (History Read/Update pairs interleaved
   with their non-History counterparts), not ascending NodeId -- same
   convention as s_server_capabilities_type_refs below. */
static const mu_reference_t s_operation_limits_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11565}}, true}, /* MaxNodesPerRead */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12161}}, true}, /* MaxNodesPerHistoryReadData */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12162}}, true}, /* MaxNodesPerHistoryReadEvents */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11567}}, true}, /* MaxNodesPerWrite */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12163}}, true}, /* MaxNodesPerHistoryUpdateData */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12164}}, true}, /* MaxNodesPerHistoryUpdateEvents */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11569}}, true}, /* MaxNodesPerMethodCall */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11570}}, true}, /* MaxNodesPerBrowse */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11571}}, true}, /* MaxNodesPerRegisterNodes */
    {{0, MU_NODEID_NUMERIC, {46}},
     {0, MU_NODEID_NUMERIC, {11572}},
     true}, /* MaxNodesPerTranslateBrowsePathsToNodeIds */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11573}}, true}, /* MaxNodesPerNodeManagement */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11574}}, true}  /* MaxMonitoredItemsPerCall */
};

/* ServerCapabilitiesType.ModellingRules(2019)/AggregateFunctions(2754):
   Mandatory, TypeDefinition FolderType(61). */
static const mu_reference_t s_mandatory_foldertype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {61}}, true}};

/* ServerCapabilitiesType.<VendorCapability>(11562): OptionalPlaceholder(11508)
   (not the plain Optional(80) rule -- this is a placeholder BrowseName slot,
   OPC-10000-5 §6.3.2 Table 10), TypeDefinition ServerVendorCapabilityType(2137).
   NodeClass is Variable per NodeIds.csv (one WebFetch pass of the spec table
   mislabeled it Object; the CSV is authoritative). */
static const mu_reference_t s_optionalplaceholder_vendorcapability_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {11508}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2137}}, true}};

/* ServerType.ServerStatus(2007): Mandatory, TypeDefinition ServerStatusType(2138). */
static const mu_reference_t s_mandatory_serverstatustype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2138}}, true}};

/* ServerType.ServerCapabilities(2009): Mandatory, TypeDefinition
   ServerCapabilitiesType(2013). */
static const mu_reference_t s_mandatory_servercapabilitiestype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2013}}, true}};

/* ServerType.ServerDiagnostics(2010): Mandatory, TypeDefinition
   ServerDiagnosticsType(2020). */
static const mu_reference_t s_mandatory_serverdiagnosticstype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2020}}, true}};

/* ServerType.VendorServerInfo(2011): Mandatory, TypeDefinition
   VendorServerInfoType(2033). */
static const mu_reference_t s_mandatory_vendorserverinfotype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2033}}, true}};

/* ServerType.ServerRedundancy(2012): Mandatory, TypeDefinition
   ServerRedundancyType(2034). */
static const mu_reference_t s_mandatory_serverredundancytype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {78}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {2034}}, true}};

#if MUC_OPCUA_CU_NAMESPACES
/* ServerType.Namespaces(11527): Optional, TypeDefinition NamespacesType(11645).
   spec 090 (CU 5801 Part A): moved (with node 11527 and NamespacesType itself)
   onto the CU_NAMESPACES gate -- see s_base_object_type_refs above. */
static const mu_reference_t s_optional_namespacestype_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {80}}, true},
    {{0, MU_NODEID_NUMERIC, {40}}, {0, MU_NODEID_NUMERIC, {11645}}, true}};
#endif

/* ServerType's 4 Optional built-in Methods (GetMonitoredItems(11489)/
   ResendData(12871)/SetSubscriptionDurable(12746)/
   RequestServerStateChange(12883)): Methods carry no HasTypeDefinition
   (OPC-10000-3 -- Methods aren't typed), just the HasModellingRule(37)->
   Optional(80) edge. Per the CU 5801 Kconfig help text, InstanceDeclarations
   (including Optional ones) must be provided "even if ... not used in any
   instance of the Server", so these are unconditional here -- not gated by
   MUC_OPCUA_CU_SUBSCRIPTION_STANDARD (which only governs the separate
   Server-instance Method nodes, e.g. 11492). InputArguments/OutputArguments
   (one level below each Method) are out of this core slice's scope. */
static const mu_reference_t s_optional_method_refs[] = {
    {{0, MU_NODEID_NUMERIC, {37}}, {0, MU_NODEID_NUMERIC, {80}}, true}};

/* ServerCapabilitiesType(2013) HasProperty(46)/HasComponent(47) -> its 24 own
   declarations (OPC-10000-5 §6.3.2 Table 10). RoleSet(16295) is dropped: its
   TypeDefinition RoleSetType(15607) doesn't exist yet in this address space
   (deferred to the role-based security slice). */
static const mu_reference_t s_server_capabilities_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2014}}, true},  /* ServerProfileArray */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2016}}, true},  /* LocaleIdArray */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2017}}, true},  /* MinSupportedSampleRate */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2732}}, true},  /* MaxBrowseContinuationPoints */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2733}}, true},  /* MaxQueryContinuationPoints */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2734}}, true},  /* MaxHistoryContinuationPoints */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {19809}}, true}, /* MaxLogObjectContinuationPoints */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {3049}}, true},  /* SoftwareCertificates */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11549}}, true}, /* MaxArrayLength */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {11550}}, true}, /* MaxStringLength */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12910}}, true}, /* MaxByteStringLength */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11551}}, true}, /* OperationLimits */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2019}}, true},  /* ModellingRules */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2754}}, true},  /* AggregateFunctions */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24088}}, true}, /* MaxSessions */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24089}}, true}, /* MaxSubscriptions */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24090}}, true}, /* MaxMonitoredItems */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24091}}, true}, /* MaxSubscriptionsPerSession */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24103}}, true}, /* MaxMonitoredItemsPerSubscription */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24092}}, true}, /* MaxSelectClauseParameters */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24093}}, true}, /* MaxWhereClauseParameters */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {31770}}, true}, /* MaxMonitoredItemsQueueSize */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11562}}, true}, /* <VendorCapability> */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24094}}, true}  /* ConformanceUnits */
};

/* ServerType(2004) HasProperty(46)/HasComponent(47) -> its 17 direct children
   (OPC-10000-5 §6.3.1 table). */
static const mu_reference_t s_server_type_refs[] = {
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2005}}, true}, /* ServerArray */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2006}}, true}, /* NamespaceArray */
    /* spec 090 (CU 5801 fix): UrisVersion(15003) removed -- Optional property
       owned by CU 3994 (Session Sessionless Invocation), entirely
       unimplemented in this codebase. See the s_str_ConformanceUnits comment
       above for the full rationale. */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2007}}, true},  /* ServerStatus */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2008}}, true},  /* ServiceLevel */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {2742}}, true},  /* Auditing */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {12882}}, true}, /* EstimatedReturnTime */
#if MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    /* spec 090 (CU 5801 fix): LocalTime(17612) re-gated onto its true owner
       CU_BASE_INFO_LOCALTIME (full-only) -- see the node definition below. */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {17612}}, true}, /* LocalTime */
#endif
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2009}}, true}, /* ServerCapabilities */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2010}}, true}, /* ServerDiagnostics */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2011}}, true}, /* VendorServerInfo */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2012}}, true}, /* ServerRedundancy */
#if MUC_OPCUA_CU_NAMESPACES
    /* spec 090 (CU 5801 Part A): Namespaces(11527) moved to the CU_NAMESPACES
       gate along with the node itself and NamespacesType -- see
       s_base_object_type_refs above. */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11527}}, true}, /* Namespaces */
#endif
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {11489}}, true}, /* GetMonitoredItems */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {12871}}, true}, /* ResendData */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {12746}}, true}, /* SetSubscriptionDurable */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {12883}}, true}  /* RequestServerStateChange */
};
#endif

#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE
/* CU 3641: Argument DataType HasEncoding refs to its Default XML/Binary encodings. */
static const mu_reference_t s_argument_refs[] = {{{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {297}}, true},
                                                 {{0, MU_NODEID_NUMERIC, {38}}, {0, MU_NODEID_NUMERIC, {298}}, true}};
#endif

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
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2365}}, true},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* spec 083 (CU 3189): BaseDataVariableType HasSubtype -> ServerType-tree
       VariableTypes. */
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2137}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2138}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2150}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2164}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2165}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2171}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2172}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2196}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2197}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2243}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {2244}}, true},
    {{0, MU_NODEID_NUMERIC, {45}}, {0, MU_NODEID_NUMERIC, {3051}}, true}
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
    MU_VALUESOURCE_STATIC,
    {.static_value = {
         .type = MU_TYPE_EXTENSIONOBJECT, .value = {.array = s_gmi_input_args}, .is_array = true, .array_length = 1}}};
static const mu_value_source_t s_gmi_output_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {
         .type = MU_TYPE_EXTENSIONOBJECT, .value = {.array = s_gmi_output_args}, .is_array = true, .array_length = 2}}};
static const mu_value_source_t s_resend_input_value = {MU_VALUESOURCE_STATIC,
                                                       {.static_value = {.type = MU_TYPE_EXTENSIONOBJECT,
                                                                         .value = {.array = s_resend_input_args},
                                                                         .is_array = true,
                                                                         .array_length = 1}}};

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
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24096}}, true}, /* MaxSubscriptions */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24097}}, true}, /* MaxMonitoredItems */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24098}}, true}, /* MaxSubscriptionsPerSession */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {24104}}, true}, /* MaxMonitoredItemsPerSubscription */
    {{0, MU_NODEID_NUMERIC, {47}}, {0, MU_NODEID_NUMERIC, {2997}}, true},  /* AggregateFunctions (HasComponent) */
    {{0, MU_NODEID_NUMERIC, {46}}, {0, MU_NODEID_NUMERIC, {31916}}, true}, /* MaxMonitoredItemsQueueSize */
#endif
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

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
/* CU 3911/4055: ServerCapabilities subscription limits. Advertised values ARE the
   enforced capacity macros, so they cannot drift. */
static const mu_value_source_t s_max_subscriptions_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_INTERN_MAX_SUBSCRIPTIONS}}};
static const mu_value_source_t s_max_monitored_items_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_INTERN_MAX_MONITORED_ITEMS}}};
static const mu_value_source_t s_max_subscriptions_per_session_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_INTERN_MAX_SUBSCRIPTIONS}}};
static const mu_value_source_t s_max_monitored_items_per_subscription_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_INTERN_MAX_MONITORED_ITEMS}}};
static const mu_value_source_t s_max_monitored_items_queue_size_value = {
    MU_VALUESOURCE_STATIC, {.static_value = {.type = MU_TYPE_UINT32, .value.ui32 = MU_INTERN_MONITORED_QUEUE_DEPTH}}};
#endif

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
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
     s_double_subtype_refs,
     sizeof(s_double_subtype_refs) / sizeof(s_double_subtype_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {12}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_String},
     {6, s_str_String},
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES || (MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION)
     s_string_subtype_refs,
     sizeof(s_string_subtype_refs) / sizeof(s_string_subtype_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {13}},
     MU_NODECLASS_DATATYPE,
     {8, s_str_DateTime},
     {8, s_str_DateTime},
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
     s_datetime_subtype_refs,
     sizeof(s_datetime_subtype_refs) / sizeof(s_datetime_subtype_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: built-in leaf DataTypes sorted between DateTime(13) and NodeId(17). */
    {{0, MU_NODEID_NUMERIC, {14}},
     MU_NODECLASS_DATATYPE,
     {4, s_str_Guid},
     {4, s_str_Guid},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {15}},
     MU_NODECLASS_DATATYPE,
     {10, s_str_ByteString},
     {10, s_str_ByteString},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {16}},
     MU_NODECLASS_DATATYPE,
     {10, s_str_XmlElement},
     {10, s_str_XmlElement},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
    {{0, MU_NODEID_NUMERIC, {17}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_NodeId},
     {6, s_str_NodeId},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {18}},
     MU_NODECLASS_DATATYPE,
     {14, s_str_ExpandedNodeId},
     {14, s_str_ExpandedNodeId},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
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
#if MU_HAVE_STRUCTURE_TYPE
    {{0, MU_NODEID_NUMERIC, {22}},
     MU_NODECLASS_DATATYPE,
     {9, s_str_Structure},
     {9, s_str_Structure},
     s_structure_refs,
     sizeof(s_structure_refs) / sizeof(s_structure_refs[0]),
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {23}},
     MU_NODECLASS_DATATYPE,
     {9, s_str_DataValue},
     {9, s_str_DataValue},
     NULL,
     0,
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
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    {{0, MU_NODEID_NUMERIC, {25}},
     MU_NODECLASS_DATATYPE,
     {14, s_str_DiagnosticInfo},
     {14, s_str_DiagnosticInfo},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
    /* CU 4426: Number(i=26, subtype of BaseDataType). Sorted slot between
       BaseDataType(24) and References(31). Decimal(50) sorts later (before 58). */
    {{0, MU_NODEID_NUMERIC, {26}},
     MU_NODECLASS_DATATYPE,
     {6, s_str_Number},
     {6, s_str_Number},
     s_number_subtype_refs,
     sizeof(s_number_subtype_refs) / sizeof(s_number_subtype_refs[0]),
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* not OR'd with SERVERTYPE: it depends on BASE_TYPES in Kconfig, so this
       block already compiles whenever SERVERTYPE is on */
    /* CU 3188: Integer(27)/UInteger(28) subtypes of Number, Enumeration(29) of
       BaseDataType. Sorted between Number(26) and References(31). */
    {{0, MU_NODEID_NUMERIC, {27}},
     MU_NODECLASS_DATATYPE,
     {7, s_str_Integer},
     {7, s_str_Integer},
     s_integer_subtype_refs,
     sizeof(s_integer_subtype_refs) / sizeof(s_integer_subtype_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {28}},
     MU_NODECLASS_DATATYPE,
     {8, s_str_UInteger},
     {8, s_str_UInteger},
     s_uinteger_subtype_refs,
     sizeof(s_uinteger_subtype_refs) / sizeof(s_uinteger_subtype_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {29}},
     MU_NODECLASS_DATATYPE,
     {11, s_str_Enumeration},
     {11, s_str_Enumeration},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_enumeration_refs_with_message_security_mode,
     sizeof(s_enumeration_refs_with_message_security_mode) / sizeof(s_enumeration_refs_with_message_security_mode[0]),
#elif MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
     s_enumeration_refs,
     sizeof(s_enumeration_refs) / sizeof(s_enumeration_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: HasModellingRule ReferenceType (i=37, subtype of NonHierarchicalReferences).
       Sorted between Organizes(35) and HasEncoding(38). */
    {{0, MU_NODEID_NUMERIC, {37}},
     MU_NODECLASS_REFERENCETYPE,
     {16, s_str_HasModellingRule},
     {16, s_str_HasModellingRule},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE || MUC_OPCUA_CU_BASE_INFO_BASE_TYPES || MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* CU 3641/3188/3189: HasEncoding ReferenceType (i=38, subtype of NonHierarchicalReferences). */
    {{0, MU_NODEID_NUMERIC, {38}},
     MU_NODECLASS_REFERENCETYPE,
     {11, s_str_HasEncoding},
     {11, s_str_HasEncoding},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
    /* CU 4426: Decimal(i=50, subtype of Number). Sorted slot between HasComponent(47)
       and BaseObjectType(58). */
    {{0, MU_NODEID_NUMERIC, {50}},
     MU_NODECLASS_DATATYPE,
     {7, s_str_Decimal},
     {7, s_str_Decimal},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
     s_folder_type_refs,
     sizeof(s_folder_type_refs) / sizeof(s_folder_type_refs[0]),
#else
     NULL,
     0,
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE || MUC_OPCUA_CU_BASE_INFO_BASE_TYPES || MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* CU 3641/3188/3189: DataTypeEncodingType (i=76, subtype of BaseObjectType) — the
       type definition of every Encoding Object. */
    {{0, MU_NODEID_NUMERIC, {76}},
     MU_NODECLASS_OBJECTTYPE,
     {20, s_str_DataTypeEncodingType},
     {20, s_str_DataTypeEncodingType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: ModellingRuleType (i=77, subtype of BaseObjectType) — the type
       definition of every ModellingRule Object below. */
    {{0, MU_NODEID_NUMERIC, {77}},
     MU_NODECLASS_OBJECTTYPE,
     {17, s_str_ModellingRuleType},
     {17, s_str_ModellingRuleType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_DATA_ACCESS || MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* Spec 060 ModellingRule targets so a property's HasModellingRule resolves;
       CU 3188 additionally exposes them as ModellingRuleType(77) instances. */
    {{0, MU_NODEID_NUMERIC, {78}},
     MU_NODECLASS_OBJECT,
     {9, s_str_Mandatory},
     {9, s_str_Mandatory},
     NULL,
     0,
     NULL,
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
     .type_definition = {0, MU_NODEID_NUMERIC, {77}}},
#else
     .type_definition = {0}},
#endif
    {{0, MU_NODEID_NUMERIC, {80}},
     MU_NODECLASS_OBJECT,
     {8, s_str_Optional},
     {8, s_str_Optional},
     NULL,
     0,
     NULL,
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
     .type_definition = {0, MU_NODEID_NUMERIC, {77}}},
#else
     .type_definition = {0}},
#endif
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: ExposesItsArray(83), sorted between Optional(80) and Root(84). */
    {{0, MU_NODEID_NUMERIC, {83}},
     MU_NODECLASS_OBJECT,
     {15, s_str_ExposesItsArray},
     {15, s_str_ExposesItsArray},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {77}}},
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
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: specialized leaf DataTypes Duration(290, subtype of Double),
       NumericRange(291, subtype of String), UtcTime(294, subtype of DateTime).
       Sorted between ReferenceTypes(91) and Argument(296). */
    {{0, MU_NODEID_NUMERIC, {290}},
     MU_NODECLASS_DATATYPE,
     {8, s_str_Duration},
     {8, s_str_Duration},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {291}},
     MU_NODECLASS_DATATYPE,
     {12, s_str_NumericRange},
     {12, s_str_NumericRange},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {294}},
     MU_NODECLASS_DATATYPE,
     {7, s_str_UtcTime},
     {7, s_str_UtcTime},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 090 (CU 5801 fix): LocaleId(295, subtype of String(12), no
       encodings -- OPC-10000-5 §12.2.11.1) is the Mandatory element DataType
       of ServerCapabilitiesType.LocaleIdArray(2016). Sorted between
       UtcTime(294) and Argument(296). */
    {{0, MU_NODEID_NUMERIC, {295}},
     MU_NODECLASS_DATATYPE,
     {8, s_str_LocaleId},
     {8, s_str_LocaleId},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE
    /* CU 3641: Argument DataType (i=296, subtype of Structure) + its Default XML(297)
       / Default Binary(298) Encoding Objects (DataTypeEncodingType 76). */
    {{0, MU_NODEID_NUMERIC, {296}},
     MU_NODECLASS_DATATYPE,
     {8, s_str_Argument},
     {8, s_str_Argument},
     s_argument_refs,
     sizeof(s_argument_refs) / sizeof(s_argument_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {297}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {298}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 091 (CU 5801): MessageSecurityMode(302, subtype of Enumeration(29),
       OPC-10000-4 §7.20 Table 139) is the closure DataType for
       SessionSecurityDiagnosticsType.SecurityMode(2251). No EnumStrings child
       is added, matching the ServerState(852)/RedundancySupport(851)
       precedent. Sorted between the Argument encodings (298) and
       BuildInfo(338). */
    {{0, MU_NODEID_NUMERIC, {302}},
     MU_NODECLASS_DATATYPE,
     {19, s_str_MessageSecurityMode},
     {19, s_str_MessageSecurityMode},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* spec 083 (CU 3189): ServerType structured DataTypes (subtypes of
       Structure 22) + their Default XML/Binary Encoding Objects, and the two
       ServerType enums (subtypes of Enumeration 29). Sorted between the
       Argument encodings (298) and ServerType (2004). */
    {{0, MU_NODEID_NUMERIC, {338}},
     MU_NODECLASS_DATATYPE,
     {9, s_str_BuildInfo},
     {9, s_str_BuildInfo},
     s_build_info_refs,
     sizeof(s_build_info_refs) / sizeof(s_build_info_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {339}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {340}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 090 (CU 5801 fix): SignedSoftwareCertificate(344, subtype of
       Structure(22)) + its Default XML(345)/Default Binary(346) Encoding
       Objects (DataTypeEncodingType 76) -- OPC-10000-4 §7.37, OPC-10000-5
       §12.3.13. Mandatory element DataType of
       ServerCapabilitiesType.SoftwareCertificates(3049). Sorted between
       BuildInfo's Default_Binary(340) and RedundancySupport(851). */
    {{0, MU_NODEID_NUMERIC, {344}},
     MU_NODECLASS_DATATYPE,
     {25, s_str_SignedSoftwareCertificate},
     {25, s_str_SignedSoftwareCertificate},
     s_signed_software_certificate_refs,
     sizeof(s_signed_software_certificate_refs) / sizeof(s_signed_software_certificate_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {345}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {346}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
    {{0, MU_NODEID_NUMERIC, {851}},
     MU_NODECLASS_DATATYPE,
     {17, s_str_RedundancySupport},
     {17, s_str_RedundancySupport},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {852}},
     MU_NODECLASS_DATATYPE,
     {11, s_str_ServerState},
     {11, s_str_ServerState},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {853}},
     MU_NODECLASS_DATATYPE,
     {23, s_str_RedundantServerDataType},
     {23, s_str_RedundantServerDataType},
     s_redundant_server_data_type_refs,
     sizeof(s_redundant_server_data_type_refs) / sizeof(s_redundant_server_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {854}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {855}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {856}},
     MU_NODECLASS_DATATYPE,
     {35, s_str_SamplingIntervalDiagnosticsDataType},
     {35, s_str_SamplingIntervalDiagnosticsDataType},
     s_sampling_interval_diagnostics_data_type_refs,
     sizeof(s_sampling_interval_diagnostics_data_type_refs) / sizeof(s_sampling_interval_diagnostics_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {857}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {858}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {859}},
     MU_NODECLASS_DATATYPE,
     {32, s_str_ServerDiagnosticsSummaryDataType},
     {32, s_str_ServerDiagnosticsSummaryDataType},
     s_server_diagnostics_summary_data_type_refs,
     sizeof(s_server_diagnostics_summary_data_type_refs) / sizeof(s_server_diagnostics_summary_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {860}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {861}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {862}},
     MU_NODECLASS_DATATYPE,
     {20, s_str_ServerStatusDataType},
     {20, s_str_ServerStatusDataType},
     s_server_status_data_type_refs,
     sizeof(s_server_status_data_type_refs) / sizeof(s_server_status_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {863}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {864}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {865}},
     MU_NODECLASS_DATATYPE,
     {26, s_str_SessionDiagnosticsDataType},
     {26, s_str_SessionDiagnosticsDataType},
     s_session_diagnostics_data_type_refs,
     sizeof(s_session_diagnostics_data_type_refs) / sizeof(s_session_diagnostics_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {866}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {867}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {868}},
     MU_NODECLASS_DATATYPE,
     {34, s_str_SessionSecurityDiagnosticsDataType},
     {34, s_str_SessionSecurityDiagnosticsDataType},
     s_session_security_diagnostics_data_type_refs,
     sizeof(s_session_security_diagnostics_data_type_refs) / sizeof(s_session_security_diagnostics_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {869}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {870}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {871}},
     MU_NODECLASS_DATATYPE,
     {22, s_str_ServiceCounterDataType},
     {22, s_str_ServiceCounterDataType},
     s_service_counter_data_type_refs,
     sizeof(s_service_counter_data_type_refs) / sizeof(s_service_counter_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {872}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {873}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {874}},
     MU_NODECLASS_DATATYPE,
     {31, s_str_SubscriptionDiagnosticsDataType},
     {31, s_str_SubscriptionDiagnosticsDataType},
     s_subscription_diagnostics_data_type_refs,
     sizeof(s_subscription_diagnostics_data_type_refs) / sizeof(s_subscription_diagnostics_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {875}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {876}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
    {{0, MU_NODEID_NUMERIC, {2004}},
     MU_NODECLASS_OBJECTTYPE,
     {10, s_str_ServerType},
     {10, s_str_ServerType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_server_type_refs,
     sizeof(s_server_type_refs) / sizeof(s_server_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerType(2004) direct children
       ServerArray/NamespaceArray/ServerStatus/ServiceLevel/ServerCapabilities/
       ServerDiagnostics/VendorServerInfo/ServerRedundancy (OPC-10000-5 §6.3.1
       table). Sorted between ServerType(2004) and ServerCapabilitiesType(2013);
       the remaining ServerType children (Auditing, EstimatedReturnTime,
       LocalTime, Namespaces, and the 4 built-in Methods) sort elsewhere --
       see s_server_type_refs above for the full node list. */
    {{0, MU_NODEID_NUMERIC, {2005}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ServerArray},
     {11, s_str_ServerArray},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = 1,
     .data_type = 12}, /* String[] (OPC-10000-5 §6.3.1) */
    {{0, MU_NODEID_NUMERIC, {2006}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_NamespaceArray},
     {14, s_str_NamespaceArray},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = 1,
     .data_type = 12}, /* String[] (OPC-10000-5 §6.3.1) */
    {{0, MU_NODEID_NUMERIC, {2007}},
     MU_NODECLASS_VARIABLE,
     {12, s_str_ServerStatus},
     {12, s_str_ServerStatus},
     s_mandatory_serverstatustype_refs,
     sizeof(s_mandatory_serverstatustype_refs) / sizeof(s_mandatory_serverstatustype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2138}},
     .value_rank = -1,
     .data_type = 862}, /* ServerStatusDataType (OPC-10000-5 §6.3.1) */
    {{0, MU_NODEID_NUMERIC, {2008}},
     MU_NODECLASS_VARIABLE,
     {12, s_str_ServiceLevel},
     {12, s_str_ServiceLevel},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 3}, /* Byte (OPC-10000-5 §6.3.1) */
    {{0, MU_NODEID_NUMERIC, {2009}},
     MU_NODECLASS_OBJECT,
     {18, s_str_ServerCapabilities},
     {18, s_str_ServerCapabilities},
     s_mandatory_servercapabilitiestype_refs,
     sizeof(s_mandatory_servercapabilitiestype_refs) / sizeof(s_mandatory_servercapabilitiestype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2013}}},
    {{0, MU_NODEID_NUMERIC, {2010}},
     MU_NODECLASS_OBJECT,
     {17, s_str_ServerDiagnostics},
     {17, s_str_ServerDiagnostics},
     s_mandatory_serverdiagnosticstype_refs,
     sizeof(s_mandatory_serverdiagnosticstype_refs) / sizeof(s_mandatory_serverdiagnosticstype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2020}}},
    {{0, MU_NODEID_NUMERIC, {2011}},
     MU_NODECLASS_OBJECT,
     {16, s_str_VendorServerInfo},
     {16, s_str_VendorServerInfo},
     s_mandatory_vendorserverinfotype_refs,
     sizeof(s_mandatory_vendorserverinfotype_refs) / sizeof(s_mandatory_vendorserverinfotype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2033}}},
    {{0, MU_NODEID_NUMERIC, {2012}},
     MU_NODECLASS_OBJECT,
     {16, s_str_ServerRedundancy},
     {16, s_str_ServerRedundancy},
     s_mandatory_serverredundancytype_refs,
     sizeof(s_mandatory_serverredundancytype_refs) / sizeof(s_mandatory_serverredundancytype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2034}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* spec 083 (CU 3189): ServerType-tree ObjectTypes. Sorted between
       ServerType(2004) and Server(2253). */
    {{0, MU_NODEID_NUMERIC, {2013}},
     MU_NODECLASS_OBJECTTYPE,
     {22, s_str_ServerCapabilitiesType},
     {22, s_str_ServerCapabilitiesType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_server_capabilities_type_refs,
     sizeof(s_server_capabilities_type_refs) / sizeof(s_server_capabilities_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType(2013) own declarations
       ServerProfileArray/LocaleIdArray/MinSupportedSampleRate/ModellingRules
       (OPC-10000-5 §6.3.2 Table 10). Sorted between ServerCapabilitiesType(2013)
       and ServerDiagnosticsType(2020); the remaining ServerCapabilitiesType
       declarations sort elsewhere (dual-placed around the DataAccess block, or
       above NodeId 11461) -- see s_server_capabilities_type_refs above for the
       full 24-node list. */
    {{0, MU_NODEID_NUMERIC, {2014}},
     MU_NODECLASS_VARIABLE,
     {18, s_str_ServerProfileArray},
     {18, s_str_ServerProfileArray},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = 1,
     .data_type = 12}, /* String[] (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2016}},
     MU_NODECLASS_VARIABLE,
     {13, s_str_LocaleIdArray},
     {13, s_str_LocaleIdArray},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = 1,
     .data_type = 295}, /* LocaleId[] (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2017}},
     MU_NODECLASS_VARIABLE,
     {22, s_str_MinSupportedSampleRate},
     {22, s_str_MinSupportedSampleRate},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 290}, /* Duration (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2019}},
     MU_NODECLASS_OBJECT,
     {14, s_str_ModellingRules},
     {14, s_str_ModellingRules},
     s_mandatory_foldertype_refs,
     sizeof(s_mandatory_foldertype_refs) / sizeof(s_mandatory_foldertype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
    {{0, MU_NODEID_NUMERIC, {2020}},
     MU_NODECLASS_OBJECTTYPE,
     {21, s_str_ServerDiagnosticsType},
     {21, s_str_ServerDiagnosticsType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2026}},
     MU_NODECLASS_OBJECTTYPE,
     {30, s_str_SessionsDiagnosticsSummaryType},
     {30, s_str_SessionsDiagnosticsSummaryType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2029}},
     MU_NODECLASS_OBJECTTYPE,
     {28, s_str_SessionDiagnosticsObjectType},
     {28, s_str_SessionDiagnosticsObjectType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2033}},
     MU_NODECLASS_OBJECTTYPE,
     {20, s_str_VendorServerInfoType},
     {20, s_str_VendorServerInfoType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2034}},
     MU_NODECLASS_OBJECTTYPE,
     {20, s_str_ServerRedundancyType},
     {20, s_str_ServerRedundancyType},
     s_server_redundancy_type_refs,
     sizeof(s_server_redundancy_type_refs) / sizeof(s_server_redundancy_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2036}},
     MU_NODECLASS_OBJECTTYPE,
     {25, s_str_TransparentRedundancyType},
     {25, s_str_TransparentRedundancyType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2039}},
     MU_NODECLASS_OBJECTTYPE,
     {28, s_str_NonTransparentRedundancyType},
     {28, s_str_NonTransparentRedundancyType},
     s_non_transparent_redundancy_type_refs,
     sizeof(s_non_transparent_redundancy_type_refs) / sizeof(s_non_transparent_redundancy_type_refs[0]),
     NULL,
     .type_definition = {0}},
    /* spec 083 (CU 3189): ServerType-tree VariableTypes. Sorted between
       NonTransparentRedundancyType(2039) and Server(2253). */
    {{0, MU_NODEID_NUMERIC, {2137}},
     MU_NODECLASS_VARIABLETYPE,
     {26, s_str_ServerVendorCapabilityType},
     {26, s_str_ServerVendorCapabilityType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2138}},
     MU_NODECLASS_VARIABLETYPE,
     {16, s_str_ServerStatusType},
     {16, s_str_ServerStatusType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_server_status_type_refs,
     sizeof(s_server_status_type_refs) / sizeof(s_server_status_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801): ServerStatusType(2138) own declarations
       (OPC-10000-5 §7.6). Sorted between ServerStatusType(2138) and
       ServerDiagnosticsSummaryType(2150). */
    {{0, MU_NODEID_NUMERIC, {2139}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_StartTime},
     {9, s_str_StartTime},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 294}, /* UtcTime, not DateTime -- OPC-10000-5 §7.6 Table 8 and the
                            official NodeSet2.xml both give DataType=UtcTime(294)
                            (a DateTime subtype) for StartTime/CurrentTime */
    {{0, MU_NODEID_NUMERIC, {2140}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_CurrentTime},
     {11, s_str_CurrentTime},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 294}, /* UtcTime (OPC-10000-5 §7.6 Table 8) */
    {{0, MU_NODEID_NUMERIC, {2141}},
     MU_NODECLASS_VARIABLE,
     {5, s_str_State},
     {5, s_str_State},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 852}, /* ServerState (OPC-10000-5 §7.6 Table 8) */
    {{0, MU_NODEID_NUMERIC, {2142}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_BuildInfo},
     {9, s_str_BuildInfo},
     s_mandatory_buildinfotype_refs,
     sizeof(s_mandatory_buildinfotype_refs) / sizeof(s_mandatory_buildinfotype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {3051}},
     .value_rank = -1,
     .data_type = 338}, /* BuildInfo (OPC-10000-5 §7.6 Table 8) */
#endif
    {{0, MU_NODEID_NUMERIC, {2150}},
     MU_NODECLASS_VARIABLETYPE,
     {28, s_str_ServerDiagnosticsSummaryType},
     {28, s_str_ServerDiagnosticsSummaryType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_server_diagnostics_summary_type_refs,
     sizeof(s_server_diagnostics_summary_type_refs) / sizeof(s_server_diagnostics_summary_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801): ServerDiagnosticsSummaryType(2150) own declarations
       (OPC-10000-5 §7.8). Sorted between ServerDiagnosticsSummaryType(2150)
       and SamplingIntervalDiagnosticsArrayType(2164). NodeId 2158 is skipped
       in the official numbering -- not a gap here. */
    {{0, MU_NODEID_NUMERIC, {2151}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_ServerViewCount},
     {15, s_str_ServerViewCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2152}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_CurrentSessionCount},
     {19, s_str_CurrentSessionCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2153}},
     MU_NODECLASS_VARIABLE,
     {21, s_str_CumulatedSessionCount},
     {21, s_str_CumulatedSessionCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2154}},
     MU_NODECLASS_VARIABLE,
     {28, s_str_SecurityRejectedSessionCount},
     {28, s_str_SecurityRejectedSessionCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2155}},
     MU_NODECLASS_VARIABLE,
     {20, s_str_RejectedSessionCount},
     {20, s_str_RejectedSessionCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2156}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_SessionTimeoutCount},
     {19, s_str_SessionTimeoutCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2157}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_SessionAbortCount},
     {17, s_str_SessionAbortCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2159}},
     MU_NODECLASS_VARIABLE,
     {23, s_str_PublishingIntervalCount},
     {23, s_str_PublishingIntervalCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2160}},
     MU_NODECLASS_VARIABLE,
     {24, s_str_CurrentSubscriptionCount},
     {24, s_str_CurrentSubscriptionCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2161}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_CumulatedSubscriptionCount},
     {26, s_str_CumulatedSubscriptionCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2162}},
     MU_NODECLASS_VARIABLE,
     {29, s_str_SecurityRejectedRequestsCount},
     {29, s_str_SecurityRejectedRequestsCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
    {{0, MU_NODEID_NUMERIC, {2163}},
     MU_NODECLASS_VARIABLE,
     {21, s_str_RejectedRequestsCount},
     {21, s_str_RejectedRequestsCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.8) */
#endif
    {{0, MU_NODEID_NUMERIC, {2164}},
     MU_NODECLASS_VARIABLETYPE,
     {36, s_str_SamplingIntervalDiagnosticsArrayType},
     {36, s_str_SamplingIntervalDiagnosticsArrayType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2165}},
     MU_NODECLASS_VARIABLETYPE,
     {31, s_str_SamplingIntervalDiagnosticsType},
     {31, s_str_SamplingIntervalDiagnosticsType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_sampling_interval_diagnostics_type_refs,
     sizeof(s_sampling_interval_diagnostics_type_refs) / sizeof(s_sampling_interval_diagnostics_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 091 (CU 5801): SamplingIntervalDiagnosticsType(2165)'s
       SamplingInterval own declaration (OPC-10000-5 §7.10 Table 80). Sorted
       between SamplingIntervalDiagnosticsType(2165) and
       SubscriptionDiagnosticsArrayType(2171). The remaining 3 own
       declarations (SampledMonitoredItemsCount/MaxSampledMonitoredItemsCount/
       DisabledMonitoredItemsSamplingCount, NodeIds 11697-11699) sort far away
       in the 11xxx neighborhood -- see below, near NamespacesType(11645). */
    {{0, MU_NODEID_NUMERIC, {2166}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_SamplingInterval},
     {16, s_str_SamplingInterval},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 290}, /* Duration (OPC-10000-5 §7.10 Table 80) */
#endif
    {{0, MU_NODEID_NUMERIC, {2171}},
     MU_NODECLASS_VARIABLETYPE,
     {32, s_str_SubscriptionDiagnosticsArrayType},
     {32, s_str_SubscriptionDiagnosticsArrayType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2172}},
     MU_NODECLASS_VARIABLETYPE,
     {27, s_str_SubscriptionDiagnosticsType},
     {27, s_str_SubscriptionDiagnosticsType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2196}},
     MU_NODECLASS_VARIABLETYPE,
     {27, s_str_SessionDiagnosticsArrayType},
     {27, s_str_SessionDiagnosticsArrayType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2197}},
     MU_NODECLASS_VARIABLETYPE,
     {30, s_str_SessionDiagnosticsVariableType},
     {30, s_str_SessionDiagnosticsVariableType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2243}},
     MU_NODECLASS_VARIABLETYPE,
     {35, s_str_SessionSecurityDiagnosticsArrayType},
     {35, s_str_SessionSecurityDiagnosticsArrayType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {2244}},
     MU_NODECLASS_VARIABLETYPE,
     {30, s_str_SessionSecurityDiagnosticsType},
     {30, s_str_SessionSecurityDiagnosticsType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_session_security_diagnostics_type_refs,
     sizeof(s_session_security_diagnostics_type_refs) / sizeof(s_session_security_diagnostics_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 091 (CU 5801): SessionSecurityDiagnosticsType(2244)'s 8 own
       declarations that fall outside the DataAccess band (OPC-10000-5 §7.16
       Table 86). ClientCertificate(3058, ByteString) falls inside the
       2365..11461 DataAccess block, so it is dual-placed further below,
       mirroring the BuildInfoType(3051) own-declarations dual-copy precedent
       -- see the DataAccess-off/-on copies near BuildDate(3057). Sorted
       between SessionSecurityDiagnosticsType(2244) and
       SubscriptionDiagnosticsArrayType(2171). */
    {{0, MU_NODEID_NUMERIC, {2245}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_SessionId},
     {9, s_str_SessionId},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 17}, /* NodeId (OPC-10000-5 §7.16 Table 86) */
    {{0, MU_NODEID_NUMERIC, {2246}},
     MU_NODECLASS_VARIABLE,
     {21, s_str_ClientUserIdOfSession},
     {21, s_str_ClientUserIdOfSession},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.16 Table 86) */
    {{0, MU_NODEID_NUMERIC, {2247}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_ClientUserIdHistory},
     {19, s_str_ClientUserIdHistory},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = 1,
     .data_type = 12}, /* String[] (OPC-10000-5 §7.16 Table 86) */
    {{0, MU_NODEID_NUMERIC, {2248}},
     MU_NODECLASS_VARIABLE,
     {23, s_str_AuthenticationMechanism},
     {23, s_str_AuthenticationMechanism},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.16 Table 86) */
    {{0, MU_NODEID_NUMERIC, {2249}},
     MU_NODECLASS_VARIABLE,
     {8, s_str_Encoding},
     {8, s_str_Encoding},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.16 Table 86) */
    {{0, MU_NODEID_NUMERIC, {2250}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_TransportProtocol},
     {17, s_str_TransportProtocol},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.16 Table 86) */
    {{0, MU_NODEID_NUMERIC, {2251}},
     MU_NODECLASS_VARIABLE,
     {12, s_str_SecurityMode},
     {12, s_str_SecurityMode},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 302}, /* MessageSecurityMode (OPC-10000-5 §7.16 Table 86) */
    {{0, MU_NODEID_NUMERIC, {2252}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_SecurityPolicyUri},
     {17, s_str_SecurityPolicyUri},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.16 Table 86) */
#endif
#endif
    {{0, MU_NODEID_NUMERIC, {2253}},
     MU_NODECLASS_OBJECT,
     {6, s_str_Server},
     {6, s_str_Server},
     s_server_refs,
     sizeof(s_server_refs) / sizeof(s_server_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2004}},
     .event_notifier = MU_SERVER_EVENTNOTIFIER},
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION && !MUC_OPCUA_CU_DATA_ACCESS
    /* spec 085 (CU 5801): ServerStatusType(2138) own declarations
       SecondsTillShutdown/ShutdownReason (DataAccess-off copy), mirroring the
       in-DataAccess copy above (after EnumStrings(2377) there). Sorts after
       LocaleIdArray(2271) and before AggregateFunctions(2997). Task 4 adds
       three ServerCapabilitiesType properties (MaxBrowseContinuationPoints/
       MaxQueryContinuationPoints/MaxHistoryContinuationPoints) and one
       ServerType property (Auditing) that also fall inside the 2365..11461
       DataAccess block, so they're dual-placed here too -- see the
       DataAccess-on copies below (after EnumStrings(2377) there). */
    {{0, MU_NODEID_NUMERIC, {2732}},
     MU_NODECLASS_VARIABLE,
     {27, s_str_MaxBrowseContinuationPoints},
     {27, s_str_MaxBrowseContinuationPoints},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 5}, /* UInt16 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2733}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxQueryContinuationPoints},
     {26, s_str_MaxQueryContinuationPoints},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 5}, /* UInt16 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2734}},
     MU_NODECLASS_VARIABLE,
     {28, s_str_MaxHistoryContinuationPoints},
     {28, s_str_MaxHistoryContinuationPoints},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 5}, /* UInt16 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2742}},
     MU_NODECLASS_VARIABLE,
     {8, s_str_Auditing},
     {8, s_str_Auditing},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 1}, /* Boolean (OPC-10000-5 §6.3.1) */
    {{0, MU_NODEID_NUMERIC, {2752}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_SecondsTillShutdown},
     {19, s_str_SecondsTillShutdown},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.6 Table 8) */
    {{0, MU_NODEID_NUMERIC, {2753}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_ShutdownReason},
     {14, s_str_ShutdownReason},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 21}, /* LocalizedText (OPC-10000-5 §7.6 Table 8) */
    /* ServerCapabilitiesType.AggregateFunctions(2754): distinct NodeId from
       the Server-instance AggregateFunctions Folder(2997) just below. */
    {{0, MU_NODEID_NUMERIC, {2754}},
     MU_NODECLASS_OBJECT,
     {18, s_str_AggregateFunctions},
     {18, s_str_AggregateFunctions},
     s_mandatory_foldertype_refs,
     sizeof(s_mandatory_foldertype_refs) / sizeof(s_mandatory_foldertype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC && !MUC_OPCUA_CU_DATA_ACCESS
    /* CU 3911: AggregateFunctions(2997). When DataAccess is enabled this node is
       emitted inside the DataAccess block instead (after 2377), so that exactly one
       copy exists and the numeric run stays ascending in both configurations. */
    {{0, MU_NODEID_NUMERIC, {2997}},
     MU_NODECLASS_OBJECT,
     {18, s_str_AggregateFunctions},
     {18, s_str_AggregateFunctions},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION && !MUC_OPCUA_CU_DATA_ACCESS
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType.SoftwareCertificates
       (3049, DataAccess-off copy), mirroring the in-DataAccess copy below.
       Sorts after AggregateFunctions(2997) and before BuildInfoType(3051). */
    {{0, MU_NODEID_NUMERIC, {3049}},
     MU_NODECLASS_VARIABLE,
     {20, s_str_SoftwareCertificates},
     {20, s_str_SoftwareCertificates},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = 1,
     .data_type = 344}, /* SignedSoftwareCertificate[] (OPC-10000-5 §6.3.2 Table 10) */
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801): ServerStatusType(2138) own declarations
       SecondsTillShutdown/ShutdownReason (DataAccess-on copy; fall inside the
       2365..11461 DataAccess block, so they're dual-placed -- see the
       DataAccess-off mirror after LocaleIdArray(2271) below). Sorts after
       EnumStrings(2377) and before AggregateFunctions(2997). Task 4 adds
       three ServerCapabilitiesType properties and one ServerType property
       (Auditing) that also fall in this range -- see the DataAccess-off
       mirror above for the shared rationale. */
    {{0, MU_NODEID_NUMERIC, {2732}},
     MU_NODECLASS_VARIABLE,
     {27, s_str_MaxBrowseContinuationPoints},
     {27, s_str_MaxBrowseContinuationPoints},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 5}, /* UInt16 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2733}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxQueryContinuationPoints},
     {26, s_str_MaxQueryContinuationPoints},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 5}, /* UInt16 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2734}},
     MU_NODECLASS_VARIABLE,
     {28, s_str_MaxHistoryContinuationPoints},
     {28, s_str_MaxHistoryContinuationPoints},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 5}, /* UInt16 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {2742}},
     MU_NODECLASS_VARIABLE,
     {8, s_str_Auditing},
     {8, s_str_Auditing},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 1}, /* Boolean (OPC-10000-5 §6.3.1) */
    {{0, MU_NODEID_NUMERIC, {2752}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_SecondsTillShutdown},
     {19, s_str_SecondsTillShutdown},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.6 Table 8) */
    {{0, MU_NODEID_NUMERIC, {2753}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_ShutdownReason},
     {14, s_str_ShutdownReason},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 21}, /* LocalizedText (OPC-10000-5 §7.6 Table 8) */
    {{0, MU_NODEID_NUMERIC, {2754}},
     MU_NODECLASS_OBJECT,
     {18, s_str_AggregateFunctions},
     {18, s_str_AggregateFunctions},
     s_mandatory_foldertype_refs,
     sizeof(s_mandatory_foldertype_refs) / sizeof(s_mandatory_foldertype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    /* CU 3911: AggregateFunctions Folder (i=2997), HasComponent child of
       ServerCapabilities(2268), FolderType(61). Sorts after the DataAccess block
       (2365..2377) and before LocalTime (8912). */
    {{0, MU_NODEID_NUMERIC, {2997}},
     MU_NODECLASS_OBJECT,
     {18, s_str_AggregateFunctions},
     {18, s_str_AggregateFunctions},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType.SoftwareCertificates
       (3049, DataAccess-on copy). Sorts after AggregateFunctions(2997) and
       before BuildInfoType(3051). */
    {{0, MU_NODEID_NUMERIC, {3049}},
     MU_NODECLASS_VARIABLE,
     {20, s_str_SoftwareCertificates},
     {20, s_str_SoftwareCertificates},
     s_mandatory_property_refs,
     sizeof(s_mandatory_property_refs) / sizeof(s_mandatory_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = 1,
     .data_type = 344}, /* SignedSoftwareCertificate[] (OPC-10000-5 §6.3.2 Table 10) */
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* spec 083 (CU 3189): BuildInfoType(3051, DataAccess-on copy). Sorts
       after AggregateFunctions(2997) and before EnumValueType(7594). */
    {{0, MU_NODEID_NUMERIC, {3051}},
     MU_NODECLASS_VARIABLETYPE,
     {13, s_str_BuildInfoType},
     {13, s_str_BuildInfoType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_build_info_type_refs,
     sizeof(s_build_info_type_refs) / sizeof(s_build_info_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801): BuildInfoType(3051) own declarations
       (OPC-10000-5 §7.7 Table 77; DataAccess-on copy -- all 6 fall inside the
       2365..11461 DataAccess block, so they're dual-placed -- see the
       DataAccess-off mirror below). Sorts after BuildInfoType(3051) and
       before EnumValueType(7594). */
    {{0, MU_NODEID_NUMERIC, {3052}},
     MU_NODECLASS_VARIABLE,
     {10, s_str_ProductUri},
     {10, s_str_ProductUri},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3053}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_ManufacturerName},
     {16, s_str_ManufacturerName},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3054}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ProductName},
     {11, s_str_ProductName},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3055}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_SoftwareVersion},
     {15, s_str_SoftwareVersion},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3056}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_BuildNumber},
     {11, s_str_BuildNumber},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3057}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_BuildDate},
     {9, s_str_BuildDate},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 294}, /* UtcTime, not DateTime -- OPC-10000-5 §7.7 Table 77 and
                            the official NodeSet2.xml both give DataType=UtcTime(294) */
    /* spec 091 (CU 5801): SessionSecurityDiagnosticsType.ClientCertificate
       (3058, ByteString, DataAccess-on copy) -- falls inside the
       2365..11461 DataAccess block, so it is dual-placed, mirroring the
       BuildInfoType own-declarations dual-copy just above (see the
       DataAccess-off mirror below). Sorts after BuildDate(3057) and before
       EnumValueType(7594). */
    {{0, MU_NODEID_NUMERIC, {3058}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_ClientCertificate},
     {17, s_str_ClientCertificate},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 15}, /* ByteString (OPC-10000-5 §7.16 Table 86) */
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: EnumValueType(7594, subtype of Structure) + its Default XML(7616)/
       Default Binary(8251) Encoding Objects. Sorts after AggregateFunctions(2997)
       and before LocalTime TimeZoneDataType(8912). */
    {{0, MU_NODEID_NUMERIC, {7594}},
     MU_NODECLASS_DATATYPE,
     {13, s_str_EnumValueType},
     {13, s_str_EnumValueType},
     s_enum_value_type_refs,
     sizeof(s_enum_value_type_refs) / sizeof(s_enum_value_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {7616}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {8251}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && !MUC_OPCUA_CU_DATA_ACCESS
    /* spec 083 (CU 3189): BuildInfoType(3051, DataAccess-off copy), mirroring
       the in-DataAccess copy above. Sorts after AggregateFunctions(2997) and before
       EnumValueType(7594). */
    {{0, MU_NODEID_NUMERIC, {3051}},
     MU_NODECLASS_VARIABLETYPE,
     {13, s_str_BuildInfoType},
     {13, s_str_BuildInfoType},
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_build_info_type_refs,
     sizeof(s_build_info_type_refs) / sizeof(s_build_info_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION && !MUC_OPCUA_CU_DATA_ACCESS
    /* spec 085 (CU 5801): BuildInfoType(3051) own declarations (DataAccess-off
       copy), mirroring the in-DataAccess copy above. Sorts after
       BuildInfoType(3051) and before EnumValueType(7594). */
    {{0, MU_NODEID_NUMERIC, {3052}},
     MU_NODECLASS_VARIABLE,
     {10, s_str_ProductUri},
     {10, s_str_ProductUri},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3053}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_ManufacturerName},
     {16, s_str_ManufacturerName},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3054}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_ProductName},
     {11, s_str_ProductName},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3055}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_SoftwareVersion},
     {15, s_str_SoftwareVersion},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3056}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_BuildNumber},
     {11, s_str_BuildNumber},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 12}, /* String (OPC-10000-5 §7.7 Table 77) */
    {{0, MU_NODEID_NUMERIC, {3057}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_BuildDate},
     {9, s_str_BuildDate},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 294}, /* UtcTime, not DateTime -- OPC-10000-5 §7.7 Table 77 and
                            the official NodeSet2.xml both give DataType=UtcTime(294) */
    /* spec 091 (CU 5801): SessionSecurityDiagnosticsType.ClientCertificate
       (3058, ByteString, DataAccess-off copy), mirroring the in-DataAccess
       copy above. Sorts after BuildDate(3057) and before EnumValueType(7594). */
    {{0, MU_NODEID_NUMERIC, {3058}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_ClientCertificate},
     {17, s_str_ClientCertificate},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 15}, /* ByteString (OPC-10000-5 §7.16 Table 86) */
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES && !MUC_OPCUA_CU_DATA_ACCESS
    /* CU 3188 (Data-Access-off variant): EnumValueType(7594) + its Encoding Objects,
       mirroring the in-DataAccess copy above. Sorted after AggregateFunctions(2997)
       and before LocalTime TimeZoneDataType(8912). */
    {{0, MU_NODEID_NUMERIC, {7594}},
     MU_NODECLASS_DATATYPE,
     {13, s_str_EnumValueType},
     {13, s_str_EnumValueType},
     s_enum_value_type_refs,
     sizeof(s_enum_value_type_refs) / sizeof(s_enum_value_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {7616}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {8251}},
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerType.GetMonitoredItems(11489), the
       ObjectType-level Method InstanceDeclaration -- distinct NodeId from the
       Server-instance Method Server_GetMonitoredItems(11492) just below. */
    {{0, MU_NODEID_NUMERIC, {11489}},
     MU_NODECLASS_METHOD,
     {17, s_str_GetMonitoredItems},
     {17, s_str_GetMonitoredItems},
     s_optional_method_refs,
     sizeof(s_optional_method_refs) / sizeof(s_optional_method_refs[0]),
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
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: OptionalPlaceholder(11508)/MandatoryPlaceholder(11510) ModellingRule
       Objects (ModellingRuleType 77). Sorted between 11494 and 11702. */
    {{0, MU_NODEID_NUMERIC, {11508}},
     MU_NODECLASS_OBJECT,
     {19, s_str_OptionalPlaceholder},
     {19, s_str_OptionalPlaceholder},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {77}}},
    {{0, MU_NODEID_NUMERIC, {11510}},
     MU_NODECLASS_OBJECT,
     {20, s_str_MandatoryPlaceholder},
     {20, s_str_MandatoryPlaceholder},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {77}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
/* spec 085 (CU 5801) Task 4: ServerType.Namespaces(11527, Optional) plus
   ServerCapabilitiesType.MaxArrayLength(11549)/MaxStringLength(11550)/
   OperationLimits(11551, Optional)/<VendorCapability>(11562,
   OptionalPlaceholder). Sorted between MandatoryPlaceholder(11510) and
   OperationLimitsType(11564). */
#if MUC_OPCUA_CU_NAMESPACES
    /* spec 090 (CU 5801 Part A): Namespaces(11527) moved to the CU_NAMESPACES
       gate -- its TypeDefinition NamespacesType(11645) only compiles there. */
    {{0, MU_NODEID_NUMERIC, {11527}},
     MU_NODECLASS_OBJECT,
     {10, s_str_Namespaces},
     {10, s_str_Namespaces},
     s_optional_namespacestype_refs,
     sizeof(s_optional_namespacestype_refs) / sizeof(s_optional_namespacestype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {11645}}},
#endif
    {{0, MU_NODEID_NUMERIC, {11549}},
     MU_NODECLASS_VARIABLE,
     {14, s_str_MaxArrayLength},
     {14, s_str_MaxArrayLength},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {11550}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_MaxStringLength},
     {15, s_str_MaxStringLength},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {11551}},
     MU_NODECLASS_OBJECT,
     {15, s_str_OperationLimits},
     {15, s_str_OperationLimits},
     s_optional_operationlimitstype_refs,
     sizeof(s_optional_operationlimitstype_refs) / sizeof(s_optional_operationlimitstype_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {11564}}},
    {{0, MU_NODEID_NUMERIC, {11562}},
     MU_NODECLASS_VARIABLE,
     {18, s_str_VendorCapability_Placeholder},
     {18, s_str_VendorCapability_Placeholder},
     s_optionalplaceholder_vendorcapability_refs,
     sizeof(s_optionalplaceholder_vendorcapability_refs) / sizeof(s_optionalplaceholder_vendorcapability_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {2137}},
     .value_rank = -1,
     .data_type = 24}, /* BaseDataType (OPC-10000-5 §6.3.2 Table 10; the official
                           NodeSet2.xml UAVariable i=11562 carries no explicit
                           DataType/ValueRank attrs, so both fall back to the
                           UANodeSet.xsd defaults i=24/-1) */
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* spec 083 (CU 3189): OperationLimitsType. Sorted between
       MandatoryPlaceholder(11510) and MaxArrayLength(11702). spec 090 (CU 5801
       Part B): its own InstanceDeclarations (below) complete when
       TYPE_INFORMATION is also on -- this ObjectType node itself is genuinely
       used at embedded+ (ServerCapabilitiesType.OperationLimits(11551) and the
       runtime OperationLimits(11704) object both reference it), unlike the
       namespace/file subtree moved out below. */
    {{0, MU_NODEID_NUMERIC, {11564}},
     MU_NODECLASS_OBJECTTYPE,
     {19, s_str_OperationLimitsType},
     {19, s_str_OperationLimitsType},
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
     s_operation_limits_type_refs,
     sizeof(s_operation_limits_type_refs) / sizeof(s_operation_limits_type_refs[0]),
#else
     NULL,
     0,
#endif
     NULL,
     .type_definition = {0}},
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 090 (CU 5801 Part B): OperationLimitsType(11564)'s 12 own Optional
       Property InstanceDeclarations (OPC-10000-5 §6.3.11 Table 20). NodeIds
       11565/11567/11569-11574 grounded against the official OPC Foundation
       NodeIds.csv (schemas 1.05); the 4 History Read/Update properties land
       at 12161-12164 (sorted slot further below, before SetSubscriptionDurable
       (12746)) since NodeIds.csv assigns them non-contiguous ids. Sorted
       between OperationLimitsType(11564) and FileType/CU_NAMESPACES block
       below (or MaxArrayLength(11702) if CU_NAMESPACES is off). */
    {{0, MU_NODEID_NUMERIC, {11565}},
     MU_NODECLASS_VARIABLE,
     {15, s_str_MaxNodesPerRead},
     {15, s_str_MaxNodesPerRead},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {11567}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_MaxNodesPerWrite},
     {16, s_str_MaxNodesPerWrite},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {11569}},
     MU_NODECLASS_VARIABLE,
     {21, s_str_MaxNodesPerMethodCall},
     {21, s_str_MaxNodesPerMethodCall},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {11570}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxNodesPerBrowse},
     {17, s_str_MaxNodesPerBrowse},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {11571}},
     MU_NODECLASS_VARIABLE,
     {24, s_str_MaxNodesPerRegisterNodes},
     {24, s_str_MaxNodesPerRegisterNodes},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {11572}},
     MU_NODECLASS_VARIABLE,
     {40, s_str_MaxNodesPerTranslateBrowsePathsToNodeIds},
     {40, s_str_MaxNodesPerTranslateBrowsePathsToNodeIds},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {11573}},
     MU_NODECLASS_VARIABLE,
     {25, s_str_MaxNodesPerNodeManagement},
     {25, s_str_MaxNodesPerNodeManagement},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {11574}},
     MU_NODECLASS_VARIABLE,
     {24, s_str_MaxMonitoredItemsPerCall},
     {24, s_str_MaxMonitoredItemsPerCall},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
#endif
#if MUC_OPCUA_CU_NAMESPACES
    /* spec 083 (CU 3189)/spec 090 (CU 5801 Part A): FileType/
       AddressSpaceFileType/NamespaceMetadataType/NamespacesType. Moved off the
       bare SERVERTYPE gate onto their true (full-only) owner CU_NAMESPACES --
       embedded/standard no longer expose these types they never use;
       NamespaceMetadataType(11616) is properly owned by
       CU_NAMESPACE_METADATA(3545) (also full-only), grouped here as a
       follow-up refinement. full still compiles these 4 types via
       CU_NAMESPACES, but their own InstanceDeclarations (full's namespace
       completeness) remain a separate, later CU 5801 slice. Sorted between
       MandatoryPlaceholder(11510) and MaxArrayLength(11702). */
    {{0, MU_NODEID_NUMERIC, {11575}},
     MU_NODECLASS_OBJECTTYPE,
     {8, s_str_FileType},
     {8, s_str_FileType},
     s_file_type_refs,
     sizeof(s_file_type_refs) / sizeof(s_file_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11595}},
     MU_NODECLASS_OBJECTTYPE,
     {20, s_str_AddressSpaceFileType},
     {20, s_str_AddressSpaceFileType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11616}},
     MU_NODECLASS_OBJECTTYPE,
     {21, s_str_NamespaceMetadataType},
     {21, s_str_NamespaceMetadataType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11645}},
     MU_NODECLASS_OBJECTTYPE,
     {14, s_str_NamespacesType},
     {14, s_str_NamespacesType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 091 (CU 5801): SamplingIntervalDiagnosticsType(2165)'s remaining 3
       own declarations (OPC-10000-5 §7.10 Table 80) -- SamplingInterval(2166)
       itself sorts next to the type node further above; these 3 land in the
       11xxx neighborhood per their official NodeIds. Sorted between
       NamespacesType(11645, CU_NAMESPACES-gated) and MaxArrayLength(11702). */
    {{0, MU_NODEID_NUMERIC, {11697}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_SampledMonitoredItemsCount},
     {26, s_str_SampledMonitoredItemsCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.10 Table 80) */
    {{0, MU_NODEID_NUMERIC, {11698}},
     MU_NODECLASS_VARIABLE,
     {29, s_str_MaxSampledMonitoredItemsCount},
     {29, s_str_MaxSampledMonitoredItemsCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.10 Table 80) */
    {{0, MU_NODEID_NUMERIC, {11699}},
     MU_NODECLASS_VARIABLE,
     {35, s_str_DisabledMonitoredItemsSamplingCount},
     {35, s_str_DisabledMonitoredItemsSamplingCount},
     s_mandatory_bdv_refs,
     sizeof(s_mandatory_bdv_refs) / sizeof(s_mandatory_bdv_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {63}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §7.10 Table 80) */
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE
    /* spec 083 (CU 3189): EndpointUrlListDataType/NetworkGroupDataType
       (subtypes of Structure 22) + their Default XML/Binary Encoding Objects.
       Sorted between MaxMonitoredItemsPerCall(11714) and Union(12756). */
    {{0, MU_NODEID_NUMERIC, {11943}},
     MU_NODECLASS_DATATYPE,
     {23, s_str_EndpointUrlListDataType},
     {23, s_str_EndpointUrlListDataType},
     s_endpoint_url_list_data_type_refs,
     sizeof(s_endpoint_url_list_data_type_refs) / sizeof(s_endpoint_url_list_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11944}},
     MU_NODECLASS_DATATYPE,
     {20, s_str_NetworkGroupDataType},
     {20, s_str_NetworkGroupDataType},
     s_network_group_data_type_refs,
     sizeof(s_network_group_data_type_refs) / sizeof(s_network_group_data_type_refs[0]),
     NULL,
     .type_definition = {0}},
    /* spec 083 (CU 3189): NonTransparentNetworkRedundancyType. Sorted between
       NetworkGroupDataType(11944) and Default_XML(11949). */
    {{0, MU_NODEID_NUMERIC, {11945}},
     MU_NODECLASS_OBJECTTYPE,
     {35, s_str_NonTransparentNetworkRedundancyType},
     {35, s_str_NonTransparentNetworkRedundancyType},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {11949}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {11950}},
     MU_NODECLASS_OBJECT,
     {11, s_str_Default_XML},
     {11, s_str_Default_XML},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {11957}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
    {{0, MU_NODEID_NUMERIC, {11958}},
     MU_NODECLASS_OBJECT,
     {14, s_str_Default_Binary},
     {14, s_str_Default_Binary},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {76}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 090 (CU 5801 Part B): OperationLimitsType(11564)'s remaining 4 own
       Property InstanceDeclarations -- the History Read/Update pair -- whose
       official NodeIds.csv (schemas 1.05) NodeIds fall in this non-contiguous
       range rather than alongside 11565-11574. Sorted between Default
       Binary(11958) and SetSubscriptionDurable(12746). */
    {{0, MU_NODEID_NUMERIC, {12161}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxNodesPerHistoryReadData},
     {26, s_str_MaxNodesPerHistoryReadData},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {12162}},
     MU_NODECLASS_VARIABLE,
     {28, s_str_MaxNodesPerHistoryReadEvents},
     {28, s_str_MaxNodesPerHistoryReadEvents},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {12163}},
     MU_NODECLASS_VARIABLE,
     {28, s_str_MaxNodesPerHistoryUpdateData},
     {28, s_str_MaxNodesPerHistoryUpdateData},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
    {{0, MU_NODEID_NUMERIC, {12164}},
     MU_NODECLASS_VARIABLE,
     {30, s_str_MaxNodesPerHistoryUpdateEvents},
     {30, s_str_MaxNodesPerHistoryUpdateEvents},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerType.SetSubscriptionDurable(12746), the
       ObjectType-level Method InstanceDeclaration -- distinct NodeId from any
       Server-instance Method. Sorted between NamespacesType's Default
       Binary(11958) and Union(12756). */
    {{0, MU_NODEID_NUMERIC, {12746}},
     MU_NODECLASS_METHOD,
     {22, s_str_SetSubscriptionDurable},
     {22, s_str_SetSubscriptionDurable},
     s_optional_method_refs,
     sizeof(s_optional_method_refs) / sizeof(s_optional_method_refs[0]),
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_BASE_TYPES
    /* CU 3188: Union(12756) abstract DataType (subtype of Structure). Sorted between
       MaxMonitoredItemsPerCall(11714) and ResendData(12873). */
    {{0, MU_NODEID_NUMERIC, {12756}},
     MU_NODECLASS_DATATYPE,
     {5, s_str_Union},
     {5, s_str_Union},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerType.ResendData(12871), the
       ObjectType-level Method InstanceDeclaration -- distinct NodeId from the
       Server-instance Method Server_ResendData(12873) just below. */
    {{0, MU_NODEID_NUMERIC, {12871}},
     MU_NODECLASS_METHOD,
     {10, s_str_ResendData},
     {10, s_str_ResendData},
     s_optional_method_refs,
     sizeof(s_optional_method_refs) / sizeof(s_optional_method_refs[0]),
     NULL,
     .type_definition = {0}},
#endif
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
#if MUC_OPCUA_CU_BASE_INFO_DATATYPES
    /* CU 2483: DurationString/TimeString/DateString (subtypes of String). Sorted
       slot between InputArguments(12874) and EstimatedReturnTime(12885). */
    {{0, MU_NODEID_NUMERIC, {12879}},
     MU_NODECLASS_DATATYPE,
     {14, s_str_DurationString},
     {14, s_str_DurationString},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {12880}},
     MU_NODECLASS_DATATYPE,
     {10, s_str_TimeString},
     {10, s_str_TimeString},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {12881}},
     MU_NODECLASS_DATATYPE,
     {10, s_str_DateString},
     {10, s_str_DateString},
     NULL,
     0,
     NULL,
     .type_definition = {0}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerType.EstimatedReturnTime(12882, Optional
       Property) and ServerType.RequestServerStateChange(12883, Optional
       Method) -- distinct NodeIds from the Server-instance
       EstimatedReturnTime(12885) below. Sorted between DateString(12881) and
       EstimatedReturnTime(12885). */
    {{0, MU_NODEID_NUMERIC, {12882}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_EstimatedReturnTime},
     {19, s_str_EstimatedReturnTime},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 13}, /* DateTime (OPC-10000-5 §6.3.1) */
    {{0, MU_NODEID_NUMERIC, {12883}},
     MU_NODECLASS_METHOD,
     {24, s_str_RequestServerStateChange},
     {24, s_str_RequestServerStateChange},
     s_optional_method_refs,
     sizeof(s_optional_method_refs) / sizeof(s_optional_method_refs[0]),
     NULL,
     .type_definition = {0}},
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType.MaxByteStringLength
       (12910, Optional Property). Sorted between EstimatedReturnTime(12885)
       and BaseAnalogType(15318). */
    {{0, MU_NODEID_NUMERIC, {12910}},
     MU_NODECLASS_VARIABLE,
     {19, s_str_MaxByteStringLength},
     {19, s_str_MaxByteStringLength},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
/* spec 090 (CU 5801 fix): ServerType.UrisVersion(15003)/VersionTime(20998)
   removed -- see the s_str_ConformanceUnits comment above for the
   grounded rationale (Optional property owned by unimplemented CU 3994). */
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
    /* spec 090 (CU 5801 fix): ServerType.LocalTime(17612, Optional Property)
       -- distinct NodeId from the Server-instance Server.LocalTime(17634)
       below. Re-gated from the bare SERVERTYPE && TYPE_INFORMATION condition
       onto its true owner CU_BASE_INFO_LOCALTIME (full-only, CU 2476),
       matching its TimeZoneDataType(8912) DataType's own gate -- the old gate
       exposed this property at embedded/standard without TimeZoneDataType
       compiled in, a dangling reference. Sorted between
       DefaultInstanceBrowseName(17605) and LocalTime(17634). */
    {{0, MU_NODEID_NUMERIC, {17612}},
     MU_NODECLASS_VARIABLE,
     {9, s_str_LocalTime},
     {9, s_str_LocalTime},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 8912}, /* TimeZoneDataType (OPC-10000-5 §6.3.1) */
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
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType.
       MaxLogObjectContinuationPoints(19809, Optional Property). Sorted
       between InterfaceTypes(17708) and MaxSessions(24088). */
    {{0, MU_NODEID_NUMERIC, {19809}},
     MU_NODECLASS_VARIABLE,
     {30, s_str_MaxLogObjectContinuationPoints},
     {30, s_str_MaxLogObjectContinuationPoints},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 5}, /* UInt16 (OPC-10000-5 §6.3.2 Table 10) */
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType's remaining Optional
       Property declarations (MaxSessions/MaxSubscriptions/MaxMonitoredItems/
       MaxSubscriptionsPerSession/MaxSelectClauseParameters/
       MaxWhereClauseParameters/ConformanceUnits) -- distinct NodeIds from the
       Server-instance subscription-limit Variables in the
       MUC_OPCUA_CU_SUBSCRIPTION_BASIC block below (interleaved further down
       for MaxMonitoredItemsPerSubscription(24103) and
       MaxMonitoredItemsQueueSize(31770), which numerically fall inside that
       block's NodeId run). */
    {{0, MU_NODEID_NUMERIC, {24088}},
     MU_NODECLASS_VARIABLE,
     {11, s_str_MaxSessions},
     {11, s_str_MaxSessions},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {24089}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_MaxSubscriptions},
     {16, s_str_MaxSubscriptions},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {24090}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxMonitoredItems},
     {17, s_str_MaxMonitoredItems},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {24091}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxSubscriptionsPerSession},
     {26, s_str_MaxSubscriptionsPerSession},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {24092}},
     MU_NODECLASS_VARIABLE,
     {25, s_str_MaxSelectClauseParameters},
     {25, s_str_MaxSelectClauseParameters},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {24093}},
     MU_NODECLASS_VARIABLE,
     {24, s_str_MaxWhereClauseParameters},
     {24, s_str_MaxWhereClauseParameters},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
    {{0, MU_NODEID_NUMERIC, {24094}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_ConformanceUnits},
     {16, s_str_ConformanceUnits},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = 1,
     .data_type = 20}, /* QualifiedName[] (OPC-10000-5 §6.3.2 Table 10) */
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    /* CU 3911/4055: ServerCapabilities subscription limits. Placed after 17708 so
       the numeric run stays ascending on every shipped profile (none of which
       enable the Locations(31915)/Currency(23498) tail alongside these). */
    {{0, MU_NODEID_NUMERIC, {24096}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_MaxSubscriptions},
     {16, s_str_MaxSubscriptions},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_subscriptions_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {24097}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxMonitoredItems},
     {17, s_str_MaxMonitoredItems},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_monitored_items_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
    {{0, MU_NODEID_NUMERIC, {24098}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxSubscriptionsPerSession},
     {26, s_str_MaxSubscriptionsPerSession},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_subscriptions_per_session_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType.
       MaxMonitoredItemsPerSubscription(24103, Optional Property) -- distinct
       NodeId from the Server-instance MaxMonitoredItemsPerSubscription(24104)
       just below. */
    {{0, MU_NODEID_NUMERIC, {24103}},
     MU_NODECLASS_VARIABLE,
     {32, s_str_MaxMonitoredItemsPerSubscription},
     {32, s_str_MaxMonitoredItemsPerSubscription},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    {{0, MU_NODEID_NUMERIC, {24104}},
     MU_NODECLASS_VARIABLE,
     {32, s_str_MaxMonitoredItemsPerSubscription},
     {32, s_str_MaxMonitoredItemsPerSubscription},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_monitored_items_per_subscription_value,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}}},
#endif
#if MUC_OPCUA_CU_BASE_INFO_SERVERTYPE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    /* spec 085 (CU 5801) Task 4: ServerCapabilitiesType.
       MaxMonitoredItemsQueueSize(31770, Optional Property) -- distinct NodeId
       from the Server-instance MaxMonitoredItemsQueueSize(31916) just below. */
    {{0, MU_NODEID_NUMERIC, {31770}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxMonitoredItemsQueueSize},
     {26, s_str_MaxMonitoredItemsQueueSize},
     s_optional_property_refs,
     sizeof(s_optional_property_refs) / sizeof(s_optional_property_refs[0]),
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {68}},
     .value_rank = -1,
     .data_type = 7}, /* UInt32 (OPC-10000-5 §6.3.2 Table 10) */
#endif
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    {{0, MU_NODEID_NUMERIC, {31916}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxMonitoredItemsQueueSize},
     {26, s_str_MaxMonitoredItemsQueueSize},
     s_property_type_ref,
     sizeof(s_property_type_ref) / sizeof(s_property_type_ref[0]),
     &s_max_monitored_items_queue_size_value,
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
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    /* CU 3911: AggregateFunctions Folder (i=2997), FolderType(61). */
    {{0, MU_NODEID_NUMERIC, {2997}},
     MU_NODECLASS_OBJECT,
     {18, s_str_AggregateFunctions},
     {18, s_str_AggregateFunctions},
     NULL,
     0,
     NULL,
     .type_definition = {0, MU_NODEID_NUMERIC, {61}}},
#endif
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
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    /* CU 3911/4055: ServerCapabilities subscription limits (minimal table). */
    {{0, MU_NODEID_NUMERIC, {24096}},
     MU_NODECLASS_VARIABLE,
     {16, s_str_MaxSubscriptions},
     {16, s_str_MaxSubscriptions},
     NULL,
     0,
     &s_max_subscriptions_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {24097}},
     MU_NODECLASS_VARIABLE,
     {17, s_str_MaxMonitoredItems},
     {17, s_str_MaxMonitoredItems},
     NULL,
     0,
     &s_max_monitored_items_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {24098}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxSubscriptionsPerSession},
     {26, s_str_MaxSubscriptionsPerSession},
     NULL,
     0,
     &s_max_subscriptions_per_session_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {24104}},
     MU_NODECLASS_VARIABLE,
     {32, s_str_MaxMonitoredItemsPerSubscription},
     {32, s_str_MaxMonitoredItemsPerSubscription},
     NULL,
     0,
     &s_max_monitored_items_per_subscription_value,
     .type_definition = {0}},
    {{0, MU_NODEID_NUMERIC, {31916}},
     MU_NODECLASS_VARIABLE,
     {26, s_str_MaxMonitoredItemsQueueSize},
     {26, s_str_MaxMonitoredItemsQueueSize},
     NULL,
     0,
     &s_max_monitored_items_queue_size_value,
     .type_definition = {0}},
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

/* Live ServerStatus (2256): emit the caller-owned mu_server_status_t scratch as a
   scalar ServerStatusDataType ExtensionObject (spec 084). CurrentTime is refreshed
   from the time adapter on every read; identity/build fields were set once by the
   server-init caller. The encoder reads value.array as a const mu_server_status_t*
   (OPC-10000-5 §12.10). */
static opcua_statuscode_t base_server_status_read(void *ctx, const mu_nodeid_t *id, mu_variant_t *v) {
    mu_server_status_t *st = (mu_server_status_t *)ctx; /* mutable scratch, caller-owned */
    (void)id;
    st->current_time = (st->time && st->time->get_time) ? st->time->get_time(st->time->context) : 0;
    *v = (mu_variant_t){0};
    v->type = MU_TYPE_EXTENSIONOBJECT;
    v->value.array = st;
    v->ext_encoding_id = 864u; /* ServerStatusDataType_Encoding_DefaultBinary */
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_FACET_CORE_2022_SERVER */

#if defined(MUC_OPCUA_FACET_CORE_2022_SERVER) && MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS
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
    v->ext_encoding_id = 861u; /* ServerDiagnosticsSummaryDataType_Encoding_DefaultBinary */
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
#if MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS
                          ,
                          const mu_diagnostics_summary_t *diag
#endif
) {
    s->rt.time = time;
    s->rt.start_time = start_time;
#if MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS
    s->rt.diag = diag;
#endif
    /* ServerStatus (2256) scratch (spec 084): caller-owned, mutable, never a static --
       CurrentTime is refreshed on each read by base_server_status_read. */
    s->server_status = (mu_server_status_t){0};
    s->server_status.time = time;
    s->server_status.start_time = start_time;
    s->server_status.state = 0;

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

#if MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS
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

    /* ServerStatus (2256, spec 084): a live ServerStatusDataType ExtensionObject.
       This runtime node shadows the static NULL-valued 2256 node in the const base
       address space via mu_resolve_node's user -> dynamic -> base lookup order.
       Reuses s_server_status_refs so Browse of ServerStatus's children (State,
       CurrentTime, StartTime) still works. */
    s->values[MU_BASE_RUNTIME_SERVER_STATUS_INDEX].type = MU_VALUESOURCE_CALLBACK;
    s->values[MU_BASE_RUNTIME_SERVER_STATUS_INDEX].data.callback.read = base_server_status_read;
    s->values[MU_BASE_RUNTIME_SERVER_STATUS_INDEX].data.callback.context = &s->server_status;
    s->nodes[MU_BASE_RUNTIME_SERVER_STATUS_INDEX] = (mu_node_t){
        .node_id = {.namespace_index = 0, .identifier_type = MU_NODEID_NUMERIC, .identifier.numeric = 2256u},
        .node_class = MU_NODECLASS_VARIABLE,
        .browse_name = {.length = (opcua_int32_t)12, .data = (const opcua_byte_t *)s_str_ServerStatus},
        .display_name = {.length = (opcua_int32_t)12, .data = (const opcua_byte_t *)s_str_ServerStatus},
        .references = s_server_status_refs,
        .reference_count = sizeof(s_server_status_refs) / sizeof(s_server_status_refs[0]),
        .value = &s->values[MU_BASE_RUNTIME_SERVER_STATUS_INDEX],
        .type_definition = {0, MU_NODEID_NUMERIC, {.numeric = 63u}}}; /* BaseDataVariableType */

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
