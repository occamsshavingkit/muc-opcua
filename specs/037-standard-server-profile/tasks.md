# Tasks: Standard 2017 UA Server Profile

**Input**: Design documents from `/specs/037-standard-server-profile/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Tests are mandatory per Constitution IV for all protocol parsing,
serialization, service dispatch, StatusCodes, and wire-visible changes.

**Organization**: Tasks grouped by user story for independent implementation.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story (e.g., US1, US2, US3)
- Include exact file paths
- Include OPC UA part/section references

## Path Conventions

- **Public API**: `include/muc_opcua/`
- **Core protocol**: `src/core/`, `src/encoding/`, `src/services/`
- **Address space**: `src/address_space/`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/fuzz/`
- **Documentation**: `docs/traceability/`

---

## Phase 1: Setup (Profile Infrastructure)

**Purpose**: CMake flags, profile preset, and traceability scaffolding for all 11 new facets

- [x] T001 Add 11 new CMake feature flags in `cmake/MucOpcUaOptions.cmake`: MUC_OPCUA_DATA_ACCESS, MUC_OPCUA_METHOD_SERVER, MUC_OPCUA_EVENT_FILTER_WHERE, MUC_OPCUA_AUDITING, MUC_OPCUA_COMPLEX_TYPES, MUC_OPCUA_SERVER_DIAGNOSTICS (activate existing dead flag), MUC_OPCUA_REDUNDANCY, MUC_OPCUA_AGGREGATE_FULL, MUC_OPCUA_REVERSE_CONNECT, MUC_OPCUA_TIME_SYNC, MUC_OPCUA_NAMESPACES — each in `MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS` list at `CMakeLists.txt:22`
- [x] T002 [P] Create `standard` profile preset in `cmake/profiles/standard.cmake` enabling all 11 flags plus embedded baseline, raising capacity minima (MU_MAX_SESSIONS >= 50, MU_MAX_SUBSCRIPTIONS >= 50, MU_MAX_MONITORED_ITEMS >= 1000) per OPC-10000-7 §6.6 Standard profile
- [x] T003 [P] Wire `standard` profile into `CMakeLists.txt` profile selection and add source-file gating for new `.c` files in `src/CMakeLists.txt` behind each flag
- [x] T004 [P] Add dependency guard checks in `include/muc_opcua/features.h` for new flags (e.g., `MUC_OPCUA_DATA_ACCESS` requires `MUC_OPCUA_BASE_NODES`, `MUC_OPCUA_AUDITING` requires `MUC_OPCUA_EVENTS`)
- [x] T005 [P] Add traceability table skeleton in `docs/traceability/037-standard-server-profile.md` mapping each OPC-10000-7 conformance unit to implementation file and test file
- [x] T006 Update `ServerProfileArray` in `src/address_space/base_nodes.c` to include `http://opcfoundation.org/UA-Profile/Server/StandardUA2017` when `MUC_OPCUA_PROFILE` is `standard` per OPC-10000-7 §4.3

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Address space enumeration and event infrastructure that all user stories depend on

**CRITICAL**: No user story work begins until Phase 2 completes.

- [x] T007 [P] Add `EventNotifier` field (uint8_t) to `mu_node_t` in `include/muc_opcua/address_space.h` and populate it from static node definitions in `src/address_space/base_nodes.c` per OPC-10000-3 §5.4.6
- [x] T008 [P] Add `ServerRedundancy` object (ns=0;i=2296) with `RedundancySupport` (ns=0;i=3709) and `ServiceLevel` (ns=0;i=11311) Variables to static nodes in `src/address_space/base_nodes.c` per OPC-10000-5 §6.3.11
- [x] T009 [P] Add `Namespaces` object (ns=0;i=11715) with `NamespaceMetadataType` instances to static nodes in `src/address_space/base_nodes.c` per OPC-10000-5 §6.2.10
- [x] T010 [P] Define `mu_event_fields_t` struct in `include/muc_opcua/types.h` with all 9 BaseEventType fields (EventId, EventType, SourceNode, SourceName, Time, ReceiveTime, LocalTime, Message, Severity) per OPC-10000-5 §6.4.2
- [x] T011 [P] Define audit event type structs in new `include/muc_opcua/services/audit.h`: `mu_audit_event_t` with union for 4 event types (OpenSecureChannel, CreateSession, ActivateSession, WriteUpdate) per OPC-10000-5 §6.5
- [x] T012 [P] Define `mu_method_callback_t` typedef and `mu_method_call_t` struct in new `include/muc_opcua/services/method.h` per contracts/method-call.md and OPC-10000-4 §5.12.2.2

