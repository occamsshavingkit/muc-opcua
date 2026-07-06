# Feature Specification: Implement Placeholder Stub Tests

**Feature Branch**: `043-implement-stub-tests`
**Created**: 2026-07-06
**Status**: Draft
**Input**: User description: "implement the placeholder stubs in the current code. There are five in TODO.md and grep -i STUB on the code might find more."

## User Scenarios & Testing

### User Story 1 - Aggregate Function Behavioral Tests (Priority: P1)

A developer can verify that the 3 existing aggregate functions (Average, Minimum,
Maximum) handle edge cases correctly by running unit tests that directly exercise
the accumulate-and-publish cycle with known inputs and expected outputs
(OPC-10000-13). Note: only 3 of 42 OPC-10000-13 aggregates are implemented;
remaining functions (PercentDeadband, Range, DurationGood, DurationBad,
AggregateStatus, Count, etc.) are deferred pending `MUC_OPCUA_AGGREGATE_FULL`
feature implementation.

**Why this priority**: Aggregate functions touch the data-change pipeline
(subscriptions, deadbands, percent-banding). Incorrect aggregation silently
corrupts monitoring data. Even with only 3 functions, edge-case verification is
critical.

**Independent Test**: Run `test_aggregate_full` — each of the 3 existing
aggregate functions is tested via direct API calls with known-input/
expected-output vectors, covering edge cases not tested by the existing
full-pipeline `test_aggregate.c`.

**Acceptance Scenarios**:

1. **Given** an Average aggregate with sample_count > 0, **When** published,
   **Then** the published value is `sum / sample_count` as a `MU_TYPE_DOUBLE`.
2. **Given** a Minimum aggregate with multiple samples, **When** samples include
   decreasing values, **Then** the published minimum tracks the lowest value seen.
3. **Given** a Maximum aggregate with multiple samples, **When** samples include
   increasing values, **Then** the published maximum tracks the highest value seen.
4. **Given** an aggregate with sample_count == 0 and no previous value, **When**
   published, **Then** `MU_STATUS_BAD_NODATA` is returned.
5. **Given** a non-numeric variant (e.g., string) accumulated into an aggregate,
   **When** accumulated, **Then** the sample_count is NOT incremented.

---

### User Story 2 - Audit Event Dispatch Tests (Priority: P2)

A developer can verify that `mu_raise_audit_event` handles edge cases correctly
without crashing, and that the auditing configuration flag is accessible.
Callback-based dispatch tests are deferred pending implementation of the
callback mechanism (OPC-10000-5 §6.5).

**Why this priority**: Audit events are a security-relevant path. Even though
the current implementation is a no-op, verifying NULL-safety and configuration
accessibility prevents future regressions when callbacks are added.

**Independent Test**: Run `test_audit_events` — covers NULL-safety, config flag
access, and documents what is deferred.

**Acceptance Scenarios**:

1. **Given** a server instance with auditing disabled, **When**
   `mu_raise_audit_event` is called with a valid event, **Then** the function
   returns without crash and the server remains valid.
2. **Given** NULL server or NULL event pointer, **When**
   `mu_raise_audit_event` is called, **Then** the function returns without crash.
3. **Given** a server config with `auditing_enabled` set, **When** read,
   **Then** the flag correctly reflects the configured value.
4. **Given** the codebase does not yet have a callback mechanism, **When** a
   developer reads the test file, **Then** the deferred work is documented
   as a comment listing pending API additions (callback typedef, registration,
   dispatch).

---

### User Story 3 - Complex Type Round-Trip Tests (Priority: P2)

A developer can verify that complex type registration API and type definition
field validation works correctly. Encode/decode round-trip tests are deferred
pending implementation of `mu_binary_encode_struct`/`mu_binary_decode_struct`
in `src/encoding/binary_complex.c` (currently a dead stub).

**Why this priority**: Complex type encoding is used by PubSub, alarms,
historical data, and advanced diagnostics. The registration API is the
foundation — round-trip tests require an encoder that does not yet exist.

**Independent Test**: Run `test_complex_types` — existing registration
and field validation tests pass. STUB comment replaced with deferred-work
documentation. No behavioral test changes in this feature.

**Acceptance Scenarios**:

1. **Given** the `mu_structure_field_t` and `mu_structure_definition_t` types,
   **When** a developer reads `test_complex_types.c`, **Then** the file clearly
   documents what is tested (registration API, type definitions) vs. what is
   deferred (encode/decode round-trips pending `binary_complex.c` implementation).
