# Feature Specification: Implement Feature-Gated Stub Tests

**Feature Branch**: `044-impl-feature-gated-stubs`
**Created**: 2026-07-06
**Status**: Draft
**Input**: User description: "knock out STUB3,4,8 from @TODO.md - do everything needed to implement the stubbed functions."

## User Scenarios & Testing

### User Story 1 - Reverse Connect (Priority: P1)

A developer deploying an OPC UA server behind a NAT/firewall can configure the
server to initiate the TCP connection to a known client endpoint, enabling
client-server communication when inbound connections are blocked.

**Why this priority**: Reverse connect is a core OPC UA transport feature
(OPC-10000-6 §7.5). The cmake flag and test stub already exist; only the
implementation is missing.

**Independent Test**: Configure server with `MUC_OPCUA_REVERSE_CONNECT` and a
reverse connect URL, verify the server opens a TCP connection to the endpoint
instead of listening for incoming connections.

**Acceptance Scenarios**:

1. **Given** a server config with `reverse_connect_url` set to a valid endpoint,
   **When** the server starts, **Then** it opens an outbound TCP connection to
   that URL instead of calling `listen()`.
2. **Given** a reverse-connected server with an active TCP connection, **When**
   the client sends a HELLO over that connection, **Then** the server responds
   with ACK per standard OPC UA TCP protocol.
3. **Given** a server config without `reverse_connect_url`, **When** the server
   starts, **Then** it uses the normal `listen()` path — no behavior change.

---

### User Story 2 - Time Synchronization (Priority: P2)

A developer can enable server-side time synchronization so that the server
populates `ServerTimestamp` in its ResponseHeaders, allowing clients to correct
clock drift per OPC-10000-4 §A.2.

**Why this priority**: Time sync is a standard OPC UA feature requested by the
micro and embedded server profiles. The cmake flag and test stub exist; only the
implementation is missing.

**Independent Test**: Configure server with `MUC_OPCUA_TIME_SYNC`, send a Read
request, verify the response contains a non-zero ServerTimestamp.

**Acceptance Scenarios**:

1. **Given** a server with `MUC_OPCUA_TIME_SYNC` enabled and a known tick value,
   **When** a service request is processed, **Then** the ResponseHeader contains
   a `ServerTimestamp` matching the time the request was received.
2. **Given** a server with `MUC_OPCUA_TIME_SYNC` disabled, **When** a service
   request is processed, **Then** the ResponseHeader `ServerTimestamp` is zero
   (existing behavior unchanged).

---

### User Story 3 - Async OPC-UA Conformance Inventory (Priority: P3)

A developer can verify that the project's async-capable features, devcontainer
setup, codegen tests, dotnet interop, and fuzz harnesses are documented in a
machine-checkable conformance inventory.

**Why this priority**: The inventory doc supports the async OPC UA conformance
profile by providing a structured, machine-verifiable list of features.

**Independent Test**: Run `test_async_opcua_inventory` — verifies the inventory
doc exists and contains the required sections (devcontainer, codegen, dotnet,
fuzz, schemas, tools).

**Acceptance Scenarios**:

1. **Given** the project root, **When** the inventory doc is checked,
   **Then** `docs/conformance/async-opcua-inventory.md` exists and is readable.
2. **Given** the inventory doc, **When** scanned for required sections,
   **Then** devcontainer, codegen tests, dotnet interop, fuzz, schemas, and
   tools sections are present.

---

### Edge Cases

- What happens when a reverse connect URL is unreachable? The server should
  attempt the connection and return an error status if it fails.
- What happens when a reverse connect fails mid-lifecycle? The server should
  re-attempt or signal the error to the application.
- What happens when `MUC_OPCUA_TIME_SYNC` is enabled but no time adapter is
  configured? The server should handle gracefully (timestamp = 0).
- What happens when the async inventory doc is missing on disk? The test
  should skip gracefully with a clear message (existing behavior retained).

## Requirements

### Functional Requirements

**US1 — Reverse Connect**

- **FR-001**: Server MUST accept a `reverse_connect_url` field in the server
  config that, when set, causes the TCP adapter to connect outbound instead of
  listening (OPC-10000-6 §7.5)
