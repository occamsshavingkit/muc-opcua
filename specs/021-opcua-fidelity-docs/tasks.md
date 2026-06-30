# Tasks: OPC UA Fidelity Docs and Next Work

**Input**: Design documents from `specs/021-opcua-fidelity-docs/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`

**Tests**: Documentation-only tasks use documentation/conformance checks. Protocol behavior tasks require RED tests before implementation.

**Organization**: Tasks are grouped by user story so documentation, citation, aggregate, subscription, and profile-claim work can be delivered independently.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel with other tasks in the same phase because it writes different files and has no dependency on incomplete tasks.
- **[Story]**: Maps to the user story in `spec.md`.
- **[RESOURCE-RISK: flash/RAM/throughput]**: Treat with extra size/performance care before implementation.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish baseline and active feature context.

- [X] T001a Run the clean baseline host CMake/CTest command
- [X] T001b Record the clean baseline host CMake/CTest command in `specs/021-opcua-fidelity-docs/quickstart.md`
- [X] T002 [P] Verify `AGENTS.md` points to `specs/021-opcua-fidelity-docs/plan.md`
- [X] T003 [P] Verify `.specify/feature.json` points to `specs/021-opcua-fidelity-docs`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared citation and evidence rules that all story work depends on.

- [X] T004a [P] Record FreeRTOS/lwIP transport citation decisions for OPC-10000-6 §5.2, §6.7.2, §7.1.2.2-§7.1.2.4, and §7.2 in `specs/021-opcua-fidelity-docs/research.md`
- [X] T004b [P] Record conformance citation decisions for OPC-10000-4 §5.8 and §5.12.2 in `specs/021-opcua-fidelity-docs/research.md`
- [X] T004c [P] Record aggregate citation decisions for OPC-10000-4 §7.22.4 and OPC-10000-13 Average/Minimum/Maximum sections in `specs/021-opcua-fidelity-docs/research.md`
- [X] T004d [P] Record MonitoredItem/Subscription citation decisions for OPC-10000-4 §5.13 and §5.14 in `specs/021-opcua-fidelity-docs/research.md`
- [X] T004e [P] Record profile-claim citation decisions for OPC-10000-7 §4.2 and §4.3 in `specs/021-opcua-fidelity-docs/research.md`
- [X] T004f [P] Record async-opcua localhost perf-pass hotspot evidence for OPC-10000-6 chunk encode/decode and request encode/decode triage in `specs/021-opcua-fidelity-docs/research.md`
- [X] T005 [P] Define documentation acceptance contracts in `specs/021-opcua-fidelity-docs/contracts/freertos-lwip-getting-started.md`
- [X] T006 [P] Define conformance citation and claim-hygiene contracts in `specs/021-opcua-fidelity-docs/contracts/conformance-citations.md`

**Checkpoint**: Shared traceability is ready for user-story work.

---

## Phase 3: User Story 1 - Bring Up FreeRTOS + lwIP (Priority: P1) [MVP]

**Goal**: Close GitHub issue #222 with concrete FreeRTOS + lwIP getting-started documentation.

**Independent Test**: A reader can use `docs/getting-started.md` to identify build mode, adapters, static buffers, polling loop, and RAM categories for a FreeRTOS + lwIP integration.

### Implementation for User Story 1

- [X] T007a [US1] Add the external-platform CMake shape to `docs/getting-started.md`, grounded in OPC-10000-6 §7.2
- [X] T007b [US1] Add the lwIP nonblocking TCP adapter callback guidance to `docs/getting-started.md`, grounded in OPC-10000-6 §7.1.2.2-§7.1.2.4 and §7.2
- [X] T007c [US1] Add the FreeRTOS time adapter guidance to `docs/getting-started.md`, grounded in OPC-10000-6 §5.2 DateTime encoding
- [X] T007d [US1] Add the entropy adapter guidance to `docs/getting-started.md`, grounded in OPC-10000-4 §5.7.2 and §5.7.3 nonce use
- [X] T007e [US1] Add the static storage, transport buffers, and server task loop guidance to `docs/getting-started.md`, grounded in OPC-10000-6 §6.7.2 and §7.1.2.3-§7.1.2.4
- [X] T007f [US1] Add `external` platform selection for application-owned TCP/IP adapters in `CMakeLists.txt` and `cmake/MicroOpcUaOptions.cmake`
- [X] T008 [US1] Add a cross-link from the FreeRTOS + lwIP section to deeper adapter guidance in `docs/getting-started.md`
- [X] T009 [US1] Verify documentation wording keeps the core no-heap claim separate from FreeRTOS/lwIP platform heap and task-stack resources in `docs/getting-started.md`