2. **Given** the existing registration tests, **When** run, **Then** all tests
   pass and the `/* STUB: ... */` marker is replaced with a documentation
   comment describing deferred work.

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

- **FR-001**: `test_aggregate_full` MUST test the 3 existing aggregate functions
  (Average, Minimum, Maximum) with edge-case known-input/expected-output vectors
  via direct API calls (OPC-10000-13)
- **FR-002**: `test_aggregate_full` MUST test zero-sample fallback behavior
  (sample_count=0) for both no-previous-value (BAD_NODATA) and has-previous-value
  (publish-as-is) cases (OPC-10000-13)
- **FR-003**: `test_audit_events` MUST verify `mu_raise_audit_event` does not
  crash when called with valid inputs or NULL arguments (OPC-10000-5 §6.5)
- **FR-004**: `test_audit_events` MUST verify `auditing_enabled` config flag
  is readable and reflects the configured value
- **FR-005**: `test_complex_types` MUST document deferred work (encode/decode
  round-trips pending `binary_complex.c` implementation) and remove the
  `/* STUB: */` placeholder marker (OPC-10000-3 §5.6.4)
- **FR-006**: `test_minimal_server_flow` MUST exercise the full server
  lifecycle: connect → HELLO/ACK → OpenSecureChannel → CreateSession →
  ActivateSession → Read → CloseSession → disconnect
- **FR-007**: `test_minimal_server_flow` MUST verify all StatusCodes are Good
  for successful operations
- **FR-008**: `test_discovery_endpoint_no_session` MUST exercise FindServers
  and GetEndpoints without an active session
- **FR-009**: All new tests MUST gate on the same `MUC_OPCUA_*` feature flags
  that gate the feature implementation
- **FR-010**: The `/* STUB: */` comment markers MUST be removed from all
  5 test files after implementation

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
- **Out of Scope**: Implementing the 42 aggregate functions themselves (only
  3 exist in `src/services/subscription_aggregate.c`; remaining 39 deferred
  behind `MUC_OPCUA_AGGREGATE_FULL`)
- **Out of Scope**: Modifying the audit event API or implementation
- **Out of Scope**: Adding new complex type definitions

### Key Entities

- **Aggregate Function**: A computation (Average, Minimum, Maximum) applied over
  a time series of DataValues. 3 of 42 OPC-10000-13 functions are implemented;
  remaining functions deferred behind `MUC_OPCUA_AGGREGATE_FULL`.
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

- **SC-001**: All 5 `/* STUB: */` comment markers are removed from the
  codebase
- **SC-002**: `test_aggregate_full` contains at least 6 edge-case test
  functions exercising the 3 existing aggregate functions via direct API calls
- **SC-003**: `test_audit_events` exercises NULL-safety and config flag access
  paths
- **SC-004**: `test_complex_types` contains documentation of deferred
  encode/decode work and no longer has the `/* STUB: */` marker
- **SC-005**: `test_minimal_server_flow` successfully completes the full
  server lifecycle without crashes or memory leaks
- **SC-006**: `test_discovery_endpoint_no_session` returns valid endpoint
  descriptions and server listings without requiring a session
- **SC-007**: All 5 tests pass in every profile where the relevant feature
  flags are enabled
- **SC-008**: No target test file uses `TEST_IGNORE_MESSAGE` as a placeholder
  — all tests exercise real code paths or document deferred work

## Assumptions

- The aggregate function implementations exist in `src/services/subscription_aggregate.c`
  (3 functions: Average, Minimum, Maximum) and produce correct results per OPC-10000-13
- `mu_raise_audit_event` in `src/services/audit_events.c` is a no-op — callback
  dispatch is deferred pending implementation of `mu_audit_callback_t`
- The complex type encoder/decoder (`mu_binary_encode_struct` /
  `mu_binary_decode_struct`) does not exist — `src/encoding/binary_complex.c`
  is a dead stub. Round-trip tests are deferred until the encoder is implemented
- The server implementation supports the complete lifecycle (connect, secure
  channel, session, read, close) without needing additional feature work
- Discovery services (FindServers, GetEndpoints) work correctly without a
  session per OPC-10000-4 §5.5.4
- `MUC_OPCUA_REVERSE_CONNECT` and `MUC_OPCUA_TIME_SYNC` are deferred
  features with no implementation to test — their stub files may need to
  stay as stubs or be removed
