# Tasks: Core Feature Expansion

**Input**: Design documents from `/specs/009-core-feature-expansion/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, quickstart.md

**Tests**: Tests are mandatory for protocol parsing, serialization, service dispatch, StatusCodes, SecureChannel/session state, address-space behavior, security behavior, and wire-visible changes.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Build options and feature flag configurations.

- [x] T001 Configure CMake option `MICRO_OPCUA_SERVICE_WRITE` in `cmake/MicroOpcUaOptions.cmake`
- [x] T002 Configure CMake option `MICRO_OPCUA_MULTIPLE_CONNECTIONS` in `cmake/MicroOpcUaOptions.cmake`
- [x] T003 Configure CMake option `MICRO_OPCUA_EVENTS` in `cmake/MicroOpcUaOptions.cmake`
- [x] T004 Add feature build configurations and source gating in `CMakeLists.txt`
- [x] T005 Create empty placeholder directories for new headers and sources

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core interfaces and API structures.

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [x] T006 Define public error/status mappings for write operations in `include/micro_opcua/status.h`
- [x] T007 Define public error/status mappings for certificate verification in `include/micro_opcua/status.h`
- [x] T008 Define target OPC UA profiles and conformance units in `docs/traceability/009-core-feature-expansion.md`
- [x] T009 Add size report test updates to track memory consumption in `tests/unit/test_size_report.c`

**Checkpoint**: Foundation ready - user story implementation can begin.

---

## Phase 3: User Story 1 - Write Service (Priority: P1) [MVP]

**Goal**: Support writing variable value attributes.

**Independent Test**: Connect an OPC UA client and write a new value to a mutable variable.

### Tests for User Story 1

- [x] T010 [P] [US1] Add unit test for Write request decoder and encoder (citing OPC-10000-6 §5.10.4) in `tests/unit/test_write_decoder.c`
- [x] T011 [P] [US1] Add integration test for Write service dispatching and write handler execution (citing OPC-10000-4 §5.10.4) in `tests/integration/test_write_service.c`

### Implementation for User Story 1

- [x] T012 [P] [US1] Implement Write service decoder `mu_binary_read_write_request` (citing OPC-10000-6 §5.10.4) in `src/encoding/binary_reader.c`
- [x] T013 [US1] Implement Write service dispatching and callback execution (citing OPC-10000-4 §5.10.4) in `src/core/service_dispatch.c`
- [x] T014 [US1] Measure size impact of Write service implementation in `specs/009-core-feature-expansion/plan.md`

**Checkpoint**: User Story 1 works independently.

---

## Phase 4: User Story 2 - Multiple Concurrent Connections (Priority: P1)

**Goal**: Support concurrent client connections without heap allocation.

**Independent Test**: Connect multiple clients simultaneously and read values.

### Tests for User Story 2

- [x] T015 [P] [US2] Add unit test for connection list multiplexing (citing OPC-10000-4 §5.13) in `tests/unit/test_connection_multiplex.c`
- [x] T016 [P] [US2] Add integration test validating concurrent connection limits and clean timeouts (citing OPC-10000-4 §5.13) in `tests/integration/test_multiple_connections.c`

### Implementation for User Story 2

- [x] T017 [US2] Implement active connections array management and multiplexing (citing OPC-10000-4 §5.13) in `src/core/server.c`
- [x] T018 [US2] Measure size impact of concurrent connection multiplexer in `specs/009-core-feature-expansion/plan.md`

**Checkpoint**: User Story 2 works independently.

---

## Phase 5: User Story 3 - Modern Security Policies (Priority: P2)

**Goal**: Support Aes128_Sha256_RsaOaep and Aes256_Sha256_RsaPss policies.

**Independent Test**: Secure channel handshake using the new policies.

### Tests for User Story 3

- [x] T019 [P] [US3] Add unit test validating policy match for AES-128 and AES-256 secure channel setup (citing OPC-10000-7 §6.2) in `tests/unit/test_security_policy_selection.c`
- [x] T020 [P] [US3] Add integration test verifying secure channel creation with modern policies (citing OPC-10000-7 §6.2) in `tests/integration/test_secure_handshake_modern.c`

### Implementation for User Story 3

- [x] T021 [US3] Register security policies `Aes128_Sha256_RsaOaep` and `Aes256_Sha256_RsaPss` (citing OPC-10000-7 §6.2) in `src/security/security_policy.c`
- [x] T022 [US3] Measure size impact of new policies in `specs/009-core-feature-expansion/plan.md`

**Checkpoint**: User Story 3 works independently.

---

## Phase 6: User Story 4 - X.509 Certificate User Authentication (Priority: P2)

**Goal**: Support CertificateIdentityToken validation during ActivateSession.

**Independent Test**: Connect using a client certificate and verify session activation.

### Tests for User Story 4

- [x] T023 [P] [US4] Add unit test for `CertificateIdentityToken` decoder (citing OPC-10000-6 §7.38.3) in `tests/unit/test_cert_token_decoder.c`
- [x] T024 [P] [US4] Add integration test for certificate-based user authentication (citing OPC-10000-4 §7.36.4) in `tests/integration/test_user_auth_certificate.c`

### Implementation for User Story 4

- [x] T025 [P] [US4] Implement binary decoder for `CertificateIdentityToken` (citing OPC-10000-6 §7.38.3) in `src/encoding/binary_reader.c`
- [x] T026 [US4] Verify client certificate signature in `ActivateSession` (citing OPC-10000-4 §7.36.4) in `src/core/service_dispatch.c`
- [x] T027 [US4] Measure size impact of certificate user auth in `specs/009-core-feature-expansion/plan.md`

**Checkpoint**: User Story 4 works independently.

---

## Phase 7: User Story 5 - Alarms & Conditions (Events) (Priority: P3)

**Goal**: Support alarm events publishing via subscriptions.

**Independent Test**: Subscribe to events, trigger an alarm condition, and verify alarm event receipt.

### Tests for User Story 5

- [x] T028 [P] [US5] Add unit test for event notification payload construction and serializing (citing OPC-10000-4 §5.12.1) in `tests/unit/test_event_serializer.c`
- [x] T029 [P] [US5] Add integration test for Alarm event generation and delivery (citing OPC-10000-4 §5.12.1 and OPC-10000-9) in `tests/integration/test_event_notifications.c`

### Implementation for User Story 5

- [x] T030 [P] [US5] Implement event queue buffer management (citing OPC-10000-4 §5.12.1) in `src/services/subscription.c`
- [x] T031 [US5] Generate and serialize alarm events during subscription polling (citing OPC-10000-4 §5.12.1 and OPC-10000-9) in `src/services/subscription.c`
- [x] T032 [US5] Measure size impact of event notification engine in `specs/009-core-feature-expansion/plan.md`

**Checkpoint**: User Story 5 works independently.

---

## Phase 8: Compliance, Size, and Polish

**Purpose**: Verification and polish before feature completion.

- [x] T033 Run all unit and integration tests in `Makefile`
- [x] T034 Run AddressSanitizer and UndefinedBehaviorSanitizer builds in `Makefile`
- [x] T035 Run formatting with clang-format
- [x] T036 Measure flash/RAM size impact and compare with plan.md budget
- [x] T037 Complete documentation of traceability mappings in `docs/traceability/009-core-feature-expansion.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup completion; blocks all stories
- **User Stories (Phase 3+)**: Depend on Foundational phase completion
- **Compliance/Polish**: Depends on all user stories being complete