---

## Phase 3: User Story 1 — Data Access Server Facet (Priority: P1) [MVP]

**Goal**: Percent deadband filtering and AnalogItem metadata (EURange, EngineeringUnits, InstrumentRange) per OPC-10000-7 §6.6.8

**Independent Test**: Create AnalogItem variable with EURange [0,100], create MonitoredItem with percent deadband=10, change value by 5 (no notification) then by 15 (notification received).

### Tests for User Story 1

- [x] T013 [P] [US1] Add deadband unit test with binary-encoded DataChangeFilter for percent deadband in `tests/unit/test_percent_deadband.c` citing OPC-10000-4 §7.22.2 Table 115 — test must fail before implementation
- [x] T014 [P] [US1] Add deadband error case test (percent deadband without EURange → Bad_DeadbandFilterInvalid) in `tests/unit/test_percent_deadband.c` citing OPC-10000-4 §7.22.2
- [x] T015 [P] [US1] Add AnalogItem property read test in `tests/unit/test_analog_item.c` (read EURange, EngineeringUnits, InstrumentRange Properties on an AnalogItem node) citing OPC-10000-3 §5.6.2

### Implementation for User Story 1

- [x] T016 [US1] Define AnalogItemType property NodeIds in `include/muc_opcua/opcua_ids.h`: EURange (ns=0;i=113), EngineeringUnits (ns=0;i=115), InstrumentRange (ns=0;i=117)
- [x] T017 [US1] Implement `mu_range_t` and `mu_eu_information_t` types in `include/muc_opcua/types.h` per OPC-10000-8 §5.6.2 and §5.6.3
- [x] T018 [P] [US1] Implement Range and EUInformation binary encoder/decoder in `src/encoding/binary_containers.c` per OPC-10000-6 §5.2.2.17 and §5.2.2.18
- [x] T019 [P] [US1] Implement percent deadband evaluation logic in `src/services/deadband.c`: compute `abs(change) > (deadbandValue / 100.0) * (EURange.High - EURange.Low)` per OPC-10000-4 §7.22.2
- [x] T020 [US1] Wire percent deadband evaluation into `src/core/service_dispatch.c:1436` replacing `Bad_MonitoredItemFilterUnsupported` return for percent deadband type, gated behind `MUC_OPCUA_DATA_ACCESS`
- [x] T021 [US1] Wire AnalogItemType Properties (EURange, EngineeringUnits, InstrumentRange) as HasProperty references from Variable nodes into `src/address_space/base_nodes.c` per OPC-10000-3 §5.6.2
- [x] T022 [US1] Add integration test for end-to-end percent deadband: create MonitoredItem, change value, verify notification delivery/suppression in `tests/integration/test_subscriptions.c`
- [x] T023 [US1] Update traceability table in `docs/traceability/037-standard-server-profile.md` with US1 file mappings and measure flash/RAM delta

**Checkpoint**: Percent deadband works end-to-end; AnalogItem metadata readable via Browse/Read.

---

## Phase 4: User Story 2 — Method Server Facet (Priority: P1)

**Goal**: Arbitrary custom method dispatch via Call service per OPC-10000-7 §6.6.25

**Independent Test**: Register a custom method callback, call it via Call service with input arguments, verify correct output arguments returned.

### Tests for User Story 2

- [x] T024 [P] [US2] Add Call service test for arbitrary registered method in `tests/unit/test_method_call_arbitrary.c` citing OPC-10000-4 §5.12.2.2 — test must fail before implementation
- [x] T025 [P] [US2] Add Call service error test for unregistered MethodId returning Bad_MethodInvalid in `tests/unit/test_method_call_arbitrary.c` citing OPC-10000-4 §7.38.2
- [x] T026 [P] [US2] Add Call service error test for input argument type mismatch returning Bad_TypeMismatch in `tests/unit/test_method_call_arbitrary.c`

### Implementation for User Story 2

