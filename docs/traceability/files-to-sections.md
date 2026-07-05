# Traceability: Files to OPC UA Sections

This document maps implementation and test files back to OPC UA normative sections.

| File | Entity/Component | OPC UA Part | Section | Description |
|------|------------------|-------------|---------|-------------|
| `include/muc_opcua/config.h` | Limits | Part 6 | 7.1.2.3, 7.1.2.4 | Compile-time bounds for chunks/messages |
| `include/muc_opcua/features.h` | Profile Configuration | Part 7 | 6.5.* | Feature-gate dependency guards (#error on illegal profile combinations) |
| `include/muc_opcua/types.h` | Built-in Types | Part 6 | 5.2.1, 5.2.2.* | Numeric, String, NodeId, Variant types |
| `include/muc_opcua/status.h` | StatusCodes | OPC-10000-4 / OPC-10000-6 | 7.38.2 / 7.1.5 | Public StatusCode constants and TCP-specific StatusCodes |
| `include/muc_opcua/platform.h` | Platform Adapters | Part 4 / 6 | 5.6.2.2 / 7.2 | Adapter Interfaces |
| `include/muc_opcua/transport.h` | OPC UA TCP Transport | OPC-10000-4 / OPC-10000-6 | 5.6.2.2 / 7.1.2.2 | OPC UA TCP protocol version constant (ServerProtocolVersion / ACK ProtocolVersion) |
| `include/muc_opcua/server.h` | Server API | Part 4 / 6 | 5.6.2.2 / 7.1.2.3 | Config & Lifecycle APIs |
| `src/core/status.c` | StatusCodes | Part 4 / 6 | 7.38.2 / 7.1.5 | Status Helper `mu_status_name` |
| `src/core/server.c` | Server Core | OPC-10000-4 / OPC-10000-6 | 7.38.2 / 5.2.2.9, 6.7.2, 7.1.2.2, 7.2 | TCP stream/chunk handling, request type-id validation, and status/error mapping |
| `src/core/service_dispatch.c` | Service Dispatch | OPC-10000-4 / OPC-10000-6 / OPC-10000-13 | 5.5.2.2, 5.5.4.2, 5.6.2, 5.7.2, 5.7.3, 5.11.4.2, 5.13.2.4, 5.13.3.4, 5.14.5, 7.22.4, 7.38.2 / 5.2.2.15, 5.2.5 / 5.4.3.5, 5.4.3.10, 5.4.3.11 | Service request validation, dispatch, malformed-input rejection, filters, and exact StatusCodes |
| `src/core/dispatch_session.c` | Session Dispatch | OPC-10000-4 | 5.7.2, 5.7.3, 5.7.4 | CreateSession, ActivateSession, and CloseSession handlers extracted from service_dispatch.c |
| `src/core/dispatch_discovery.c` | Discovery Dispatch | OPC-10000-4 | 5.5.2.2, 5.5.4.2 | GetEndpoints and FindServers handlers extracted from service_dispatch.c |
| `src/core/dispatch_attribute.c` | Attribute Dispatch | OPC-10000-4 | 5.11.2, 5.11.4 | Read and Write service handlers extracted from service_dispatch.c |
| `src/core/dispatch_view.c` | View Dispatch | OPC-10000-4 | 5.9.2, 5.9.3, 5.9.4 | Browse, BrowseNext, and TranslateBrowsePathsToNodeIds handlers extracted from service_dispatch.c |
| `src/core/dispatch_subscription.c` | Subscription Dispatch | OPC-10000-4 | 5.14.2, 5.14.3, 5.14.4, 5.14.5, 5.14.6, 5.14.8 | CreateSubscription, ModifySubscription, SetPublishingMode, Publish, Republish, and DeleteSubscriptions handlers extracted from service_dispatch.c |
| `src/core/dispatch_node_mgmt.c` | NodeManagement Dispatch | OPC-10000-4 | 5.8.2, 5.8.3, 5.8.4, 5.8.5 | AddNodes, AddReferences, DeleteNodes, and DeleteReferences handlers extracted from service_dispatch.c |
| `src/core/dispatch_method.c` | Method Dispatch | OPC-10000-4 / OPC-10000-5 | 5.12.2.2 / 9.1, 9.2 | Call handler (GetMonitoredItems, ResendData) extracted from service_dispatch.c |
| `src/core/service_context.h` | Service Context | OPC-10000-4 | — | Lightweight header for service-layer modules; incremental core↔services separation |
| `src/core/message_chunk.c` | MessageChunk | OPC-10000-6 | 6.7.2, 7.1.2.2 | TCP message type, chunk finality, abort chunk, and MessageHeader validation |
| `src/core/tcp_connection.c` | OPC UA TCP | OPC-10000-6 | 7.1.2.3, 7.1.2.4, 7.2 | Hello/Acknowledge parsing, endpoint URL limits, and negotiated buffer bounds |
| `include/muc_opcua/services/node_management.h` | NodeManagement | OPC-10000-4 | 5.8.2, 5.8.3, 5.8.4, 5.8.5 | NodeManagement services interface |
| `src/services/node_management.c` | NodeManagement | OPC-10000-4 | 5.8.2.2, 5.8.3.2, 7.24.2 | AddNodes/AddReferences decoding and bounded ownership of persistent node data |
| `src/services/query.h` | Query | OPC-10000-4 | B.2.3, B.2.4, 7.7.1, 7.9 | Query services interface |
| `src/services/query.c` | Query | OPC-10000-4 | B.2.3, B.2.4, 7.9 | QueryFirst processing and QueryNext continuation-point validation |
| `src/services/discovery.c` | Discovery | OPC-10000-4 | 5.5.2.2, 5.5.4.2, 7.40.2.1 | FindServers/GetEndpoints endpoint metadata and user identity token advertisement |
| `src/services/history.c` | Historical Access | OPC-10000-4 / OPC-10000-6 | 5.11.3.2, 7.9 / 5.2.2.15 | HistoryRead details ExtensionObject bounds and owned continuation-point storage |
| `src/services/session.c` | Session | OPC-10000-4 | 5.7.2.1, 5.7.2.2, 5.7.2.3, 5.7.3, 7.38.2 | Session token/nonce generation, SecureChannel binding, and session StatusCodes |
| `src/services/subscription_monitor.c` | Subscription MonitoredItem sampling + deadband | OPC-10000-4 | 5.13, 7.21, 7.22.2 | Monitored item lifecycle, sampling, deadband, and queue functions |
| `src/services/subscription_publish.c` | Subscription publish cycle + timer | OPC-10000-4 / OPC-10000-6 | 5.13, 5.14, 5.14.5.1, 7.20.1, 7.22.1 | Publish response encoding, notification delivery, and publish timer |
| `src/services/subscription_aggregate.c` | Subscription aggregate + trigger + resend | OPC-10000-4 / OPC-10000-13 | 5.13.5, 5.14, 7.22.4 / 5.4.3.5, 5.4.3.10, 5.4.3.11 | Aggregate, triggered items, resend data, and standard-only features |
| `src/services/event_filter.c` | EventFilter where-clause evaluation | OPC-10000-4 / OPC-10000-5 | 7.22.3 / 6.4.2 | ContentFilter evaluation, select-clause extraction for BaseEventType fields |
| `include/muc_opcua/services/method.h` | Method Server Facet API | OPC-10000-4 | 5.12.2.2 | Custom method callback registration and dispatch interface |
| `include/muc_opcua/services/audit.h` | Audit event type definitions | OPC-10000-5 | 6.5 | Audit event structs for Standard profile |
| `src/services/audit_events.c` | Audit event construction stub | OPC-10000-5 | 6.5 | Audit event delivery infrastructure |
| `src/core/dispatch_transfer.c` | TransferSubscriptions handler | OPC-10000-4 | 5.14.7 | TransferSubscriptions service dispatch |
| `src/encoding/binary_reader.c` | Binary Decoding | OPC-10000-6 | 5.2, 5.2.5 | Primitive bounds, declared-length checks, and array length validation |
| `src/encoding/binary_extension_object.c` | ExtensionObject Encoding | OPC-10000-6 | 5.2.2.15 | ExtensionObject header/body bounds and skip validation |
| `src/encoding/binary_query.c` | Query Encoding | OPC-10000-4 / OPC-10000-6 | B.2.3, 7.7.1 / 5.2.2.15 | QueryFirst ContentFilter and operand ExtensionObject decoding |
| `tests/unit/test_query_encoding.c` | Tests | Part 6 | 5.2 | Query binary encode/decode tests |
| `tests/unit/test_query_service.c` | Tests | OPC-10000-4 | B.2.3, B.2.4, 7.7.1, 7.9 | QueryFirst ContentFilter capacity and QueryNext continuation-point tests |
| `tests/unit/test_node_management.c` | Tests | OPC-10000-4 | 5.8.2, 5.8.3, 5.8.4, 5.8.5 | NodeManagement positive tests |
| `tests/unit/test_node_management_errors.c` | Tests | OPC-10000-4 | 5.8.2.2, 5.8.3.2, 7.24.2 | AddNodes/AddReferences decode and persistent-data lifetime tests |
| `tests/unit/test_address_space_dynamic.c` | Tests | OPC-10000-4 / OPC-10000-5 | 5.8, 5.9.2 / 6.3 | Dynamic Address Space tests |
| `tests/unit/test_binary_array_errors.c` | Tests | OPC-10000-6 | 5.2.5 | Null, empty, positive, and malformed array-length decoder tests |
| `tests/unit/test_binary_extension_object_errors.c` | Tests | OPC-10000-6 | 5.2.2.15 | ExtensionObject declared-body overrun and underrun tests |
| `tests/unit/test_binary_string_errors.c` | Tests | OPC-10000-6 | 5.2 | String and ByteString declared-length boundary tests |
| `tests/unit/test_browse_service.c` | Tests | OPC-10000-4 | 5.9.2.4, 7.38.2 | BrowseDirection validation and exact StatusCode tests |
| `tests/unit/test_conformance_docs.c` | Tests | OPC-10000-4 / OPC-10000-7 / OPC-10000-13 | 5.8, 7.22.4, 7.38.2, B.2.3, B.2.4 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10 | Conformance-claim, StatusCode documentation, stale-number, Query/NodeManagement section, aggregate NodeId, and fuzz-placeholder gate tests |
| `tests/unit/test_discovery_encode.c` | Tests | OPC-10000-4 / OPC-10000-6 | 7.2, 7.14, 7.41 / 5.2 | EndpointDescription and ApplicationDescription binary encoding tests |
| `tests/unit/test_discovery_endpoint.c` | Tests | OPC-10000-4 | 5.5.2.2, 5.5.4.2 | FindServers and GetEndpoints no-session, URL, profile, and locale filter tests |
| `tests/unit/test_discovery_services.c` | Tests | OPC-10000-4 / OPC-10000-6 | 5.5.2.2, 5.5.4.2 / 5.2.5 | Discovery malformed-array and truncated-body tests |
| `tests/unit/test_dispatch_services.c` | Tests | OPC-10000-4 / OPC-10000-6 / OPC-10000-13 | 5.5.2.2, 5.5.4.2, 5.6.2, 5.7.2, 5.7.3, 5.11.2.3, 5.11.4.2, 5.13.2.4, 5.13.3.4, 5.14.5, 7.22.4, 7.38.2 / 5.2.2.15, 5.2.5 / 5.4.3.5, 5.4.3.10, 5.4.3.11 | Service dispatch malformed-input, filter, and exact-status tests |
| `tests/unit/test_history.c` | Tests | OPC-10000-4 / OPC-10000-6 | 5.11.3.2, 7.9 / 5.2.2.15 | HistoryRead details ExtensionObject and continuation-point ownership tests |
| `tests/unit/test_message_chunk_errors.c` | Tests | OPC-10000-6 | 6.7.2, 7.1.2.2 | Invalid MessageType, abort chunk, and non-final chunk rejection tests |
| `tests/unit/test_read_service.c` | Tests | OPC-10000-4 | 5.11.2.3, 7.38.2, 7.39 | Read TimestampsToReturn and exact StatusCode tests |
| `tests/unit/test_secure_channel.c` | Tests | OPC-10000-4 / OPC-10000-6 | 5.6.2, 7.38.2 / 6.7.2, 7.1.2.3, 7.1.2.4 | OpenSecureChannel, entropy-failure, and channel close behavior tests |
| `tests/unit/test_security_identity_errors.c` | Tests | OPC-10000-4 / OPC-10000-6 | 5.5.4.2, 5.7.3, 5.7.3.3, 7.38.2, 7.40.2.1 / 5.2.2.15 | ActivateSession user identity and SecurityPolicy None credential tests |
| `tests/unit/test_service_state_errors.c` | Tests | OPC-10000-4 / OPC-10000-6 | 7.38.2 / 5.2.2.9 | Session-state rejection and malformed service type-id tests |
| `tests/unit/test_session.c` | Tests | OPC-10000-4 | 5.7.2, 5.7.2.1, 5.7.2.2, 5.7.2.3, 7.38.2 | CreateSession truncation, token freshness, channel binding, and nonce tests |
| `tests/unit/test_subscriptions_errors.c` | Tests | OPC-10000-4 / OPC-10000-6 | 5.13.2.4, 5.13.3.4, 5.14.5.1, 7.22.1, 7.22.2, 7.38.2 / 5.2.2.15 | MonitoringFilter error and oversize Publish response tests |
| `tests/unit/test_tcp_connection.c` | Tests | OPC-10000-6 | 7.1.2.3, 7.1.2.4 | HEL/ACK negotiated buffer-size boundary tests |
| `tests/unit/test_traceability_docs.c` | Tests | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 / OPC-10000-14 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.13.2.4, 5.13.3.4, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.2.16, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 / 7.2.4.4.2, 7.2.4.5.2, 7.2.4.5.5 | Files-to-sections, sections-to-files, and feature traceability coverage gate tests |
| `tests/unit/test_write_errors.c` | Tests | OPC-10000-4 | 5.11.4.2, 5.11.4.3, 5.11.4.4, 7.38.2 | Write request parameter and per-operation StatusCode tests |
| `tests/fuzz/fuzz_binary_reader.c` | Fuzz Tests | OPC-10000-6 | 5.2, 5.2.5 | Binary reader fuzz coverage for declared lengths and malformed array counts |
| `tests/fuzz/fuzz_message_chunk.c` | Fuzz Tests | OPC-10000-6 | 6.7.2, 7.1.2.2 | MessageChunk fuzz coverage for message type, size, abort, and continuation chunks |
| `tests/integration/test_secure_handshake_modern.c` | Integration Tests | OPC-10000-4 / OPC-10000-6 | 5.5.4.2, 5.6.2, 5.7.2, 5.7.3, 5.11.2 / 6.7.2, 7.1.2.2, 7.1.2.3, 7.2 | Modern secure-channel handshake, session activation, GetEndpoints, and Read flow tests |
| `tests/integration/test_subscriptions.c` | Integration Tests | OPC-10000-4 | 5.6.2, 5.7.2, 5.7.3, 5.13, 5.14, 7.20.1, 7.22.2 | Subscription, MonitoredItem, Publish, Republish, and session-ownership integration tests |
| `tests/integration/test_user_auth_plaintext.c` | Integration Tests | OPC-10000-4 | 5.5.4.2, 5.7.3, 5.7.3.3, 7.38.2, 7.40.2.1 | SecurityPolicy None anonymous support and username/password rejection tests |
| `tests/benchmark/audit_latency_benchmark.c` | Benchmark Tests | OPC-10000-4 / OPC-10000-6 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.11.2 / 5.2 | Valid minimal discovery/session/read latency scenario |
| `docs/conformance/services.md` | Conformance Documentation | OPC-10000-4 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4.2, 5.6.2, 5.7.2, 5.7.3, 5.8, 5.11.2, 5.13, 5.14, 7.22.4 / 4.2, 4.3 / 5.4.3.5, 5.4.3.10, 5.4.3.11 | Profile-targeting service matrix and scoped AggregateFilter notes; no profile-compliant or CTT-verified claim |
| `docs/conformance/profile-embedded.md` | Conformance Documentation | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.13.2, 5.14.5, 7.22.2, 7.22.4 / 5.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, 7.2 / 4.2, 4.3, 6.6.69 / 5.4.3.5, 5.4.3.10, 5.4.3.11 | Embedded 2017 profile-targeting map for transport, encoding, subscriptions, and scoped aggregate evidence |
| `docs/conformance/security.md` | Conformance Documentation | OPC-10000-4 | 5.5.4.2 | SecurityPolicy None endpoint metadata caveat for anonymous, non-production interoperability |
| `docs/conformance/identity-policy.md` | Conformance Documentation | OPC-10000-4 | 5.5.4.2, 5.7.3, 5.7.3.2, 5.7.3.3, 7.38.2 | User-token policy claims for ActivateSession and username/password rejection over SecurityPolicy None |
| `docs/conformance/status.md` | Conformance Documentation | OPC-10000-4 / OPC-10000-7 | 7.38.2 / 4.2, 4.3 | Audit-hardening StatusCode names and numeric values plus profile-targeting conformance status |
| `docs/traceability/files-to-sections.md` | Traceability Evidence | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.8, 5.10.2, 5.10.3, 5.10.4, 5.11.2, 5.13, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, 7.2 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | File-to-section coverage for changed source, test, documentation, specification, and evidence artifacts |
| `docs/traceability/mcp-query-ledger.md` | Traceability Evidence | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 | 5.5.2.2, 5.5.4.2, 5.6.2, 5.7.2, 5.7.3, 5.9.2.4, 5.11.2.3, 5.11.3.2, 5.11.4.2, 5.13.2.4, 5.13.3.4, 5.14.5, 7.22.1, 7.22.4, 7.38.2, 7.39 / 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4 / 4.2, 4.3 | MCP citation query ledger for exact audit-hardening OPC UA section references |
| `docs/traceability/020-audit-hardening.md` | Traceability Evidence | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.8, 5.11.2, 5.13.2.4, 5.13.3.4, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, 7.2 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | FR-to-OPC-section and FR-to-evidence map for audit-hardening requirements |
| `docs/validation/audit-hardening.md` | Validation Evidence | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.11.2, 5.13.2.4, 5.13.3.4, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, 7.2 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Host, sanitizer, fuzz, embedded, conformance-doc, size, static-analysis, and latency validation ledger |
| `docs/validation/audit-hardening-closure.md` | Validation Evidence | OPC-10000-4 / OPC-10000-7 | 7.38.2 / 4.2, 4.3 | Audit closure skeleton requiring exact per-finding OPC UA citations before any fixed, deferred, or out-of-scope outcome |
| `docs/size/audit-hardening-baseline.md` | Size Evidence | OPC-10000-4 / OPC-10000-6 | 5.6.2, 5.7.2, 5.7.3, 5.13, 5.14, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4 | Pre-change size and stack baseline for audit-hardening protocol surfaces; not post-change pass evidence |
| `docs/validation/artifacts/020-audit-hardening-latency-pre-change.json` | Validation Evidence | OPC-10000-4 / OPC-10000-6 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.11.2 / 5.2 | Pre-change latency baseline data for the valid minimal discovery/session/read scenario |
| `docs/validation/artifacts/020-audit-hardening-latency-pre-change.benchmark.stdout` | Validation Evidence | OPC-10000-4 / OPC-10000-6 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.11.2 / 5.2 | Benchmark stdout transcript for the same pre-change latency baseline scenario |
| `specs/020-audit-hardening/spec.md` | Feature Specification | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.8, 5.11.2, 5.13, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, 7.2 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Audit-hardening requirements, normative scope, conformance honesty, and success criteria |
| `specs/020-audit-hardening/plan.md` | Implementation Plan | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.8, 5.10.2, 5.10.3, 5.10.4, 5.11.2, 5.13, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, 7.2 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Audit-hardening technical plan, size budget, task-order rules, and profile-targeting status |
| `specs/020-audit-hardening/research.md` | Research Decisions | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.4.2, 5.7.2, 5.13.2.4, 5.13.3.4, 7.22.4, 7.38.2 / 5.2.2.15, 5.2.5, 7.1.2.3, 7.1.2.4 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Decision record for malformed arrays, ExtensionObject bounds, exact StatusCodes, security freshness, HEL/ACK, ownership, and scoped aggregate support |
| `specs/020-audit-hardening/data-model.md` | Planning Model | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.8, 5.11.2, 5.13, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Data entities and validation rules for request decoding, state, credentials, persistent data, conformance claims, and closure evidence |
| `specs/020-audit-hardening/quickstart.md` | Validation Instructions | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 | 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.3, 7.1.2.4 / 4.2, 4.3 | Developer validation workflow for malformed-input, exact StatusCode, docs, fuzz, CI, and closure checks |
| `specs/020-audit-hardening/contracts/audit-hardening-contract.md` | Planning Contract | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.7.2, 5.9.2.4, 5.11.2, 5.13.2.4, 5.13.3.4, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Atomic task, ordering, protocol behavior, evidence, and verification contract |
| `specs/020-audit-hardening/checklists/requirements.md` | Spec Checklist | OPC-10000-4 / OPC-10000-7 | 7.38.2 / 4.2, 4.3 | Spec-quality checklist confirming exact OPC UA services, sections, StatusCodes, and audit-closure scope remain covered |
| `specs/020-audit-hardening/tasks.md` | Task Plan | OPC-10000-4 / OPC-10000-6 / OPC-10000-7 / OPC-10000-13 | 5.5.2, 5.5.4, 5.6.2, 5.7.2, 5.7.3, 5.8, 5.10.2, 5.10.3, 5.10.4, 5.11.2, 5.13, 5.14, 7.22.1, 7.22.4, 7.38.2 / 5.2, 5.2.2.15, 5.2.5, 6.7.2, 7.1.2.2, 7.1.2.3, 7.1.2.4, 7.2 / 4.2, 4.3 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Serialized audit-hardening task list with exact OPC UA citations for tests, implementation, documentation, evidence, and verification |
| `src/address_space/base_nodes.h` | Base Information | Part 5 | 6.3, 7 | Standard Server node set interface + resolver |
| `src/address_space/base_nodes.c` | Base Information | Part 5 | 6.3, 7 | Default Server/ServerStatus/ServerCapabilities nodes |
| `src/security/security_policy.h` | SecurityPolicy | Part 7 | 6.x | Policy id + Basic256Sha256 parameter table |
| `src/security/security_policy.c` | SecurityPolicy | Part 7 | 6.x | SecurityPolicyUri parsing |
| `src/security/certificate.h` | Certificates | Part 6 / 4 | 6.7.4 / 7.2 | Thumbprint + peer cert validation interface |
| `src/security/certificate.c` | Certificates | Part 6 / 4 | 6.7.4 / 7.2 | SHA-1 thumbprint, RSA key-size validation |
| `src/security/asym_chunk.h` | Asymmetric Chunk | Part 6 | 6.7.2 | OPN chunk sign/encrypt interface |
| `src/security/asym_chunk.c` | Asymmetric Chunk | Part 6 | 6.7.2 | OPN OAEP encrypt + RSA-SHA256 sign + padding |
| `tests/unit/test_asym_chunk.c` | Tests | Part 6 | 6.7.2 | OPN wrap/unwrap round-trip + tamper tests |
| `src/security/sym_chunk.h` | Symmetric Chunk | Part 6 | 6.7.2, 6.7.5 | MSG chunk sign/encrypt + key-derivation interface |
| `src/security/sym_chunk.c` | Symmetric Chunk | Part 6 | 6.7.2, 6.7.5 | MSG AES-256-CBC + HMAC-SHA256 (Sign / SignAndEncrypt) |
| `tests/unit/test_sym_chunk.c` | Tests | Part 6 | 6.7.2, 6.7.5 | MSG wrap/unwrap round-trip + tamper tests |
| `tests/integration/test_server_handshake_secure.c` | Tests | Part 6 | 6.7.2-6.7.5 | End-to-end Basic256Sha256 SignAndEncrypt handshake |
| `tests/integration/test_write_service.c` | Integration Tests | OPC-10000-4 / OPC-10000-6 | 5.6.2, 5.7.2, 5.7.3, 5.11.4.2, 5.11.4.4 / 7.1.2.2, 7.1.2.3 | End-to-end Write service session flow and per-operation StatusCode tests |
| `tests/unit/test_security_policy.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `tests/unit/test_certificate.c` | Tests | Part 6 / 4 | 6.7.4 / 7.2 | Thumbprint + cert validation tests |
| `src/security/key_derivation.h` | Key Derivation | Part 6 | 6.7.5 | P-SHA256 PRF interface for channel keys |
| `src/security/key_derivation.c` | Key Derivation | Part 6 | 6.7.5 | P-SHA256 (RFC 5246) key material expansion |
| `src/platform/host_crypto_adapter.h` | Crypto Adapter | Part 7 | 6.x | Host crypto backend interface (Basic256Sha256) |
| `src/platform/host_crypto_adapter.c` | Crypto Adapter | Part 7 | 6.x | OpenSSL primitives for SecurityPolicy Basic256Sha256 |
| `tests/unit/test_crypto.c` | Tests | Part 6 / 7 | 6.7.5 / 6.x | Known-answer crypto + P-SHA256 tests |
| `tests/unit/test_status.c` | Tests | OPC-10000-4 / OPC-10000-6 | 7.38.2 / 7.1.5 | Exact StatusCode numeric values and status-name helper tests |
| `tests/unit/test_server_config.c` | Tests | Part 6 | 7.1.2.3, 7.1.2.4 | Test config validation |
| `tests/unit/test_platform_adapter_contract.c` | Tests | Part 6 | 7.2 | Test adapters structure |
| `tests/unit/test_write_decoder.c` | Tests | Part 6 | 5.10.4 | Test Write service request decoder and response encoder |
| `tests/unit/test_connection_multiplex.c` | Tests | OPC-10000-4 | 5.7.2.1, 7.38.2 | Cross-channel ActivateSession and session-bound service rejection tests |
| `fuzz_binary_types.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_string.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_callbacks.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_primitives.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_extension_object.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_service_dispatch.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_nodeid.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_string_limits.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_browse_limits.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_message_chunk.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_variant_datavalue.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_values.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_binary_nodeid_errors.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_no_heap_lifecycle.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_server_address_space_config.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_public_headers.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_address_space_validation.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_minimal_server_flow.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_discovery_endpoint_no_session.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `test_single_client_limit.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `fake_platform.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `fake_platform.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `unity_config.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `mu_arduino_adapter.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `mu_arduino_adapter.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `value_source.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `address_space.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `node_id.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `host_tcp_adapter.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `host_tcp_adapter.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_variant.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_writer.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_le.h` | OPC-10000-6 | 5.2.1 | OPC UA Binary little-endian pack/unpack helpers | shared by binary_reader.c / binary_writer.c |
| `binary_nodeid.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `src/encoding/binary_variant.c` | OPC UA Part 6 | 5.3.13 | Variant encoding | Variant support |
| `src/encoding/variant_type.c` | OPC UA Part 4 | 5.11.4.2 | Variant DataType assignability (subtypes accepted) | Write value-type check |
| `src/encoding/binary_datavalue.c` | OPC UA Part 6 | 5.3.14 | DataValue encoding | DataValue support |
| `src/encoding/uadp_encoder.c` | OPC UA Part 14 / Part 6 | 7.2.4.4.2, 7.2.4.5.2, 7.2.4.5.3, 7.2.4.5.4, 7.2.4.5.5 / 5.2.2.16 | Scoped UADP NetworkMessage, PayloadHeader, DataSet payload sizing, Data Key Frame, and Variant field encode/decode | PubSub UADP |
| `src/core/pubsub.c` | OPC UA Part 14 | 5.4.6.2.2, 7.3.2.1 | Cooperative UADP/UDP publisher timing and UDP send dispatch | PubSub runtime |
| `src/core/server.c` | OPC UA Part 14 | 5.4.6.2.2, 7.3.2.1 | `mu_server_poll()` drives connectionless PubSub publishing independent of TCP Sessions | PubSub runtime |
| `src/platform/host_udp_adapter.c` | OPC UA Part 14 | 7.3.2.1 | Host UDP datagram transport for UADP | PubSub network |
| `include/muc_opcua/pubsub.h` | OPC UA Part 14 / Part 6 | 7.2.4.4.2, 7.2.4.5.2, 7.2.4.5.3, 7.2.4.5.4, 7.2.4.5.5, 7.3.2.1 / 5.2.2.16 | Scoped PubSub Publisher API, decoder API, and caller-owned field/output contract | PubSub network |
| `tests/unit/test_uadp_encoding.c` | OPC UA Part 14 / Part 6 | 7.2.4.4.2, 7.2.4.5.2, 7.2.4.5.3, 7.2.4.5.4, 7.2.4.5.5 / 5.2.2.16 | Byte-level UADP encoder and decoder coverage, including DataSet payload sizing | PubSub UADP tests |
| `tests/unit/test_pubsub.c` | OPC UA Part 14 | 5.4.6.2.2, 7.3.2.1 | Publisher timing, destination address, and connectionless poll coverage | PubSub runtime tests |
| `binary_extension_object.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_datavalue.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_reader.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `binary_string.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `browse.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `session.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `read.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `discovery.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `secure_channel.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `browse.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `services/alarms_conditions.h` | Alarms and Conditions | Part 9 | 5.11 | Advanced alarms and conditions |
| `services/history.h` | Historical Access | Part 4 / 11 | 5.10.3, 5.10.4 | Historical Access structures and types |
| `session.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `read.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `write.h` | Attribute Write | Part 4 | 5.10.4 | Write service interface |
| `write.c` | Attribute Write | Part 4 | 5.10.4 | Write service implementation |

### Implementation (`src/`)

| File | Part | Sections | Notes |
|------|------|----------|-------|
| `core/server.c` | 4 | 5.4, 5.5 | Service dispatch |
| `services/alarms_conditions.c` | 9 | 5.11 | Alarms and Conditions execution |
| `services/history.c` | 4, 11 | 5.10.3, 5.10.4 | HistoryRead, HistoryUpdate parsing and dispatch |
| `services/read.c` | 4 | 5.10.2 | Read service implementation |
| `secure_channel.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `discovery.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_dispatch.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `uasc.c` | UASC MessageChunk framing | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Symmetric response chunk framing |
| `uasc.h` | UASC MessageChunk framing | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Symmetric response chunk framing |
| `test_uasc_framing.c` | Tests | Part 6 | 6.7.2, 6.7.3, 6.7.7 | Test symmetric response framing |
| `service_header.c` | RequestHeader / ResponseHeader | Part 4 | 7.32, 7.33 | Common request/response header codec |
| `service_header.h` | RequestHeader / ResponseHeader | Part 4 | 7.32, 7.33 | Common request/response header types |
| `test_service_header.c` | Tests | Part 4 | 7.32, 7.33 | Test request/response header codec |
| `sequence.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_message.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_message.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `message_chunk.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `sequence.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `tcp_connection.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `server_internal.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `safe_mem.h` | Utility | — | — | Inline bounds-checked memcpy wrappers for static analysis safety |
| `ctz.h` | Utility | — | — | Portable count-trailing-zeros (de Bruijn fallback for non-GCC/Clang) |
| `tcp_connection.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `message_chunk.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `service_dispatch.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `opcua_ids.c` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `opcua_types.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `muc_opcua.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `address_space.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `opcua_ids.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `encoding.h` | Traceability mapped | OPC UA Part 4 / 6 | 5.5.4.2, 5.7, 5.11, 5.13, 5.14 / 5.2, 6.7, 7.2 | Placeholder replaced; see feature-specific rows and tests for exact service/encoding coverage |
| `subscription.c` | Subscription + MonitoredItem service sets (Micro) | Part 4 | 5.13, 5.14 | No-heap data-change subscription engine |
| `subscription.h` | Subscription + MonitoredItem service sets (Micro) | Part 4 | 5.13, 5.14, 7.17, 7.21 | Subscription engine data model + contract |
| `mbedtls_crypto_adapter.c` | Crypto Adapter | Part 7 | 6.x | Mbed TLS primitives for SecurityPolicy Basic256Sha256 |
| `wolfssl_crypto_adapter.c` | Crypto Adapter | Part 7 | 6.x | wolfSSL primitives for SecurityPolicy Basic256Sha256 |
| `test_mbedtls_adapter.c` | Tests | Part 7 | 6.x | Test Mbed TLS crypto adapter |
| `test_wolfssl_adapter.c` | Tests | Part 7 | 6.x | Test wolfSSL crypto adapter |
| `src/security/trustlist.c` | TrustList | Part 4 | 5.6.2 | Application Authentication TrustList |
| `include/muc_opcua/security.h` | TrustList | Part 4 | 5.6.2 | Application Authentication TrustList Header |
| `tests/unit/test_aggregate.c` | Tests | OPC-10000-4 / OPC-10000-6 / OPC-10000-13 | 5.13.2.4, 7.22.4 / 5.2.2.9, 5.2.2.15 / 4.2.2.4, 4.2.2.9, 4.2.2.10, 5.4.3.5, 5.4.3.10, 5.4.3.11 | Unit tests for scoped MonitoredItem AggregateFilter support and unsupported aggregate rejection |
| `specs/018-aggregate-subscriptions/spec.md` | Specification | Part 4 / 13 | 7.22.4 / 4.2.2.4, 4.2.2.9, 4.2.2.10 | Specification for Aggregate Subscriptions |
| `include/muc_opcua/services/alarms_conditions.h` | Alarms and Conditions | Part 9 | 5.5, 5.7, 5.8 | Alarms & Conditions types and API |
| `src/services/alarms_conditions.c` | Alarms and Conditions | Part 9 | 5.5, 5.7, 5.8 | Alarms & Conditions method processing |
| `tests/unit/test_alarms_conditions.c` | Tests | Part 9 | 5.5, 5.7, 5.8 | Test Alarms & Conditions functionality |
| `src/address_space/complex_types.c` | ComplexType Server Facet | OPC-10000-3 | 5.6.4, 5.6.5 | Structure/Enum type registration |
| `src/services/diagnostics.c` | Server Diagnostics | OPC-10000-5 | 6.3.3, 6.3.5 | Diagnostics counter management |
| `src/encoding/binary_complex.c` | ComplexType Binary Encoding | OPC-10000-6 | 5.2.2.9, 5.2.2.12 | Structure/Enum binary encode |
| `include/muc_opcua/services/diagnostics.h` | Diagnostics API | OPC-10000-5 | 6.3 | Server diagnostics counter API |
| `include/muc_opcua/address_space/complex_types.h` | ComplexType API | OPC-10000-3 | 5.6.4, 5.6.5 | Structure/Enum type definitions |
| `tests/unit/test_percent_deadband.c` | Tests | OPC-10000-4 | 7.22.2 | Percent deadband evaluation unit tests |
| `tests/unit/test_analog_item.c` | Tests | OPC-10000-3 | 5.6.2 | AnalogItemType property validation |
| `tests/unit/test_event_filter_where.c` | Tests | OPC-10000-4 | 7.22.3 | EventFilter where-clause test |
| `tests/unit/test_event_filter_select.c` | Tests | OPC-10000-5 | 6.4.2 | Select-clause extraction test |
| `tests/unit/test_event_notifier.c` | Tests | OPC-10000-3 | 5.4.6 | EventNotifier attribute test |
| `tests/unit/test_method_call_arbitrary.c` | Tests | OPC-10000-4 | 5.12.2.2 | Custom method registration test |
| `tests/unit/test_audit_events.c` | Tests | OPC-10000-5 | 6.5 | Audit event types test |
| `tests/unit/test_complex_types.c` | Tests | OPC-10000-3 | 5.6.4, 5.6.5 | Complex type registration test |
| `tests/unit/test_diagnostics.c` | Tests | OPC-10000-5 | 6.3.3, 6.3.5 | Diagnostics counter test |
| `tests/unit/test_transfer_subscriptions.c` | Tests | OPC-10000-4 | 5.14.7 | TransferSubscriptions handler test |
| `tests/unit/test_aggregate_full.c` | Tests | OPC-10000-13 | — | Aggregate function set test |
| `tests/unit/test_reverse_connect.c` | Tests | OPC-10000-6 | 7.5 | Reverse Connect infrastructure test |
| `tests/unit/test_time_sync.c` | Tests | OPC-10000-4 | A.2 | Time Sync infrastructure test |
| `tests/unit/test_service_message.c` | Tests | OPC-10000-4 / OPC-10000-6 | 5.5.4.2, 7.38.2 / 5.2 | Service prefix parse/write round-trip test |
