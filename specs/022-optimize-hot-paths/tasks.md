# Tasks: Optimize Hot Paths

**Input**: Design documents from `specs/022-optimize-hot-paths/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/benchmark-report.md`, `quickstart.md`

**Tests**: Mandatory for every protocol parsing, serialization, service dispatch,
StatusCode, framing, or wire-visible optimization. Tests must be added or
identified before implementation changes.

**Organization**: Tasks are grouped by user story and ordered for incremental,
independently testable delivery. Each implementation task is one job in one
primary file. Tasks marked `[ROM/RAM/throughput risk]` require extra review of
ROM, static RAM, heap, stack, and benchmark deltas.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish the authoritative optimization baseline and traceability target.

- [X] T001 Capture the pre-optimization strict lab baseline in `docs/benchmarks/speed-baseline.json`
- [X] T002 Create the optimization traceability document skeleton in `docs/traceability/022-optimize-hot-paths.md`
- [X] T003 Add optimization resource-gate placeholders in `docs/traceability/022-optimize-hot-paths.md`
- [X] T004 Add OPC UA section anchor placeholders for OPC-10000-6 sections 5.2.1, 5.2.2.4, 5.2.2.7, 5.2.2.9, 5.2.5, 6.7.2, 6.7.2.2, 6.7.2.4, 6.7.3, 6.7.4, 7.1.2.2, and 7.2; OPC-10000-4 sections 5.11.2.1, 5.11.2.2, 5.11.2.4, 5.11.4.2, 5.11.4.4, 6.5.8, and 7.38.2; and OPC-10000-7 section 4.2 in `docs/traceability/022-optimize-hot-paths.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Prepare shared documentation needed by all user stories.

**CRITICAL**: No optimization implementation should begin until this phase is complete.

- [X] T005 Add the baseline summary from docs/benchmarks/speed-baseline.json to `docs/traceability/022-optimize-hot-paths.md`
- [X] T006 [P] Add the benchmark-report evidence contract reference to `specs/022-optimize-hot-paths/quickstart.md`

**Checkpoint**: Foundation ready; user story work can begin.

---

## Phase 3: User Story 1 - Verify Repeatable Optimization Evidence (Priority: P1) [MVP]

**Goal**: Ensure optimization claims are accepted only when backed by authoritative controlled benchmark reports.

**Independent Test**: Run the benchmark runner tests and verify an authoritative report passes validation while non-authoritative scheduling data is rejected.

### Tests for User Story 1

- [X] T007 [US1] Add benchmark report required-field contract tests in `tests/tools/test_speed_benchmark.py`
- [X] T008 [US1] Add benchmark scheduling-authority contract tests in `tests/tools/test_speed_benchmark.py`
- [X] T009 [US1] Add benchmark host-fingerprint contract tests in `tests/tools/test_speed_benchmark.py`
- [X] T010 [US1] Add benchmark threshold comparison contract tests in `tests/tools/test_speed_benchmark.py`
- [X] T011 [US1] Add benchmark result-shape contract tests in `tests/tools/test_speed_benchmark.py`

### Implementation for User Story 1

- [X] T012 [US1] Implement benchmark report required-field validation in `scripts/run_speed_bench.py`
- [X] T013 [US1] Implement benchmark scheduling-authority validation in `scripts/run_speed_bench.py`
- [X] T014 [US1] Implement benchmark host-fingerprint validation in `scripts/run_speed_bench.py`
- [X] T015 [US1] Implement benchmark threshold comparison validation in `scripts/run_speed_bench.py`
- [X] T016 [US1] Implement benchmark result-shape validation in `scripts/run_speed_bench.py`
- [X] T017 [US1] Document authoritative versus exploratory benchmark evidence in `docs/benchmarks/speed-benchmark.md`
- [X] T018 [US1] Refresh the authoritative baseline after report validation in `docs/benchmarks/speed-baseline.json`
- [X] T019 [US1] Record User Story 1 verification evidence in `docs/traceability/022-optimize-hot-paths.md`

**Checkpoint**: User Story 1 independently rejects non-authoritative benchmark evidence and accepts strict lab reports.

---

## Phase 4: User Story 2 - Improve Implemented OPC UA Hot Paths (Priority: P2)

**Goal**: Improve one or more implemented hot paths while preserving resource gates and observable OPC UA behavior.

**Independent Test**: Run targeted unit tests and `make speed-compare`; at least one targeted workload improves by >=5% normalized throughput with no resource gate failure.

### Tests and Benchmarks for User Story 2

- [X] T020 [US2] Add matrix coverage tests for new hot-path benchmark scenarios in `tests/tools/test_speed_benchmark.py`
- [X] T021 [US2] Add the MessageChunk header benchmark scenario for OPC-10000-6 sections 6.7.2, 6.7.2.2, and 7.1.2.2 in `tests/benchmark/hotpath_benchmark.c`
- [X] T022 [US2] Add the Binary primitive benchmark scenario for OPC-10000-6 sections 5.2.1 and 5.2.5 in `tests/benchmark/hotpath_benchmark.c`
- [X] T023 [US2] Add the UASC response framing benchmark scenario for OPC-10000-6 sections 6.7.2, 6.7.2.4, and 6.7.4 in `tests/benchmark/hotpath_benchmark.c`
- [X] T024 [P] [US2] Add Message Header equivalence tests for OPC-10000-6 sections 6.7.2.2 and 7.1.2.2 in `tests/unit/test_message_chunk.c`
- [X] T025 [P] [US2] Add malformed MessageChunk boundary tests for OPC-10000-6 sections 6.7.2 and 6.7.3 in `tests/unit/test_message_chunk_errors.c`
- [X] T026 [P] [US2] Add Binary primitive status-preservation tests for OPC-10000-6 section 5.2.1 in `tests/unit/test_binary_primitives.c`
- [X] T027 [P] [US2] Add Binary array length preservation tests for OPC-10000-6 section 5.2.5 in `tests/unit/test_binary_array_errors.c`
- [X] T027a [P] [US2] Add Binary String length preservation tests for OPC-10000-6 section 5.2.2.4 in `tests/unit/test_binary_string_errors.c`
- [X] T027b [P] [US2] Add Binary ByteString length preservation tests for OPC-10000-6 section 5.2.2.7 in `tests/unit/test_binary_bytestring_errors.c`
- [X] T027c [P] [US2] Add Binary NodeId variant preservation tests for OPC-10000-6 section 5.2.2.9 in `tests/unit/test_binary_nodeid_errors.c`
- [X] T028 [P] [US2] Add Read batch equivalence tests for OPC-10000-4 sections 5.11.2.2, 5.11.2.4, and 7.38.2 in `tests/unit/test_read_service.c`
- [X] T029 [P] [US2] Add Write batch equivalence tests for OPC-10000-4 sections 5.11.4.2, 5.11.4.4, 6.5.8, and 7.38.2 in `tests/unit/test_write_service.c`
- [X] T030 [P] [US2] Add Write negative-path equivalence tests for OPC-10000-4 sections 5.11.4.4 and 7.38.2 in `tests/unit/test_write_errors.c`
- [X] T030a [P] [US2] Add unsupported-service StatusCode regression for OPC-10000-4 section 7.38.2 and OPC-10000-7 section 4.2 in `tests/unit/test_service_dispatch.c`

### Implementation for User Story 2

- [X] T031 [US2] [ROM/RAM/throughput risk] Optimize fixed-size Message Header parsing for OPC-10000-6 sections 6.7.2.2 and 7.1.2.2 in `src/core/message_chunk.c`
- [X] T032 [US2] [ROM/RAM/throughput risk] Optimize fixed-size Message Header writing for OPC-10000-6 sections 6.7.2.2 and 7.1.2.2 in `src/core/message_chunk.c`
- [X] T033 [US2] [ROM/RAM/throughput risk] Optimize Sequence Header parsing for OPC-10000-6 section 6.7.2.4 in `src/core/message_chunk.c`
- [X] T034 [US2] [ROM/RAM/throughput risk] Optimize Sequence Header writing for OPC-10000-6 section 6.7.2.4 in `src/core/message_chunk.c`
- [X] T035 [US2] [ROM/RAM/throughput risk] Optimize Binary reader scalar primitive bounds checks for OPC-10000-6 section 5.2.1 in `src/encoding/binary_reader.c`
- [X] T036 [US2] [ROM/RAM/throughput risk] Optimize Binary reader scalar primitive StatusCode preservation for OPC-10000-6 section 5.2.1 in `src/encoding/binary_reader.c`
- [X] T037 [US2] [ROM/RAM/throughput risk] Optimize Binary reader array length bounds checks for OPC-10000-6 section 5.2.5 in `src/encoding/binary_reader.c`
- [X] T038 [US2] [ROM/RAM/throughput risk] Optimize Binary reader array element traversal bounds for OPC-10000-6 section 5.2.5 in `src/encoding/binary_reader.c`
- [X] T039 [US2] [ROM/RAM/throughput risk] Optimize Binary writer primitive capacity checks for OPC-10000-6 section 5.2.1 in `src/encoding/binary_writer.c`
- [X] T040 [US2] [ROM/RAM/throughput risk] Optimize Binary writer primitive StatusCode preservation for OPC-10000-6 section 5.2.1 in `src/encoding/binary_writer.c`
- [X] T041 [US2] [ROM/RAM/throughput risk] Optimize little-endian primitive helper operations for OPC-10000-6 section 5.2.1 in `src/encoding/binary_le.h`
- [X] T042 [US2] [ROM/RAM/throughput risk] Optimize Read dispatch setup while preserving OPC-10000-4 sections 5.11.2.2, 5.11.2.4, and 7.38.2 behavior in `src/core/service_dispatch.c`
- [X] T043 [US2] [ROM/RAM/throughput risk] Optimize Read service processing while preserving OPC-10000-4 sections 5.11.2.1, 5.11.2.4, and 7.38.2 behavior in `src/services/read.c`
- [X] T044 [US2] [ROM/RAM/throughput risk] Optimize Write dispatch validation while preserving OPC-10000-4 sections 5.11.4.2, 5.11.4.4, and 7.38.2 behavior in `src/core/service_dispatch.c`
- [X] T045 [US2] [ROM/RAM/throughput risk] Optimize Write callback audit handoff while preserving OPC-10000-4 section 6.5.8 behavior in `src/core/service_dispatch.c`
- [X] T046 [US2] [ROM/RAM/throughput risk] Optimize Write service processing while preserving OPC-10000-4 sections 5.11.4.2, 5.11.4.4, and 7.38.2 behavior in `src/services/write.c`
- [X] T047 [US2] [ROM/RAM/throughput risk] Optimize UASC response framing while preserving OPC-10000-6 sections 6.7.2, 6.7.2.4, and 6.7.4 behavior in `src/core/uasc.c`
- [X] T048 [US2] [ROM/RAM/throughput risk] Optimize OPC UA TCP fixed response header writes while preserving OPC-10000-6 sections 7.1.2.2 and 7.2 behavior in `src/core/tcp_connection.c`
- [X] T049 [US2] Record targeted User Story 2 test and benchmark deltas in `docs/traceability/022-optimize-hot-paths.md`

**Checkpoint**: User Story 2 has at least one measured hot-path improvement and passes resource gates.

---

## Phase 5: User Story 3 - Preserve OPC UA Fidelity While Optimizing (Priority: P3)

**Goal**: Prove that optimized behavior remains traceable to OPC UA requirements and does not broaden compatibility claims.

**Independent Test**: Review traceability and run conformance-oriented documentation checks plus protocol negative-path tests.

### Tests for User Story 3

- [X] T050 [US3] Add Binary traceability document assertions for OPC-10000-6 sections 5.2.1, 5.2.2.4, 5.2.2.7, 5.2.2.9, and 5.2.5 in `tests/unit/test_traceability_docs.c`
- [X] T051 [US3] Add transport framing traceability document assertions for OPC-10000-6 sections 6.7.2, 6.7.2.2, 6.7.2.4, 6.7.4, 7.1.2.2, and 7.2 in `tests/unit/test_traceability_docs.c`
- [X] T052 [US3] Add service StatusCode traceability document assertions for OPC-10000-4 sections 5.11.2.4, 5.11.4.4, and 7.38.2 in `tests/unit/test_traceability_docs.c`
- [X] T053 [US3] Add Write audit and conformance-claim traceability document assertions for OPC-10000-4 section 6.5.8 and OPC-10000-7 section 4.2 in `tests/unit/test_traceability_docs.c`

### Implementation for User Story 3

- [X] T054 [US3] Map MessageChunk source changes to OPC-10000-6 sections 6.7.2, 6.7.2.2, 6.7.2.4, and 7.1.2.2 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T055 [US3] Map Binary reader source changes to OPC-10000-6 sections 5.2.1, 5.2.2.4, 5.2.2.7, 5.2.2.9, and 5.2.5 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T056 [US3] Map Binary writer source changes to OPC-10000-6 sections 5.2.1, 5.2.2.4, 5.2.2.7, and 5.2.2.9 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T057 [US3] Map little-endian helper source changes to OPC-10000-6 section 5.2.1 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T058 [US3] Map Read dispatch source changes to OPC-10000-4 sections 5.11.2.2, 5.11.2.4, and 7.38.2 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T059 [US3] Map Read service source changes to OPC-10000-4 sections 5.11.2.1, 5.11.2.4, and 7.38.2 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T060 [US3] Map Write dispatch source changes to OPC-10000-4 sections 5.11.4.2, 5.11.4.4, 6.5.8, and 7.38.2 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T061 [US3] Map Write service source changes to OPC-10000-4 sections 5.11.4.2, 5.11.4.4, and 7.38.2 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T062 [US3] Map UASC response framing source changes to OPC-10000-6 sections 6.7.2, 6.7.2.4, and 6.7.4 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T063 [US3] Map OPC UA TCP fixed response header source changes to OPC-10000-6 sections 7.1.2.2 and 7.2 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T064 [US3] Record MessageChunk decoder fuzz smoke evidence for OPC-10000-6 section 6.7.2 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T065 [US3] Record Binary decoder fuzz smoke evidence for OPC-10000-6 sections 5.2.1, 5.2.2.4, 5.2.2.7, 5.2.2.9, and 5.2.5 in `docs/traceability/022-optimize-hot-paths.md`
- [X] T066 [US3] Record interop smoke evidence showing unchanged compatibility claims for OPC-10000-7 section 4.2 in `docs/traceability/022-optimize-hot-paths.md`

**Checkpoint**: User Story 3 documents the preserved protocol contract and evidence for all optimized paths.

---

## Phase 6: Compliance, Size, and Polish

**Purpose**: Cross-cutting validation before completion.

- [X] T067 Run the full host test suite using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T068 Record the full host test suite result in `docs/traceability/022-optimize-hot-paths.md`
- [X] T069 Run the first strict speed comparison using `scripts/run_speed_bench.py`
- [X] T070 Run the second consecutive strict speed comparison using `scripts/run_speed_bench.py`
- [X] T071 Record affected workload deltas and consecutive comparison agreement in `docs/traceability/022-optimize-hot-paths.md`
- [X] T072 Run nano, micro, and embedded profile builds using `Makefile`
- [X] T073 Run the full profile host build using `CMakeLists.txt`
- [X] T074 Record nano, micro, embedded, and full profile build evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T075 Run the sanitizer test suite using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T076 Record sanitizer test evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T077 Run the formatting check using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T078 Record formatting-check evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T079 Run the cppcheck static-analysis target using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T080 Record cppcheck evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T081 Run the clang-tidy static-analysis target using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T082 Record clang-tidy evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T083 Run the no-heap source check using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T084 Record no-heap source-check evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T085 Run stack-usage verification using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T086 Record stack-usage verification evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T087 Run the embedded Pico cross-compile using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T088 Record embedded Pico cross-compile evidence in `docs/traceability/022-optimize-hot-paths.md`
- [X] T089 Run whitespace validation using `specs/022-optimize-hot-paths/quickstart.md`
- [X] T090 Record the whitespace validation completion note in `docs/traceability/022-optimize-hot-paths.md`
- [X] T091 Refresh the accepted final baseline only after a net win and all Phase 6 evidence gates are verified in `docs/benchmarks/speed-baseline.json`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup; blocks all user stories.
- **User Story 1 (Phase 3)**: Depends on Foundational; MVP for trustworthy evidence.
- **User Story 2 (Phase 4)**: Depends on User Story 1 so optimization evidence can be accepted.
- **User Story 3 (Phase 5)**: Depends on User Story 2 implementation candidates.
- **Compliance/Polish (Phase 6)**: Depends on all selected user stories.

### Within Each User Story

- Tests and benchmark coverage before implementation.
- One implementation concern per task.
- One primary file path per task.
- OPC UA behavior-preservation tasks cite exact OPC UA sections in the task line.
- Resource-risk tasks require ROM/RAM/heap/stack and throughput review before completion.
- Traceability and evidence updates before each checkpoint is considered complete.

### Parallel Opportunities

- T006 can run in parallel with T005 after T001 through T004.
- T024 through T030, plus T027a through T027c and T030a, can run in parallel after T020 through T023.
- T017 and T019 can run in parallel after T012 through T016 pass targeted tests.
- Documentation-only traceability tasks T054 through T063 should be serialized because they update the same file.
- Implementation tasks T031 through T048 should be serialized to isolate throughput and resource deltas.

## Parallel Execution Examples

### User Story 1

Run T007 through T011 first, then T012 through T016. T017 and T019 can proceed in parallel after validation behavior is stable, while T018 waits for the accepted baseline refresh.

### User Story 2

Run T020 through T023 first to establish benchmark coverage. Then run T024 through T030, plus T027a through T027c and T030a, in parallel because they touch separate test files. Implementation tasks T031 through T048 should be executed one at a time to isolate throughput and resource deltas.

### User Story 3

Run T050 through T053 before traceability updates. Complete T054 through T063 after each related optimization candidate lands. Complete T064 through T066 after their respective fuzz or interop evidence exists.

## Implementation Strategy

### MVP First

Complete Phases 1-3 first. The MVP is a trustworthy optimization evidence gate that rejects non-authoritative benchmark data and accepts strict lab reports.

### Incremental Delivery

After MVP, complete one User Story 2 optimization candidate at a time. For each candidate: add or identify tests, make the single-concern optimization, run targeted tests, run the strict benchmark comparison, record deltas, and either keep or revert the candidate based on evidence.

### Final Hardening

Complete User Story 3 and Phase 6 only after at least one candidate is accepted. The final baseline should be refreshed only when the accepted code is a net win and all correctness, resource, and traceability gates pass.

## Notes

- `[P]` tasks are parallelizable because they touch different files and do not depend on incomplete tasks in the same phase.
- `[ROM/RAM/throughput risk]` tasks must be treated with extra care and measured before being marked complete.
- Do not combine implementation tasks across multiple source files.
- Do not combine parse and write changes in one task.
- Do not combine benchmark validation dimensions in one task.
- Do not change OPC UA wire-visible behavior without a matching test and exact OPC UA section reference.
- Do not broaden compatibility, profile, service, encoding, transport, security policy, or conformance claims in this feature.
