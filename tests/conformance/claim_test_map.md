# Claim → Test Map

GENERATED from profiles/opcua-profile-manifest.yaml by
scripts/profile_manifest/generate.py — do not edit; regenerate with:
  python3 scripts/profile_manifest/generate.py \
      --manifest profiles/opcua-profile-manifest.yaml --outputs claim_map

Machine-checked mapping from each claimed OPC UA conformance unit /
behavior to the test(s) that verify it, and the profiles those tests
must run in. Enforced by `check_claim_map.py` (registered as the
`test_claim_map` ctest in every profile build): for the profile a build
targets, every row listing that profile MUST have a backing test that is
registered in that build. A claimed unit whose backing test is absent
from its profile fails the build (OPC-10000-7 §4.2/§4.3).

Profiles column: comma-separated subset of
`nano,micro,embedded,standard,full`, or `all`.
Backing test column: comma-separated ctest names (as registered).

| Claim / conformance unit | OPC UA § | Profiles | Backing test |
|--------------------------|----------|----------|--------------|
| Secure channel message crypto | OPC-10000-7 §4.3 | micro, embedded, standard, full | test_secure_handshake_modern |
| Monitor Items 500 | OPC-10000-4 §5.13.2 |  | test_profile_surface |
| Monitor MinQueueSize_05 | OPC-10000-4 §5.13.2 |  | test_profile_surface |
| Address Space AddIn Reference |  | full | test_profile_surface |
| Address Space AddIn DefaultInstanceBrowsename |  | full | test_profile_surface |
| Base Info LocalTime |  | full | test_profile_surface |
| SecurityPolicy Support |  | all | test_security_policy, test_secure_channel |
| Base Info Selection List |  | full | test_profile_surface |
| Address Space Atomicity |  | all | test_profile_surface |
| Address Space Full Array Only |  | all | test_profile_surface |
| Base Info ValueAsText |  | full | test_profile_surface |
| Base Info OptionSet |  | full | test_profile_surface |
| Base Info Core Structure 2 |  | micro, embedded, standard, full | test_profile_surface |
| Base Info Core Views Folder |  | micro, embedded, standard, full | test_profile_surface |
| Base Info Estimated Return Time |  | full | test_profile_surface |
| Base Info Namespace Metadata |  | micro, embedded, standard, full | test_profile_surface |
| Address Space Base |  | micro, embedded, standard, full | test_profile_surface |
| Address Space Interfaces |  | full | test_profile_surface |
| Documentation - Core Capacities |  | all | test_profile_surface |
| Base Info Server Capabilities 2 |  | all | test_profile_surface |
| Base Info Locations Object |  | full | test_profile_surface |
| Address Space NonVolatile and Constant |  | full | test_profile_surface |
| Base Info Currency |  | full | test_profile_surface |
| Push Model for Global Certificate and TrustList Management |  | embedded, standard, full | test_claim_map |
| Base Info Rational Number |  | full | test_type_system |
| Base Info NormalizedString DataType |  | full | test_type_system |
| Base Info DecimalString DataType |  | full | test_type_system |
| Base Info Date DataTypes |  | embedded, standard, full | test_type_system |
| Base Info BitFieldMaskDataType |  | full | test_profile_surface |
| Base Info KeyValuePair |  | full | test_profile_surface |
| Base Info Subvariables of Structures |  | full | test_profile_surface |
| Base Info AssociatedWith |  | full | test_profile_surface |
| Base Info EUInformation |  | full | test_profile_surface |
| Base Info OrderedList |  | full | test_profile_surface |
| Base Info Audio Type |  | full | test_profile_surface |
| Base Info Spatial Data |  | full | test_profile_surface |
| Base Info HasOrderedComponent |  | full | test_profile_surface |
| Base Info Deprecated Information |  | full | test_profile_surface |
| Base Info Image DataTypes |  | full | test_profile_surface |
| Base Info ContentFilter |  | full | test_profile_surface |
| Monitored Items Deadband Filter |  | embedded, standard, full | test_profile_surface |
| Base Info GetMonitoredItems Method |  | embedded, standard, full | test_profile_surface |
| Monitor Basic |  | micro, embedded, standard, full | test_profile_surface |
| Monitor Triggering |  | embedded, standard, full | test_profile_surface |
| Base Info Core Types Folders |  | embedded, standard, full | test_profile_surface |
| Base Info Base Types |  | embedded, standard, full | test_profile_surface |
| Base Info ServerType |  | embedded, standard, full | test_type_system |
| Base Info Fixed SamplingInterval |  | full | test_profile_surface |
| Base Info OptionSet DataType |  | full | test_profile_surface |
| Base Info Range DataType |  | full | test_profile_surface |
| Monitor Queueing |  | embedded, standard, full | test_profile_surface |
| Base Info ResendData Method |  | embedded, standard, full | test_profile_surface |
| Base Info UaBinary File |  | full | test_profile_surface |
| Base Info StatusResult DataType |  | full | test_profile_surface |
| Base Info UriString |  | full | test_profile_surface |
| Base Info Method Argument DataType |  | embedded, standard, full | test_profile_surface |
| Base Info SemanticVersionString |  | full | test_profile_surface |
| Base Info IsExecutableOn |  | full | test_profile_surface |
| Base Info IsExecutingOn |  | full | test_profile_surface |
| Base Info Controls |  | full | test_profile_surface |
| Base Info Utilizes |  | full | test_profile_surface |
| Base Info Requires |  | full | test_profile_surface |
| Base Info IsPhysicallyConnectedTo |  | full | test_profile_surface |
| Base Info RepresentsSameEntityAs |  | full | test_profile_surface |
| Base Info RepresentsSameHardwareAs |  | full | test_profile_surface |
| Base Info RepresentsSameFunctionalityAs |  | full | test_profile_surface |
| Base Info IsHostedBy |  | full | test_profile_surface |
| Base Info HasPhysicalComponent |  | full | test_profile_surface |
| Base Info HasContainedComponent |  | full | test_profile_surface |
| Base Info HasAttachedComponent |  | full | test_profile_surface |
| Base Info Server Capabilities Subscriptions |  | micro, embedded, standard, full | test_profile_surface |
| Base Info SemanticChange Bit |  | micro, embedded, standard, full | test_profile_surface |
| Base Info ReferenceDescription |  | full | test_profile_surface |
| Base Info TrimmedString |  | full | test_profile_surface |
| Base Info Handle DataType |  | full | test_profile_surface |
| Base Info Server Capabilities MaxMonitoredItemsQueueSize |  | micro, embedded, standard, full | test_profile_surface |
| Base Info Decimal DataType |  | embedded, standard, full | test_profile_surface |
| Monitor Items 2 |  | micro, embedded, standard, full | test_profile_surface |
| Monitor Value Change V2 |  | micro, embedded, standard, full | test_profile_surface |
| Base Info Type Information |  | embedded, standard, full | test_claim_map |
| Base Info Portable IDs |  | full | test_profile_surface |
| Core 2022 Server Facet | OPC-10000-4 §5.12/5.13 | micro, embedded, standard, full | test_subscriptions, test_subscriptions_errors |
| Core 2022 Server Facet | OPC-10000-7 | embedded, standard, full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| Core 2022 Server Facet | OPC-10000-7 | full | test_ecc_crypto |
| Core 2022 Server Facet | OPC-10000-9 | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| Core 2022 Server Facet | OPC-10000-8 §5.3 | full | test_analog_item, test_da_type_nodes, test_eu_information |
| Core 2022 Server Facet | OPC-10000-4 §5.11 | full | test_method_call_arbitrary, test_method_call_errors |
| Core 2022 Server Facet | OPC-10000-4 §5.11 | full | test_method_call |
| Core 2022 Server Facet | OPC-10000-4 §7.36 | all | test_user_auth_plaintext, test_user_auth_certificate, test_user_auth_secure_e2e |
| Core 2022 Server Facet | OPC-10000-7 | micro, embedded, standard, full | test_connection_multiplex |
| Core 2022 Server Facet | OPC-10000-4 §7.4 | full | test_event_filter_where, test_event_filter_select |
| Core 2022 Server Facet | OPC-10000-4 §5.14.7 | full | test_transfer_subscriptions |
| Core 2022 Server Facet | OPC-10000-5 §6.3 | full | test_diagnostics |
| Core 2022 Server Facet | OPC-10000-6 | full | test_complex_types |
| Core 2022 Server Facet | OPC-10000-4 §7.x | full | test_audit_events, test_event_notifications |
| Core 2022 Server Facet | OPC-10000-3 | full | test_address_space_dynamic |
| Core 2022 Server Facet | OPC-10000-6 | full | test_message_chunk_errors |
| Core 2022 Server Facet | OPC-10000-4 | micro, embedded, standard, full | test_base_server_behaviour |
| Core 2022 Server Facet | OPC-10000-4 §7.28 | all | test_time_sync, test_dispatch_services |
| Core 2022 Server Facet | OPC-10000-3 | full | test_address_space_string_limits, test_binary_nodeid_errors |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.3 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.4 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.5 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.6 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.7 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.8 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.9 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.10 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.13 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.14 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.15 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.19 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.20 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.23 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.24 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.25 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.28 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.29 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.30 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.31 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.32 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.33 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.34 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.35 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.11 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.12 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.16 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.17 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.18 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.21 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.22 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.26 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.27 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.36 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.37 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.38 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 §4.2.2.39 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-13 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-14 | full | test_uadp_encoding, test_pubsub |
| Core 2022 Server Facet | OPC-10000-6 §7.1.3 | full | test_reverse_connect |
| Core 2022 Server Facet | OPC-10000-4 | full | test_read_browsename_namespace |
| Core 2022 Server Facet | OPC-10000-5 | embedded, standard, full | test_type_system |
| Core 2022 Server Facet | OPC-10000-5 | embedded, standard, full | test_type_system |
| Core 2022 Server Facet | OPC-10000-5 | embedded, standard, full | test_type_system |
| Core 2017 Server Facet: Attribute Read | OPC-10000-4 §5.10.2 | all | test_read_service |
| Core 2017: View Basic / TranslateBrowsePath | OPC-10000-4 §5.8 | all | test_browse_service, test_browse_limits, test_view_services |
| Core 2017: Discovery Find Servers Self / Get Endpoints | OPC-10000-4 §5.4 | all | test_discovery_endpoint |
| Core 2017: View RegisterNodes | OPC-10000-4 §5.9 | all | test_view_services, test_profile_surface |
| Core 2017 Attribute Write | OPC-10000-4 §5.10.4 | full | test_write_service |
| Historical Access Server Facet | OPC-10000-11 | full | test_history |
| Historical Raw Data 2022 Server Facet | OPC-10000-11 | full | test_profile_surface |
| Historical Data Update 2022 Server Facet | OPC-10000-11 | full | test_profile_surface |
| Historical Data Insert 2022 Server Facet | OPC-10000-11 | full | test_profile_surface |
| Historical Data Replace 2022 Server Facet | OPC-10000-11 | full | test_profile_surface |
| Historical Data Delete 2022 Server Facet | OPC-10000-11 | full | test_profile_surface |
| Historical Access Replace Value | OPC-10000-11 | full | test_profile_surface |
| Query | OPC-10000-4 §5.9 | full | test_query_service |
| NodeManagement | OPC-10000-4 §5.7 | full | test_node_management, test_node_management_errors |
| View TranslateBrowsePath | OPC-10000-4 §5.9.4 | all | test_browse_service, test_view_services |
| Discovery Get Endpoints | OPC-10000-4 §5.5.1, 5.5.4 | all | test_discovery_endpoint, test_discovery_services |
| Discovery Find Servers Self | OPC-10000-4 §5.5.2 | all | test_discovery_services, test_discovery_endpoint |
| Attribute Write Values | OPC-10000-4 §5.11.4 | full | test_write_value_gate, test_write_service |
| Session Change User | OPC-10000-4 §5.7.3 | full | test_session, test_session_auth |
| Security Administration |  | full | test_profile_surface |
| Time Sync – OS based support |  | full | test_time_sync |
| Time Sync – IEEE 1588 (PTP) |  | full | test_claim_map |
| Time Sync – IEEE 802.1AS |  | full | test_claim_map |
| Time Sync – NTP |  | full | test_time_sync |
| Security Role Server Authorization |  | full | test_role_management |
| Security Invalid user token |  | embedded, standard, full | test_profile_surface |
| Attribute Write StatusCode & Timestamp | OPC-10000-4 §5.11.4 | full | test_write_service, test_write_response |
| Attribute Read |  | all | test_profile_surface |
| View RegisterNodes |  | all | test_profile_surface |
| Security User X509 |  | standard, full | test_profile_surface |
| Subscription PublishRequest Queue Overflow |  | micro, embedded, standard, full | test_profile_surface |
| Attribute Write Index | OPC-10000-4 §5.11.4 | full | test_write_service |
| Session Base |  | all | test_profile_surface |
| Base Info Diagnostics | OPC-10000-5 §6.3.1, 6.3.3, 8.3.2, 12.9 | full | test_diagnostics, test_profile_surface |
| View Basic 2 | OPC-10000-4 §5.9.2, 5.9.3 | all | test_browse_service, test_browse_limits, test_view_services |
| Subscription Multiple |  | embedded, standard, full | test_profile_surface |
| Subscription Retransmission Queue |  | embedded, standard, full | test_profile_surface |
| Security User Name Password 2 |  | embedded, standard, full | test_profile_surface |
| Security User Token Unencrypted |  | full | test_profile_surface |
| Subscription Basic |  | micro, embedded, standard, full | test_profile_surface |
| Time Sync - Configure Clock Skew |  | full | test_profile_surface |
| Subscription Publish Basic |  | micro, embedded, standard, full | test_profile_surface |
| Base Services Diagnostics | OPC-10000-4 §7.32, 7.38 | full | test_service_header |
| Session General Service Behaviour |  | all | test_profile_surface |
| Time Sync – UA based support |  | full | test_profile_surface |
| Time Sync - Support |  | all | test_profile_surface |
| Protocol UA TCP | OPC-10000-6 §7.1 | all | test_tcp_connection |
| UA Binary Encoding | OPC-10000-6 §5 | all | test_binary_primitives, test_binary_nodeid |
| UA Secure Conversation | OPC-10000-6 §6 | all | test_secure_channel |
| Address Space Base | OPC-10000-3 §4 | all | test_address_space_validation, test_base_server_behaviour |
| Session Base | OPC-10000-4 §5.6 | all | test_session, test_session_auth |
| Base Info Core Structure 2 | OPC-10000-3 §4 | all | test_base_server_behaviour |
| Base Info Core Views Folder | OPC-10000-3 §4 | full | test_browse_service, test_view_services |
| Base Info Server Capabilities 2 | OPC-10000-3 §4 | all | test_base_server_behaviour |
| Session General Service Behaviour | OPC-10000-4 §5.6 | all | test_dispatch_session_order, test_base_server_behaviour |
| Base Info Namespace Metadata | OPC-10000-3 §4 | full | test_base_server_behaviour, test_read_browsename_namespace |
| Monitor QueueSize_ServerMax |  | full | test_profile_surface |
| Address Space Events 2 |  | full | test_claim_map |
| Monitor Events |  | full | test_profile_surface |
| Monitor Complex Event Filter |  | full | test_profile_surface |
| Node Management Add Node |  | full | test_profile_surface |
| Node Management Delete Node |  | full | test_profile_surface |
| Node Management Add Ref |  | full | test_profile_surface |
| Node Management Delete Ref |  | full | test_profile_surface |
| Base Info Events Capabilities |  | full | test_claim_map, test_read_service |
| Auditing Secure Communication |  | full | test_claim_map, test_event_notifications |
| Auditing Services |  | full | test_event_notifications |
| Auditing Write |  | full | test_claim_map, test_event_notifications |
| Auditing NodeManagement | OPC-10000-5 | full | test_claim_map |
| Auditing Method | OPC-10000-5 | full | test_claim_map |
| A & C Auditing | OPC-10000-9 §5.10 | full | test_claim_map |
| A & C Dialog Auditing | OPC-10000-9 §5.10.5 | full | test_claim_map |
| A & C Confirm Auditing | OPC-10000-9 §5.10.7 | full | test_claim_map |
| Session Cancel | OPC-10000-4 §5.6.5 | standard, full | test_claim_map |
| Discovery Register | OPC-10000-4 §5.4.5 | standard, full | test_claim_map |
| Security Policy Required | OPC-10000-7 §6.5 | embedded, standard, full | test_profile_surface |
| Discovery Register2 | OPC-10000-4 §5.4.6 | standard, full | test_profile_surface |
| Security ECC Policy | OPC-10000-7 §6.5 | full | test_profile_surface |
| Session Multiple | OPC-10000-4 §5.6 | micro, embedded, standard, full | test_profile_surface |
| Security Default ApplicationInstance Certificate |  | all | test_certificate_validity, test_server_config |
| Base Info Custom Type System |  | full | test_profile_surface |
| Base Info Engineering Units |  | full | test_profile_surface |
| Security – No Application Authentication |  | full | test_profile_surface |
| KeyCredential Service | OPC-10000-12 §8.5-8.6 | full | test_key_credential |
| User Role Management | OPC-10000-12 §9.5-9.6 | full | test_role_management |
| Certificate Management | OPC-10000-12 §7.5-7.6 | full | test_certificate_management |
| A & C Alarm | OPC-10000-9 §5.10 | full | test_profile_surface |
| A & C Acknowledge | OPC-10000-9 §5.7 | full | test_profile_surface |
| A & C ConditionClasses | OPC-10000-9 §5.9 | full | test_profile_surface |
| A & C First in Group Alarm | OPC-10000-9 §5.10.3 | full | test_profile_surface |
| A & C Condition Sub-Classes | OPC-10000-9 §5.9.11 | full | test_profile_surface |
| A & C Re-Alarming | OPC-10000-9 §5.10.4 | full | test_profile_surface |
| Alarms & Conditions | OPC-10000-9 §5 | full | test_alarms_conditions |
| Data Access TwoState |  | full | test_profile_surface |
| Data Access Complex Number |  | full | test_profile_surface |
| Data Access DiscreteItemType |  | full | test_profile_surface |
| Data Access MultiStateDictionaryEntryDBT |  | full | test_profile_surface |
| Data Access Semantic Changes |  | full | test_profile_surface |
| Data Access ValueAsDictionaryEntries Property |  | full | test_profile_surface |
| Data Access MultiStateValueDiscrete |  | full | test_profile_surface |
| Data Access DoubleComplex Number |  | full | test_profile_surface |
| Data Access MultiState |  | full | test_profile_surface |
| Data Access PercentDeadband |  | full | test_profile_surface |
| Data Access YArrayItemType |  | full | test_profile_surface |
| Data Access XYArrayItemType |  | full | test_profile_surface |
| Data Access ImageItemType |  | full | test_profile_surface |
| Data Access CubeItemType |  | full | test_profile_surface |
| Data Access NDimensionArrayItemType |  | full | test_profile_surface |
| Data Access AxisInformationType |  | full | test_profile_surface |
| Data Access DataItems |  | full | test_profile_surface |
| Data Access BaseAnalogType |  | full | test_profile_surface |
| Data Access AnalogItemType |  | full | test_profile_surface |
| Data Access AnalogUnitType |  | full | test_profile_surface |
| Data Access AnalogUnitRangeType |  | full | test_profile_surface |
| Data Access ArrayItem2Type |  | full | test_profile_surface |
| Base Info Node Management Capabilities |  | full | test_profile_surface |
| Base Info Choice States |  | full | test_profile_surface |
| Base Info System Status Underlying System |  | full | test_profile_surface |
| Base Info Available States and Transitions |  | full | test_profile_surface |
| Base Info Finite State Machine Instance |  | full | test_profile_surface |
| Base Info Device Failure |  | full | test_profile_surface |
| Base Info SemanticChange |  | full | test_profile_surface |
| Base Info System Status |  | full | test_profile_surface |
| Base Info EventQueueOverflow EventType |  | full | test_profile_surface |
| Base Info FileType Write |  | full | test_profile_surface |
| Base Info FileDirectoryType Base |  | full | test_profile_surface |
| Base Info LocalTime Events |  | full | test_profile_surface |
| Base Info OrderedList Change Notification |  | full | test_profile_surface |
| Base Info TemporaryFileTransferType Sync Read |  | full | test_profile_surface |
| Base Info TemporaryFileTransferType Async Read |  | full | test_profile_surface |
| Base Info TemporaryFileTransferType Sync Write |  | full | test_profile_surface |
| Base Info TemporaryFileTransferType Async Write |  | full | test_profile_surface |
| Base Info Client Events |  | full | test_profile_surface |
| Base Info Progress Events |  | full | test_profile_surface |
| Aggregate Subscription – Average | OPC-10000-13 §5.4.3.5 | full | test_profile_surface |
| Aggregate Subscription – Count | OPC-10000-13 §5.4.3.21 | full | test_profile_surface |
| Aggregate Subscription – Delta | OPC-10000-13 §5.4.3.27 | full | test_profile_surface |
| Aggregate Subscription – DeltaBounds | OPC-10000-13 §5.4.3.30 | full | test_profile_surface |
| Aggregate Subscription – DurationBad | OPC-10000-13 §5.4.3.32 | full | test_profile_surface |
| Aggregate Subscription – DurationGood | OPC-10000-13 §5.4.3.31 | full | test_profile_surface |
| Aggregate Subscription – DurationInStateZero | OPC-10000-13 §5.4.3.22 | full | test_profile_surface |
| Aggregate Subscription – End | OPC-10000-13 §5.4.3.26 | full | test_profile_surface |
| Aggregate Subscription – Interpolative | OPC-10000-13 §5.4.3.4 | full | test_profile_surface |
| Aggregate Subscription – Maximum | OPC-10000-13 §5.4.3.11 | full | test_profile_surface |
| Aggregate Subscription – Maximum2 | OPC-10000-13 §5.4.3.16 | full | test_profile_surface |
| Aggregate Subscription – Minimum | OPC-10000-13 §5.4.3.10 | full | test_profile_surface |
| Aggregate Subscription – Minimum2 | OPC-10000-13 §5.4.3.15 | full | test_profile_surface |
| Aggregate Subscription – PercentBad | OPC-10000-13 §5.4.3.34 | full | test_profile_surface |
| Aggregate Subscription – PercentGood | OPC-10000-13 §5.4.3.33 | full | test_profile_surface |
| Aggregate Subscription – Range | OPC-10000-13 §5.4.3.14 | full | test_profile_surface |
| Aggregate Subscription – Start | OPC-10000-13 §5.4.3.25 | full | test_profile_surface |
| Aggregate Subscription – TimeAverage | OPC-10000-13 §5.4.3.6 | full | test_profile_surface |
| Aggregate Subscription – TimeAverage2 | OPC-10000-13 §5.4.3.7 | full | test_profile_surface |
| Aggregate Subscription – Total | OPC-10000-13 §5.4.3.8 | full | test_profile_surface |
| Aggregate Subscription – Total2 | OPC-10000-13 §5.4.3.9 | full | test_profile_surface |
| Aggregate Subscription – WorstQuality | OPC-10000-13 §5.4.3.35 | full | test_profile_surface |
| Aggregate Subscription – WorstQuality2 | OPC-10000-13 §5.4.3.36 | full | test_profile_surface |
| Aggregate Subscription – MinimumActualTime | OPC-10000-13 §5.4.3.12 | full | test_profile_surface |
| Aggregate Subscription – MaximumActualTime | OPC-10000-13 §5.4.3.13 | full | test_profile_surface |
| Aggregate Subscription – MinimumActualTime2 | OPC-10000-13 §5.4.3.17 | full | test_profile_surface |
| Aggregate Subscription – MaximumActualTime2 | OPC-10000-13 §5.4.3.18 | full | test_profile_surface |
| Aggregate Subscription – Range2 | OPC-10000-13 §5.4.3.19 | full | test_profile_surface |
| Aggregate Subscription – DurationInStateNonZero | OPC-10000-13 §5.4.3.23 | full | test_profile_surface |
| Aggregate Subscription – NumberOfTransitions | OPC-10000-13 §5.4.3.24 | full | test_profile_surface |
| Aggregate Subscription – StartBound | OPC-10000-13 §5.4.3.28 | full | test_profile_surface |
| Aggregate Subscription – EndBound | OPC-10000-13 §5.4.3.29 | full | test_profile_surface |
| Aggregate Subscription – StandardDeviationSample | OPC-10000-13 §5.4.3.37 | full | test_profile_surface |
| Aggregate Subscription – VarianceSample | OPC-10000-13 §5.4.3.38 | full | test_profile_surface |
| Aggregate Subscription – StandardDeviationPopulation | OPC-10000-13 §5.4.3.39 | full | test_profile_surface |
| Aggregate Subscription – VariancePopulation | OPC-10000-13 §5.4.3.40 | full | test_profile_surface |
| Base Info State Machine DescriptionNodeIdDataType |  | full | test_profile_surface |
| Base Info TemporaryFileTransferType Base |  | full | test_profile_surface |
| Base Info Model Change |  | full | test_profile_surface |
| Address Space User Access Level Base |  | full | test_profile_surface |
| Address Space DataTypeDefinition Attribute |  | full | test_profile_surface |
| Base Info Method Capabilities |  | full | test_profile_surface |
| Address Space Method Meta Data |  | full | test_profile_surface |
| Address Space Dictionary URI |  | full | test_profile_surface |
| Address Space Dictionary IRDI |  | full | test_profile_surface |
| Base Info FileType Base |  | full | test_profile_surface |
| Base Info Model Change General |  | full | test_profile_surface |
| Base Info Security Role Capabilities |  | full | test_profile_surface |
| Address Space Notifier Hierarchy |  | full | test_profile_surface |
| Address Space UserWriteMask Multilevel |  | full | test_profile_surface |
| Address Space Source Hierarchy |  | full | test_profile_surface |
| Base Info RequestServerStateChange Method |  | full | test_profile_surface |
| Base Info State Machine Instance |  | full | test_profile_surface |
| Address Space WriteMask |  | full | test_profile_surface |
| Address Space Dictionary Entries |  | full | test_profile_surface |
| Base Info History ReadEvents Capabilities |  | full | test_profile_surface |
| Base Info History ReadData Capabilities |  | full | test_profile_surface |
| Base Info History UpdateData Capabilities |  | full | test_profile_surface |
| Base Info History UpdateEvents Capabilities |  | full | test_profile_surface |
| Base Info History Read Capabilities |  | full | test_profile_surface |
| Address Space Method |  | full | test_profile_surface |
| Address Space UserWriteMask |  | full | test_profile_surface |
| User Token JWT Server Facet | OPC-10000-7 §CU 1697 | full | test_jwt_activate_session |
| Certificate Manager Pull | OPC-10000-12 §7.6, 7.9 | full | test_certificate_manager |
