# Tasks: Aggregate Subscriptions

**Input**: Design documents from `/specs/018-aggregate-subscriptions/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, quickstart.md

**Tests**: Tests are mandatory for filter decoding, aggregation logic, publishing schedules, and zero-heap checks.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, build tooling, and test setup

- [x] T001 Add `test_aggregate.c` to unit tests and configure in `tests/unit/CMakeLists.txt`
- [x] T002 Update `docs/traceability/files-to-sections.md` with Aggregate Subscriptions sections

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Struct definitions and decoder setup

- [x] T003 [P] Define `mu_aggregate_state_t` and update `mu_monitored_item_t` structure in `src/core/subscription.c` or internal headers
- [x] T004 Define `MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY` (`730`, OPC-10000-4 §7.22.4) in `include/micro_opcua/opcua_ids.h`
- [x] T005 Document target conformance units in `docs/traceability/files-to-sections.md`

## Phase 3: User Story 1 - Average Aggregate (Priority: P1) [MVP]

**Goal**: Support calculating and publishing the mathematical Average of values over the processing interval.

**Independent Test**: Monitored item created with Average AggregateFilter publishes the correct average over the interval.

### Tests for User Story 1

- [x] T006 [P] [US1] Add decoder test for `AggregateFilter` in `tests/unit/test_aggregate.c`
- [x] T007 [P] [US1] Add dispatch/sampling test for Average aggregation in `tests/unit/test_aggregate.c`

### Implementation for User Story 1

- [x] T008 [US1] Implement decoding of `AggregateFilter` in `src/core/subscription.c`
- [x] T009 [US1] Implement sampling accumulation logic for Average in `src/core/subscription.c`
- [x] T010 [US1] Implement interval check and publishing queue dispatch in `src/core/subscription.c`

## Phase 4: User Story 2 - Min/Max Aggregates (Priority: P2)

**Goal**: Support calculating and publishing Minimum and Maximum values over the processing interval.

**Independent Test**: Monitored item created with Minimum/Maximum AggregateFilters publishes the correct extremes.

### Tests for User Story 2

- [x] T011 [P] [US2] Add unit test for Min/Max aggregation in `tests/unit/test_aggregate.c`

### Implementation for User Story 2

- [x] T012 [US2] Implement sampling accumulation logic for Minimum and Maximum in `src/core/subscription.c`
- [x] T013 [US2] Integrate Min/Max check into publishing queue dispatch in `src/core/subscription.c`

## Phase 5: User Story 3 - Zero-heap containment (Priority: P3)

**Goal**: Bounded memory footprint with zero heap allocations.

**Independent Test**: Memory analysis validates zero heap usage for aggregate creation and processing.

### Tests for User Story 3

- [x] T014 [US3] Add memory containment verification test in `tests/unit/test_aggregate.c`

### Implementation for User Story 3

- [x] T015 [US3] Verify strict stack/static allocation limits in `src/core/subscription.c`

## Phase 6: Compliance, Size, and Polish

**Purpose**: Final verification, formatting, and traceability mapping

- [x] T016 Run all unit and integration tests and ensure 100% success
- [x] T017 Run clang-format on all modified files
- [x] T018 Record final code and data footprint in `specs/018-aggregate-subscriptions/plan.md`

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup completion
- **User Stories (Phase 3+)**: Depend on Foundational phase completion
- **Compliance/Polish**: Depends on all stories completion