- [x] T027 [US2] Implement method registration API `mu_server_register_method()` in `src/core/server.c` using a static dispatch table (max 16 entries) with O(1) lookup per research.md Decision 2
- [x] T028 [US2] Implement arbitrary method dispatch in `src/core/dispatch_method.c`: replace the 2-method hardcoded switch with lookup through the registration table, gated behind `MUC_OPCUA_METHOD_SERVER` — keep existing built-in methods (GetMonitoredItems, ResendData) as fallback
- [x] T029 [US2] Implement input argument validation (count match, type check) in `src/core/dispatch_method.c` per contracts/method-call.md error contract
- [x] T030 [US2] Wire `MUC_OPCUA_METHOD_SERVER` gate in `src/core/service_dispatch.c` dispatch table: when OFF, only built-in methods work; when ON, arbitrary registered methods are dispatched
- [x] T031 [US2] Update traceability table in `docs/traceability/037-standard-server-profile.md` with US2 file mappings and measure flash/RAM delta

**Checkpoint**: Arbitrary methods callable via Call service; built-in methods still work.

---

## Phase 5: User Story 3 — Standard Event Subscription + Address Space Notifier (Priority: P2)

**Goal**: EventFilter where-clause evaluation, full BaseEventType field extraction, and per-source-node event filtering per OPC-10000-7 §6.6.23 and §6.6.174

**Independent Test**: Create Event MonitoredItem with where-clause `Severity >= 500`, trigger events at Severity 300 and 700, verify only Severity 700 delivered. Create Event MonitoredItem for specific source node, verify only events from that node delivered.

### Tests for User Story 3

- [x] T032 [P] [US3] Add EventFilter where-clause evaluation unit test in `tests/unit/test_event_filter_where.c` citing OPC-10000-4 §7.22.3 — test must fail before implementation. Cover: Equals, GreaterThan, LessThan, And, Or, Not, Like operators
- [x] T033 [P] [US3] Add select-clause extraction unit test in `tests/unit/test_event_filter_select.c` for all 9 BaseEventType fields citing OPC-10000-5 §6.4.2 — test must fail before implementation
- [x] T034 [P] [US3] Add event notifier (source-node filtering) unit test in `tests/unit/test_event_notifier.c` citing OPC-10000-3 §5.4.6: create MonitoredItems on 2 nodes, trigger events on only 1 node, verify only that node's MonitoredItem receives events
- [x] T035 [P] [US3] Add fuzz target for EventFilter where-clause evaluator in `tests/fuzz/fuzz_event_filter_where.c` with malformed ContentFilter inputs
- [x] T036 [P] [US3] Add integration test for end-to-end event filtering with where-clause in `tests/integration/test_event_notifications.c`

### Implementation for User Story 3

- [x] T037 [US3] Implement ContentFilter where-clause evaluation engine in `src/services/event_filter.c` per contracts/event-filter.md and OPC-10000-4 §7.22.3: recursive evaluator supporting Equals, GreaterThan, LessThan, And, Or, Not, Like operators with nesting depth limit 4, gated behind `MUC_OPCUA_EVENT_FILTER_WHERE`
- [x] T038 [US3] Implement select-clause extraction from SimpleAttributeOperand BrowsePaths in `src/services/event_filter.c`: map BrowsePath last element to BaseEventType field (9 field names) per OPC-10000-5 §6.4.2
- [x] T039 [US3] Wire where-clause evaluation into `src/core/service_dispatch.c:1032-1049` (CreateMonitoredItems EventFilter parsing): replace skip-over with evaluation call, apply filter during event delivery in `src/services/subscription_publish.c:660-758`
- [x] T040 [US3] Implement per-node EventNotifier gating in `src/services/subscription_publish.c`: when event is triggered, check source node's EventNotifier attribute (uint8_t from `mu_node_t`) and only deliver to MonitoredItems whose monitored_node matches the event source or the Server Object per OPC-10000-3 §5.4.6
- [x] T041 [US3] Wire `MUC_OPCUA_EVENT_FILTER_WHERE` gate: when OFF, where-clause is ignored (all events delivered, preserving current behavior)
- [x] T042 [US3] Update traceability table in `docs/traceability/037-standard-server-profile.md` with US3 file mappings and measure flash/RAM delta

**Checkpoint**: Events filtered by where-clause and source node; 9 BaseEventType fields extractable.

---

## Phase 6: User Story 4 — Auditing (Priority: P2)

**Goal**: Four audit event types raised for security-relevant service calls per OPC-10000-7 §6.6.57

**Independent Test**: Open SecureChannel → verify AuditOpenSecureChannelEventType received. Create/Activate session → verify audit events. Write to variable → verify AuditWriteUpdateEventType.

