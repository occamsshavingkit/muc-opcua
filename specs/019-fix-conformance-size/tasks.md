# Tasks: OPC UA Conformance and Size Repairs

**Input**: Design documents from `/specs/019-fix-conformance-size/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/protocol-repair-contract.md, quickstart.md

**Tests**: Tests are mandatory for StatusCodes, NodeIds, AggregateFilter validation, GetEndpoints filtering, stack-budget behavior, and any wire-visible behavior changed by this feature.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing. Protocol tests and fixtures appear before implementation tasks.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it touches a different file and has no dependency on incomplete tasks.
- **[Story]**: User story label from `spec.md`.
- Each task names one primary file path or one verification command.
- Each protocol task cites exact OPC UA part/section references.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish feature traceability and measurement locations before protocol edits.

- [X] T001 Create `docs/traceability/019-fix-conformance-size.md` with audit baselines and source links for OPC-10000-4 §7.38.2, OPC-10000-4 §7.22.4, OPC-10000-4 §5.5.4.2, OPC-10000-6 §5.2.2.9, OPC-10000-6 §5.2.2.15, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, OPC-10000-13 §5.4.3.11, and OPC-10000-7 §4.3
- [X] T002 [P] Add `specs/019-fix-conformance-size/verification.md` with pre-change audit baselines and planned commands tied to OPC-10000-7 §4.3 profile-claim honesty

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Prepare tests that fail against the audited defects before implementation.

**CRITICAL**: User story implementation begins only after these protocol tests exist.

- [X] T003 [P] Add exact StatusCode value assertions in `tests/unit/test_status.c` for OPC-10000-4 §7.38.2 and OPC Foundation `StatusCode.csv`
- [X] T004 [P] Add exact AggregateFilter and AggregateFunction NodeId assertions in `tests/unit/test_aggregate.c` for OPC-10000-4 §7.22.4, OPC-10000-6 §5.2.2.9, OPC-10000-6 §5.2.2.15, OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10
- [X] T005 [P] Add GetEndpoints `profileUris` request/response tests in `tests/unit/test_discovery_endpoint.c` for OPC-10000-4 §5.5.4.2
- [X] T006 Add stack-budget behavior notes to `specs/019-fix-conformance-size/verification.md` explaining compiler-emitted frame handling while preserving OPC-10000-7 §4.3 conformance-claim honesty

**Checkpoint**: Tests and verification notes capture the audited failures.

---

## Phase 3: User Story 1 - Correct Wire Identifiers (Priority: P1) [MVP]

**Goal**: Public constants match standard OPC UA wire identifiers.

**Independent Test**: `test_status` and `test_aggregate` pass with official numeric values and reject stale aggregate IDs.

### Tests for User Story 1

- [X] T007 [US1] Confirm `tests/unit/test_status.c` fails before implementation for incorrect OPC-10000-4 §7.38.2 StatusCode values
- [X] T008 [US1] Confirm `tests/unit/test_aggregate.c` fails before implementation for incorrect OPC-10000-4 §7.22.4 AggregateFilter and OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10 AggregateFunction NodeIds

### Implementation for User Story 1

- [X] T009 [US1] Correct duplicated and wrong StatusCode macros in `include/micro_opcua/status.h` using OPC-10000-4 §7.38.2 and OPC Foundation `StatusCode.csv`
- [X] T010 [US1] Align optional status-name diagnostics in `src/core/status.c` with corrected OPC-10000-4 §7.38.2 StatusCode values
- [X] T011 [US1] Correct AggregateFilter and AggregateFunction constants in `include/micro_opcua/opcua_ids.h` using OPC-10000-4 §7.22.4, OPC-10000-6 §5.2.2.9, OPC-10000-6 §5.2.2.15, OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10
- [X] T012 [US1] Update `docs/traceability/statuscodes.md` with corrected OPC-10000-4 §7.38.2 StatusCode values changed by this feature
- [X] T013 [US1] Update `docs/traceability/019-fix-conformance-size.md` with implementation/test mappings for OPC-10000-4 §7.38.2, OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10

**Checkpoint**: Wire identifier tests pass independently.

---

## Phase 4: User Story 2 - Standard-Traceable Service Behavior (Priority: P1)

**Goal**: AggregateFilter validation and GetEndpoints filtering follow cited OPC UA sections or fail with cited StatusCodes.

**Independent Test**: `test_aggregate` and `test_discovery_endpoint` exercise supported, unsupported, matched, and unmatched requests.

### Tests for User Story 2

- [X] T014 [US2] Add stale aggregate NodeId rejection coverage in `tests/unit/test_aggregate.c` for OPC-10000-4 §5.13.2.4, OPC-10000-4 §7.22.4, OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10
- [X] T015 [US2] Confirm `tests/unit/test_discovery_endpoint.c` fails before implementation for unmatched `profileUris` filtering required by OPC-10000-4 §5.5.4.2

### Implementation for User Story 2

- [X] T016 [US2] Apply `profileUris` filtering in `src/core/service_dispatch.c` for GetEndpoints per OPC-10000-4 §5.5.4.2
- [X] T017 [US2] Tighten AggregateFilter validation comments and filter-result mapping in `src/core/service_dispatch.c` for OPC-10000-4 §5.13.2.4, OPC-10000-4 §5.13.3.4, OPC-10000-4 §7.22.4, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, and OPC-10000-13 §5.4.3.11
- [X] T018 [US2] Update `tests/fixtures/services/discovery.md` to document GetEndpoints `profileUris` cases per OPC-10000-4 §5.5.4.2
- [X] T019 [US2] Update `docs/conformance/services.md` service-section citations for GetEndpoints and monitored-item filter behavior per OPC-10000-4 §5.5.4.2, OPC-10000-4 §5.13.2.4, OPC-10000-4 §5.13.3.4, and OPC-10000-4 §7.22.4
- [X] T020 [US2] Update `docs/traceability/019-fix-conformance-size.md` with service behavior mappings for OPC-10000-4 §5.5.4.2 and OPC-10000-4 §7.22.4

**Checkpoint**: Service behavior tests pass independently.

---

## Phase 5: User Story 3 - Smaller and Faster Embedded Defaults (Priority: P2)

**Goal**: Correctness repairs include a low-risk ROM/speed fix and a RAM configuration control without weakening protocol behavior.

**Independent Test**: Session tests, build config tests, stack-budget test, and size measurement complete successfully.

### Tests for User Story 3

- [X] T021 [US3] Add session timeout conversion boundary assertions in `tests/unit/test_session.c` while preserving OPC-10000-4 §5.7.2.2 session timeout semantics
- [X] T022 [US3] Add connection RX buffer configuration assertions in `tests/unit/test_build_config.c` while preserving OPC-10000-6 §7.2 OPC UA TCP message handling constraints
- [X] T023 [US3] Add missing-root-frame stack-usage fixture in `tests/fixtures/stack_usage/missing-root/server.su` for OPC-10000-7 §4.3 claim-honest stack-budget validation

### Implementation for User Story 3

- [X] T024 [US3] Replace the remaining session timeout double cast in `src/services/session.c` with integer conversion while preserving OPC-10000-4 §5.7.2.2 revised session timeout behavior
- [X] T025 [US3] Add `MU_CONNECTION_RX_BUFFER_SIZE` default configuration in `include/micro_opcua/config.h` while preserving OPC-10000-6 §7.2 OPC UA TCP receive-buffer bounds
- [X] T026 [US3] Use `MU_CONNECTION_RX_BUFFER_SIZE` in `src/core/server_internal.h` for per-connection RX storage while preserving OPC-10000-6 §7.2 OPC UA TCP receive-buffer bounds
- [X] T027 [US3] Make `scripts/check_stack_usage.sh` tolerate missing optimized-away internal helper frames while enforcing emitted frame thresholds and preserving OPC-10000-7 §4.3 conformance-claim honesty
- [X] T028 [US3] Record post-change profile size measurements in `specs/019-fix-conformance-size/verification.md` with deltas against the audit baseline and OPC-10000-7 §4.3 claim status
- [X] T029 [US3] Update `docs/traceability/019-fix-conformance-size.md` with RAM/ROM/stack mappings tied to OPC-10000-7 §4.3 and the project size-discipline gate

**Checkpoint**: Size/performance changes are measured and documented.

---

## Phase 6: User Story 4 - Complete Traceability and Honest Claims (Priority: P2)

**Goal**: Documentation and tasks preserve exact OPC UA citations for future implementation and review.

**Independent Test**: Traceability and conformance doc tests pass and no stale AggregateFilter references remain.

### Tests for User Story 4

- [X] T030 [US4] Extend `tests/unit/test_traceability_docs.c` to reject stale AggregateFilter citations and `TBD` protocol rows using OPC-10000-4 §7.22.4 and OPC-10000-7 §4.3
- [X] T031 [US4] Extend `tests/unit/test_conformance_docs.c` to reject unsupported profile-compliant claims without CTT evidence per OPC-10000-7 §4.3

### Implementation for User Story 4

- [X] T032 [US4] Update `specs/018-aggregate-subscriptions/spec.md` stale AggregateFilter and AggregateFunction citations to OPC-10000-4 §7.22.4, OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10
- [X] T033 [US4] Update `specs/018-aggregate-subscriptions/plan.md` conformance and size claims to profile-targeting with OPC-10000-7 §4.3 and AggregateFilter OPC-10000-4 §7.22.4
- [X] T034 [US4] Update `specs/018-aggregate-subscriptions/data-model.md` aggregate NodeIds and unsupported behavior references to OPC-10000-4 §7.22.4, OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10
- [X] T035 [US4] Update `specs/018-aggregate-subscriptions/quickstart.md` aggregate filter NodeIds to OPC-10000-4 §7.22.4, OPC-10000-13 §4.2.2.4, OPC-10000-13 §4.2.2.9, and OPC-10000-13 §4.2.2.10
- [X] T036 [US4] Update `specs/018-aggregate-subscriptions/tasks.md` completed task references to corrected OPC-10000-4 §7.22.4 and OPC-10000-13 NodeIds for audit continuity
- [X] T037 [US4] Update `docs/conformance/profile-embedded.md` aggregate facet citation and support status per OPC-10000-4 §7.22.4, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, OPC-10000-13 §5.4.3.11, and OPC-10000-7 §4.3
- [X] T038 [US4] Update `docs/traceability/files-to-sections.md` to replace protocol `TBD` rows with exact OPC UA references including OPC-10000-4 §5.5.4.2, OPC-10000-4 §7.22.4, OPC-10000-4 §7.38.2, OPC-10000-6 §5.2.2.9, and OPC-10000-6 §5.2.2.15
- [X] T039 [US4] Update `docs/traceability/sections-to-files.md` with corrected mappings for OPC-10000-4 §5.5.4.2, OPC-10000-4 §7.22.4, OPC-10000-4 §7.38.2, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, and OPC-10000-13 §5.4.3.11

**Checkpoint**: Traceability tests pass and stale references are removed.

---

## Phase 7: Compliance, Size, and Polish

**Purpose**: Validate feature behavior and measurements before completion.

- [X] T040 Run `cmake -S . -B build/019-tests -DMICRO_OPCUA_BUILD_TESTS=ON -DMICRO_OPCUA_OPTIMIZE_SIZE=ON -DMICRO_OPCUA_STACK_USAGE=ON -DCMAKE_BUILD_TYPE=Debug` for OPC-10000-4 §7.38.2, OPC-10000-4 §7.22.4, OPC-10000-4 §5.5.4.2, OPC-10000-6 §5.2.2.9, OPC-10000-6 §5.2.2.15, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, OPC-10000-13 §5.4.3.11, and OPC-10000-7 §4.3 verification
- [X] T041 Run `cmake --build build/019-tests -j` for OPC-10000-4 §7.38.2, OPC-10000-4 §7.22.4, OPC-10000-4 §5.5.4.2, OPC-10000-6 §5.2.2.9, OPC-10000-6 §5.2.2.15, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, OPC-10000-13 §5.4.3.11, and OPC-10000-7 §4.3 verification
- [X] T042 Run `ctest --test-dir build/019-tests --output-on-failure` for OPC-10000-4 §7.38.2, OPC-10000-4 §7.22.4, OPC-10000-4 §5.5.4.2, OPC-10000-6 §5.2.2.9, OPC-10000-6 §5.2.2.15, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, OPC-10000-13 §5.4.3.11, and OPC-10000-7 §4.3 verification
- [X] T043 Run `scripts/measure_size.sh all` under OPC-10000-7 §4.3 profile-claim honesty
- [X] T044 Run `rg -n "7\\.16|11565|11569|11570|profile-compliant|TBD" specs/018-aggregate-subscriptions docs specs/019-fix-conformance-size` to detect stale OPC-10000-4 §7.22.4 and OPC-10000-7 §4.3 references
- [X] T045 Run `bash scripts/check_stack_usage.sh --build-dir tests/fixtures/stack_usage/missing-root --threshold 10240` for OPC-10000-7 §4.3 claim-honest stack-budget validation
- [X] T046 Update `specs/019-fix-conformance-size/verification.md` with final pass/fail results for OPC-10000-4 §7.38.2, OPC-10000-4 §7.22.4, OPC-10000-4 §5.5.4.2, OPC-10000-6 §5.2.2.9, OPC-10000-6 §5.2.2.15, OPC-10000-13 §5.4.3.5, OPC-10000-13 §5.4.3.10, OPC-10000-13 §5.4.3.11, and OPC-10000-7 §4.3

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup completion and blocks user story work.
- **US1 and US2 (P1)**: Depend on Foundational tests; US2 depends on corrected identifiers from US1 for aggregate validation.
- **US3 (P2)**: Depends on Foundational and can proceed after US1 because it changes independent size/performance files.
- **US4 (P2)**: Depends on US1 and US2 decisions so docs reflect final behavior.
- **Compliance/Polish**: Depends on desired user stories being complete.

### Parallel Opportunities

- T002 can run in parallel with T001.
- T003, T004, and T005 can run in parallel because each touches a different file; T006 follows T002 because both update `specs/019-fix-conformance-size/verification.md`.
- T009, T010, and T011 must run sequentially with their related tests but touch separate implementation files.
- US3 file edits T024, T025, T026, and T027 can be prepared independently after tests exist, then verified together.
- US4 documentation tasks T032 through T039 touch separate files and can be parallelized after protocol values are final.

### Implementation Strategy

1. Complete US1 first as the MVP because incorrect wire identifiers invalidate other conformance work.
2. Complete US2 next to repair service behavior using the corrected identifiers.
3. Complete US3 to remove a low-risk soft-float path, expose a RAM control, and fix the stack-budget gate.
4. Complete US4 to align docs and traceability with the corrected behavior.
5. Run the full validation commands and record measurements.

## Notes

- Protocol tasks cite exact OPC UA sections so implementation can re-open the relevant section without rediscovery.
- Tests intentionally use external numeric values instead of local macro aliases.
- Size reductions must not remove checks, bounds validation, or StatusCode fidelity.
