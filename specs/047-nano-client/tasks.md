---
description: "Task list for Nano Client — Discovery + Read"
---

# Tasks: Nano Client — Discovery + Read

**Input**: Design documents from `specs/047-nano-client/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Tests are mandatory for protocol parsing, serialization, service
dispatch, StatusCodes, SecureChannel/session state, and wire-visible behavior.

**Organization**: Tasks are grouped by user story to enable independent
implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 (Connect), US2 (Read), US3 (Browse)
- Include exact file paths in descriptions
- Include OPC UA part/section references in protocol task descriptions
- Include size impact tasks

## Path Conventions

- **Public API**: `include/muc_opcua/client.h` (NEW)
- **Client implementation**: `src/client/` (NEW directory)
- **Tests**: `tests/unit/test_nano_client.c`
- **Shared infrastructure**: `src/encoding/`, `src/core/`, `src/security/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: CMake profile gating, directory structure, public headers

- [ ] T001 [P] Add `MUC_OPCUA_CLIENT_PROFILE` CMake option to `src/CMakeLists.txt`
  following the server's `MUC_OPCUA_PROFILE` pattern — nano profile enables only
  client connection + read + browse services. Reject illegal client/server profile
  combinations with `#error` in `include/muc_opcua/features.h`
- [ ] T002 [P] Create `include/muc_opcua/client.h` with forward declarations,
  `mu_client_state_t` enum (DISCONNECTED, RESOLVING, CONNECTING, CHANNEL_OPENING,
  CHANNEL_OPEN, SESSION_CREATING, ACTIVATING, ACTIVE, CLOSING, ERROR), and
  `mu_client_transport_t` adapter interface typedef
- [ ] T003 [P] Create `src/client/` directory structure with stub .c files for
  client.c, client_secure_channel.c, client_session.c, client_read.c,
  client_browse.c, client_state.c, client_errors.c
- [ ] T004 [P] Define `mu_client_config_t`, `mu_client_t`, `mu_identity_token_t`,
  and `mu_client_transport_t` in `include/muc_opcua/client.h` per contracts/api.md

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure before user story work

- [ ] T005 [P] Implement client state machine in `src/client/client_state.c`
  — state transitions per data-model.md, validation of legal transitions,
  error state on any fault. Include timeout/watchdog for each connected state
  to detect connection loss (FR-015)
- [ ] T006 [P] Implement `mu_client_init` in `src/client/client.c` — validate
  config, store transport pointer, initialize state to DISCONNECTED
- [ ] T007 [P] Implement host TCP adapter (`mu_client_transport_t`) using POSIX
  sockets in `src/platform/posix_client_transport.c` — connect, send, recv,
  close with configurable timeout
- [ ] T008 [P] Implement service type ID sharing in `src/core/service_ids.c`
  — share request/response type ID tables between server and client instead
  of duplicating (~100 bytes ROM saving). Or document why duplication is
  necessary (LOW-002)

## Phase 3: User Story 1 — Discover and Connect (Priority: P1) [MVP]

**Goal**: Client can connect to a server, open SecureChannel, and establish a session

**Independent Test**: Client connects to muc-opcua server, opens channel and session

### Implementation for User Story 1

- [ ] T009 [US1] Implement `mu_client_connect` in `src/client/client.c` — resolve
  hostname, TCP connect via transport, send Hello, await Acknowledge
  (OPC-10000-6 §7.1.2.3/§7.1.2.4). This is the top-level orchestrator that
  chains T006+T007 + T011 into a single connect call (HIGH-003)
- [ ] T010 [US1] Implement `mu_client_get_endpoints` in `src/client/client.c`
  — construct GetEndpoints request, send via SecureChannel, parse response
  (OPC-10000-4 §5.4.2)
- [ ] T011 [US1] Implement OpenSecureChannel in `src/client/client_secure_channel.c`
  — construct OPN request with asymmetric security header (Basic256Sha256),
  send, parse OPN response, extract channel ID and security token
  (OPC-10000-4 §5.5.2). Adapt server's crypto stubs for client role
  (asymmetric encrypt/sign outgoing, decrypt/verify incoming) (FR-013)
- [ ] T012 [US1] Implement CloseSecureChannel in `src/client/client_secure_channel.c`
  — construct and send close message (OPC-10000-4 §5.5.3)
- [ ] T013 [US1] Implement `mu_client_create_session` in `src/client/client_session.c`
  — construct CreateSession request with client description, send, parse
  response for session ID and auth token (OPC-10000-4 §5.6.2)
- [ ] T014 [US1] Implement `mu_client_activate_session` in `src/client/client_session.c`
  — construct ActivateSession request with `mu_identity_token_t` (anonymous),
  send, parse response (OPC-10000-4 §5.6.3)
- [ ] T015 [US1] Implement `mu_client_close_session` in `src/client/client.c`
  — send CloseSession request, delete subscription flag to false, await response
  (OPC-10000-4 §5.6.4)
- [ ] T016 [US1] Implement `mu_client_disconnect` in `src/client/client.c`
  — close SecureChannel via T012, close TCP via transport, reset state
  to DISCONNECTED (separate API from close_session per MEDIUM-006)