### Tests for User Story 4

- [x] T043 [P] [US4] Add audit event unit test for OpenSecureChannel in `tests/unit/test_audit_events.c` citing OPC-10000-5 §6.5.3 — test must fail before implementation
- [x] T044 [P] [US4] Add audit event unit test for CreateSession and ActivateSession in `tests/unit/test_audit_events.c` citing OPC-10000-5 §6.5.4 and §6.5.5 — test must fail before implementation
- [x] T045 [P] [US4] Add audit event unit test for Write in `tests/unit/test_audit_events.c` citing OPC-10000-5 §6.5.8 — test must fail before implementation
- [x] T046 [P] [US4] Add audit disable test: runtime disable flag prevents audit event generation in `tests/unit/test_audit_events.c`
- [x] T047 [P] [US4] Add integration test: subscribe to Server Object events, trigger all 4 audit types, verify all received in `tests/integration/test_event_notifications.c`

### Implementation for User Story 4

- [x] T048 [US4] Implement audit event constructor functions in `src/services/audit_events.c` per contracts/auditing.md: `mu_raise_audit_open_channel()`, `mu_raise_audit_create_session()`, `mu_raise_audit_activate_session()`, `mu_raise_audit_write_update()` — each builds the appropriate event subtype per OPC-10000-5 §6.5 hierarchy, gated behind `MUC_OPCUA_AUDITING`
- [x] T049 [US4] Implement audit event delivery through existing Publish pipeline: audit events use `mu_server_trigger_event()` with Server Object (ns=0;i=2253) as source node per contracts/auditing.md
- [x] T050 [US4] Wire audit hooks into service handlers: `src/core/dispatch_secure_channel.c` (OpenSecureChannel), `src/core/dispatch_session.c` (CreateSession, ActivateSession), `src/core/dispatch_attribute.c` (Write)
- [x] T051 [US4] Set Server Object EventNotifier attribute to SubscribeToEvents (1) in `src/address_space/base_nodes.c` when `MUC_OPCUA_AUDITING` is ON per OPC-10000-3 §5.4.6
- [x] T052 [US4] Add runtime audit disable via `mu_server_config_t.auditing_enabled` field in `include/muc_opcua/server.h`
- [x] T053 [US4] Update traceability table in `docs/traceability/037-standard-server-profile.md` with US4 file mappings and measure flash/RAM delta

**Checkpoint**: All 4 audit event types raised and delivered through Publish.

---

## Phase 7: User Story 5 — ComplexType Server Facet (Priority: P2)

**Goal**: Custom Structure and Enumeration DataTypes discoverable via Browse, with DataTypeDefinition readable, and binary encode/decode support per OPC-10000-3 §5.6.4, §5.6.5

**Independent Test**: Register a custom structure, browse the DataType hierarchy to find it, read DataTypeDefinition, encode/decode a value of that type.

### Tests for User Story 5

- [x] T054 [P] [US5] Add StructureDefinition registration and browse test in `tests/unit/test_complex_types.c` citing OPC-10000-3 §5.6.4 — test must fail before implementation
- [x] T055 [P] [US5] Add EnumDefinition registration and browse test in `tests/unit/test_complex_types.c` citing OPC-10000-3 §5.6.5
- [x] T056 [P] [US5] Add structure binary encode/decode round-trip test in `tests/unit/test_complex_types.c` citing OPC-10000-6 §5.2.2.12
- [x] T057 [P] [US5] Add enumeration binary encode/decode test in `tests/unit/test_complex_types.c` citing OPC-10000-6 §5.2.2.9
- [x] T058 [P] [US5] Add fuzz target for structure binary decoder in `tests/fuzz/fuzz_complex_encode.c`

### Implementation for User Story 5

