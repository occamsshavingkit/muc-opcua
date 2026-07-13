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
| Core 2022 Server Facet | OPC-10000-4 §7.x | full | test_audit_events |
| Core 2022 Server Facet | OPC-10000-3 | full | test_address_space_dynamic |
| Core 2022 Server Facet | OPC-10000-6 | full | test_message_chunk_errors |
| Core 2022 Server Facet | OPC-10000-4 | micro, embedded, standard, full | test_base_server_behaviour |
| Core 2022 Server Facet | OPC-10000-4 §7.28 | full | test_time_sync, test_dispatch_services |
| Core 2022 Server Facet | OPC-10000-3 | full | test_address_space_string_limits, test_binary_nodeid_errors |
| Core 2022 Server Facet | OPC-10000-13 | full | test_aggregate, test_aggregate_full |
| Core 2022 Server Facet | OPC-10000-14 | full | test_uadp_encoding, test_pubsub |
| Core 2022 Server Facet | OPC-10000-6 §7.1.3 | full | test_reverse_connect |
| Core 2022 Server Facet | OPC-10000-4 | full | test_read_browsename_namespace |
| Core 2017 Server Facet: Attribute Read | OPC-10000-4 §5.10.2 | all | test_read_service |
| Core 2017: View Basic / TranslateBrowsePath | OPC-10000-4 §5.8 | all | test_browse_service, test_browse_limits, test_view_services |
| Core 2017: Discovery Find Servers Self / Get Endpoints | OPC-10000-4 §5.4 | all | test_discovery_endpoint |
| Core 2017: View RegisterNodes | OPC-10000-4 §5.9 | all | test_view_services, test_profile_surface |
| Core 2017 Attribute Write | OPC-10000-4 §5.10.4 | full | test_write_service |
| Historical Access Server Facet | OPC-10000-11 | full | test_history |
| Query | OPC-10000-4 §5.9 | full | test_query_service |
| NodeManagement | OPC-10000-4 §5.7 | full | test_node_management, test_node_management_errors |
| Protocol UA TCP | OPC-10000-6 §7.1 | all | test_tcp_connection |
| UA Binary Encoding | OPC-10000-6 §5 | all | test_binary_primitives, test_binary_nodeid |
| UA Secure Conversation | OPC-10000-6 §6 | all | test_secure_channel |
| Address Space Base | OPC-10000-3 §4 | all | test_address_space_validation, test_base_server_behaviour |
| Session Base | OPC-10000-4 §5.6 | all | test_session, test_session_auth |
| Base Info Core Structure 2 | OPC-10000-3 §4 | all | test_base_server_behaviour |
| Base Info Core Views Folder | OPC-10000-3 §4 | all | test_browse_service, test_view_services |
| Base Info Server Capabilities 2 | OPC-10000-3 §4 | all | test_base_server_behaviour |
| Session General Service Behaviour | OPC-10000-4 §5.6 | all | test_dispatch_session_order, test_base_server_behaviour |
| Base Info Namespace Metadata | OPC-10000-3 §4 | all | test_base_server_behaviour, test_read_browsename_namespace |
