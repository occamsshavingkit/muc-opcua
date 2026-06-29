# Tasks: Advanced Alarms & Conditions

**Input**: Design documents from `/specs/016-advanced-alarms-and-conditions/`
**Prerequisites**: plan.md (required), spec.md (required for user stories),
research.md, data-model.md, quickstart.md

**Tests**: Tests are mandatory for protocol parsing, serialization, service
dispatch, StatusCodes, SecureChannel/session state, address-space behavior,
security behavior, and wire-visible changes. Non-protocol documentation-only tasks
may omit tests when the feature specification explicitly allows it.

**Organization**: Tasks are grouped by user story to enable independent
implementation and testing. Protocol tests and fixtures must appear before the
implementation tasks they validate.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, build tooling, and traceability structure

- [X] T001 Define `MICRO_OPCUA_SERVICE_ALARMS_CONDITIONS` in `include/micro_opcua/config.h`.
- [X] T002 [P] Add `test_alarms_conditions.c` under `tests/unit/` and integrate it into `CMakeLists.txt`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before user story work

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [X] T003 Document cited OPC UA sections for Alarms and Conditions in `docs/traceability/files-to-sections.md`.
- [X] T004 Define `mu_condition_t` struct and `mu_condition_id_t` in `include/micro_opcua/services/alarms_conditions.h`.
- [X] T005 [P] Add `conditions` array to `mu_server_t` struct in `src/core/server_internal.h` and update storage size calculations in `config.h`.

**Checkpoint**: Foundation ready - user story implementation can begin.

---

## Phase 3: User Story 1 - Acknowledgeable Condition State Transitions (Priority: P1) [MVP]

**Goal**: As an OPC UA client operator, I want to receive notifications when an alarm enters an Active state, and I want to be able to acknowledge it so that other operators know the issue is being handled.

**Independent Test**: Can be independently tested by generating an active alarm on the server, having a client subscribe to events, and calling the `Acknowledge` method to observe the state transition.

### Tests for User Story 1

> Write these tests first and confirm they fail before implementation.

- [X] T006 [US1] Write test for setting a condition active and verify Event triggered in `tests/unit/test_alarms_conditions.c`.
- [X] T007 [US1] Write test for Acknowledge method call verifying state change to Acked in `tests/unit/test_alarms_conditions.c`.
- [X] T008 [US1] Write test for Invalid Condition Id during Acknowledge method call in `tests/unit/test_alarms_conditions.c`.

### Implementation for User Story 1

- [X] T009 [US1] Implement `mu_alarms_set_active` API in `src/services/alarms_conditions.c` to mutate active state and fire event.
- [X] T010 [US1] Implement `mu_alarms_conditions_method_dispatch` in `src/services/alarms_conditions.c` to intercept `Acknowledge` method calls.
- [X] T011 [US1] Update `src/core/service_dispatch.c` to conditionally call `mu_alarms_conditions_method_dispatch` for Call requests.

---

## Phase 4: User Story 2 - Dialog Conditions (Priority: P2)

**Goal**: As a server process, I want to present a Dialog Condition to clients when user input (e.g., confirmation) is required to proceed, so that critical actions are explicitly approved.

**Independent Test**: Can be tested by triggering a Dialog Condition on the server, verifying the client receives it, and having the client call `Respond` to answer the dialog.

### Tests for User Story 2

> Write these tests first and confirm they fail before implementation.

- [X] T012 [US2] Write test for triggering a Dialog Condition in `tests/unit/test_alarms_conditions.c`.
- [X] T013 [US2] Write test for successful `Respond` method call to a Dialog Condition in `tests/unit/test_alarms_conditions.c`.

### Implementation for User Story 2

- [X] T014 [US2] Define `mu_dialog_condition_t` structure (inheriting condition state) in `include/micro_opcua/services/alarms_conditions.h`.
- [X] T015 [US2] Implement `mu_alarms_trigger_dialog` API in `src/services/alarms_conditions.c`.
- [X] T016 [US2] Expand `mu_alarms_conditions_method_dispatch` to handle the `Respond` method.

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Final review, cleanup, size verification, and examples

- [X] T017 Verify all failure paths return proper `MU_STATUS_BAD_*` codes.
- [X] T018 Integrate alarms logic into `examples/minimal_server/main.c` (optional, under `#ifdef`).
- [X] T019 Ensure `mu_server_storage` size check passes (`MU_MAX_CONDITIONS` logic correctly applies to `MU_SERVER_STORAGE_BYTES`).
- [X] T020 Run `tests/unit/test_traceability_docs.c` to ensure all new `.c` files are mapped.
