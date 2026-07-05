# Traceability: Standard 2017 UA Server Profile

**Feature**: 037-standard-server-profile
**Profile URI**: `http://opcfoundation.org/UA-Profile/Server/StandardUA2017`
**OPC Reference**: OPC-10000-7 §6.6, profile ID 1663

## Conformance Unit → Implementation Mapping

| Conformance Unit / Facet | OPC Reference | Implementation File | Test File | Status |
|--------------------------|---------------|---------------------|-----------|--------|
| Data Access Server Facet | OPC-10000-7 §6.6.8 | src/services/deadband.c, src/address_space/base_nodes.c | tests/unit/test_percent_deadband.c, tests/unit/test_analog_item.c | Planned |
| Method Server Facet | OPC-10000-7 §6.6.25 | src/core/dispatch_method.c | tests/unit/test_method_call_arbitrary.c | Planned |
| Standard Event Subscription | OPC-10000-7 §6.6.23 | src/services/event_filter.c | tests/unit/test_event_filter_where.c | Planned |
| Address Space Event Notifier | OPC-10000-7 §6.6.174 | src/services/subscription_publish.c | tests/unit/test_event_notifier.c | Planned |
| Auditing Server Facet | OPC-10000-7 §6.6.57 | src/services/audit_events.c | tests/unit/test_audit_events.c | Planned |
| ComplexType Server Facet | OPC-10000-7 | src/address_space/complex_types.c, src/encoding/binary_complex.c | tests/unit/test_complex_types.c | Planned |
| Base Server Behavior Facet | OPC-10000-7 §6.6.2 | src/services/diagnostics.c, src/address_space/base_nodes.c | tests/unit/test_diagnostics.c | Planned |
| Client Redundancy Facet | OPC-10000-7 §6.6.38 | src/core/dispatch_transfer.c | tests/unit/test_transfer_subscriptions.c | Planned |
| Aggregate Server Facet | OPC-10000-7 §6.6.72 | src/services/subscription_aggregate.c | tests/unit/test_aggregate_full.c | Planned |
| Historical Access Server Facet | OPC-10000-7 §6.6.49 | src/services/history.c | tests/unit/test_history.c | Existing |
| Reverse Connect Facet | OPC-10000-7 §6.6.69.2 | src/services/reverse_connect.c | tests/unit/test_reverse_connect.c | Planned |
| Security Time Sync | OPC-10000-4 §A.2 | src/services/time_sync.c | tests/unit/test_time_sync.c | Planned |

## Spec Compliance

| Requirement | Task | Status |
|-------------|------|--------|
| FR-001 (percent deadband) | T013-T023 | Pending |
| FR-002 (EURange/EngineeringUnits) | T015, T017-T018, T021 | Pending |
| FR-003 (InstrumentRange) | T015, T021 | Pending |
| FR-004 (arbitrary methods) | T024-T031 | Pending |
| FR-005 (EventFilter where-clause) | T032-T042 | Pending |
| FR-006 (BaseEventType fields) | T033, T038 | Pending |
| FR-007 (EventNotifier per-node) | T034, T040 | Pending |
| FR-008 (AuditOpenSecureChannel) | T043, T048, T050 | Pending |
| FR-009 (AuditCreateSession) | T044, T048, T050 | Pending |
| FR-010 (AuditActivateSession) | T044, T048, T050 | Pending |
| FR-011 (AuditWriteUpdate) | T045, T048, T050 | Pending |
| FR-012 (Structure DataTypes) | T054-T065 | Pending |
| FR-013 (Enum DataTypes) | T055, T063 | Pending |
| FR-014 (DiagnosticsSummary) | T066-T077 | Pending |
| FR-015 (SessionsDiagnostics) | T067, T071, T075 | Pending |
| FR-016 (ServerRedundancy) | T008, T068 | Pending |
| FR-017 (Namespaces) | T009, T069 | Pending |
| FR-018 (TransferSubscriptions) | T078, T083 | Pending |
| FR-019 (full aggregates) | T079-T080, T084-T085 | Pending |
| FR-020 (SecurityPolicies) | T098 | Pending |
| FR-021 (Reverse Connect) | T081, T086 | Pending |
| FR-022 (Time Sync) | T082, T087 | Pending |
| FR-023 (108 tests pass) | T090 | Pending |
| FR-024 (CMake gating) | T001-T004 | DONE |
| FR-025 (ASAN/UBSan) | T091 | Pending |
| FR-026 (zero static data) | T093 | Pending |
