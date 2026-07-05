# Tasks: Backlog Critical Fixes (Round 1)

**Feature**: 038-backlog-critical-fixes
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)
**Generated**: 2026-07-05

## Format: `- [ ] [ID] [P?] [Story?] Description with file path`

- **[P]** = parallelizable (different files, no dependencies)
- **[US1-6]** = user story label from spec.md

---

## Phase 1: Setup

- [ ] T001 Verify all 121 existing tests pass on current branch (`038-backlog-critical-fixes`) baseline in `build/test/`

---

## Phase 2: CI/CD Pipeline Integrity (US1) — CRITICAL

**Goal**: CI pipeline must fail when static analysis, sanitizers, or fuzzing detect issues
**Independent test**: Submit a PR with a deliberate cppcheck violation — CI must go red

- [ ] T002 [P] [US1] Remove `|| true` from `format-check`, `cppcheck`, `clang-tidy` steps in `.github/workflows/ci.yml`
- [ ] T003 [P] [US1] Remove `|| true` from sanitizer ctest step in `.github/workflows/ci.yml`
- [ ] T004 [P] [US1] Remove `|| true` from fuzz build step in `.github/workflows/ci.yml`
- [ ] T005 [US1] Add `--error-exitcode=1` to clang-tidy target in `cmake/MucOpcUaStaticAnalysis.cmake`
- [ ] T006 [P] [US1] Wire `MUC_OPCUA_SANITIZERS` cache variable to inject `-fsanitize=` flags — add sanitizer flag injection logic to `CMakeLists.txt` (root, near line 209)
- [ ] T007 [P] [US1] Add `standard` profile to CI test matrix in `.github/workflows/ci.yml`
- [ ] T008 [P] [US1] Add code coverage CI job (gcovr) to `.github/workflows/ci.yml`
- [ ] T009 [P] [US1] Set `CMAKE_BUILD_TYPE=Debug` for CI host-build and test-profile jobs in `.github/workflows/ci.yml`

---

## Phase 3: Undefined Behavior Fixes (US4) — P0

**Goal**: Buffer-safety bugs fixed; server robust against crafted input
**Independent test**: `mu_checked_memcpy_off` with offset > capacity returns NULL; expanded NodeId reader with NULL buffer returns error

- [ ] T010 [P] [US4] Add `if (dst_offset > dst_cap) return NULL;` guard to `mu_checked_memcpy_off` in `src/core/safe_mem.h:24`
- [ ] T011 [P] [US4] Add `if (!reader->buffer) return BAD_UNEXPECTEDERROR;` guard to `mu_binary_read_expanded_nodeid` in `src/encoding/binary_nodeid.c:228`

---

## Phase 4: Code Quality — Session/Timeout Fixes (US2) — P0

**Goal**: Session tokens can't be hardcoded; session timeout works in all profiles
**Independent test**: `mu_session_create` triggers deprecation warning; session timeout fires in micro profile build

- [ ] T012 [US2] Add `__attribute__((deprecated))` to `mu_session_create` in `src/services/session.c` and `src/services/session.h`
- [ ] T013 [US2] Gate session timeout on `MUC_OPCUA_SESSION_TIMEOUT` instead of `MUC_OPCUA_MULTI_CHUNK` in `src/services/session.h` and `src/core/server.c`
- [ ] T014 [US2] Define `MUC_OPCUA_SESSION_TIMEOUT` flag in CMakeLists.txt or config.h for profiles that need it

---

## Phase 5: Code Quality — Function Decomposition (US2) — P0

**Goal**: Long functions split into testable helpers with verified behavior
**Independent test**: Full test suite passes after each extraction

- [ ] T015 [US2] Extract per-token-type helpers from `handle_activate_session` in `src/core/dispatch_session.c` (lines 472-847) — target ≤3 helper functions, each <150 lines
- [ ] T016 [US2] Extract common logic from `mu_server_poll`'s duplicated `#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS` branches in `src/core/server.c` (lines 809-1044)
- [ ] T017 [US2] Decompose `handle_create_monitored_items` in `src/core/service_dispatch.c` (lines 1357-1565) into filter-resolution, node-validation, item-allocation, and response-encode helpers

---

## Phase 6: Architecture Layer Cleanup (US3) — P1

**Goal**: Clean separation between core platform and service logic
**Independent test**: Service files can include `service_context.h` without pulling in full `server_internal.h`

- [ ] T018 [P] [US3] Create `src/core/service_context.h` with only the fields services require (session, config, subs, address_space) from `struct mu_server`
- [ ] T019 [US3] Update service-layer files (`session.c`, `subscription_publish.c`, `browse.c`, `discovery.c`) to include `service_context.h` instead of `server_internal.h`
- [ ] T020 [P] [US3] Add `#error` guard in `CMakeLists.txt` or `config.h` when `MUC_OPCUA_STANDARD_PROFILE` and `MUC_OPCUA_PROFILE=standard` are set inconsistently

---

## Phase 7: Test Coverage — New Tests (US5) — P0

**Goal**: Replace placeholder tests with behavioral tests; add tests for untested hot-path code
**Independent test**: `test_transfer_subscriptions` no longer only checks constants; `test_service_message.c` exists with ≥3 test cases