### Verification for User Story 1

- [X] T010a [US1] Run `ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs`
- [X] T010b [US1] Record the T010a result in `specs/021-opcua-fidelity-docs/quickstart.md`
- [X] T010c [US1] Run `cmake -S . -B build/external-doc-check -DMICRO_OPCUA_PLATFORM=external -DMICRO_OPCUA_PROFILE=nano -DMICRO_OPCUA_OPTIMIZE_SIZE=ON && cmake --build build/external-doc-check`
- [X] T010d [US1] Record the T010c result in `specs/021-opcua-fidelity-docs/quickstart.md`

**Checkpoint**: FreeRTOS + lwIP getting-started docs are independently reviewable.

---

## Phase 4: User Story 2 - Trust Conformance Citations (Priority: P1)

**Goal**: Correct known service-matrix citation drift before future implementation tasks reuse those references.

**Independent Test**: `docs/conformance/services.md` cites Method Call as OPC-10000-4 §5.12.2 and NodeManagement as OPC-10000-4 §5.8, while conformance-doc tests still pass.

### Implementation for User Story 2

- [X] T011 [US2] Correct the Method Call citation to OPC-10000-4 §5.12.2 in `docs/conformance/services.md`
- [X] T012 [US2] Correct the AddNodes/DeleteNodes/AddReferences/DeleteReferences citation to OPC-10000-4 §5.8 in `docs/conformance/services.md`

### Verification for User Story 2

- [X] T013a [US2] Run `ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs`
- [X] T013b [US2] Record the T013a result in `specs/021-opcua-fidelity-docs/quickstart.md`

**Checkpoint**: Conformance citation rows are corrected.

---

## Phase 5: User Story 3 - Finish Aggregate Filter Fidelity (Priority: P2)

**Goal**: Keep AggregateFilter behavior scoped to supported Average, Minimum, and Maximum cases with explicit unsupported/malformed failures.

**Independent Test**: Aggregate tests cover supported calculations, unsupported function NodeIds, malformed ExtensionObjects, and disallowed filter uses.

### Tests for User Story 3

- [X] T014 [P] [US3] [RESOURCE-RISK: throughput] Add a RED test for a non-Average/Minimum/Maximum AggregateFunction NodeId returning `Bad_MonitoredItemFilterUnsupported` per OPC-10000-4 §7.22.4 and OPC-10000-13 §4.2.2.4/§4.2.2.9/§4.2.2.10 in `tests/unit/test_aggregate.c`
- [X] T015 [P] [US3] [RESOURCE-RISK: flash/throughput] Add a RED test for a malformed AggregateFilter ExtensionObject per OPC-10000-4 §7.22.4 and OPC-10000-6 §5.2.2.15 in `tests/unit/test_aggregate.c`
- [X] T016a [P] [US3] [RESOURCE-RISK: throughput] Add a RED test proving Average remains limited to OPC-10000-13 §5.4.3.5 scoped behavior in `tests/unit/test_aggregate.c`
- [X] T016b [P] [US3] [RESOURCE-RISK: throughput] Add a RED test proving Minimum remains limited to OPC-10000-13 §5.4.3.10 scoped behavior in `tests/unit/test_aggregate.c`
- [X] T016c [P] [US3] [RESOURCE-RISK: throughput] Add a RED test proving Maximum remains limited to OPC-10000-13 §5.4.3.11 scoped behavior in `tests/unit/test_aggregate.c`

### Implementation for User Story 3

