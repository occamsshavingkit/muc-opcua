# Claim → Test Map (feature 028)

Machine-checked mapping from each claimed OPC UA conformance unit / behavior to the
test(s) that verify it, and the profiles those tests must run in. Enforced by
`check_claim_map.py` (registered as the `test_claim_map` ctest in every profile
build): for the profile a build targets, every row listing that profile MUST have a
backing test that is registered in that build. A claimed unit whose backing test is
absent from its profile fails the build. This replaces markdown substring matching
with an enforced "claimed unit ⇒ profile-runnable test" link (OPC-10000-7 §4.2/§4.3).

Profiles column: comma-separated subset of `nano,micro,embedded,full`, or `all`.
Backing test column: comma-separated ctest names (as registered), no `test_` typos.

| Claim / conformance unit | OPC UA § | Profiles | Backing test |
|--------------------------|----------|----------|--------------|
| Attribute Read service | OPC-10000-4 §5.11.2 | all | test_read_service |
| Read operation limit → Bad_TooManyOperations | OPC-10000-4 §5.11.2 / §7.38.2 | all | test_read_service |
| View Browse service | OPC-10000-4 §5.9.2 | all | test_browse_service |
| View Browse operation limit | OPC-10000-4 §5.9.2 / §7.38.2 | all | test_browse_limits |
| View TranslateBrowsePaths | OPC-10000-4 §5.9.4 | all | test_view_services |
| View RegisterNodes/UnregisterNodes support (full-only; else Bad_ServiceUnsupported) | OPC-10000-4 §5.9.5 / §5.9.6 / §7.38.2 | all | test_view_services, test_profile_surface |
| Base Information node set presence per profile | OPC-10000-5 / OPC-10000-7 Core Server Facet | all | test_profile_surface |
| NodeManagement AddNodes duplicate NodeId → Bad_NodeIdExists | OPC-10000-4 §5.8 / §7.38.2 | full | test_node_management |
| Discovery GetEndpoints / FindServers | OPC-10000-4 §5.5.4.2 | all | test_discovery_endpoint |
| SecureChannel Open / policy rejection | OPC-10000-4 §5.5 / OPC-10000-6 §6.7 | all | test_secure_channel |
| Session Create / Activate / Close | OPC-10000-4 §5.6 / §5.7 | all | test_session |
| CreateSession overflow → Bad_TooManySessions | OPC-10000-4 §5.7.2 / §7.38.2 | all | test_session |
| Session state errors (StatusCodes) | OPC-10000-4 §7.38.2 | all | test_service_state_errors |
| Service dispatch / unsupported service | OPC-10000-4 §7.38.2 | all | test_dispatch_services |
| Binary encoding: built-in primitives | OPC-10000-6 §5.2.2.2 / §5.2.2.3 | all | test_binary_primitives |
| Binary primitive boundary round-trips | OPC-10000-6 §5.2.2.2 / §5.2.2.3 / §5.2.2.5 | all | test_binary_primitives |
| Binary encoding: String / ByteString bounds | OPC-10000-6 §5.2.2.4 | all | test_binary_string_errors |
| Binary encoding: ExtensionObject | OPC-10000-6 §5.2.2.15 | all | test_binary_extension_object_errors |
| Binary encoding: ExpandedNodeId malformed decode | OPC-10000-6 §5.2.2.10 | all | test_binary_nodeid_errors |
| Binary encoding: NodeId Guid/Opaque explicit rejection | OPC-10000-6 §5.2.2.9 | all | test_binary_nodeid_errors |
| Binary encoding: DataValue timestamp/picosecond masks | OPC-10000-6 §5.2.2.17 | all | test_binary_variant_datavalue |
| Binary encoding: QualifiedName / LocalizedText malformed decode | OPC-10000-6 §5.2.2.13 / §5.2.2.14 | all | test_binary_variant_datavalue |
| Transport: MessageChunk framing | OPC-10000-6 §6.7.2 / §7.1.2 | all | test_message_chunk |
| Transport: MessageChunk continuation rejected beyond max_chunk_count=1 | OPC-10000-6 §6.7.2 / §7.1.2.4 | all | test_message_chunk_errors |
| Transport: UA-TCP HELLO/ACK | OPC-10000-6 §7.1.2.3 | all | test_tcp_connection |
| Transport: HEL EndpointUrl over-length → Bad_TcpEndpointUrlInvalid | OPC-10000-6 §7.1.2.3 | all | test_tcp_connection |
| Transport: CloseSecureChannel CLO closes channel | OPC-10000-4 §5.6.3 / OPC-10000-6 §6.7.3 | all | test_server_handshake |
| Data-change Subscriptions / MonitoredItems | OPC-10000-4 §5.13 / §5.14 | micro,embedded,full | test_subscriptions |
| Enhanced DataChange Subscription 2017 facet minima (queue depth ≥5, items ≥500, subs ≥5, publish ≥10) | OPC-10000-7 (EnhancedDataChangeSubscription2017, id 1678) | standard,full | test_subscriptions_capacity |
| Republish unavailable NotificationMessage → Bad_MessageNotAvailable | OPC-10000-4 §5.14.6.3 | micro,embedded,full | test_subscriptions |
| SetMonitoringMode invalid MonitoredItemId | OPC-10000-4 §5.13.4 / §7.38.2 | micro,embedded,full | test_subscriptions |
| SetPublishingMode invalid SubscriptionId | OPC-10000-4 §5.14.4 / §7.38.2 | micro,embedded,full | test_subscriptions |
| Attribute Write service | OPC-10000-4 §5.11.4 | micro,embedded,full | test_write_service |
| UserName identity token (auth) | OPC-10000-4 §5.7.3 / §7.37 | micro,embedded,full | test_user_auth_plaintext |
| SecurityPolicy Basic256Sha256 asym chunk | OPC-10000-6 §6.7 / OPC-10000-7 | embedded,full | test_asym_chunk |
| SecurityPolicy Basic256Sha256 oversized OPN → Bad_RequestTooLarge | OPC-10000-6 §6.7.2 / §7.1.5 | embedded,full | test_asym_chunk |
| SecurityPolicy symmetric chunk (sign/encrypt) | OPC-10000-6 §6.7.5 | embedded,full | test_sym_chunk |
| Certificate validation (parse / key-size / validity) | OPC-10000-4 §5.5 | embedded,full | test_certificate, test_certificate_validity |
| Application authentication TrustList | OPC-10000-4 §5.6.2 | embedded,full | test_security_trustlist |
| X509 user identity token (auth) | OPC-10000-4 §5.7.3 | embedded,full | test_user_auth_certificate |
| Secured handshake end-to-end (ServerSignature/ClientSignature) | OPC-10000-4 §5.6.3 / §5.7.3 | embedded,full | test_server_handshake_secure |
| X509 user token e2e (real signature) | OPC-10000-4 §5.7.3 | embedded,full | test_user_auth_secure_e2e |
| UserName accept+reject over secured channel | OPC-10000-4 §5.7.3 / §7.36 | embedded,full | test_user_auth_secure_e2e |
| Encrypted password + ServerNonce anti-replay | OPC-10000-4 §5.6.3.2 / §7.40.2.2 | embedded,full | test_user_auth_secure_e2e |
| Fail-closed application trust at OPN | OPC-10000-4 §5.5 / §6.1.3 | embedded,full | test_user_auth_secure_e2e |
| ClientSignature algorithm URI must match negotiated policy | OPC-10000-4 §7.37 / OPC-10000-7 SecurityPolicy | embedded,full | test_secure_handshake_modern |
| Aggregate subscriptions | OPC-10000-4 §7.22 / OPC-10000-13 | embedded,full | test_aggregate |
| Alarms & Conditions / Events | OPC-10000-9 | embedded,full | test_alarms_conditions |
| Historical Access (HistoryRead/Update) | OPC-10000-11 | full | test_history |
| HistoryRead unsupported details → Bad_HistoryOperationUnsupported | OPC-10000-4 §5.11.3 / OPC-10000-11 | full | test_history |
| Query services (QueryFirst/QueryNext) | OPC-10000-4 §5.9 | full | test_query_service |
| NodeManagement (AddNodes/DeleteReferences) | OPC-10000-4 §5.8 | full | test_node_management |
| NodeManagement DeleteReferences missing reference → Bad_NotFound | OPC-10000-4 §5.8 / §7.38.2 | full | test_node_management |
| PubSub UADP encoding | OPC-10000-14 | full | test_uadp_encoding |