- [x] T059 [US5] Implement `mu_structure_definition_t`, `mu_structure_field_t`, `mu_enum_definition_t`, `mu_enum_field_t` types in `include/muc_opcua/address_space/complex_types.h` per data-model.md and OPC-10000-3 §5.6.4, §5.6.5
- [x] T060 [US5] Implement `mu_register_structure_type()` and `mu_register_enumeration_type()` in `src/address_space/complex_types.c`: static registration table, create DataType nodes under Structure/Enumeration with HasSubtype references, set DataTypeDefinition attribute per contracts/complex-types.md, gated behind `MUC_OPCUA_COMPLEX_TYPES`
- [x] T061 [US5] Implement Structure binary encoder in `src/encoding/binary_complex.c`: encode fields in declaration order, EncodingMask for optional fields per OPC-10000-6 §5.2.2.12
- [x] T062 [US5] Implement Structure binary decoder in `src/encoding/binary_complex.c`: decode fields from binary, check EncodingMask for optional fields per OPC-10000-6 §5.2.2.12
- [x] T063 [US5] Implement Enumeration binary encoder/decoder in `src/encoding/binary_complex.c`: Int32 value per OPC-10000-6 §5.2.2.9
- [x] T064 [US5] Wire DataTypeDefinition attribute read in `src/core/dispatch_attribute.c`: when attribute_id requests DataTypeDefinition on a DataType node, return the registered definition per OPC-10000-3 §5.6.3
- [x] T065 [US5] Update traceability table in `docs/traceability/037-standard-server-profile.md` with US5 file mappings and measure flash/RAM delta

**Checkpoint**: Custom structures and enumerations discoverable, definable, and encodable.

---

## Phase 8: User Story 6 — Server Diagnostics + Base Server Behavior (Priority: P3)

**Goal**: ServerDiagnosticsSummary, SessionsDiagnosticsSummary, and Namespaces nodes per OPC-10000-5 §6.3

**Independent Test**: Read ServerDiagnosticsSummary counters, create/close sessions, verify counters increment. Read Namespaces node, verify metadata for namespace 0.

### Tests for User Story 6

- [x] T066 [P] [US6] Add diagnostics counter unit test in `tests/unit/test_diagnostics.c`: read summary, create sessions and subscriptions, verify counters increment per OPC-10000-5 §6.3.3 — test must fail before implementation
- [x] T067 [P] [US6] Add session diagnostics unit test in `tests/unit/test_diagnostics.c`: verify per-session counters (ReadCount, WriteCount, etc.) per OPC-10000-5 §6.3.5
- [x] T068 [P] [US6] Add ServerRedundancy read test in `tests/unit/test_view_services.c`: verify RedundancySupport = None(0) and ServiceLevel present
- [x] T069 [P] [US6] Add Namespaces node read test in `tests/unit/test_view_services.c`: verify NamespaceUri, NamespaceVersion for namespace 0 per OPC-10000-5 §6.2.10

### Implementation for User Story 6

- [x] T070 [US6] Implement `mu_diagnostics_t` struct and atomic counter macros in `src/services/diagnostics.c`: CumulatedSessionCount, CurrentSessionCount, SecurityRejectedSessionCount, RejectedSessionCount, SessionTimeoutCount, SessionAbortCount, CurrentSubscriptionCount, CumulatedSubscriptionCount per OPC-10000-5 §6.3.3, gated behind `MUC_OPCUA_SERVER_DIAGNOSTICS`
- [x] T071 [US6] Implement session diagnostics array in `src/services/diagnostics.c`: fixed-size array of `mu_session_diagnostics_t` indexed by session handle, with per-service request counters per OPC-10000-5 §6.3.5
- [x] T072 [US6] Wire diagnostics counter updates into session lifecycle: `src/core/dispatch_session.c` (create, close, timeout, reject) and subscription lifecycle: `src/services/subscription_publish.c` (create, delete)
- [x] T073 [US6] Wire per-service request counters in `src/core/service_dispatch.c`: increment appropriate counter on each service call
- [x] T074 [US6] Implement ServerDiagnosticsSummary Variable node with value source callback in `src/address_space/base_nodes.c` per data-model.md Server Object hierarchy
- [x] T075 [US6] Implement SessionsDiagnosticsSummary Variable node with value source callback and SessionDiagnosticsArray in `src/address_space/base_nodes.c` per OPC-10000-5 §6.3.5
- [x] T076 [US6] Wire `MUC_OPCUA_SERVER_DIAGNOSTICS` gate: when OFF, diagnostic nodes are absent from address space; when ON, active counter source samples on read
- [x] T077 [US6] Update traceability table in `docs/traceability/037-standard-server-profile.md` with US6 file mappings and measure flash/RAM delta

**Checkpoint**: Diagnostics counters increment; Namespaces and ServerRedundancy nodes readable.

---

## Phase 9: User Story 7 — Remaining Facets and Hardening (Priority: P3)

**Goal**: TransferSubscriptions, full aggregate functions, Reverse Connect, Security Time Sync per OPC-10000-4 §5.14.7, OPC-10000-13, OPC-10000-6 §7.5, OPC-10000-4 §A.2