- [ ] T021 [P] [US5] Replace placeholder in `tests/unit/test_transfer_subscriptions.c` with behavioral tests: valid transfer, 16-item cap overflow, negative count → Bad_DecodingError, response encoding
- [ ] T022 [P] [US5] Replace placeholders in `tests/unit/test_time_sync.c` and `tests/unit/test_reverse_connect.c` with behavioral tests or explicit `#warning "STUB"` markers
- [ ] T023 [US5] Create `tests/unit/test_service_message.c` with ≥3 test cases for `mu_parse_service_prefix` and `mu_write_service_prefix` round-trip
- [ ] T023a [P] [US5] Replace placeholder in `tests/unit/test_aggregate_full.c` with behavioral tests or explicit `#warning "STUB"` markers
- [ ] T023b [P] [US5] Replace placeholder in `tests/unit/test_audit_events.c` with behavioral tests or explicit `#warning "STUB"` markers
- [ ] T023c [P] [US5] Replace placeholder in `tests/unit/test_complex_types.c` with behavioral tests or explicit `#warning "STUB"` markers

---

## Phase 8: Test Coverage — CI + Interop (US5) — P0

**Goal**: Coverage tracked in CI, fuzzers exercised, interop tests cover Subscribe/Publish
**Independent test**: Coverage report appears in CI; fuzz CI step passes; interop_smoke.py has Subscribe/Publish test

- [ ] T024 [P] [US5] Add fuzz CI job running each of 7 fuzz targets for 60 seconds in `.github/workflows/ci.yml` (overlaps with T008 coverage job)
- [ ] T025 [P] [US5] Add Subscribe/Publish and Call service tests to `tests/interop/interop_smoke.py`

---

## Phase 9: Documentation — Version Tracking (US6) — P1

**Goal**: Semantic versioning established for all consumers
**Independent test**: `version.h` exports `MUC_OPCUA_VERSION_*` macros; `CHANGELOG.md` exists

- [ ] T026 [P] [US6] Create `CHANGELOG.md` following keepachangelog.com format with Unreleased section and initial entries
- [ ] T027 [US6] Create `include/muc_opcua/version.h.in` CMake template with `MUC_OPCUA_VERSION_MAJOR/MINOR/PATCH`
- [ ] T028 [US6] Configure CMake to generate `version.h` from template, add `git tag v0.1.0`

---

## Phase 10: Polish & Verification

- [ ] T029 Run full test suite — `cd build/test && ctest --output-on-failure` — verify all 121 standard tests pass, no regressions
- [ ] T030 Push branch to trigger CI run (or use `gh workflow run` if `workflow_dispatch` is configured) — verify new CI steps succeed

---

## Dependencies

```
T001 (baseline)
 ├─ T002-T009 (US1: CI/CD) ── independent, parallel
 ├─ T010-T011 (US4: UB fixes) ── independent, parallel
 ├─ T012-T014 (US2: session/timeout) ── depends on US1? (no, separate concern)
 ├─ T015-T017 (US2: decomposition) ── depends on T001 (needs tests passing first)
 ├─ T018-T020 (US3: architecture) ── independent
 ├─ T021-T023c (US5: new tests) ── independent
 ├─ T024-T025 (US5: CI+fuzz+interop) ── depends on T001 + US1 (CI baseline)
 ├─ T026-T028 (US6: versioning) ── independent
 └─ T029-T030 (verification) ── depends on all above
```

## Parallel Execution

```
# Batch 1 — can all run concurrently:
T002, T003, T004 (CI || true removals)
T010, T011 (UB fixes)
T012-T014 (session/timeout)
T021, T022, T023a, T023b, T023c (new tests + stub replacements)
T026, T027 (versioning)

# Batch 2 — after Batch 1 baseline established:
T005, T006, T007, T008, T009 (remaining CI fixes)
T015, T016, T017 (function decomposition — needs T001 confirming tests pass)
T018, T019, T020 (architecture — needs service files from other phases stable)
T023, T024, T025 (remaining tests + interop)
T028 (tag after T026+T027)

# Batch 3 — verification:
T029, T030
```

## Task Summary

| Phase | Tasks | Story | Priority |
|-------|-------|-------|----------|
| 1: Setup | T001 | — | P0 |
| 2: CI/CD | T002-T009 | US1 | P0 |
| 3: UB Fixes | T010-T011 | US4 | P0 |
| 4: Session/Timeout | T012-T014 | US2 | P0 |
| 5: Decomposition | T015-T017 | US2 | P0 |
| 6: Architecture | T018-T020 | US3 | P1 |
| 7: New Tests | T021-T023c | US5 | P0 |
| 8: CI+Interop | T024-T025 | US5 | P0 |
| 9: Versioning | T026-T028 | US6 | P1 |
| 10: Verification | T029-T030 | — | P0 |
| **Total** | **33 tasks** | | |

---

## Implementation Strategy

**MVP (minimum viable round)**:
- T001 (baseline) → T010-T014 (UB + session fixes: 5 tasks) → T029 (verify)
- This closes the 4 most impactful bugs in ~6 tasks

**Full deliverable**: All 33 tasks, executing parallel batches as shown above

**Per AGENTS.md rules**:
- Each task is coherent and atomic
- Tasks can be assigned individually to agents
- OPC-UA spec grounding references are included where applicable
- `speckit-analyze` must run after tasks generation
