# OPC UA Profile Roadmap

GENERATED from profiles/opcua-profile-manifest.yaml by
scripts/profile_manifest/generate.py — do not edit; regenerate with:
  python3 scripts/profile_manifest/generate.py \
      --manifest profiles/opcua-profile-manifest.yaml --outputs roadmap

Full OPC UA item matrix tracking implementation state, Kconfig mapping,
profile availability, and test coverage. Items span implemented, claimed,
deferred, and unimplemented states. This document is a roadmap, not a
compliance certificate — it records what the project implements and what
remains future work.

## Summary

| State | Count |
|-------|-------|
| claimed | 34 |
| implemented | 1 |
| deferred | 7 |
| unimplemented | 121 |

## Item matrix

| Item | Kind | State | OPC reference | Kconfig | Profiles | Backing tests |
|------|------|-------|---------------|---------|----------|---------------|
| service_read | conformance_unit | claimed | OPC-10000-4 §5.10.2 Core 2017 Server Facet | SERVICE_READ | all | test_read_service |
| service_browse | conformance_unit | claimed | OPC-10000-4 §5.8 Core 2017 | SERVICE_BROWSE | all | test_browse_service, test_browse_limits, test_view_services |
| service_discovery | conformance_unit | claimed | OPC-10000-4 §5.4 Core 2017 | SERVICE_DISCOVERY | all | test_discovery_endpoint |
| service_register_nodes | conformance_unit | claimed | OPC-10000-4 §5.9 Core 2017 | SERVICE_REGISTER_NODES | all | test_view_services, test_profile_surface |
| service_write | conformance_unit | claimed | OPC-10000-4 §5.10.4 Core 2017 Attribute Write | SERVICE_WRITE | full | test_write_service |
| service_history | conformance_unit | claimed | OPC-10000-11 Historical Access Server Facet | SERVICE_HISTORY | full | test_history |
| service_query | conformance_unit | claimed | OPC-10000-4 §5.9 Query | SERVICE_QUERY | full | test_query_service |
| service_nodemanagement | conformance_unit | claimed | OPC-10000-4 §5.7 NodeManagement | SERVICE_NODEMANAGEMENT | full | test_node_management, test_node_management_errors |
| base_nodes | facet | claimed | OPC-10000-7 §4.2 Core 2017 Server Facet | BASE_NODES | all | test_profile_surface |
| base_type_system | facet | claimed | OPC-10000-5 Base Info Type System | BASE_TYPE_SYSTEM | embedded, standard, full | test_type_system, test_method_call |
| extended_nodeids | facet | claimed | OPC-10000-3 Extended (string/GUID) NodeIds | EXTENDED_NODEIDS | full | test_address_space_string_limits, test_binary_nodeid_errors |
| namespaces | facet | claimed | OPC-10000-4 Namespace metadata | NAMESPACES | full | test_read_browsename_namespace |
| dynamic_nodes | facet | claimed | OPC-10000-3 Dynamic address space | DYNAMIC_NODES | full | test_address_space_dynamic |
| complex_types | facet | claimed | OPC-10000-6 Structure/complex type encoding | COMPLEX_TYPES | full | test_complex_types |
| user_auth | facet | claimed | OPC-10000-4 §7.36 UserNamePassword (id 1096) / X509 (id 1097) | USER_AUTH | all | test_user_auth_plaintext, test_user_auth_certificate, test_user_auth_secure_e2e |
| multiple_connections | facet | claimed | OPC-10000-7 MicroEmbeddedDevice2017 Session Minimum 2 Parallel | MULTIPLE_CONNECTIONS | micro, embedded, standard, full | test_connection_multiplex |
| session_timeout | facet | claimed | OPC-10000-4 Session idle timeout | SESSION_TIMEOUT | micro, embedded, standard, full | test_base_server_behaviour |
| security | facet | claimed | OPC-10000-7 Security Policy Required + Default AppInstance Certificate | SECURITY | embedded, standard, full | test_asym_chunk, test_sym_chunk, test_certificate, test_certificate_validity, test_security_trustlist, test_server_handshake_secure, test_secure_handshake_modern |
| ecc | facet | claimed | OPC-10000-7 Security ECC Policy (curve25519 / nistP256) | ECC | full | test_ecc_crypto |
| multi_chunk | facet | claimed | OPC-10000-6 Multi-chunk messages / index-range Read | MULTI_CHUNK | full | test_message_chunk_errors |
| time_sync | facet | claimed | OPC-10000-4 §7.28 Security time synchronization (clock-skew validation) | TIME_SYNC | full | test_time_sync, test_dispatch_services |
| auditing | facet | claimed | OPC-10000-4 §7.x Audit events | AUDITING | full | test_audit_events |
| subscriptions | facet | claimed | OPC-10000-4 §5.12/5.13 EmbeddedDataChangeSubscription | SUBSCRIPTIONS | micro, embedded, standard, full | test_subscriptions, test_subscriptions_errors |
| subscriptions_standard | facet | claimed | OPC-10000-7 Standard/Enhanced DataChange Subscription 2017 | SUBSCRIPTIONS_STANDARD | embedded, standard, full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| events | facet | claimed | OPC-10000-9 Event notifications & Alarms/Conditions | EVENTS | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| event_filter_where | facet | claimed | OPC-10000-4 §7.4 Standard Event Subscription facet / ContentFilter | EVENT_FILTER_WHERE | full | test_event_filter_where, test_event_filter_select |
| aggregate_full | facet | claimed | OPC-10000-13 Aggregate Subscription facet | AGGREGATE_FULL | full | test_aggregate, test_aggregate_full |
| redundancy | facet | claimed | OPC-10000-4 §5.14.7 Client Redundancy (TransferSubscriptions) | REDUNDANCY | full | test_transfer_subscriptions |
| method_server | facet | claimed | OPC-10000-4 §5.11 Method Server facet (arbitrary Call) | METHOD_SERVER | full | test_method_call_arbitrary, test_method_call_errors |
| custom_methods | facet | claimed | OPC-10000-4 §5.11 Custom method registration (alias of Method Server) | CUSTOM_METHODS | full | test_method_call |
| pubsub | facet | claimed | OPC-10000-14 PubSub | PUBSUB | full | test_uadp_encoding, test_pubsub |
| data_access | facet | claimed | OPC-10000-8 §5.3 Data Access Server Facet | DATA_ACCESS | full | test_analog_item, test_da_type_nodes, test_eu_information |
| reverse_connect | facet | claimed | OPC-10000-6 §7.1.3 Reverse Connect (server-initiated ReverseHello) | REVERSE_CONNECT | full | test_reverse_connect |
| server_diagnostics | facet | claimed | OPC-10000-5 §6.3 ServerDiagnostics object + counters | SERVER_DIAGNOSTICS | full | test_diagnostics |
| read_cache | optimization | implemented |  | READ_CACHE | — | — |
| opc_file_server_facet | facet | unimplemented | OPC-10000-20 File Server Facet | — | — | — |
| opc_json_encoding | facet | unimplemented | OPC-10000-6 §5.3 JSON Encoding | — | — | — |
| opc_xml_encoding | facet | unimplemented | OPC-10000-6 §5.4 XML Encoding | — | — | — |
| opc_https_transport | facet | unimplemented | OPC-10000-7 HTTPS Transport | — | — | — |
| opc_websocket_transport | facet | unimplemented | OPC-10000-7 WebSocket Transport | — | — | — |
| opc_monitor_items_500 | conformance_unit | unimplemented | OPC-10000-4 §5.13.2 Monitor Items 500 | — | — | — |
| opc_monitor_minqueuesize_05 | conformance_unit | unimplemented | OPC-10000-4 §5.13.2 Monitor MinQueueSize_05 | — | — | — |
| opc_facet_1029 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_facet_1322 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_CORE_2022_SERVER | all | — |
| opc_facet_1636 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_facet_1637 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_cu_2317 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2328 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2352 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2389 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2400 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2407 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2446 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2447 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2476 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2478 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2479 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2480 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2600 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2711 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2786 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2808 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2809 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2820 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2936 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2969 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3072 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3073 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3127 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3147 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3175 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3184 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3186 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3192 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3198 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3530 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3545 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3554 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3560 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3802 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3808 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3912 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3983 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3985 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_4053 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_4237 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_5240 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_5505 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_5793 | conformance_unit | unimplemented |  | — | all | — |
| opc_facet_1219 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER | embedded, standard, full | — |
| opc_facet_1324 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER | embedded, standard, full | — |
| opc_facet_1631 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER | embedded, standard, full | — |
| opc_facet_1695 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER | embedded, standard, full | — |
| opc_facet_1696 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_USER_TOKEN_X509_CERTIFICATE_SERVER | standard, full | — |
| opc_cu_2863 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3125 | conformance_unit | unimplemented |  | — | standard, full | — |
| opc_facet_2250 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_EMBEDDED_DATACHANGE_SUBSCRIPTION_2022_SERVER | micro, embedded, standard, full | — |
| opc_cu_2231 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2423 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2481 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2482 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2483 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2484 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2485 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2490 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2491 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2500 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2512 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2513 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2514 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2516 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2517 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2518 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2536 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2823 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2928 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2940 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2963 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3143 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3146 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3185 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3188 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3189 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3196 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3207 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3214 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3532 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3534 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3535 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3536 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3544 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3547 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3550 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3551 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3641 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3644 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3645 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3727 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3747 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3748 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3749 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3750 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3751 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3752 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3753 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3754 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3755 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3756 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3757 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3758 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3759 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3911 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3913 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3922 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3996 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_4052 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_4054 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_4055 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_4426 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_5207 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_5208 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_5801 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_5868 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |

