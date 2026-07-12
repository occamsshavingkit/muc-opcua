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
| Core 2017 Server Facet: Attribute Read | OPC-10000-4 §5.10.2 | all | test_read_service |
| Core 2017: View Basic / TranslateBrowsePath | OPC-10000-4 §5.8 | all | test_browse_service, test_browse_limits, test_view_services |
| Core 2017: Discovery Find Servers Self / Get Endpoints | OPC-10000-4 §5.4 | all | test_discovery_endpoint |
| Core 2017: View RegisterNodes | OPC-10000-4 §5.9 | all | test_view_services, test_profile_surface |
| Core 2017 Attribute Write | OPC-10000-4 §5.10.4 | full | test_write_service |
| Historical Access Server Facet | OPC-10000-11 | full | test_history |
| Query | OPC-10000-4 §5.9 | full | test_query_service |
| NodeManagement | OPC-10000-4 §5.7 | full | test_node_management, test_node_management_errors |
| Core 2017 Server Facet | OPC-10000-7 §4.2 | all | test_profile_surface |
| Base Info Type System | OPC-10000-5 | embedded, standard, full | test_type_system, test_method_call |
| Extended (string/GUID) NodeIds | OPC-10000-3 | full | test_address_space_string_limits, test_binary_nodeid_errors |
| Namespace metadata | OPC-10000-4 | full | test_read_browsename_namespace |
| Dynamic address space | OPC-10000-3 | full | test_address_space_dynamic |
| Structure/complex type encoding | OPC-10000-6 | full | test_complex_types |
| UserNamePassword (id 1096) / X509 (id 1097) | OPC-10000-4 §7.36 | all | test_user_auth_plaintext, test_user_auth_certificate, test_user_auth_secure_e2e |
| MicroEmbeddedDevice2017 Session Minimum 2 Parallel | OPC-10000-7 | micro, embedded, standard, full | test_connection_multiplex |
| Session idle timeout | OPC-10000-4 | micro, embedded, standard, full | test_base_server_behaviour |
| Security Policy Required + Default AppInstance Certificate | OPC-10000-7 | embedded, standard, full | test_asym_chunk, test_sym_chunk, test_certificate, test_certificate_validity, test_security_trustlist, test_server_handshake_secure, test_secure_handshake_modern |
| Security ECC Policy (curve25519 / nistP256) | OPC-10000-7 | full | test_ecc_crypto |
| Multi-chunk messages / index-range Read | OPC-10000-6 | full | test_message_chunk_errors |
| Security time synchronization (clock-skew validation) | OPC-10000-4 §7.28 | full | test_time_sync, test_dispatch_services |
| Audit events | OPC-10000-4 §7.x | full | test_audit_events |
| EmbeddedDataChangeSubscription | OPC-10000-4 §5.12/5.13 | micro, embedded, standard, full | test_subscriptions, test_subscriptions_errors |
| Standard/Enhanced DataChange Subscription 2017 | OPC-10000-7 | embedded, standard, full | test_subscriptions_capacity, test_subscription_deadband, test_subscription_publish |
| Event notifications & Alarms/Conditions | OPC-10000-9 | full | test_alarms_conditions, test_event_notifier, test_event_serializer |
| Standard Event Subscription facet / ContentFilter | OPC-10000-4 §7.4 | full | test_event_filter_where, test_event_filter_select |
| Aggregate Subscription facet | OPC-10000-13 | full | test_aggregate, test_aggregate_full |
| Client Redundancy (TransferSubscriptions) | OPC-10000-4 §5.14.7 | full | test_transfer_subscriptions |
| Method Server facet (arbitrary Call) | OPC-10000-4 §5.11 | full | test_method_call_arbitrary, test_method_call_errors |
| Custom method registration (alias of Method Server) | OPC-10000-4 §5.11 | full | test_method_call |
| PubSub | OPC-10000-14 | full | test_uadp_encoding, test_pubsub |
| Data Access Server Facet | OPC-10000-8 §5.3 | full | test_analog_item, test_da_type_nodes, test_eu_information |
| Reverse Connect (server-initiated ReverseHello) | OPC-10000-6 §7.1.3 | full | test_reverse_connect |
| ServerDiagnostics object + counters | OPC-10000-5 §6.3 | full | test_diagnostics |