**Independent Test**: Transfer subscription between sessions → notifications arrive in new session. MonitoredItem with AggregateFilter Count → count published. Reverse Connect → server connects to client.

### Tests for User Story 7

- [x] T078 [P] [US7] Add TransferSubscriptions unit test in `tests/unit/test_transfer_subscriptions.c` citing OPC-10000-4 §5.14.7 — test must fail before implementation
- [x] T079 [P] [US7] Add full aggregate function (Count, Range, TimeAverage, Delta, DurationGood, DurationBad, PercentGood, PercentBad, Total) unit test in `tests/unit/test_aggregate_full.c` citing OPC-10000-13 — test must fail before implementation
- [x] T080 [P] [US7] Add aggregate filter unsupported function test (return Bad_MonitoredItemFilterUnsupported for unimplemented functions) in `tests/unit/test_aggregate_full.c`
- [x] T081 [P] [US7] Add Reverse Connect unit test in `tests/unit/test_reverse_connect.c` citing OPC-10000-6 §7.5
- [x] T082 [P] [US7] Add time sync unit test in `tests/unit/test_time_sync.c` citing OPC-10000-4 §A.2: verify SourceTimestamp and ServerTimestamp populated in Read/Write responses

### Implementation for User Story 7

- [x] T083 [P] [US7] Implement TransferSubscriptions handler in `src/core/dispatch_transfer.c`: validate source session, update subscription session_id, transfer queued notifications per research.md Decision 8, gated behind `MUC_OPCUA_REDUNDANCY`, citing OPC-10000-4 §5.14.7
- [x] T084 [P] [US7] Implement extended aggregate functions in `src/services/subscription_aggregate.c`: add Count, Range, TimeAverage, Delta, DurationGood, DurationBad, PercentGood, PercentBad, WorstQuality, Total, MinimumActualTime, MaximumActualTime, AnnotationCount, Start, End, DurationInStateZero — stateless streaming accumulators per research.md Decision 7, gated behind `MUC_OPCUA_AGGREGATE_FULL`, citing OPC-10000-13
- [x] T085 [P] [US7] Wire new aggregate functions into aggregate filter decode in `src/core/service_dispatch.c:939-940`: expand switch to match all implemented function NodeIds
- [x] T086 [P] [US7] Implement Reverse Connect TCP client in `src/services/reverse_connect.c`: outbound TCP connection to configured endpoint, reuse existing tcp_connection infrastructure, retry with backoff per research.md Decision 9, gated behind `MUC_OPCUA_REVERSE_CONNECT`, citing OPC-10000-6 §7.5
- [x] T087 [P] [US7] Implement Security Time Sync in `src/services/time_sync.c`: ensure Read/Write responses populate SourceTimestamp from value source and ServerTimestamp from time adapter per OPC-10000-4 §A.2, gated behind `MUC_OPCUA_TIME_SYNC`
- [x] T088 [US7] Update traceability table in `docs/traceability/037-standard-server-profile.md` with US7 file mappings and measure flash/RAM delta
- [x] T089 [US7] Wire all 3 new dispatch entries into `src/core/service_dispatch.c` dispatch table: TransferSubscriptions (ns=0;i=841), and ensure existing Read/Write paths check MUC_OPCUA_TIME_SYNC for timestamp population

**Checkpoint**: TransferSubscriptions works; aggregate functions beyond Avg/Min/Max; Reverse Connect and Time Sync operational.

---

## Phase 10: Compliance, Size, and Polish

**Purpose**: Cross-cutting validation, existing profile regression, size measurement, and traceability finalization