- [ ] T017 [US1] Implement connection loss detection in `src/client/client.c`
  and `src/client/client_state.c` — detect SecureChannel timeout, TCP
  disconnect, and invalid sequence numbers; transition to ERROR state
  and return Bad_ConnectionClosed or Bad_Timeout (FR-015)
- [ ] T018 [P] [US1] Add interop test in `tests/unit/test_nano_client.c` — connect
  to muc-opcua server with Basic256Sha256, get endpoints, open channel,
  create + activate session, close session, disconnect — verify all
  StatusCodes are GOOD
- [ ] T019 [P] [US1] Add SecurityPolicy None interop test in
  `tests/unit/test_nano_client.c` — connect with None, verify channel opens
  without crypto (LOW-005)
- [ ] T020 [P] [US1] Add CloseSecureChannel-only test — open channel then
  close without creating session, verify clean disconnect
  (MEDIUM-003)
- [ ] T021 [P] [US1] Add connection loss test — simulate TCP drop, verify
  client returns Bad_ConnectionClosed (FR-015)

## Phase 4: User Story 2 — Read Variable Values (Priority: P1)

**Goal**: Client can read variable values from a connected server

**Independent Test**: Client reads a known variable and verifies value

### Implementation for User Story 2

- [ ] T022 [US2] Implement `mu_client_read` in `src/client/client_read.c`
  — construct ReadRequest with node IDs, timestampsToReturn, maxAge;
  send; parse ReadResponse; return values with StatusCodes
  (OPC-10000-4 §5.10.2)
- [ ] T023 [US2] Handle error responses in client_read — Bad_NodeIdUnknown,
  Bad_NodeIdInvalid per spec acceptance criteria
- [ ] T024 [P] [US2] Add read tests in `tests/unit/test_nano_client.c` — read
  known variable and verify value; read non-existent node and verify error

## Phase 5: User Story 3 — Browse the Address Space (Priority: P2)

**Goal**: Client can browse the server's address space

**Independent Test**: Client browses root node and verifies child nodes

### Implementation for User Story 3

- [ ] T025 [US3] Implement `mu_client_browse` in `src/client/client_browse.c`
  — construct BrowseRequest with nodeId, direction, nodeClassMask, resultMask;
  send; parse BrowseResponse; return references (OPC-10000-4 §5.8.2)
- [ ] T026 [US3] Implement `mu_client_browse_next` in `src/client/client_browse.c`
  — if continuation point in response, send BrowseNextRequest to get
  next batch (OPC-10000-4 §5.8.3)
- [ ] T027 [US3] Implement `mu_client_translate_browse_paths_to_node_ids` in
  `src/client/client_browse.c` — resolve relative paths to NodeIds
  (OPC-10000-4 §5.8.4)
- [ ] T028 [P] [US3] Add browse tests in `tests/unit/test_nano_client.c` — browse
  Objects folder, verify child nodes; test BrowseNext with continuation;
  test TranslateBrowsePaths with known path

## Phase 6: Compliance, Size, and Polish

**Purpose**: Cross-cutting validation

- [ ] T029 Run host unit and interop tests — `ctest -R test_nano_client`
- [ ] T030 Run full test suite to confirm no server regression
- [ ] T031 Run formatting and static analysis (clang-format, cppcheck)
- [ ] T032 Run embedded cross-compile for Pico SDK with `MUC_OPCUA_CLIENT_PROFILE=nano`
- [ ] T033 Measure flash/RAM impact and compare with plan.md budget
- [ ] T034 Measure round-trip timing — full connect-to-disconnect cycle on host
  should complete under 5 seconds (SC-001, MEDIUM-001)
- [ ] T035 Verify zero-heap — compile client with `MUC_OPCUA_ALLOW_HEAP=OFF`,
  confirm no `malloc`/`calloc` linked in client code (SC-004, MEDIUM-002)
- [ ] T036 Gate unsupported services — add `#if MUC_OPCUA_CLIENT_PROFILE==nano`
  guards to ensure Write, Subscribe, Call, HistoryRead, etc. are either
  not compiled or return Bad_NotImplemented (OPC-003, HIGH-004)
- [ ] T037 Update docs/traceability/ with client service-to-section mappings
- [ ] T038 Verify Nano client state machine — each state transition exercised
  in at least one test (including RESOLVING and CONNECTING intermediate
  states per MEDIUM-004)

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup
- **US1 — Connect (Phase 3)**: Depends on Foundational
- **US2 — Read (Phase 4)**: Depends on US1 (needs active session)
- **US3 — Browse (Phase 5)**: Depends on US1 (needs active session)
- **Compliance/Polish**: Depends on US1 + US2 + US3

### Parallel Opportunities

- T001, T002, T003, T004 can run in parallel
- T005, T006, T007, T008 can run in parallel
- T018, T019, T020, T021 can run alongside T009-T017 after T006+T007
- T024 can run alongside T022-T023
- T028 can run alongside T025-T027

### Implementation Checkpoints

After each phase completes, run `ctest -R test_nano_client` to verify
no regressions before moving to the next phase (LOW-003).

### MVP Scope (Phase 3 only)

User Story 1 is the MVP — client connects, opens channel, establishes session,
and handles connection loss. US2 (Read) and US3 (Browse) add value on top
but are not required for a minimal working client.