## Capacities

| Capacity | Kind | Override | nano | micro | embedded | standard | full |
|----------|------|----------|------|-------|----------|----------|------|
| max_sessions | profile_varying | MU_MAX_SESSIONS | 2 | 2 | 2 | 50 | 100 |
| max_connections | profile_varying | MU_MAX_CONNECTIONS | 1 | 2 | 4 | 50 | 100 |
| max_subscriptions | profile_varying | MU_MAX_SUBSCRIPTIONS | 2 | 2 | 2 | 50 | 100 |
| max_monitored_items | profile_varying | MU_MAX_MONITORED_ITEMS | 8 | 8 | 100 | 1000 | 2000 |
| max_publish_requests | profile_varying | MU_MAX_PUBLISH_REQUESTS | 4 | 4 | 5 | 50 | 100 |
| monitored_queue_depth | profile_varying | MU_MONITORED_QUEUE_DEPTH | 1 | 1 | 2 | 5 | 5 |
| max_array_length | profile_varying | MU_MAX_ARRAY_LENGTH | 512 | 512 | 2048 | 8192 | 8192 |
| max_trigger_links | invariant | MU_MAX_TRIGGER_LINKS | 4 | 4 | 4 | 4 | 4 |
| max_where_elements | invariant | MU_MAX_WHERE_ELEMENTS | 8 | 8 | 8 | 8 | 8 |
| max_where_operands | invariant | MU_MAX_WHERE_OPERANDS | 16 | 16 | 16 | 16 | 16 |
| where_blob_bytes | invariant | MU_WHERE_BLOB_BYTES | 64 | 64 | 64 | 64 | 64 |
| max_address_space_nodes | invariant | MU_MAX_ADDRESS_SPACE_NODES | 64 | 64 | 64 | 64 | 64 |
| max_dynamic_nodes | invariant | MU_MAX_DYNAMIC_NODES | 32 | 32 | 32 | 32 | 32 |
| max_dynamic_references | invariant | MU_MAX_DYNAMIC_REFERENCES | 64 | 64 | 64 | 64 | 64 |
| max_dynamic_browse_name_length | invariant | MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH | 64 | 64 | 64 | 64 | 64 |
| max_dynamic_display_name_length | invariant | MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH | 64 | 64 | 64 | 64 | 64 |
| max_dynamic_string_nodeid_length | invariant | MU_MAX_DYNAMIC_STRING_NODEID_LENGTH | 64 | 64 | 64 | 64 | 64 |
| max_query_continuation_points | invariant | MU_MAX_QUERY_CONTINUATION_POINTS | 2 | 2 | 2 | 2 | 2 |
| max_conditions | invariant | MU_MAX_CONDITIONS | 10 | 10 | 10 | 10 | 10 |
| max_secure_channels | derived | MU_MAX_SECURE_CHANNELS | 1 | 2 | 4 | 50 | 100 |
| max_dynamic_reference_string_nodeid_length | derived | MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH | 64 | 64 | 64 | 64 | 64 |