- [x] T090 Run full host test suite: `ctest --test-dir build/test --output-on-failure` — all 108 existing + all new tests must pass
- [x] T091 Run ASAN/UBSan test suite: `cmake -DMUC_OPCUA_SANITIZERS=address,undefined && make` — zero issues
- [x] T092 Run fuzz targets for event filter and complex encode (60s each): `tests/fuzz/fuzz_event_filter_where.c`, `tests/fuzz/fuzz_complex_encode.c` — no crashes
- [x] T093 Measure nano/micro/embedded profile binary sizes with `arm-none-eabi-size` and verify <2% regression from HANDOFF.md baseline (nano: 17,392 B, micro: 27,818 B, embedded: 52,525 B)
- [x] T094 Measure standard profile binary size and verify <80 KB `.text` on ARM Cortex-M0+ (Thumb)
- [x] T095 Run existing profile regression: `make nano && make micro && make embedded` — all compile and pass tests with zero regressions
- [x] T096 Build Pico SDK cross-compile target for standard profile and verify zero errors
- [x] T097 Run cppcheck and clang-format on all new source files — zero warnings
- [x] T098 [P] Verify 5 required SecurityPolicies (None, Basic256Sha256, Aes128-Sha256-RsaOaep, Aes256-Sha256-RsaPss, Basic256) are already present and gated correctly for standard profile per FR-020. Test that GetEndpoints returns all 5 policies when `MUC_OPCUA_PROFILE=standard`.
- [x] T099 [P] Run interop test against an external OPC UA client (Python asyncua or .NET reference client) exercising all 7 new facets: percent deadband, arbitrary method call, event filter, audit events, complex types, diagnostics, and TransferSubscriptions in `tests/integration/test_standard_interop.c` per SC-014. Verify all facets detectable from client perspective.
- [x] T100 Verify ServerProfileArray advertises `http://opcfoundation.org/UA-Profile/Server/StandardUA2017` when built with `MUC_OPCUA_PROFILE=standard`
- [x] T101 Finalize traceability table in `docs/traceability/037-standard-server-profile.md`: every implementation file and test file mapped to its OPC UA spec section
- [x] T102 Verify quickstart.md walkthrough on host: `cmake -DMUC_OPCUA_PROFILE=standard && cmake --build` completes successfully

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can begin immediately
- **Foundational (Phase 2)**: Depends on Setup (needs CMake flags wired and headers in place)
- **US1 (Phase 3)**: Depends on Foundational. Parallel: T013-T015 tests can begin immediately after Phase 2
- **US2 (Phase 4)**: Depends on Foundational. Can run in parallel with US1 (different files)
- **US3 (Phase 5)**: Depends on Foundational + US1's event infrastructure. Can run in parallel with US2
- **US4 (Phase 6)**: Depends on Foundational + US3's event delivery pipeline. Can run in parallel with US5
- **US5 (Phase 7)**: Depends on Foundational. Can run in parallel with US4
- **US6 (Phase 8)**: Depends on Foundational. Can run in parallel with US4, US5
- **US7 (Phase 9)**: Depends on Foundational. Can run in parallel with US6
- **Compliance (Phase 10)**: Depends on all user stories being complete

### Within Each User Story

- Tests BEFORE implementation (Constitution IV)
- Headers/types BEFORE implementation files that use them
- Encoders before dispatchers that consume them
- Traceability update before checkpoint completion

### Parallel Opportunities

```
Phase 1: T002, T003, T004, T005 can run in parallel (different files)
Phase 2: T007, T008, T009, T010, T011, T012 can run in parallel (different files)
Phase 3: T013, T014, T015 can run in parallel (different test files). T018, T019 can run in parallel (encoder vs logic)
Phase 4: T024, T025, T026 can run in parallel
Phase 5: T032, T033, T034, T035, T036 can run in parallel
Phase 6: T043, T044, T045, T046, T047 can run in parallel
Phase 7: T054, T055, T056, T057, T058 can run in parallel
Phase 8: T066, T067, T068, T069 can run in parallel
Phase 9: T078, T079, T080, T081, T082 can run in parallel. T083, T084, T085, T086, T087 can run in parallel
User Stories: US1 + US2 can proceed in parallel. US3 + US4 after US1/US2. US4 + US5 can proceed in parallel. US6 + US7 can proceed in parallel.
```

### Suggested MVP Scope

User Stories 1 + 2 (Phase 3 + 4) = Data Access Facet + Method Server Facet. These are the two P1 stories and together cover the minimum Standard profile differentiator.

---

## Notes

- [P] tasks = different files, no dependencies — safe for parallel agent execution
- [Story] label maps task to a user story from spec.md
- Every protocol task cites exact OPC UA part/section references
- Tests must fail before implementation for protocol behavior (Constitution IV)
- Commit after each task or logical group of [P] parallel tasks
- Stop at checkpoints to validate independent story behavior, conformance traceability, and size impact
- All new services/code paths gated behind CMake flags; existing profiles (nano/micro/embedded) must compile and pass tests unchanged
- Zero `.data`/`.bss`/heap in steady state across all profiles
- One assignment = one task (AGENTS.md rule)
