---

description: "Task list template for feature implementation"
---

# Tasks: [FEATURE NAME]

**Input**: Design documents from `/specs/[###-feature-name]/`
**Prerequisites**: plan.md (required), spec.md (required for user stories),
research.md, data-model.md, contracts/

**Tests**: Tests are mandatory for protocol parsing, serialization, service
dispatch, StatusCodes, SecureChannel/session state, address-space behavior,
security behavior, and wire-visible changes. Non-protocol documentation-only tasks
may omit tests when the feature specification explicitly allows it.

**Organization**: Tasks are grouped by user story to enable independent
implementation and testing. Protocol tests and fixtures must appear before the
implementation tasks they validate.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions
- Include OPC UA part/section references in protocol task descriptions
- Include size impact tasks when a feature adds tables, buffers, stack use, heap
  use, or crypto dependencies

## Path Conventions

- **Public API**: `include/micro_opcua/`
- **Core protocol**: `src/core/`, `src/encoding/`, `src/services/`
- **Address space**: `src/address_space/`
- **Security interfaces**: `src/security/`
- **Platform adapters**: `platform/pico/`, `platform/arduino/`
- **Examples**: `examples/minimal_server/`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/fixtures/`,
  `tests/fuzz/`
- **Documentation**: `docs/adr/`, `docs/traceability/`

<!--
  ============================================================================
  IMPORTANT: The tasks below are SAMPLE TASKS for illustration only.

  The /speckit-tasks command MUST replace these with actual tasks based on:
  - User stories from spec.md (with priorities P1, P2, P3...)
  - OPC UA normative scope from spec.md and research.md
  - Constitution Check decisions from plan.md
  - Entities/data model from data-model.md
  - Contracts, fixtures, and generated traceability artifacts

  DO NOT keep these sample tasks in the generated tasks.md file.
  ============================================================================
-->

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, build tooling, and traceability structure

- [ ] T001 Create CMake project structure from plan.md
- [ ] T002 Configure C11 host build and warnings-as-errors in CMakeLists.txt
- [ ] T003 [P] Configure clang-format and cppcheck
- [ ] T004 [P] Add Unity or CMocka test harness under tests/unit/
- [ ] T005 [P] Add traceability table skeleton in docs/traceability/
- [ ] T006 [P] Add embedded cross-compile target for Pico SDK or documented stub

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before user story work

**CRITICAL**: No user story implementation can begin until this phase is complete.

- [ ] T007 Document target OPC UA profile/conformance-unit set from OPC-10000-7
  in docs/traceability/[feature].md
- [ ] T008 Document cited OPC UA sections for each supported and unsupported
  behavior in docs/traceability/[feature].md
- [ ] T009 [P] Define public result/status API in include/micro_opcua/status.h
- [ ] T010 [P] Define caller-provided buffer/config API in include/micro_opcua/config.h
- [ ] T011 [P] Define platform adapter interfaces for TCP, time, entropy, and
  persistence in include/micro_opcua/platform.h
- [ ] T012 Establish size budget measurement command in CMake or scripts/
- [ ] T013 Add malformed-input fixture directory for this feature under tests/fixtures/

**Checkpoint**: Foundation ready - user story implementation can begin.

---

## Phase 3: User Story 1 - [Title] (Priority: P1) [MVP]

**Goal**: [Brief description of what this story delivers]

**Independent Test**: [How to verify this story works on its own]

### Tests for User Story 1

> Write these tests first and confirm they fail before implementation.

- [ ] T014 [P] [US1] Add binary fixture for [OPC UA type/message] citing
  OPC-10000-[part] [section] in tests/fixtures/[name].bin
- [ ] T015 [P] [US1] Add round-trip unit test for [encoder/decoder] in
  tests/unit/test_[name].c
- [ ] T016 [P] [US1] Add boundary/error unit test for [malformed input] and
  expected StatusCode in tests/unit/test_[name]_errors.c
- [ ] T017 [P] [US1] Add fuzz target for [decoder/state machine] in
  tests/fuzz/fuzz_[name].c

### Implementation for User Story 1

- [ ] T018 [P] [US1] Implement [public API] in include/micro_opcua/[name].h
- [ ] T019 [P] [US1] Implement [core type/encoder] in src/encoding/[name].c
- [ ] T020 [US1] Implement [service/state transition] in src/services/[name].c
- [ ] T021 [US1] Map unsupported cases to cited OPC UA StatusCodes in
  src/core/status.c
- [ ] T022 [US1] Update docs/traceability/[feature].md with implementation and
  test file mappings
- [ ] T023 [US1] Measure and record flash/RAM impact in plan.md or
  docs/traceability/[feature].md

**Checkpoint**: User Story 1 is independently testable and size impact is known.

---

## Phase 4: User Story 2 - [Title] (Priority: P2)

**Goal**: [Brief description of what this story delivers]

**Independent Test**: [How to verify this story works on its own]

### Tests for User Story 2

- [ ] T024 [P] [US2] Add fixture or captured bytes for [message/service] in
  tests/fixtures/[name].bin
- [ ] T025 [P] [US2] Add service/state-machine test in tests/unit/test_[name].c
- [ ] T026 [P] [US2] Add integration test using host platform adapter in
  tests/integration/test_[name].c

### Implementation for User Story 2

- [ ] T027 [P] [US2] Implement [data model or address-space element] in
  src/address_space/[name].c
- [ ] T028 [US2] Implement [service behavior] in src/services/[name].c
- [ ] T029 [US2] Integrate with prior user-story components without expanding
  unsupported OPC UA surface
- [ ] T030 [US2] Update traceability and size records

**Checkpoint**: User Stories 1 and 2 both work independently.

---

## Phase 5: User Story 3 - [Title] (Priority: P3)

**Goal**: [Brief description of what this story delivers]

**Independent Test**: [How to verify this story works on its own]

### Tests for User Story 3

- [ ] T031 [P] [US3] Add unit test for [feature] in tests/unit/test_[name].c
- [ ] T032 [P] [US3] Add malformed or unsupported-case test in
  tests/unit/test_[name]_errors.c

### Implementation for User Story 3

- [ ] T033 [P] [US3] Implement [feature] in src/[area]/[name].c
- [ ] T034 [US3] Update public headers only if required by plan.md
- [ ] T035 [US3] Update traceability and size records

**Checkpoint**: Selected user stories are independently functional.

---

[Add more user story phases as needed, following the same pattern]

---

## Phase N: Compliance, Size, and Polish

**Purpose**: Cross-cutting validation required before completion

- [ ] TXXX Run host unit and integration tests
- [ ] TXXX Run fuzz targets or document unavailable fuzz tooling
- [ ] TXXX Run sanitizer build where available
- [ ] TXXX Run formatting and static analysis
- [ ] TXXX Run embedded cross-compile
- [ ] TXXX Measure flash/RAM impact and compare with plan.md budget
- [ ] TXXX Verify unsupported services/features return cited OPC UA StatusCodes
- [ ] TXXX Verify docs/traceability/ maps each implementation and test file to
  OPC UA sections
- [ ] TXXX Validate quickstart.md on host

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
- Different user stories can proceed in parallel after the foundational phase if
  they touch different files and preserve independent testability

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to a user story for traceability
- Protocol tasks must cite OPC UA part/section references
- Tests must fail before implementation for protocol behavior
- Commit after each task or logical group
- Stop at checkpoints to validate story behavior, conformance traceability, and
  embedded size impact
- Avoid vague tasks, same-file conflicts, hidden heap allocation, uncited protocol
  behavior, and cross-story dependencies that break independent testing
