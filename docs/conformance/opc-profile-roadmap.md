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
| claimed | 611 |
| implemented | 15 |
| documented | 5 |
| deferred | 7 |
| unimplemented | 11 |

## Item matrix

| Item | Kind | State | OPC reference | Kconfig | Profiles | Backing tests |
|------|------|-------|---------------|---------|----------|---------------|
| read_cache | optimization | implemented |  | READ_CACHE | — | — |
| secure_channel_crypto | optimization | implemented | OPC-10000-7 §4.3 | SECURE_CHANNEL_CRYPTO | micro, embedded, standard, full | test_secure_handshake_modern |
| opc_file_server_facet | facet | unimplemented | OPC-10000-20 File Server Facet | — | — | — |
| opc_json_encoding | facet | unimplemented | OPC-10000-6 §5.3 JSON Encoding | — | — | — |
| opc_xml_encoding | facet | unimplemented | OPC-10000-6 §5.4 XML Encoding | — | — | — |
| opc_https_transport | facet | unimplemented | OPC-10000-7 HTTPS Transport | — | — | — |
| opc_websocket_transport | facet | unimplemented | OPC-10000-7 WebSocket Transport | — | — | — |
| opc_monitor_items_500 | conformance_unit | claimed | OPC-10000-4 §5.13.2 Monitor Items 500 | — | — | test_profile_surface |
| opc_monitor_minqueuesize_05 | conformance_unit | claimed | OPC-10000-4 §5.13.2 Monitor MinQueueSize_05 | — | — | test_profile_surface |
| opc_facet_1029 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_facet_1322 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_CORE_2022_SERVER | all | — |
| opc_facet_1636 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_facet_1637 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_cu_2446 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE | full | test_profile_surface |
| opc_cu_2447 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME | full | test_profile_surface |
| opc_cu_2476 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_LOCALTIME | full | test_profile_surface |
| opc_cu_2600 | conformance_unit | claimed |  | — | all | test_security_policy, test_secure_channel |
| opc_cu_2711 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST | full | test_profile_surface |
| opc_cu_2809 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY | all | test_profile_surface |
| opc_cu_2820 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_FULL_ARRAY_ONLY | all | test_profile_surface |
| opc_cu_2969 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_VALUEASTEXT | full | test_profile_surface |
| opc_cu_3127 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_OPTIONSET | full | test_profile_surface |
| opc_cu_3184 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3186 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3198 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_ESTIMATED_RETURN_TIME | full | test_profile_surface |
| opc_cu_3545 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3554 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3560 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES | full | test_profile_surface |
| opc_cu_3808 | conformance_unit | claimed |  | — | all | test_profile_surface |
| opc_cu_3912 | conformance_unit | claimed |  | — | all | test_profile_surface |
| opc_cu_4053 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_LOCATIONS_OBJECT | full | test_profile_surface |
| opc_cu_4237 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT | full | test_profile_surface |
| opc_cu_5240 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_CURRENCY | full | test_profile_surface |
| opc_facet_1219 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER | embedded, standard, full | — |
| opc_facet_1324 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER | embedded, standard, full | — |
| opc_facet_1631 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER | embedded, standard, full | — |
| opc_facet_1695 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER | embedded, standard, full | — |
| opc_facet_1696 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_USER_TOKEN_X509_CERTIFICATE_SERVER | standard, full | — |
| opc_facet_2250 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_EMBEDDED_DATACHANGE_SUBSCRIPTION_2022_SERVER | micro, embedded, standard, full | — |
| opc_cu_2231 | conformance_unit | claimed |  | OPC_CU_2231 | embedded, standard, full | test_claim_map |
| opc_cu_2423 | conformance_unit | claimed |  | OPC_CU_2423 | full | test_type_system |
| opc_cu_2481 | conformance_unit | claimed |  | OPC_CU_2481 | full | test_type_system |
| opc_cu_2482 | conformance_unit | claimed |  | OPC_CU_2482 | full | test_type_system |
| opc_cu_2483 | conformance_unit | claimed |  | — | embedded, standard, full | test_type_system |
| opc_cu_2484 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2485 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2490 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2491 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2500 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2512 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2513 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2514 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2516 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2517 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2518 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2536 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2928 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_2940 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_2963 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3146 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3185 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3188 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3189 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_SERVERTYPE | embedded, standard, full | test_type_system |
| opc_cu_3196 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3207 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3214 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3532 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3544 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3547 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3550 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3551 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3641 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3644 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3747 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3748 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3749 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3750 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3751 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3752 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3753 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3754 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3755 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3756 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3757 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3758 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3759 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3911 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3922 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3996 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_4052 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_4054 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_4055 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_4426 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_5207 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_5208 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_5801 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION | embedded, standard, full | test_claim_map |
| opc_cu_5868 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_subscription_basic | conformance_unit | claimed | OPC-10000-4 §5.12/5.13 Core 2022 Server Facet | MUC_OPCUA_CU_SUBSCRIPTION_BASIC | micro, embedded, standard, full | test_subscriptions, test_subscriptions_errors |
| opc_cu_subscription_standard | optimization | claimed | OPC-10000-7 Core 2022 Server Facet | MUC_OPCUA_CU_SUBSCRIPTION_STANDARD | embedded, standard, full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_security_ecc | optimization | claimed | OPC-10000-7 Core 2022 Server Facet | MUC_OPCUA_CU_SECURITY_ECC | full | test_ecc_crypto |
| opc_cu_events | optimization | claimed | OPC-10000-9 Core 2022 Server Facet | MUC_OPCUA_CU_EVENTS | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_data_access | optimization | claimed | OPC-10000-8 §5.3 Core 2022 Server Facet | MUC_OPCUA_CU_DATA_ACCESS | full | test_analog_item, test_da_type_nodes, test_eu_information |
| opc_cu_method_server | optimization | claimed | OPC-10000-4 §5.11 Core 2022 Server Facet | MUC_OPCUA_CU_METHOD_SERVER | full | test_method_call_arbitrary, test_method_call_errors |
| opc_cu_custom_methods | optimization | claimed | OPC-10000-4 §5.11 Core 2022 Server Facet | MUC_OPCUA_CU_CUSTOM_METHODS | full | test_method_call |
| opc_cu_user_auth | optimization | claimed | OPC-10000-4 §7.36 Core 2022 Server Facet | MUC_OPCUA_CU_USER_AUTH | all | test_user_auth_plaintext, test_user_auth_certificate, test_user_auth_secure_e2e |
| opc_cu_multiple_connections | optimization | claimed | OPC-10000-7 Core 2022 Server Facet | MUC_OPCUA_CU_MULTIPLE_CONNECTIONS | micro, embedded, standard, full | test_connection_multiplex |
| opc_cu_event_filter_where | optimization | claimed | OPC-10000-4 §7.4 Core 2022 Server Facet | MUC_OPCUA_CU_EVENT_FILTER_WHERE | full | test_event_filter_where, test_event_filter_select |
| opc_cu_redundancy | optimization | claimed | OPC-10000-4 §5.14.7 Core 2022 Server Facet | MUC_OPCUA_CU_REDUNDANCY | full | test_transfer_subscriptions |
| opc_cu_diagnostics | optimization | claimed | OPC-10000-5 §6.3 Core 2022 Server Facet | MUC_OPCUA_CU_DIAGNOSTICS | full | test_diagnostics |
| opc_cu_complex_types | optimization | claimed | OPC-10000-6 Core 2022 Server Facet | MUC_OPCUA_CU_COMPLEX_TYPES | full | test_complex_types |
| opc_cu_auditing | optimization | claimed | OPC-10000-4 §7.x Core 2022 Server Facet | MUC_OPCUA_CU_AUDITING | full | test_audit_events, test_event_notifications |
| opc_cu_dynamic_nodes | optimization | claimed | OPC-10000-3 Core 2022 Server Facet | MUC_OPCUA_CU_DYNAMIC_NODES | full | test_address_space_dynamic |
| opc_cu_multi_chunk | optimization | claimed | OPC-10000-6 Core 2022 Server Facet | MUC_OPCUA_CU_MULTI_CHUNK | full | test_message_chunk_errors |
| opc_cu_session_timeout | optimization | claimed | OPC-10000-4 Core 2022 Server Facet | MUC_OPCUA_CU_SESSION_TIMEOUT | micro, embedded, standard, full | test_base_server_behaviour |
| opc_cu_time_sync | optimization | claimed | OPC-10000-4 §7.28 Core 2022 Server Facet | MUC_OPCUA_CU_TIME_SYNC | all | test_time_sync, test_dispatch_services |
| opc_cu_extended_nodeids | optimization | claimed | OPC-10000-3 Core 2022 Server Facet | MUC_OPCUA_CU_EXTENDED_NODEIDS | full | test_address_space_string_limits, test_binary_nodeid_errors |
| opc_cu_aggregate_interpolative | optimization | claimed | OPC-10000-13 §4.2.2.3 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_average | optimization | claimed | OPC-10000-13 §4.2.2.4 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_time_average | optimization | claimed | OPC-10000-13 §4.2.2.5 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_time_average_2 | optimization | claimed | OPC-10000-13 §4.2.2.6 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_total | optimization | claimed | OPC-10000-13 §4.2.2.7 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_total_2 | optimization | claimed | OPC-10000-13 §4.2.2.8 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_minimum | optimization | claimed | OPC-10000-13 §4.2.2.9 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_maximum | optimization | claimed | OPC-10000-13 §4.2.2.10 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_range | optimization | claimed | OPC-10000-13 §4.2.2.13 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_minimum_2 | optimization | claimed | OPC-10000-13 §4.2.2.14 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_maximum_2 | optimization | claimed | OPC-10000-13 §4.2.2.15 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_count | optimization | claimed | OPC-10000-13 §4.2.2.19 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_duration_state_zero | optimization | claimed | OPC-10000-13 §4.2.2.20 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_start | optimization | claimed | OPC-10000-13 §4.2.2.23 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_end | optimization | claimed | OPC-10000-13 §4.2.2.24 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_delta | optimization | claimed | OPC-10000-13 §4.2.2.25 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_delta_bounds | optimization | claimed | OPC-10000-13 §4.2.2.28 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_duration_good | optimization | claimed | OPC-10000-13 §4.2.2.29 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_duration_bad | optimization | claimed | OPC-10000-13 §4.2.2.30 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_percent_good | optimization | claimed | OPC-10000-13 §4.2.2.31 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_percent_bad | optimization | claimed | OPC-10000-13 §4.2.2.32 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_worst_quality | optimization | claimed | OPC-10000-13 §4.2.2.33 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_worst_quality_2 | optimization | claimed | OPC-10000-13 §4.2.2.34 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_annotation_count | optimization | claimed | OPC-10000-13 §4.2.2.35 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_min_actual_time | optimization | claimed | OPC-10000-13 §4.2.2.11 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_max_actual_time | optimization | claimed | OPC-10000-13 §4.2.2.12 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_min_actual_time_2 | optimization | claimed | OPC-10000-13 §4.2.2.16 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_max_actual_time_2 | optimization | claimed | OPC-10000-13 §4.2.2.17 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_range_2 | optimization | claimed | OPC-10000-13 §4.2.2.18 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_duration_state_nonzero | optimization | claimed | OPC-10000-13 §4.2.2.21 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_number_transitions | optimization | claimed | OPC-10000-13 §4.2.2.22 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_start_bound | optimization | claimed | OPC-10000-13 §4.2.2.26 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_end_bound | optimization | claimed | OPC-10000-13 §4.2.2.27 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_std_dev_sample | optimization | claimed | OPC-10000-13 §4.2.2.36 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_variance_sample | optimization | claimed | OPC-10000-13 §4.2.2.37 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_std_dev_population | optimization | claimed | OPC-10000-13 §4.2.2.38 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_variance_population | optimization | claimed | OPC-10000-13 §4.2.2.39 Core 2022 Server Facet | — | full | test_aggregate, test_aggregate_full |
| opc_cu_aggregate_full | optimization | claimed | OPC-10000-13 Core 2022 Server Facet | MUC_OPCUA_CU_AGGREGATE_FULL | full | test_aggregate, test_aggregate_full |
| opc_cu_pubsub | optimization | claimed | OPC-10000-14 Core 2022 Server Facet | MUC_OPCUA_CU_PUBSUB | full | test_uadp_encoding, test_pubsub |
| opc_cu_reverse_connect | optimization | claimed | OPC-10000-6 §7.1.3 Core 2022 Server Facet | MUC_OPCUA_CU_REVERSE_CONNECT | full | test_reverse_connect |
| opc_cu_namespaces | optimization | claimed | OPC-10000-4 Core 2022 Server Facet | MUC_OPCUA_CU_NAMESPACES | full | test_read_browsename_namespace |
| opc_cu_base_info_datatypes | optimization | claimed | OPC-10000-5 Core 2022 Server Facet | MUC_OPCUA_CU_BASE_INFO_DATATYPES | embedded, standard, full | test_type_system |
| opc_cu_base_info_argument_type | optimization | claimed | OPC-10000-5 Core 2022 Server Facet | MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE | embedded, standard, full | test_type_system |
| opc_cu_base_info_base_types | optimization | claimed | OPC-10000-5 Core 2022 Server Facet | MUC_OPCUA_CU_BASE_INFO_BASE_TYPES | embedded, standard, full | test_type_system |
| service_read | conformance_unit | claimed | OPC-10000-4 §5.10.2 Core 2017 Server Facet | MUC_OPCUA_CU_ATTRIBUTE_READ | all | test_read_service |
| service_browse | optimization | claimed | OPC-10000-4 §5.8 Core 2017 | MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH | all | test_browse_service, test_browse_limits, test_view_services |
| service_discovery | optimization | claimed | OPC-10000-4 §5.4 Core 2017 | MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS | all | test_discovery_endpoint |
| service_register_nodes | conformance_unit | claimed | OPC-10000-4 §5.9 Core 2017 | MUC_OPCUA_CU_VIEW_REGISTERNODES | all | test_view_services, test_profile_surface |
| service_write | optimization | claimed | OPC-10000-4 §5.10.4 Core 2017 Attribute Write | MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE | full | test_write_service |
| service_history | optimization | claimed | OPC-10000-11 Historical Access Server Facet | MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET | full | test_history |
| opc_cu_1571 | conformance_unit | claimed | OPC-10000-11 | — | full | test_profile_surface |
| opc_cu_1572 | conformance_unit | deferred | OPC-10000-11 | — | — | — |
| opc_cu_1573 | conformance_unit | claimed | OPC-10000-11 | — | full | test_profile_surface |
| opc_cu_1574 | conformance_unit | claimed | OPC-10000-11 | — | full | test_profile_surface |
| opc_cu_1575 | conformance_unit | claimed | OPC-10000-11 | — | full | test_profile_surface |
| opc_cu_1576 | conformance_unit | claimed | OPC-10000-11 | — | full | test_profile_surface |
| opc_cu_2264 | conformance_unit | claimed | OPC-10000-11 | — | full | test_profile_surface |
| opc_cu_1577 | conformance_unit | deferred | OPC-10000-11 | — | — | — |
| opc_cu_1578 | conformance_unit | deferred | OPC-10000-11 | — | — | — |
| opc_cu_1579 | conformance_unit | deferred | OPC-10000-11 | — | — | — |
| opc_cu_1580 | conformance_unit | deferred | OPC-10000-11 | — | — | — |
| opc_cu_1581 | conformance_unit | deferred | OPC-10000-11 | — | — | — |
| opc_cu_1710 | conformance_unit | deferred | OPC-10000-11 | — | — | — |
| opc_cu_2185 | conformance_unit | claimed | OPC-10000-11 | — | full | test_history |
| opc_cu_2332 | conformance_unit | claimed | OPC-10000-11 | — | full | test_history |
| service_query | optimization | claimed | OPC-10000-4 §5.9 Query | MUC_OPCUA_CU_QUERY | full | test_query_service |
| service_nodemanagement | optimization | claimed | OPC-10000-4 §5.7 NodeManagement | MUC_OPCUA_CU_NODEMANAGEMENT | full | test_node_management, test_node_management_errors |
| opc_facet_2242 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_facet_2322 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_facet_2323 | facet | unimplemented | OPC-10000-7 §4.2 | — | — | — |
| opc_facet_837 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_UA_TCP_UA_SC_UA_BINARY | all | — |
| opc_facet_1760 | facet | implemented | OPC-10000-7 §4.2 | MUC_OPCUA_FACET_SECURITY_TIME_SYNCHRONIZATION | all | — |
| opc_cu_2317 | conformance_unit | claimed | OPC-10000-4 §5.9.4 | MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH | all | test_browse_service, test_view_services |
| opc_cu_2328 | conformance_unit | claimed | OPC-10000-4 §5.5.1, 5.5.4 | MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS | all | test_discovery_endpoint, test_discovery_services |
| opc_cu_2352 | conformance_unit | claimed | OPC-10000-4 §5.5.2 | — | all | test_history |
| opc_cu_2389 | conformance_unit | claimed | OPC-10000-4 §5.11.4 | MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES | full | test_write_value_gate, test_write_service |
| opc_cu_2400 | conformance_unit | claimed | OPC-10000-4 §5.7.3 | MUC_OPCUA_CU_SESSION_CHANGE_USER | full | test_session, test_session_auth |
| opc_cu_2407 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2478 | conformance_unit | claimed |  | — | full | test_time_sync |
| opc_cu_2479 | conformance_unit | claimed |  | MUC_OPCUA_CU_TIME_SYNC_IEEE_1588_PTP | full | test_claim_map |
| opc_cu_2480 | conformance_unit | claimed |  | MUC_OPCUA_CU_TIME_SYNC_IEEE_802_1AS | full | test_claim_map |
| opc_cu_2786 | conformance_unit | claimed |  | — | full | test_time_sync |
| opc_cu_2808 | conformance_unit | claimed |  | MUC_OPCUA_CU_SECURITY_ROLE_SERVER_AUTHORIZATION | full | test_role_management |
| opc_cu_2823 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_2936 | conformance_unit | claimed | OPC-10000-4 §5.11.4 | MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP | full | test_write_service, test_write_response |
| opc_cu_3072 | conformance_unit | claimed |  | — | all | test_profile_surface |
| opc_cu_3073 | conformance_unit | claimed |  | — | all | test_profile_surface |
| opc_cu_3125 | conformance_unit | claimed |  | — | standard, full | test_profile_surface |
| opc_cu_3143 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3147 | conformance_unit | claimed | OPC-10000-4 §5.11.4 | MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE | full | test_write_service |
| opc_cu_3175 | conformance_unit | claimed |  | — | all | test_profile_surface |
| opc_cu_3192 | conformance_unit | claimed | OPC-10000-5 §6.3.1, 6.3.3, 8.3.2, 12.9 | MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS | full | test_diagnostics, test_profile_surface |
| opc_cu_3530 | conformance_unit | claimed | OPC-10000-4 §5.9.2, 5.9.3 | MUC_OPCUA_CU_VIEW_BASIC_2 | all | test_browse_service, test_browse_limits, test_view_services |
| opc_cu_3534 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3535 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3536 | conformance_unit | claimed |  | — | embedded, standard, full | test_profile_surface |
| opc_cu_3645 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3727 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3802 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3913 | conformance_unit | claimed |  | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3983 | conformance_unit | claimed | OPC-10000-4 §7.32, 7.38 | MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS | full | test_service_header |
| opc_cu_3985 | conformance_unit | claimed |  | — | all | test_profile_surface |
| opc_cu_5505 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5793 | conformance_unit | claimed |  | — | all | test_profile_surface |
| opc_cu_protocol_ua_tcp | conformance_unit | claimed | OPC-10000-6 §7.1 | MUC_OPCUA_CU_PROTOCOL_UA_TCP | all | test_tcp_connection |
| opc_cu_ua_binary_encoding | conformance_unit | claimed | OPC-10000-6 §5 | MUC_OPCUA_CU_UA_BINARY_ENCODING | all | test_binary_primitives, test_binary_nodeid |
| opc_cu_ua_secure_conversation | conformance_unit | claimed | OPC-10000-6 §6 | MUC_OPCUA_CU_UA_SECURE_CONVERSATION | all | test_secure_channel |
| opc_cu_address_space_base | conformance_unit | claimed | OPC-10000-3 §4 | MUC_OPCUA_CU_ADDRESS_SPACE_BASE | all | test_address_space_validation, test_base_server_behaviour |
| opc_cu_session_base | conformance_unit | claimed | OPC-10000-4 §5.6 | MUC_OPCUA_CU_SESSION_BASE | all | test_session, test_session_auth |
| opc_cu_core_structure_2 | conformance_unit | claimed | OPC-10000-3 §4 | MUC_OPCUA_CU_CORE_STRUCTURE_2 | all | test_base_server_behaviour |
| opc_cu_core_views_folder | conformance_unit | claimed | OPC-10000-3 §4 | MUC_OPCUA_CU_CORE_VIEWS_FOLDER | full | test_browse_service, test_view_services |
| opc_cu_server_capabilities_2 | conformance_unit | claimed | OPC-10000-3 §4 | MUC_OPCUA_CU_SERVER_CAPABILITIES_2 | all | test_base_server_behaviour |
| opc_cu_session_general_service | conformance_unit | claimed | OPC-10000-4 §5.6 | MUC_OPCUA_CU_SESSION_GENERAL_SERVICE | all | test_dispatch_session_order, test_base_server_behaviour |
| opc_cu_namespace_metadata | conformance_unit | claimed | OPC-10000-3 §4 | MUC_OPCUA_CU_NAMESPACE_METADATA | full | test_base_server_behaviour, test_read_browsename_namespace |
| opc_cu_2318 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2515 | conformance_unit | claimed |  | — | full | test_claim_map |
| opc_cu_3150 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_4030 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2380 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2394 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2939 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3153 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3194 | conformance_unit | claimed |  | — | full | test_claim_map, test_read_service |
| opc_cu_2422 | conformance_unit | claimed |  | — | full | test_claim_map, test_event_notifications |
| opc_cu_3968 | conformance_unit | claimed |  | — | full | test_event_notifications |
| opc_cu_3228 | conformance_unit | claimed |  | — | full | test_claim_map, test_event_notifications |
| opc_cu_3224 | conformance_unit | claimed | OPC-10000-5 | — | full | test_claim_map |
| opc_cu_3230 | conformance_unit | claimed | OPC-10000-5 | — | full | test_claim_map |
| opc_cu_3763 | conformance_unit | claimed | OPC-10000-9 §5.10 | — | full | test_claim_map |
| opc_cu_3764 | conformance_unit | claimed | OPC-10000-9 §5.10.5 | — | full | test_claim_map |
| opc_cu_3766 | conformance_unit | claimed | OPC-10000-9 §5.10.7 | — | full | test_claim_map |
| opc_cu_3767 | conformance_unit | claimed | OPC-10000-9 §5.10.8 | — | full | test_history |
| opc_cu_3768 | conformance_unit | claimed | OPC-10000-9 §5.10.9 | — | full | test_history |
| opc_cu_2190 | conformance_unit | claimed | OPC-10000-4 §5.6.5 | MUC_OPCUA_CU_SESSION_CANCEL | standard, full | test_claim_map |
| opc_cu_2271 | conformance_unit | claimed | OPC-10000-4 §5.4.5 | MUC_OPCUA_CU_DISCOVERY_REGISTER | standard, full | test_claim_map |
| opc_cu_2863 | conformance_unit | claimed | OPC-10000-7 §6.5 | — | embedded, standard, full | test_profile_surface |
| opc_cu_3170 | conformance_unit | claimed | OPC-10000-4 §5.4.6 | — | standard, full | test_profile_surface |
| opc_cu_3721 | conformance_unit | claimed | OPC-10000-7 §6.5 | — | full | test_profile_surface |
| opc_cu_3923 | conformance_unit | claimed | OPC-10000-4 §5.6 | — | micro, embedded, standard, full | test_profile_surface |
| opc_cu_3080 | conformance_unit | claimed |  | OPC_CU_3080 | all | test_certificate_validity, test_server_config |
| opc_cu_3201 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5592 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS | full | test_profile_surface |
| opc_cu_5814 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_key_credential_service | conformance_unit | claimed | OPC-10000-12 §8.5-8.6 | MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE | full | test_key_credential |
| opc_cu_user_role_management | conformance_unit | claimed | OPC-10000-12 §9.5-9.6 | MUC_OPCUA_CU_USER_ROLE_MANAGEMENT | full | test_role_management |
| opc_cu_certificate_management | conformance_unit | claimed | OPC-10000-12 §7.5-7.6 | MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT | full | test_certificate_management |
| opc_cu_2921 | conformance_unit | claimed | OPC-10000-9 §5.10 | — | full | test_profile_surface |
| opc_cu_2927 | conformance_unit | claimed | OPC-10000-9 §5.7 | — | full | test_profile_surface |
| opc_cu_2189 | conformance_unit | claimed | OPC-10000-9 §5.9 | — | full | test_profile_surface |
| opc_cu_2726 | conformance_unit | claimed | OPC-10000-9 §5.10.3 | — | full | test_profile_surface |
| opc_cu_2852 | conformance_unit | claimed | OPC-10000-9 §5.9.11 | — | full | test_profile_surface |
| opc_cu_2879 | conformance_unit | claimed | OPC-10000-9 §5.10.4 | — | full | test_profile_surface |
| opc_cu_alarms_conditions | optimization | claimed | OPC-10000-9 §5 | MUC_OPCUA_CU_ALARMS_CONDITIONS | full | test_alarms_conditions |
| opc_cu_2361 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2399 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2426 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2474 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2772 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2776 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2831 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2984 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2988 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3112 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3323 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3324 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3325 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3326 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3327 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3328 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3565 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3566 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3567 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3568 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3569 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3786 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2489 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2649 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2747 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2813 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2814 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2822 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2978 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3199 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3206 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3210 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3211 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3546 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3549 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3810 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3811 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3812 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3813 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_4427 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5578 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2375 | conformance_unit | claimed | OPC-10000-13 §5.4.3.5 | — | full | test_profile_surface |
| opc_cu_2958 | conformance_unit | claimed | OPC-10000-13 §5.4.3.21 | — | full | test_profile_surface |
| opc_cu_2256 | conformance_unit | claimed | OPC-10000-13 §5.4.3.27 | — | full | test_profile_surface |
| opc_cu_2194 | conformance_unit | claimed | OPC-10000-13 §5.4.3.30 | — | full | test_profile_surface |
| opc_cu_2954 | conformance_unit | claimed | OPC-10000-13 §5.4.3.32 | — | full | test_profile_surface |
| opc_cu_3105 | conformance_unit | claimed | OPC-10000-13 §5.4.3.31 | — | full | test_profile_surface |
| opc_cu_2998 | conformance_unit | claimed | OPC-10000-13 §5.4.3.22 | — | full | test_profile_surface |
| opc_cu_2743 | conformance_unit | claimed | OPC-10000-13 §5.4.3.26 | — | full | test_profile_surface |
| opc_cu_2754 | conformance_unit | claimed | OPC-10000-13 §5.4.3.4 | — | full | test_profile_surface |
| opc_cu_2381 | conformance_unit | claimed | OPC-10000-13 §5.4.3.11 | — | full | test_profile_surface |
| opc_cu_2166 | conformance_unit | claimed | OPC-10000-13 §5.4.3.16 | — | full | test_profile_surface |
| opc_cu_2376 | conformance_unit | claimed | OPC-10000-13 §5.4.3.10 | — | full | test_profile_surface |
| opc_cu_2302 | conformance_unit | claimed | OPC-10000-13 §5.4.3.15 | — | full | test_profile_surface |
| opc_cu_3010 | conformance_unit | claimed | OPC-10000-13 §5.4.3.34 | — | full | test_profile_surface |
| opc_cu_3048 | conformance_unit | claimed | OPC-10000-13 §5.4.3.33 | — | full | test_profile_surface |
| opc_cu_2377 | conformance_unit | claimed | OPC-10000-13 §5.4.3.14 | — | full | test_profile_surface |
| opc_cu_3108 | conformance_unit | claimed | OPC-10000-13 §5.4.3.25 | — | full | test_profile_surface |
| opc_cu_3075 | conformance_unit | claimed | OPC-10000-13 §5.4.3.6 | — | full | test_profile_surface |
| opc_cu_3126 | conformance_unit | claimed | OPC-10000-13 §5.4.3.7 | — | full | test_profile_surface |
| opc_cu_3062 | conformance_unit | claimed | OPC-10000-13 §5.4.3.8 | — | full | test_profile_surface |
| opc_cu_2184 | conformance_unit | claimed | OPC-10000-13 §5.4.3.9 | — | full | test_profile_surface |
| opc_cu_2201 | conformance_unit | claimed | OPC-10000-13 §5.4.3.35 | — | full | test_profile_surface |
| opc_cu_2408 | conformance_unit | claimed | OPC-10000-13 §5.4.3.36 | — | full | test_profile_surface |
| opc_cu_2974 | conformance_unit | claimed | OPC-10000-13 §5.4.3.12 | — | full | test_profile_surface |
| opc_cu_3130 | conformance_unit | claimed | OPC-10000-13 §5.4.3.13 | — | full | test_profile_surface |
| opc_cu_2952 | conformance_unit | claimed | OPC-10000-13 §5.4.3.17 | — | full | test_profile_surface |
| opc_cu_2941 | conformance_unit | claimed | OPC-10000-13 §5.4.3.18 | — | full | test_profile_surface |
| opc_cu_3047 | conformance_unit | claimed | OPC-10000-13 §5.4.3.19 | — | full | test_profile_surface |
| opc_cu_3144 | conformance_unit | claimed | OPC-10000-13 §5.4.3.23 | — | full | test_profile_surface |
| opc_cu_3099 | conformance_unit | claimed | OPC-10000-13 §5.4.3.24 | — | full | test_profile_surface |
| opc_cu_2330 | conformance_unit | claimed | OPC-10000-13 §5.4.3.28 | — | full | test_profile_surface |
| opc_cu_2207 | conformance_unit | claimed | OPC-10000-13 §5.4.3.29 | — | full | test_profile_surface |
| opc_cu_2358 | conformance_unit | claimed | OPC-10000-13 §5.4.3.37 | — | full | test_profile_surface |
| opc_cu_2281 | conformance_unit | claimed | OPC-10000-13 §5.4.3.38 | — | full | test_profile_surface |
| opc_cu_2955 | conformance_unit | claimed | OPC-10000-13 §5.4.3.39 | — | full | test_profile_surface |
| opc_cu_2178 | conformance_unit | claimed | OPC-10000-13 §5.4.3.40 | — | full | test_profile_surface |
| opc_cu_5941 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_5940 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_5937 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_5875 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_STATE_MACHINE_DESC_NODEID_DATATYPE | full | test_profile_surface |
| opc_cu_5874 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_5873 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_5869 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_5813 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5812 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5810 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5809 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5808 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5807 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5806 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5797 | conformance_unit | claimed |  | — | — | test_base_server_behaviour |
| opc_cu_5796 | conformance_unit | claimed |  | — | — | test_base_server_behaviour |
| opc_cu_5795 | conformance_unit | claimed |  | — | full | test_base_server_behaviour |
| opc_cu_5791 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_TEMPORARY_FILE_TRANSFER_TYPE_BASE | full | test_profile_surface |
| opc_cu_5776 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5775 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5664 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5663 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5662 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5661 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5660 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5659 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5658 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5656 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5655 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5654 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5653 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5652 | conformance_unit | claimed |  | — | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| opc_cu_5567 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5566 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5565 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5564 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5563 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5562 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5561 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5560 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5559 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5558 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5557 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5556 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5555 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5554 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5553 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5552 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5551 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5550 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5549 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5548 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5547 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5546 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5545 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5544 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5543 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5542 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5541 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5540 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5539 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5538 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5537 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5536 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5535 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5534 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5533 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5532 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5531 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5530 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5529 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5528 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5527 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5526 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5525 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5524 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5523 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5522 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5521 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5520 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5519 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5518 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5517 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5516 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5515 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5514 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5513 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5512 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5511 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5510 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_5303 | conformance_unit | claimed |  | — | full | test_certificate_management |
| opc_cu_5302 | conformance_unit | claimed |  | — | full | test_key_credential |
| opc_cu_5301 | conformance_unit | claimed |  | — | full | test_key_credential |
| opc_cu_5293 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5292 | conformance_unit | claimed |  | — | full | test_key_credential |
| opc_cu_5277 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5276 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5275 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5274 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_5250 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_5249 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_5248 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_5242 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_5213 | conformance_unit | claimed |  | — | full | test_audit_events, test_event_notifications |
| opc_cu_4957 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_4505 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_4503 | conformance_unit | claimed |  | — | full | test_scheduler_stub |
| opc_cu_4502 | conformance_unit | claimed |  | — | full | test_scheduler_stub |
| opc_cu_4501 | conformance_unit | claimed |  | — | full | test_scheduler_stub |
| opc_cu_4500 | conformance_unit | claimed |  | — | full | test_scheduler_stub |
| opc_cu_4467 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_4466 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_4465 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_4464 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_4463 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_4428 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3994 | conformance_unit | claimed |  | — | full | test_dispatch_session_order, test_base_server_behaviour |
| opc_cu_3979 | conformance_unit | claimed |  | — | full | test_claim_map |
| opc_cu_3969 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_MODEL_CHANGE | full | test_profile_surface |
| opc_cu_3965 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_USER_ACCESS_LEVEL_BASE | full | test_profile_surface |
| opc_cu_3941 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_DATATYPEDEFINITION_ATTRIBUTE | full | test_profile_surface |
| opc_cu_3928 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3820 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3779 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3778 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3777 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3776 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3775 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3774 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3773 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3772 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3771 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3770 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3765 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3762 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3761 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3760 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3642 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_3605 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_METHOD_CAPABILITIES | full | test_profile_surface |
| opc_cu_3586 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3584 | conformance_unit | claimed |  | — | full | test_certificate_management |
| opc_cu_3582 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3581 | conformance_unit | claimed |  | — | full | test_certificate_management |
| opc_cu_3577 | conformance_unit | documented |  | — | — | — |
| opc_cu_3576 | conformance_unit | claimed |  | — | full | test_history |
| opc_cu_3574 | conformance_unit | claimed |  | — | full | test_history |
| opc_cu_3572 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3571 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3562 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_METHOD_META_DATA | full | test_profile_surface |
| opc_cu_3542 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3541 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3540 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3539 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3538 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3525 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_DICTIONARY_URI | full | test_profile_surface |
| opc_cu_3524 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_DICTIONARY_IRDI | full | test_profile_surface |
| opc_cu_3226 | conformance_unit | claimed |  | — | full | test_claim_map |
| opc_cu_3213 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_FILE_TYPE_BASE | full | test_profile_surface |
| opc_cu_3203 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_MODEL_CHANGE_GENERAL | full | test_profile_surface |
| opc_cu_3197 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_SECURITY_ROLE_CAPABILITIES | full | test_profile_surface |
| opc_cu_3182 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3171 | conformance_unit | claimed |  | — | full | test_claim_map |
| opc_cu_3165 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3162 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3159 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3142 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_3137 | conformance_unit | documented |  | — | — | — |
| opc_cu_3121 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_3107 | conformance_unit | claimed |  | — | — | test_base_server_behaviour |
| opc_cu_3101 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3098 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3085 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3084 | conformance_unit | claimed |  | — | — | test_base_server_behaviour |
| opc_cu_3083 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3081 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3064 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_NOTIFIER_HIERARCHY | full | test_profile_surface |
| opc_cu_3061 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3060 | conformance_unit | claimed |  | — | — | test_base_server_behaviour |
| opc_cu_3055 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3053 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3049 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3043 | conformance_unit | claimed |  | — | full | test_history |
| opc_cu_3032 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3027 | conformance_unit | claimed |  | — | full | test_transfer_subscriptions |
| opc_cu_3026 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_USERWRITEMASK_MULTILEVEL | full | test_profile_surface |
| opc_cu_3020 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3018 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3015 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_3011 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3006 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_3004 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3001 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_3000 | conformance_unit | claimed |  | — | — | test_base_server_behaviour |
| opc_cu_2996 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2993 | conformance_unit | documented |  | — | — | — |
| opc_cu_2991 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2985 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2975 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2965 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2962 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2960 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2957 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2951 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2950 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2948 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2947 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2946 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2943 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2937 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2929 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2918 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_SOURCE_HIERARCHY | full | test_profile_surface |
| opc_cu_2902 | conformance_unit | claimed |  | — | full | test_user_auth_plaintext, test_user_auth_certificate, test_user_auth_secure_e2e |
| opc_cu_2897 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2896 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2893 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2881 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2877 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2873 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2871 | conformance_unit | claimed |  | — | full | test_claim_map |
| opc_cu_2867 | conformance_unit | claimed |  | — | full | test_secure_channel |
| opc_cu_2861 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2845 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_REQUEST_SERVER_STATE_CHANGE_METHOD | full | test_profile_surface |
| opc_cu_2818 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_2817 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2811 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_STATE_MACHINE_INSTANCE | full | test_profile_surface |
| opc_cu_2806 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2802 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2785 | conformance_unit | claimed |  | — | full | test_secure_channel |
| opc_cu_2781 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_WRITEMASK | full | test_profile_surface |
| opc_cu_2777 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_2759 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2746 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2740 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2730 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2709 | conformance_unit | claimed |  | — | full | test_secure_channel |
| opc_cu_2705 | conformance_unit | claimed |  | — | full | test_certificate_management |
| opc_cu_2664 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2629 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_2539 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_DICTIONARY_ENTRIES | full | test_profile_surface |
| opc_cu_2527 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_HISTORY_READ_EVENTS_CAPABILITIES | full | test_profile_surface |
| opc_cu_2526 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_HISTORY_READ_DATA_CAPABILITIES | full | test_profile_surface |
| opc_cu_2488 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_HISTORY_UPDATE_DATA_CAPABILITIES | full | test_profile_surface |
| opc_cu_2487 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_HISTORY_UPDATE_EVENTS_CAPABILITIES | full | test_profile_surface |
| opc_cu_2486 | conformance_unit | claimed |  | MUC_OPCUA_CU_BASE_INFO_HISTORY_READ_CAPABILITIES | full | test_profile_surface |
| opc_cu_2454 | conformance_unit | claimed |  | — | full | test_method_call_arbitrary, test_method_call_errors |
| opc_cu_2453 | conformance_unit | claimed |  | — | full | test_certificate_management |
| opc_cu_2450 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_2449 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_2448 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_2391 | conformance_unit | claimed |  | — | full | test_method_call_arbitrary, test_method_call_errors |
| opc_cu_2390 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2384 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2383 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2382 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2379 | conformance_unit | claimed |  | — | full | test_aliasname_stub |
| opc_cu_2362 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_METHOD | full | test_profile_surface |
| opc_cu_2354 | conformance_unit | claimed |  | — | full | test_claim_map |
| opc_cu_2353 | conformance_unit | claimed |  | — | full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| opc_cu_2350 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2346 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2345 | conformance_unit | claimed |  | — | full | test_read_service |
| opc_cu_2343 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2339 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2338 | conformance_unit | documented |  | — | — | — |
| opc_cu_2335 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2333 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2323 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2319 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2315 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2314 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2309 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2305 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2303 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2291 | conformance_unit | claimed |  | — | full | test_read_service |
| opc_cu_2289 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2282 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2276 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2275 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2273 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2267 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2263 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2258 | conformance_unit | claimed |  | — | full | test_transfer_subscriptions |
| opc_cu_2239 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2236 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2233 | conformance_unit | claimed |  | — | full | test_certificate_management |
| opc_cu_2232 | conformance_unit | claimed |  | — | full | test_certificate_management |
| opc_cu_2224 | conformance_unit | claimed |  | — | full | test_profile_surface |
| opc_cu_2223 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2220 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2210 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2203 | conformance_unit | claimed |  | — | full | test_read_service |
| opc_cu_2202 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2188 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2180 | conformance_unit | claimed |  | — | full | test_alarms_conditions |
| opc_cu_2175 | conformance_unit | claimed |  | — | — | test_history |
| opc_cu_2165 | conformance_unit | documented |  | — | — | — |
| opc_cu_2163 | conformance_unit | claimed |  | MUC_OPCUA_CU_ADDRESS_SPACE_USERWRITEMASK | full | test_profile_surface |
| mdns_discovery | optimization | implemented | OPC-10000-12 §Annex A | MUC_OPCUA_MDNS_DISCOVERY | full | — |
| cu_user_token_jwt | optimization | implemented | OPC-10000-7 §CU 1697 User Token JWT Server Facet | MUC_OPCUA_CU_USER_TOKEN_JWT | full | test_jwt_activate_session |
| cu_certificate_manager_pull | optimization | implemented | OPC-10000-12 §7.6, 7.9 | MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL | full | test_certificate_manager |
| cu_authorization_service_server | optimization | implemented | OPC-10000-7 §CU 1629 Authorization Service Server Facet | MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER | — | — |
| opc_cu_aliasname | optimization | claimed | OPC-10000-7 | MUC_OPCUA_CU_ALIASNAME | — | test_aliasname_stub |
| opc_cu_scheduler | optimization | claimed | OPC-10000-7 | MUC_OPCUA_CU_SCHEDULER | — | test_scheduler_stub |

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
| max_address_space_nodes | invariant | MU_MAX_ADDRESS_SPACE_NODES | 64 | 64 | 512 | 512 | 512 |
| max_dynamic_nodes | invariant | MU_MAX_DYNAMIC_NODES | 32 | 32 | 32 | 32 | 32 |
| max_dynamic_references | invariant | MU_MAX_DYNAMIC_REFERENCES | 64 | 64 | 64 | 64 | 64 |
| max_dynamic_browse_name_length | invariant | MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH | 64 | 64 | 64 | 64 | 64 |
| max_dynamic_display_name_length | invariant | MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH | 64 | 64 | 64 | 64 | 64 |
| max_dynamic_string_nodeid_length | invariant | MU_MAX_DYNAMIC_STRING_NODEID_LENGTH | 64 | 64 | 64 | 64 | 64 |
| max_query_continuation_points | invariant | MU_MAX_QUERY_CONTINUATION_POINTS | 2 | 2 | 2 | 2 | 2 |
| max_conditions | invariant | MU_MAX_CONDITIONS | 10 | 10 | 10 | 10 | 10 |
| max_secure_channels | derived | MU_MAX_SECURE_CHANNELS | 1 | 2 | 4 | 50 | 100 |
| max_dynamic_reference_string_nodeid_length | derived | MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH | 64 | 64 | 64 | 64 | 64 |
