# Tasks: NodeManagement Services

**Input**: Design documents from `/specs/014-node-management/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Tests are mandatory for protocol parsing, serialization, service dispatch, StatusCodes, SecureChannel/session state, address-space behavior, security behavior, and wire-visible changes.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing. Protocol tests and fixtures must appear before the implementation tasks they validate.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, build tooling, and traceability structure

- [ ] T001 Define dynamic node and reference capacities in `include/micro_opcua/config.h` (e.g. `MU_MAX_DYNAMIC_NODES`, `MU_MAX_DYNAMIC_REFERENCES`)
- [ ] T002 Add `mu_dynamic_address_space_t` struct to `src/core/server_internal.h` and attach to `mu_server_t`
- [ ] T003 Initialize dynamic address space memory blocks during `mu_server_init` in `src/core/server.c`

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before user story work

- [ ] T004 Document target OPC UA profile (NodeManagement Service Set) and cited sections in `docs/traceability/014-node-management.md`
- [ ] T005 [P] Add new service handlers to the dispatch table in `src/core/service_dispatch.c` for `AddNodes`, `DeleteNodes`, `AddReferences`, `DeleteReferences`

---

## Phase 3: User Story 1 - Add and Delete Nodes (Priority: P1) [MVP]

**Goal**: Authorized clients need to add new instances (nodes) to the address space at runtime, and later remove them.

**Independent Test**: Can be fully tested by a client sending an `AddNodesRequest` to create a node, verifying it appears in Browse/Read, and then sending a `DeleteNodesRequest` to remove it.

### Tests for User Story 1

- [ ] T006 [P] [US1] Add round-trip unit test for AddNodes/DeleteNodes encoders/decoders in `tests/unit/test_node_management.c`
- [ ] T007 [P] [US1] Add boundary/error unit test for AddNodes/DeleteNodes (OOM, InvalidNodeClass) in `tests/unit/test_node_management_errors.c`
- [ ] T008 [P] [US1] Add unit test for searching/browsing dynamic nodes alongside static nodes in `tests/unit/test_address_space.c`

### Implementation for User Story 1

- [ ] T009 [P] [US1] Implement `mu_add_nodes_request_decode`, `mu_add_nodes_response_encode`, `mu_delete_nodes_request_decode`, `mu_delete_nodes_response_encode` in `src/services/node_management.c`
- [ ] T010 [US1] Implement `mu_add_nodes_process` and `mu_delete_nodes_process` in `src/services/node_management.c` to update the dynamic address space
- [ ] T011 [US1] Update `mu_address_space_find_node` in `src/address_space/address_space.c` to search the dynamic node array as a fallback
- [ ] T012 [US1] Enforce authorization check (or admin flag) and return `Bad_UserAccessDenied` if unauthorized
- [ ] T013 [US1] Map unsupported cases to cited OPC UA StatusCodes in `src/core/status.c`

---

## Phase 4: User Story 2 - Add and Delete References (Priority: P2)

**Goal**: Authorized clients need to create new relationships (references) between existing nodes at runtime, and later remove them.

**Independent Test**: Can be fully tested by creating a reference between two nodes using `AddReferencesRequest` and observing the new relationship in a `BrowseResponse`, then removing it with `DeleteReferencesRequest`.

### Tests for User Story 2

- [ ] T014 [P] [US2] Add round-trip unit test for AddReferences/DeleteReferences encoders/decoders in `tests/unit/test_node_management.c`
- [ ] T015 [P] [US2] Add boundary/error unit test for AddReferences/DeleteReferences (OOM, TargetNotExists) in `tests/unit/test_node_management_errors.c`
- [ ] T016 [P] [US2] Add unit test for Browse service yielding dynamic references in `tests/unit/test_browse.c`

### Implementation for User Story 2

- [ ] T017 [P] [US2] Implement `mu_add_references_request_decode`, `mu_add_references_response_encode`, `mu_delete_references_request_decode`, `mu_delete_references_response_encode` in `src/services/node_management.c`
- [ ] T018 [US2] Implement `mu_add_references_process` and `mu_delete_references_process` in `src/services/node_management.c`
- [ ] T019 [US2] Update Browse service in `src/services/browse.c` to iterate over dynamic references in addition to static references
- [ ] T020 [US2] Ensure `DeleteNodes` implementation also deletes all target references to the deleted node in the dynamic array

---

## Phase 5: Compliance, Size, and Polish

**Purpose**: Cross-cutting validation required before completion

- [ ] T021 Run host unit and integration tests (ctest)
- [ ] T022 Run formatting and static analysis (clang-format, cppcheck)
- [ ] T023 Measure flash/RAM impact using `./scripts/measure_size.sh` and compare with plan.md budget
- [ ] T024 Verify docs/traceability/ maps each implementation and test file to OPC UA sections
- [ ] T025 Update `docs/integration-guide.md` with dynamic address space configuration notes

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup completion; blocks all stories
- **User Stories (Phase 3+)**: Depend on Foundational phase completion
- **Compliance/Polish**: Depends on all desired user stories being complete
