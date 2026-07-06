# Feature Specification: Implement Placeholder Stub Tests

**Feature Branch**: `043-implement-stub-tests`
**Created**: 2026-07-06
**Status**: Draft
**Input**: User description: "implement the placeholder stubs in the current code. There are five in TODO.md and grep -i STUB on the code might find more."

## User Scenarios & Testing

### User Story 1 - Aggregate Function Behavioral Tests (Priority: P1)

A developer can verify that the 42 aggregate functions compute correct results
by running unit tests exercising each function with known inputs and expected
outputs (OPC-10000-13).

**Why this priority**: Aggregate functions are the most complex stub (42
functions), touching the data-change pipeline (subscriptions, deadbands,
percent-banding). Incorrect aggregation silently corrupts monitoring data.

**Independent Test**: Run `test_aggregate_full` — each aggregate function is
tested independently with at least one known-input-to-expected-output vector.

**Acceptance Scenarios**:

1. **Given** a PercentDeadband aggregate function, **When** raw samples cross
   the deadband threshold, **Then** only samples that change by >= deadband
   percent are included.
2. **Given** a numeric aggregate function (Avg, Sum, Min, Max, Count, Duration),
   **When** fed a sequence of known inputs, **Then** the output matches the
   precomputed expected value.
3. **Given** a duration-based aggregate (DurationGood, DurationBad), **When**
   fed status-coded samples, **Then** only samples with the correct status are
   counted in the duration.
4. **Given** StatusCode aggregate (AggregateStatus), **When** fed a mix of Good
   and Bad statuses, **Then** the aggregate status correctly reflects the
   worst/best status per OPC UA rules.
5. **Given** a range aggregate (Range), **When** fed samples with varying
   values, **Then** the range is correctly computed as max - min.

---

### User Story 2 - Audit Event Dispatch Tests (Priority: P2)

A developer can verify that `mu_raise_audit_event` correctly dispatches audit
events to registered callback handlers, including negative-path tests for
null/missing handlers and event field validation (OPC-10000-5 §6.5).

**Why this priority**: Audit events are a security-relevant path. Incorrect
dispatch silently drops security-critical audit records.

**Independent Test**: Run `test_audit_events` — covers the dispatch path,
callback registration, and edge cases.

**Acceptance Scenarios**:

1. **Given** a registered audit callback, **When** `mu_raise_audit_event` is
   called with a valid event, **Then** the callback receives the event with
   correct ActionTimeStamp, ServerId, and ClientAuditEntryId.
2. **Given** no registered audit callback, **When** `mu_raise_audit_event` is
   called, **Then** the function returns without error (graceful no-op).
3. **Given** a NULL server or NULL event pointer, **When**
   `mu_raise_audit_event` is called, **Then** the function returns gracefully
   without segfault.

---

### User Story 3 - Complex Type Round-Trip Tests (Priority: P2)

A developer can verify that complex OPC UA types (structures with nested fields,
arrays of structures, optional fields) encode and decode correctly through a
full binary round-trip (OPC-10000-3 §5.6.4).

**Why this priority**: Complex type encoding is used by PubSub, alarms,
historical data, and advanced diagnostics. Encoding bugs corrupt all features
that depend on those types.

**Independent Test**: Run `test_complex_types` — each type is
encode→decode→deep-equal verified independently.

**Acceptance Scenarios**:

1. **Given** a structure type with required scalar fields, **When** encoded
   with `mu_binary_encode_struct` and decoded with `mu_binary_decode_struct`,
   **Then** the decoded value matches the original.
2. **Given** a structure type with optional fields (EncodingMask
   present/absent), **When** encoded with all optional fields present and
   decoded, **Then** the decoder correctly identifies which fields are present.
3. **Given** a structure type with nested structures, **When** round-tripped
   through encode/decode, **Then** all levels of nesting are preserved.
4. **Given** a structure type with arrays of a fundamental type, **When**
   round-tripped, **Then** array length and elements are preserved.

---

### User Story 4 - Minimal Server Flow Integration Test (Priority: P3)

