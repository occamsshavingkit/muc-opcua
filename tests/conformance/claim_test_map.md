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
| Address Space AddIn Reference |  | full | test_profile_surface |
| Address Space AddIn DefaultInstanceBrowsename |  | full | test_profile_surface |
| Base Info LocalTime |  | full | test_profile_surface |
| Base Info Selection List |  | full | test_profile_surface |
| Base Info ValueAsText |  | full | test_profile_surface |
| Base Info OptionSet |  | full | test_profile_surface |
| Base Info Estimated Return Time |  | full | test_profile_surface |
| Address Space Interfaces |  | full | test_profile_surface |
| Base Info Locations Object |  | full | test_profile_surface |
| Base Info Currency |  | full | test_profile_surface |
| Base Info ServerType |  | embedded, standard, full | test_type_system |
| Base Info Type Information |  | embedded, standard, full | test_claim_map |
| Core 2022 Server Facet | OPC-10000-4 §5.12/5.13 | micro, embedded, standard, full | test_subscriptions, test_subscriptions_errors |
| Core 2022 Server Facet | OPC-10000-7 | embedded, standard, full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| Core 2022 Server Facet | OPC-10000-7 | full | test_ecc_crypto |
| Core 2022 Server Facet | OPC-10000-9 | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| Core 2022 Server Facet | OPC-10000-8 §5.3 | full | test_analog_item, test_da_type_nodes, test_eu_information |
| Core 2022 Server Facet | OPC-10000-4 §5.11 | full | test_method_call_arbitrary, test_method_call_errors |
| Core 2022 Server Facet | OPC-10000-4 §7.36 | all | test_user_auth_plaintext, test_user_auth_certificate, test_user_auth_secure_e2e |
| Core 2022 Server Facet | OPC-10000-7 | micro, embedded, standard, full | test_connection_multiplex |
| Core 2022 Server Facet | OPC-10000-4 §7.4 | full | test_event_filter_where, test_event_filter_select |
| Core 2022 Server Facet | OPC-10000-4 §5.14.7 | full | test_transfer_subscriptions |
| Core 2022 Server Facet | OPC-10000-6 | full | test_complex_types |
| Core 2022 Server Facet | OPC-10000-4 §7.x | full | test_audit_events, test_event_notifications |
| Core 2022 Server Facet | OPC-10000-6 | full | test_message_chunk_errors |
| Core 2022 Server Facet | OPC-10000-4 | micro, embedded, standard, full | test_base_server_behaviour |
| Core 2022 Server Facet | OPC-10000-4 §7.28 | all | test_time_sync, test_dispatch_services |
| Core 2022 Server Facet | OPC-10000-3 | full | test_address_space_string_limits, test_binary_nodeid_errors |
| Core 2022 Server Facet | OPC-10000-13 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-14 | full | test_uadp_encoding, test_pubsub |
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
| Query | OPC-10000-4 §5.9 | full | test_query_service |
| NodeManagement | OPC-10000-4 §5.7 | full | test_node_management, test_node_management_errors |
| View TranslateBrowsePath | OPC-10000-4 §5.9.4 | all | test_browse_service, test_view_services |
| Discovery Get Endpoints | OPC-10000-4 §5.5.1, 5.5.4 | all | test_discovery_endpoint, test_discovery_services |
| Session Change User | OPC-10000-4 §5.7.3 | full | test_session, test_session_auth |
| Attribute Write StatusCode & Timestamp | OPC-10000-4 §5.11.4 | full | test_write_service, test_write_response |
| Attribute Write Index | OPC-10000-4 §5.11.4 | full | test_write_service |
| Base Info Diagnostics | OPC-10000-5 §6.3.1, 6.3.3, 8.3.2, 12.9 | full | test_diagnostics, test_profile_surface |
| View Basic 2 | OPC-10000-4 §5.9.2, 5.9.3 | all | test_browse_service, test_browse_limits, test_view_services |
| Base Services Diagnostics | OPC-10000-4 §7.32, 7.38 | full | test_service_header |
| Session General Service Behaviour | OPC-10000-4 §5.6 | all | test_dispatch_session_order, test_base_server_behaviour |
| Discovery Register | OPC-10000-4 §5.4.5 | standard, full | test_claim_map |
| Base Info Engineering Units |  | full | test_profile_surface |
| KeyCredential Service | OPC-10000-12 §8.5-8.6 | full | test_key_credential |
| User Role Management | OPC-10000-12 §9.5-9.6 | full | test_role_management |
| Certificate Management | OPC-10000-12 §7.5-7.6 | full | test_certificate_management |
| Alarms & Conditions | OPC-10000-9 §5 | full | test_alarms_conditions |
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
| Aggregate - StandardDeviationPopulation |  |  | test_history |
| Aggregate - Interpolative |  |  | test_history |
| Aggregate - MaximumActualTime2 |  |  | test_history |
| Aggregate - DurationGood |  |  | test_history |
| Aggregate - End |  |  | test_history |
| Aggregate - WorstQuality |  |  | test_history |
| Aggregate - Total |  |  | test_history |
| Aggregate - MaximumActualTime |  |  | test_history |
| Aggregate - Range |  |  | test_history |
| Aggregate - StandardDeviationSample |  |  | test_history |
| Aggregate - Average |  |  | test_history |
| Aggregate - NumberOfTransitions |  |  | test_history |
| Aggregate - PercentBad |  |  | test_history |
| Aggregate - Maximum |  |  | test_history |
| Aggregate - VarianceSample |  |  | test_history |
| Aggregate - VariancePopulation |  |  | test_history |
| Protocol Reverse Connect Server | OPC-10000-6 §7.1.3 | full | test_reverse_connect |
| Aggregate - MinimumActualTime |  |  | test_history |
| Aggregate - Range2 |  |  | test_history |
| Aggregate - WorstQuality2 |  |  | test_history |
| Aggregate - Minimum2 |  |  | test_history |
| Aggregate - DeltaBounds |  |  | test_history |
| Aggregate - Minimum |  |  | test_history |
| Aggregate - Start |  |  | test_history |
| Aggregate - Delta |  |  | test_history |
| Aggregate - DurationBad |  |  | test_history |
| Aggregate - TimeAverage |  |  | test_history |
| Aggregate - PercentGood |  |  | test_history |
| Aggregate - EndBound |  |  | test_history |
| Aggregate - TimeAverage2 |  |  | test_history |
| Aggregate - StartBound |  |  | test_history |
| Aggregate - Count |  |  | test_history |
| Aggregate - DurationInStateNonZero |  |  | test_history |
| Aggregate - DurationInStateZero |  |  | test_history |
| Aggregate - Total2 |  |  | test_history |
| Aggregate - Maximum2 |  |  | test_history |
| Aggregate - MinimumActualTime2 |  |  | test_history |
| User Token JWT Server Facet | OPC-10000-7 §CU 1697 | full | test_jwt_activate_session |
| Certificate Manager Pull | OPC-10000-12 §7.6, 7.9 | full | test_certificate_manager |