- [X] T017a [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest aggregate unsupported-function status fix per OPC-10000-4 §7.22.4 and OPC-10000-13 §4.2.2.4/§4.2.2.9/§4.2.2.10 in `src/services/subscription.c`
- [X] T017b [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest malformed AggregateFilter decode fix per OPC-10000-4 §7.22.4 and OPC-10000-6 §5.2.2.15 in `src/services/subscription.c`
- [X] T017c [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest scoped Average calculation fix per OPC-10000-13 §5.4.3.5 in `src/services/subscription.c`
- [X] T017d [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest scoped Minimum calculation fix per OPC-10000-13 §5.4.3.10 in `src/services/subscription.c`
- [X] T017e [US3] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest scoped Maximum calculation fix per OPC-10000-13 §5.4.3.11 in `src/services/subscription.c`
- [X] T018 [US3] Update scoped aggregate documentation and evidence for OPC-10000-4 §7.22.4 and OPC-10000-13 §5.4.3.5/§5.4.3.10/§5.4.3.11 in `docs/conformance/services.md`
- [X] T019 [US3] Measure aggregate-slice flash/RAM impact with `scripts/measure_size.sh` and record results in `docs/size/feature-size-ledger.md`

**Checkpoint**: Aggregate filter scope is tested, implemented, documented, and size-reviewed.

---

## Phase 6: User Story 4 - Harden MonitoredItem and Subscription Negative Paths (Priority: P2)

**Goal**: Reject invalid MonitoredItem and Subscription inputs deterministically with cited StatusCodes and no storage corruption.

**Independent Test**: Focused subscription tests cover invalid IDs, capacities, unsupported TransferSubscriptions, invalid publish acknowledgements, malformed filters, and state consistency.

### Tests for User Story 4

- [X] T020a [P] [US4] [RESOURCE-RISK: flash/throughput] Add a RED invalid MonitoredItem ID test per OPC-10000-4 §5.13.3 and §5.13.6 in `tests/unit/test_subscriptions_errors.c`
- [X] T020b [P] [US4] [RESOURCE-RISK: flash/throughput] Add a RED invalid Subscription ID test per OPC-10000-4 §5.14.3 and §5.14.8 in `tests/unit/test_subscriptions_errors.c`
- [X] T021a [P] [US4] [RESOURCE-RISK: RAM/throughput] Add a RED subscription capacity-exhaustion test per OPC-10000-4 §5.14.2 in `tests/unit/test_subscriptions_capacity.c`
- [X] T021b [P] [US4] [RESOURCE-RISK: RAM/throughput] Add a RED monitored-item capacity-exhaustion test per OPC-10000-4 §5.13.2 in `tests/unit/test_subscriptions_capacity.c`
- [X] T021c [P] [US4] [RESOURCE-RISK: RAM/throughput] Add a RED publish-request capacity-exhaustion test per OPC-10000-4 §5.14.5 in `tests/unit/test_subscriptions_capacity.c`
- [X] T021d [P] [US4] [RESOURCE-RISK: RAM/throughput] Add a RED queue capacity/overflow test per OPC-10000-4 §5.13.2 and §7.20.1 in `tests/unit/test_subscriptions_capacity.c`
- [X] T021e [P] [US4] [RESOURCE-RISK: RAM/throughput] Add a RED trigger-link capacity-exhaustion test per OPC-10000-4 §5.13.5 in `tests/unit/test_subscriptions_capacity.c`
- [X] T022a [P] [US4] [RESOURCE-RISK: flash/throughput] Add a RED invalid Publish acknowledgement test per OPC-10000-4 §5.14.5 in `tests/integration/test_subscriptions.c`
- [X] T022b [P] [US4] [RESOURCE-RISK: flash/throughput] Add a RED invalid Republish sequence test per OPC-10000-4 §5.14.6 in `tests/integration/test_subscriptions.c`
- [X] T023 [P] [US4] [RESOURCE-RISK: flash/throughput] Add RED unsupported TransferSubscriptions dispatch test expecting `Bad_ServiceUnsupported` per OPC-10000-4 §5.14.7 and §7.38.2 in `tests/unit/test_dispatch_services.c`

### Implementation for User Story 4

- [X] T024a [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest invalid MonitoredItem ID fix per OPC-10000-4 §5.13.3 and §5.13.6 in `src/services/subscription.c`
- [X] T024b [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest invalid Subscription ID fix per OPC-10000-4 §5.14.3 and §5.14.8 in `src/services/subscription.c`
- [X] T024c [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest subscription capacity-exhaustion fix per OPC-10000-4 §5.14.2 in `src/services/subscription.c`
- [X] T024d [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest monitored-item capacity-exhaustion fix per OPC-10000-4 §5.13.2 in `src/services/subscription.c`
- [X] T024e [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest publish-request capacity-exhaustion fix per OPC-10000-4 §5.14.5 in `src/services/subscription.c`
- [X] T024f [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest queue capacity/overflow fix per OPC-10000-4 §5.13.2 and §7.20.1 in `src/services/subscription.c`
- [X] T024g [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest trigger-link capacity-exhaustion fix per OPC-10000-4 §5.13.5 in `src/services/subscription.c`
- [X] T024h [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest invalid Publish acknowledgement fix per OPC-10000-4 §5.14.5 in `src/services/subscription.c`
- [X] T024i [US4] [RESOURCE-RISK: flash/RAM/throughput] Implement the smallest invalid Republish sequence fix per OPC-10000-4 §5.14.6 in `src/services/subscription.c`
- [X] T025 [US4] [RESOURCE-RISK: flash/throughput] Implement any required service-dispatch unsupported-service mapping per OPC-10000-4 §5.14.7 and §7.38.2 in `src/core/service_dispatch.c`
- [X] T026 [US4] Update subscription negative-path traceability for OPC-10000-4 §5.13 and §5.14 in `docs/conformance/services.md`
- [X] T027 [US4] Measure subscription-slice flash/RAM/throughput impact with `scripts/measure_size.sh` and record results in `docs/size/feature-size-ledger.md`

**Checkpoint**: Subscription negative paths are tested, implemented, documented, and size-reviewed.

---

## Phase 7: User Story 5 - Keep Profile Claims Honest (Priority: P3)

**Goal**: Keep profile, conformance-unit, and CTT wording scoped and evidence-backed.

**Independent Test**: Conformance-doc tests reject unsupported claim wording and profile docs continue to state profile-targeting status unless external CTT evidence exists.

### Implementation for User Story 5

- [X] T028 [P] [US5] Review OPC-10000-7 §4.2/§4.3 claim wording in `docs/conformance/profile-nano.md`
- [X] T029 [P] [US5] Review OPC-10000-7 §4.2/§4.3 claim wording in `docs/conformance/profile-micro.md`
- [X] T030 [P] [US5] Review OPC-10000-7 §4.2/§4.3 claim wording in `docs/conformance/profile-embedded.md`
- [X] T031 [P] [US5] Review OPC-10000-7 §4.2/§4.3 claim wording in `docs/traceability/conformance-claims.md`
- [X] T032 [US5] Record the claim-wording guard review outcome in `specs/021-opcua-fidelity-docs/quickstart.md`

### Verification for User Story 5

- [X] T033a [US5] Run `ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs`
- [X] T033b [US5] Record the T033a result in `specs/021-opcua-fidelity-docs/quickstart.md`

**Checkpoint**: Claim wording is evidence-backed and guarded.

---

## Phase 8: Compliance, Size, and Polish

**Purpose**: Verify the selected slices before completion.

- [X] T034a Run `ctest --test-dir build/baseline --output-on-failure`
- [X] T034b Record the T034a result in `specs/021-opcua-fidelity-docs/quickstart.md`
- [X] T035a Run `git diff --check`
- [X] T035b Record the T035a result in `specs/021-opcua-fidelity-docs/quickstart.md`
- [X] T036a Review incomplete US3 behavior-changing tasks in `specs/021-opcua-fidelity-docs/tasks.md` for OPC UA section citations
- [X] T036b Review incomplete US4 behavior-changing tasks in `specs/021-opcua-fidelity-docs/tasks.md` for OPC UA section citations
- [X] T037a Review incomplete US3 protocol tasks in `specs/021-opcua-fidelity-docs/tasks.md` for `[RESOURCE-RISK: ...]` where flash, RAM, or throughput may change
- [X] T037b Review incomplete US4 protocol tasks in `specs/021-opcua-fidelity-docs/tasks.md` for `[RESOURCE-RISK: ...]` where flash, RAM, or throughput may change
- [X] T038a Review async-opcua perf-pass artifacts under `/home/quackdcs/scratch/opcua-localhost-bench/perf-20260630-b92d983f-4e74d40-async-*`
- [X] T038b Record that `trace_locks` is not a current optimization priority unless fresh local profiling makes it material
- [X] T038c Confirm remaining resource-risk implementation tasks preserve local measurement requirements for flash, RAM, and throughput before completion

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies
- **Foundational (Phase 2)**: Depends on Setup; blocks user-story work
- **US1 and US2**: Depend on Foundational; can complete independently
- **US3 and US4**: Depend on Foundational and should not start until their RED tests are written
- **US5**: Depends on Foundational and can run after or alongside documentation/citation work
- **Compliance/Polish**: Depends on selected user stories

### Parallel Opportunities

- T002 and T003 can run in parallel.
- T004, T005, and T006 can run in parallel.
- T014-T016c can run in parallel before T017a-T017e.
- T020a-T023 can run in parallel before T024a-T025.
- T028-T031 can run in parallel before T032.

## Implementation Strategy

### MVP First

Complete US1 and US2 first: they close the live documentation issue and remove known citation drift without runtime cost.

### Incremental Delivery

1. Deliver US1 and US2 with documentation/conformance verification.
2. Execute US3 aggregate RED tests and minimal fixes.
3. Execute US4 subscription negative-path RED tests and minimal fixes.
4. Execute US5 profile-claim review and guards.
5. Run full CTest, diff checks, and size measurements for any protocol slice.
