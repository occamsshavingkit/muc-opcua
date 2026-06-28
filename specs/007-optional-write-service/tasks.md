# Tasks: Optional Attribute Write Service

**Input**: Design documents from `/specs/007-optional-write-service/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Tests are mandatory for protocol parsing, serialization, service dispatch, StatusCodes, and validation.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing. Protocol tests and fixtures must appear before the implementation tasks they validate.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions
- Include OPC UA part/section references in protocol task descriptions
- Include size impact tasks when a feature adds tables, buffers, stack use, heap use, or crypto dependencies

## Path Conventions

- **Public API**: `include/micro_opcua/`
- **Core protocol**: `src/core/`, `src/encoding/`, `src/services/`
- **Address space**: `src/address_space/`
- **Security interfaces**: `src/security/`
- **Platform adapters**: `platform/pico/`, `platform/arduino/`
- **Examples**: `examples/minimal_server/`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/fixtures/`, `tests/fuzz/`
- **Documentation**: `docs/adr/`, `docs/traceability/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project configuration and build options.

- [x] T001 Configure CMake option `MICRO_OPCUA_SERVICE_WRITE` in `cmake/MicroOpcUaOptions.cmake`
- [x] T002 Configure compiler flags and source file gating for Write service in `CMakeLists.txt`
- [x] T003 Add build configuration check unit test in `tests/unit/test_build_config.c`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Common definitions, interfaces, and traceability documentation.

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [x] T004 Document target OPC UA profile/conformance-unit set (Write Value Server Facet) and cited sections in `docs/traceability/007-optional-write-service.md`
- [x] T005 Define `mu_write_handler_t` and `write_handler` / `write_handler_handle` fields in `include/micro_opcua/server.h`
- [x] T006 Declare `handle_write` dispatch function in `src/core/service_dispatch.h`
- [x] T007 Add service unit test stub in `tests/unit/test_write_service.c`

**Checkpoint**: Foundation ready - user story implementation can begin.

---

## Phase 3: User Story 1 - Basic Write for Scalar Values (Priority: P1) [MVP]

**Goal**: Write a single scalar value via WriteRequest, triggering application callback.

**Independent Test**: Build with `MICRO_OPCUA_SERVICE_WRITE=ON`, invoke single-item WriteRequest for scalar Int32/Float on a read-write node, verify callback triggers with exact value and returns `Good`.

### Tests for User Story 1

> Write these tests first and confirm they fail before implementation.

- [x] T008 [P] [US1] Add binary packet fixture for single Int32 WriteRequest (citing OPC-10000-4 §5.11.4 and OPC-10000-6 §7.36) under `tests/fixtures/write_int32_req.bin`
- [x] T009 [P] [US1] Add decoder unit tests for scalar types (Int32, Float, String, Boolean) citing OPC-10000-6 §5.2 and OPC-10000-4 §5.11.4.2 in `tests/unit/test_write_decoder.c`
- [x] T010 [US1] Add integration test for single-item Write using mock callback (citing OPC-10000-4 §5.11.4) in `tests/integration/test_write_scalar.c`

### Implementation for User Story 1

- [x] T011 [P] [US1] Implement WriteValue decoder `mu_write_value_decode` (citing OPC-10000-4 §5.11.4.2 and OPC-10000-6 §7.36) in `src/encoding/binary_reader.c` and `src/encoding/binary_reader.h`
- [x] T012 [US1] Implement `handle_write` dispatch function (citing OPC-10000-4 §5.11.4) in `src/core/service_dispatch.c` and route request through `src/core/service_dispatch.c` table.
- [x] T013 [US1] Integrate application callback invocation in `handle_write` (citing OPC-10000-4 §5.11.4) in `src/core/service_dispatch.c`
- [x] T014 [US1] Measure and record flash/RAM size impact of User Story 1 in `specs/007-optional-write-service/plan.md`

**Checkpoint**: User Story 1 is independently testable and size impact is known.

---

## Phase 4: User Story 2 - Batch Write Support (Priority: P2)

**Goal**: Write multiple variables in a single WriteRequest, returning independent status codes.

**Independent Test**: Send WriteRequest with two nodes, verify callback triggers for both and response contains two status codes.

### Tests for User Story 2

- [x] T015 [P] [US2] Add binary packet fixture for batch WriteRequest (citing OPC-10000-4 §5.11.4.2 and OPC-10000-6 §7.36) in `tests/fixtures/write_batch_req.bin`
- [x] T016 [P] [US2] Add unit test for batch write loop and response status array length (citing OPC-10000-4 §5.11.4.2) in `tests/unit/test_write_service.c`
- [x] T017 [US2] Add integration test for batch writes with mix of success/failure (citing OPC-10000-4 §5.11.4.2) in `tests/integration/test_write_batch.c`

### Implementation for User Story 2

- [x] T018 [US2] Implement loop processing in `handle_write` (citing OPC-10000-4 §5.11.4.2) to decode and dispatch multiple `WriteValue` items in `src/core/service_dispatch.c`
- [x] T019 [US2] Measure and record flash/RAM size impact of User Story 2 in `specs/007-optional-write-service/plan.md`

**Checkpoint**: User Stories 1 and 2 both work independently.

---

## Phase 5: User Story 3 - Write Constraints and Error Handling (Priority: P3)

**Goal**: Return spec-mandated StatusCodes for non-Value attributes, indexRange, or disabled Write service.

**Independent Test**: Verify writing to `BrowseName` returns `Bad_AttributeIdInvalid`, writing with `indexRange` returns `Bad_WriteNotSupported`, and disabling Write service returns `Bad_ServiceUnsupported`.

### Tests for User Story 3

- [x] T020 [P] [US3] Add unit test for empty `nodesToWrite` array returning `Bad_NothingToDo` (citing OPC-10000-4 §5.11.4.3) in `tests/unit/test_write_errors.c`
- [x] T021 [P] [US3] Add unit test for non-Value attribute write returning `Bad_AttributeIdInvalid` (citing OPC-10000-4 §5.11.4.2) in `tests/unit/test_write_errors.c`
- [x] T022 [P] [US3] Add unit test for indexRange write returning `Bad_WriteNotSupported` (citing OPC-10000-4 §5.11.4.2) in `tests/unit/test_write_errors.c`
- [x] T023 [US3] Add integration test validating full error behavior (citing OPC-10000-4 §5.11.4.2 and §5.11.4.3) under `tests/integration/test_write_validation.c`

### Implementation for User Story 3

- [x] T024 [US3] Implement validation checks (attribute ID check, indexRange length check, empty array check, citing OPC-10000-4 §5.11.4.2) in `handle_write` inside `src/core/service_dispatch.c`
- [x] T025 [US3] Measure and record final size impact in `specs/007-optional-write-service/plan.md`

**Checkpoint**: Selected user stories are independently functional.

---

## Phase 6: Compliance, Size, and Polish

**Purpose**: Cross-cutting validation required before completion.

- [x] T026 Run all unit and integration tests in `Makefile`
- [x] T027 Run AddressSanitizer and UndefinedBehaviorSanitizer builds in `Makefile`
- [x] T028 Run formatting in `Makefile` and `clang-format`
- [x] T029 Run embedded size measurement in `scripts/measure_size.sh`
- [x] T030 Complete documentation of traceability mappings in `docs/traceability/007-optional-write-service.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup completion; blocks all stories
- **User Stories (Phase 3+)**: Depend on Foundational phase completion
- **Compliance/Polish**: Depends on all desired user stories being complete

### Within Each User Story

- Fixtures and tests before implementation
- Public API before implementation files
- Encoders/decoders before service dispatch that consumes them
- StatusCode mapping before integration behavior that returns it
- Traceability and size records before checkpoint completion

### Parallel Opportunities

- Setup tasks marked [P] can run in parallel
- Foundational interface tasks marked [P] can run in parallel
- Tests for one story marked [P] can run in parallel
- Different user stories can proceed in parallel after the foundational phase if they touch different files and preserve independent testability
