# Tasks: Historical Access (HA)

**Input**: Design documents from `/specs/017-historical-access/`
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

- [x] T001 Define `MICRO_OPCUA_SERVICE_HISTORY` in `include/micro_opcua/config.h`.
- [x] T002 [P] Add `test_history.c` under `tests/unit/` and integrate it into `src/CMakeLists.txt`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before user story work

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [x] T003 Document cited OPC UA sections for Historical Access in `docs/traceability/files-to-sections.md`.
- [x] T004 Define `mu_historical_data_point_t` and `mu_history_adapter_t` structs in `include/micro_opcua/services/history.h`.
- [x] T005 [P] Add `history_adapter` to `mu_server_t` and `mu_server_config_t` structs in `src/core/server_internal.h` and `include/micro_opcua/config.h`.

**Checkpoint**: Foundation ready - user story implementation can begin.

---

## Phase 3: User Story 1 - Read Historical Data (Priority: P1) [MVP]

**Goal**: As an OPC UA client, I want to query a history of a variable's past values so I can display trends and perform analysis.

**Independent Test**: Can be tested by pre-populating historical data via the backend adapter, then having a client call `HistoryRead` and verifying the data points.

### Tests for User Story 1

> Write these tests first and confirm they fail before implementation.

- [x] T006 [US1] Write test for decoding a `HistoryReadRequest` in `tests/unit/test_history.c`.
- [x] T007 [US1] Write test for encoding a `HistoryReadResponse` in `tests/unit/test_history.c`.
- [x] T008 [US1] Write test for `HistoryRead` dispatch invoking the adapter's `read_raw_modified` callback in `tests/unit/test_history.c`.
- [x] T009 [US1] Write test for pagination (continuation point output) in `tests/unit/test_history.c`.

### Implementation for User Story 1

- [x] T010 [US1] Implement `mu_history_read_request_decode` and `mu_history_read_response_encode` in `src/services/history.c`.
- [x] T011 [US1] Implement `mu_history_read_process` in `src/services/history.c` to parse parameters and call the adapter.
- [x] T012 [US1] Update `src/core/service_dispatch.c` to dispatch `HistoryRead` requests to `mu_history_read_process`.

---

## Phase 4: User Story 2 - Update Historical Data (Priority: P2)

**Goal**: As an OPC UA client, I want to insert, replace, or delete historical data points so I can correct erroneous readings or backfill data.

**Independent Test**: Can be tested by having a client call `HistoryUpdate` to insert data, and verifying the data is present in the store via `HistoryRead`.

### Tests for User Story 2

> Write these tests first and confirm they fail before implementation.

- [x] T013 [US2] Write test for decoding a `HistoryUpdateRequest` in `tests/unit/test_history.c`.
- [x] T014 [US2] Write test for `HistoryUpdate` dispatch invoking the adapter's `update_data` callback with Insert/Replace types in `tests/unit/test_history.c`.

### Implementation for User Story 2

- [x] T015 [US2] Implement `mu_history_update_request_decode` and `mu_history_update_response_encode` in `src/services/history.c`.
- [x] T016 [US2] Implement `mu_history_update_process` in `src/services/history.c` to handle Insert, Replace, and Delete, mapping to the adapter `update_data` callback.
- [x] T017 [US2] Update `src/core/service_dispatch.c` to dispatch `HistoryUpdate` requests to `mu_history_update_process`.

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Final review, cleanup, size verification, and examples

- [x] T018 Verify all unsupported `HistoryReadDetails` (e.g. Event, Processed) return `MU_STATUS_BAD_HISTORY_OPERATION_UNSUPPORTED`.
- [x] T019 Integrate a mock history adapter logic into `examples/minimal_server/main.c` (optional, under `#ifdef MICRO_OPCUA_SERVICE_HISTORY`).
- [x] T020 Run `tests/unit/test_traceability_docs.c` to ensure all new `.c` files (`history.c`) are mapped.