A developer can verify that a minimal OPC UA server lifecycle — TCP connect,
HELLO/ACK handshake, OpenSecureChannel, CreateSession, ActivateSession, Read,
CloseSession, TCP close — runs end-to-end without crashing or leaking memory
(OPC-10000-6 §7.1 / OPC-10000-4 §5.6).

**Why this priority**: Integration tests catch protocol-level bugs that unit
tests miss. A minimal server flow covers the most critical OPC UA path.

**Independent Test**: Run `test_minimal_server_flow` — the full lifecycle
completes with expected StatusCodes.

**Acceptance Scenarios**:

1. **Given** a fresh server instance, **When** a client connects via TCP,
   completes HELLO/ACK, opens a secure channel, creates and activates a
   session, performs a Read, and closes the session, **Then** all operations
   return Good and the server shuts down cleanly.
2. **Given** the above lifecycle, **When** the test repeats 3 times on the same
   server instance, **Then** no memory leaks or resource exhaustion (session
   slots freed, channel slots freed).

---

### User Story 5 - Discovery Without Session Integration Test (Priority: P3)

A developer can verify that FindServers and GetEndpoints services work correctly
without an active session (OPC-10000-4 §5.5.4).

**Why this priority**: Discovery is the first interaction any OPC UA client has
with a server. Failing here blocks all subsequent communication.

**Independent Test**: Run `test_discovery_endpoint_no_session` — discovery
services return valid endpoint descriptions without requiring a session.

**Acceptance Scenarios**:

1. **Given** a server with at least one registered endpoint, **When**
   GetEndpoints is called without a session, **Then** the response contains the
   endpoint description with correct SecurityPolicy and TransportProfile URIs.
2. **Given** a server, **When** FindServers is called without a session,
   **Then** the response lists at least one server with the correct
   ApplicationName and ApplicationURI.
3. **Given** a server configured with a specific ApplicationURI, **When**
   FindServers is filtered by that URI, **Then** only matching servers are
   returned.

---

### Edge Cases

- What happens when a disabled profile feature (e.g., `MUC_OPCUA_EVENTS` OFF)
  causes the stub test path to be unreachable? Tests must gate on the same
  `#ifdef` as the feature implementation.
- How does test_aggregate_full handle the 42 aggregate function table? Tests
  should exercise at least one function per category (numeric, duration,
  status, range, percent-change).
- What happens when a complex type definition contains a forward reference or
  recursive type? Tests must use types available in the current codebase.
- What happens when discovery endpoint buffers are full? Test must verify
  `MU_DISCOVERY_MAX_ENDPOINTS` limit is enforced.
- What happens when the server has no sessions/channels (cold start)? All
  per-connection integration tests must account for the initial state.

## Requirements

### Functional Requirements

- **FR-001**: `test_aggregate_full` MUST test at least 10 of the 42 aggregate
  functions with known-input/expected-output vectors (OPC-10000-13)
- **FR-002**: `test_aggregate_full` MUST test PercentDeadband with samples
  crossing and not crossing the threshold
- **FR-003**: `test_audit_events` MUST verify `mu_raise_audit_event` delivers
  events to a registered callback (OPC-10000-5 §6.5)
- **FR-004**: `test_audit_events` MUST verify graceful no-op when no callback
  is registered
- **FR-005**: `test_complex_types` MUST round-trip at least 3 complex type
  definitions through encode→decode→verify (OPC-10000-3 §5.6.4)
- **FR-006**: `test_minimal_server_flow` MUST exercise the full server
  lifecycle: connect → HELLO/ACK → OpenSecureChannel → CreateSession →
  ActivateSession → Read → CloseSession → disconnect
- **FR-007**: `test_minimal_server_flow` MUST verify all StatusCodes are Good
  for successful operations
- **FR-008**: `test_discovery_endpoint_no_session` MUST exercise FindServers
  and GetEndpoints without an active session
- **FR-009**: All new tests MUST gate on the same `MUC_OPCUA_*` feature flags
  that gate the feature implementation