- **FR-002**: When `reverse_connect_url` is set, the server MUST NOT call
  `listen()` — it calls `connect()` on the TCP adapter instead
- **FR-003**: When `reverse_connect_url` is NULL or empty, the server MUST use
  the existing `listen()` path unchanged
- **FR-004**: `test_reverse_connect.c` MUST test that the TCP adapter
  `connect()` is called (not `listen()`) when reverse connect is configured

**US2 — Time Synchronization**

- **FR-005**: Server MUST populate `ServerTimestamp` in every outbound
  ResponseHeader when `MUC_OPCUA_TIME_SYNC` is enabled (OPC-10000-4 §A.2)
- **FR-006**: Server MUST set `ServerTimestamp` to the current time from the
  time adapter at the point the request processing begins
- **FR-007**: When `MUC_OPCUA_TIME_SYNC` is disabled, `ServerTimestamp` MUST
  remain zero (existing behavior)
- **FR-008**: `test_time_sync.c` MUST verify that a request-response cycle
  produces a non-zero ServerTimestamp when the feature is enabled

**US3 — Async Inventory Document**

- **FR-009**: `docs/conformance/async-opcua-inventory.md` MUST exist and
  contain sections for: devcontainer, codegen tests, dotnet interop, fuzz
  harnesses, schemas, and tools
- **FR-010**: `test_async_opcua_inventory.c` MUST pass by reading the document
  and verifying each required section exists

**Cross-cutting**

- **FR-011**: All 3 `/* STUB: ... */` markers MUST be removed from the test
  files
- **FR-012**: No existing test behavior must regress

### OPC UA Normative Scope

- **OPC-001**: Reverse Connect per OPC-10000-6 §7.5 (Reverse Connect)
- **OPC-002**: Time Synchronization per OPC-10000-4 §A.2 (TimestampsToReturn)
- **OPC-003**: Async conformance inventory — no OPC UA normative reference
  (project documentation)

### Scope Boundaries

- **In Scope**: Implement reverse connect server-side logic, time sync
  ServerTimestamp population, async inventory document, and replace 3 stub
  tests
- **Out of Scope**: Reverse connect client-side support, CTT compliance
  testing for time sync, full async conformance profile
- **Out of Scope**: Modifying the TCP adapter interface — add new optional
  callbacks only
- **Out of Scope**: Changing behavior when either feature flag is OFF

### Key Entities

- **Reverse Connect URL**: An endpoint string (e.g., `opc.tcp://client:4840`)
  that the server connects to instead of listening. When set, the server
  becomes the TCP initiator.
- **ServerTimestamp**: An `opcua_datetime_t` field in the OPC UA ResponseHeader
  representing when the server processed the request. Populated from the time
  adapter tick value.
- **Async Inventory Document**: A markdown document listing project features
  relevant to the OPC UA Async Services conformance profile.

## Success Criteria

### Measurable Outcomes

- **SC-001**: All 3 `/* STUB: */` markers removed from test files and
  `test_reverse_connect_requires_build_flag`/`test_time_sync_requires_build_flag`
  replaced with real behavioral tests
- **SC-002**: Reverse connect test verifies `connect()` is called (not
  `listen()`) when `reverse_connect_url` is configured
- **SC-003**: Time sync test verifies ServerTimestamp is non-zero in
  response header when `MUC_OPCUA_TIME_SYNC` is enabled
- **SC-004**: Async inventory document passes section-presence validation
- **SC-005**: All existing tests pass on all profiles with both new features
- **SC-006**: No regression when either feature flag is OFF

## Assumptions

- `MUC_OPCUA_REVERSE_CONNECT` and `MUC_OPCUA_TIME_SYNC` cmake flags already
  exist in CMakeLists.txt and are gated per profile
- The TCP adapter interface (`mu_tcp_adapter_t`) can be extended with optional
  function pointers without breaking existing adapters
- The time adapter provides a `get_tick_ms` function that returns a meaningful
  timestamp
- Server response header construction code has a single point where
  ServerTimestamp can be injected
- The async inventory document does not need to be comprehensive — it needs
  only the sections the existing test expects to be present
