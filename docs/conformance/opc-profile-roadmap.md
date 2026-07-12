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
| claimed | 31 |
| implemented | 1 |
| deferred | 7 |
| unimplemented | 88 |

## Item matrix

| Item | Kind | State | OPC reference | Kconfig | Profiles | Backing tests |
|------|------|-------|---------------|---------|----------|---------------|
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
| opc_cu_2446 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2447 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2476 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2600 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2711 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2809 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2820 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_2969 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3127 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3184 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3186 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3198 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3545 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3554 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3560 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3808 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_3912 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_4053 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_4237 | conformance_unit | unimplemented |  | — | all | — |
| opc_cu_5240 | conformance_unit | unimplemented |  | — | all | — |
| opc_facet_1219 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER | embedded, standard, full | — |
| opc_facet_1324 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER | embedded, standard, full | — |
| opc_facet_1631 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER | embedded, standard, full | — |
| opc_facet_1695 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER | embedded, standard, full | — |
| opc_facet_1696 | facet | deferred | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_USER_TOKEN_X509_CERTIFICATE_SERVER | standard, full | — |
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
| opc_cu_2928 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2940 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_2963 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3146 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3185 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3188 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3189 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3196 | conformance_unit | unimplemented |  | — | micro, embedded, standard, full | — |
| opc_cu_3207 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3214 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3532 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3544 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3547 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3550 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3551 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3641 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
| opc_cu_3644 | conformance_unit | unimplemented |  | — | embedded, standard, full | — |
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
| opc_cu_subscription_basic | conformance_unit | claimed | OPC-10000-4 §5.12/5.13 Core 2022 Server Facet | MUC_OPCUA_CU_SUBSCRIPTION_BASIC | micro, embedded, standard, full | test_subscriptions, test_subscriptions_errors |
| opc_cu_subscription_standard | conformance_unit | claimed | OPC-10000-7 Core 2022 Server Facet | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD | embedded, standard, full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_security_ecc | conformance_unit | claimed | OPC-10000-7 Core 2022 Server Facet | MUC_OPCUA_CU_SECURITY_ECC | full | test_ecc_crypto |
| opc_cu_events | conformance_unit | claimed | OPC-10000-9 Core 2022 Server Facet | MUC_OPCUA_CU_EVENTS | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_data_access | conformance_unit | claimed | OPC-10000-8 §5.3 Core 2022 Server Facet | MUC_OPCUA_CU_DATA_ACCESS | full | test_analog_item, test_da_type_nodes, test_eu_information |
| opc_cu_method_server | conformance_unit | claimed | OPC-10000-4 §5.11 Core 2022 Server Facet | MUC_OPCUA_CU_METHOD_SERVER | full | test_method_call_arbitrary, test_method_call_errors |
| opc_cu_custom_methods | conformance_unit | claimed | OPC-10000-4 §5.11 Core 2022 Server Facet | MUC_OPCUA_CU_CUSTOM_METHODS | full | test_method_call |
| opc_cu_user_auth | conformance_unit | claimed | OPC-10000-4 §7.36 Core 2022 Server Facet | MUC_OPCUA_CU_USER_AUTH | all | test_user_auth_plaintext, test_user_auth_certificate, test_user_auth_secure_e2e |
| opc_cu_multiple_connections | conformance_unit | claimed | OPC-10000-7 Core 2022 Server Facet | MUC_OPCUA_CU_MULTIPLE_CONNECTIONS | micro, embedded, standard, full | test_connection_multiplex |
| opc_cu_event_filter_where | conformance_unit | claimed | OPC-10000-4 §7.4 Core 2022 Server Facet | MUC_OPCUA_CU_EVENT_FILTER_WHERE | full | test_event_filter_where, test_event_filter_select |
| opc_cu_redundancy | conformance_unit | claimed | OPC-10000-4 §5.14.7 Core 2022 Server Facet | MUC_OPCUA_CU_REDUNDANCY | full | test_transfer_subscriptions |
| opc_cu_diagnostics | conformance_unit | claimed | OPC-10000-5 §6.3 Core 2022 Server Facet | MUC_OPCUA_CU_DIAGNOSTICS | full | test_diagnostics |
| opc_cu_complex_types | conformance_unit | claimed | OPC-10000-6 Core 2022 Server Facet | MUC_OPCUA_CU_COMPLEX_TYPES | full | test_complex_types |
| opc_cu_auditing | conformance_unit | claimed | OPC-10000-4 §7.x Core 2022 Server Facet | MUC_OPCUA_CU_AUDITING | full | test_audit_events |
| opc_cu_dynamic_nodes | conformance_unit | claimed | OPC-10000-3 Core 2022 Server Facet | MUC_OPCUA_CU_DYNAMIC_NODES | full | test_address_space_dynamic |
| opc_cu_multi_chunk | conformance_unit | claimed | OPC-10000-6 Core 2022 Server Facet | MUC_OPCUA_CU_MULTI_CHUNK | full | test_message_chunk_errors |
| opc_cu_session_timeout | conformance_unit | claimed | OPC-10000-4 Core 2022 Server Facet | MUC_OPCUA_CU_SESSION_TIMEOUT | micro, embedded, standard, full | test_base_server_behaviour |
| opc_cu_time_sync | conformance_unit | claimed | OPC-10000-4 §7.28 Core 2022 Server Facet | MUC_OPCUA_CU_TIME_SYNC | full | test_time_sync, test_dispatch_services |
| opc_cu_extended_nodeids | conformance_unit | claimed | OPC-10000-3 Core 2022 Server Facet | MUC_OPCUA_CU_EXTENDED_NODEIDS | full | test_address_space_string_limits, test_binary_nodeid_errors |
| opc_cu_aggregate_full | conformance_unit | claimed | OPC-10000-13 Core 2022 Server Facet | MUC_OPCUA_CU_AGGREGATE_FULL | full | test_aggregate, test_aggregate_full |
| opc_cu_pubsub | conformance_unit | claimed | OPC-10000-14 Core 2022 Server Facet | MUC_OPCUA_CU_PUBSUB | full | test_uadp_encoding, test_pubsub |
| opc_cu_reverse_connect | conformance_unit | claimed | OPC-10000-6 §7.1.3 Core 2022 Server Facet | MUC_OPCUA_CU_REVERSE_CONNECT | full | test_reverse_connect |
| opc_cu_namespaces | conformance_unit | claimed | OPC-10000-4 Core 2022 Server Facet | MUC_OPCUA_CU_NAMESPACES | full | test_read_browsename_namespace |
| service_read | conformance_unit | claimed | OPC-10000-4 §5.10.2 Core 2017 Server Facet | MUC_OPCUA_CU_ATTRIBUTE_READ | all | test_read_service |
| service_browse | conformance_unit | claimed | OPC-10000-4 §5.8 Core 2017 | MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH | all | test_browse_service, test_browse_limits, test_view_services |
| service_discovery | conformance_unit | claimed | OPC-10000-4 §5.4 Core 2017 | MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS | all | test_discovery_endpoint |
| service_register_nodes | conformance_unit | claimed | OPC-10000-4 §5.9 Core 2017 | MUC_OPCUA_CU_VIEW_REGISTERNODES | all | test_view_services, test_profile_surface |
| service_write | conformance_unit | claimed | OPC-10000-4 §5.10.4 Core 2017 Attribute Write | MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE | full | test_write_service |
| service_history | conformance_unit | claimed | OPC-10000-11 Historical Access Server Facet | MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET | full | test_history |
| service_query | conformance_unit | claimed | OPC-10000-4 §5.9 Query | MUC_OPCUA_CU_QUERY | full | test_query_service |
| service_nodemanagement | conformance_unit | claimed | OPC-10000-4 §5.7 NodeManagement | MUC_OPCUA_CU_NODEMANAGEMENT | full | test_node_management, test_node_management_errors |

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