- **FR-010**: The `/* STUB: ... */` comment markers MUST be removed from all
  5 test files after tests are implemented

### OPC UA Normative Scope

- **OPC-001**: Aggregate functions tested per OPC-10000-13 (Aggregates)
- **OPC-002**: Audit event dispatch per OPC-10000-5 §6.5 (Audit Events)
- **OPC-003**: Complex type encoding per OPC-10000-3 §5.6.4 (AddressSpace
  Model / Type Encoding)
- **OPC-004**: TCP transport lifecycle per OPC-10000-6 §7.1 (OPC UA TCP)
- **OPC-005**: Discovery services per OPC-10000-4 §5.5.4 (Discovery)
- **OPC-006**: CreateSession/ActivateSession/CloseSession per OPC-10000-4 §5.6
  (Session Services)

### Scope Boundaries

- **In Scope**: Replace 5 `/* STUB: ... */` placeholder tests with real
  behavioral tests
- **Out of Scope**: Implementing `MUC_OPCUA_REVERSE_CONNECT` feature (test
  deferred, feature doesn't exist)
- **Out of Scope**: Implementing `MUC_OPCUA_TIME_SYNC` feature (test deferred,
  feature doesn't exist)
- **Out of Scope**: Creating `docs/async-opcua-inventory.md` (test deferred,
  doc doesn't exist)
- **Out of Scope**: Implementing the 42 aggregate functions themselves (they
  already exist in `src/services/aggregates/`)
- **Out of Scope**: Modifying the audit event API or implementation
- **Out of Scope**: Adding new complex type definitions

### Key Entities

- **Aggregate Function**: A computation (Avg, Sum, Min, Max, Count, Duration,
  PercentDeadband, Range, StatusCode, etc.) applied over a time series of
  DataValues. Defined in OPC-10000-13.
- **Audit Event**: A security event record with ActionTimeStamp, ServerId,
  ClientAuditEntryId, Status, and event-specific fields. Raised via
  `mu_raise_audit_event` (OPC-10000-5 §6.5).
- **Complex Type**: An OPC UA type with multiple fields, potentially nested
  structures, optional fields, or arrays. Encoded per OPC-10000-6 Binary
  encoding rules.
- **Server Lifecycle**: The sequence of OPC UA protocol exchanges from TCP
  connection to session teardown (HELLO/ACK, OpenSecureChannel,
  CreateSession, ActivateSession, Read, CloseSession).

## Success Criteria

### Measurable Outcomes

- **SC-001**: All 5 `/* STUB: ... */` comment markers are removed from the
  codebase
- **SC-002**: `test_aggregate_full` contains at least 10 aggregate function
  test cases with known-input/expected-output vectors
- **SC-003**: `test_audit_events` exercises both the registered-callback and
  no-callback paths
- **SC-004**: `test_complex_types` round-trips at least 3 distinct complex
  type definitions
- **SC-005**: `test_minimal_server_flow` successfully completes the full
  server lifecycle without crashes or memory leaks
- **SC-006**: `test_discovery_endpoint_no_session` returns valid endpoint
  descriptions without requiring a session
- **SC-007**: All 5 tests pass in every profile where the relevant feature
  flags are enabled
- **SC-008**: No new test uses `TEST_IGNORE_MESSAGE` or `TEST_IGNORE` —
  all tests exercise real code paths

## Assumptions

- The aggregate function implementations in `src/services/aggregates/`
  already exist and produce correct results per OPC-10000-13
- `mu_raise_audit_event` in `src/services/audit_events.c` is complete and
  functional
- The complex type encoder/decoder already supports round-trip for the types
  being tested
- The server implementation supports the complete lifecycle (connect, secure
  channel, session, read, close) without needing additional feature work
- Discovery services (FindServers, GetEndpoints) work correctly without a
  session per OPC-10000-4 §5.5.4
- `MUC_OPCUA_REVERSE_CONNECT` and `MUC_OPCUA_TIME_SYNC` are deferred
  features with no implementation to test — their stub files may need to
  stay as stubs or be removed
