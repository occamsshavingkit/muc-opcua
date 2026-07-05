# Tasks: Clear Remaining Backlog

**Feature**: 039-clear-remaining-backlog
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)
**Generated**: 2026-07-05

## Format: `- [ ] [ID] [P?] [Story?] Description with file path`

- **[P]** = parallelizable (different files, no dependencies)
- **[US1-6]** = user story label from spec.md

---

## Phase 1: Setup

- [ ] T001 Verify all existing tests pass on baseline: `cd build && ctest --output-on-failure`. Record count.

---

## Phase 2: US1 — Fix Verified Bugs (P0)

**Goal**: Fix 3 verified bugs from TODO.md
**Independent Test**: Each bug fix has a targeted unit test confirming correct behavior

- [ ] T002 [P] [US1] Replace hardcoded protocol version `0`/`0u` with `MU_OPC_UA_TCP_PROTOCOL_VERSION` in `src/core/tcp_connection.c` (FR-001)
- [ ] T003 [P] [US1] Fix ModifyMonitoredItems sampling interval=-1 to resolve to subscription's `publishing_interval_ms` in `src/core/service_dispatch.c` (FR-002)
- [ ] T004 [US1] Fix ModifyMonitoredItems `timestampsToReturn` — validate against known values and apply (not `(void)` discard) in `src/core/service_dispatch.c` (FR-003)

**Checkpoint**: All 3 bugs fixed and tests pass.

---

## Phase 3: US2 — Hot-Path Performance (P1)

**Goal**: Fix 13 hot-path items; eliminate memory leak, enable read cache, reduce redundant work
**Independent Test**: ASAN zero leaks (HP1); cache hit rate >0% (HP2); profiler shows ≤1 get_tick_ms per poll (HP3)

- [ ] T005 [US2] Add `free()` to callers of variant array encoding in `src/encoding/binary_variant.c` call chain (HP1, FR-004)
- [ ] T006 [US2] Fix read cache lookup logic so cache hits return stored values instead of always missing in `src/services/read.c` (HP2, FR-005)
- [ ] T007 [US2] Batch `get_tick_ms()` calls — call once per poll iteration, share result in `src/core/server.c` (HP3, FR-006)
- [ ] T008 [P] [US2] Avoid re-parsing full NodeId during auth token extraction in `src/core/dispatch_session.c` (HP4, FR-007)
- [ ] T009 [P] [US2] Batch per-primitive `ensure_bytes()` bounds checks in `src/encoding/binary_reader.c` (HP5, FR-008)
- [ ] T010 [P] [US2] Batch per-primitive `ensure_space()` bounds checks in `src/encoding/binary_writer.c` (HP6, FR-009)
- [ ] T011 [P] [US2] Remove full DataValue zero-init per result; only set populated fields in `src/services/read.c` (HP7, FR-010)
- [ ] T012 [P] [US2] Avoid `memmove()` of unconsumed recv buffer when not needed in `src/core/server.c` (HP8, FR-011)
- [ ] T013 [US2] Add `next_session_timeout_ms` tracking to avoid scanning all session slots in `src/core/server.c` (HP9, FR-012)
- [ ] T014 [P] [US2] Write type-check in `dispatch_attribute.c` must not re-read current value from address space in `src/core/dispatch_attribute.c` (HP10, FR-013)
- [ ] T015 [P] [US2] Sort-then-scan duplicate node check at startup instead of O(N^2) in `src/address_space/address_space.c` (HP11, FR-014)
- [ ] T016 [P] [US2] Parse MessageSize exactly once per chunk; remove duplicate parse in `src/core/message_chunk.c` and/or `src/core/server.c` (HP12, FR-015)
- [ ] T017 [P] [US2] Defer `listen()` from `mu_server_init()` or gate behind config flag in `src/core/server.c` (HP13, FR-016)

**Checkpoint**: All hot-path fixes applied; ASAN clean; read cache functional.

---

## Phase 4: US3 — Code Quality, Architecture & UB (P2)

**Goal**: Fix 10 items — eliminate code duplication, fix layer violations, remediate undefined behavior
**Independent Test**: Compiles with `-fstrict-aliasing -Wconversion` on GCC; passes on non-GCC compiler or fallback tested

- [ ] T018 [P] [US3] Add portable `mu_ctz_u32()` fallback for `__builtin_ctz()` with de Bruijn lookup table; update all 9 call sites in `src/core/` (CQ6, FR-017)
- [ ] T019 [P] [US3] Consolidate three duplicate LE uint32 write helpers into `mu_binary_write_u32_le` in `src/encoding/binary_encoding.h` + migrate call sites in `src/encoding/` (CQ7, FR-018)
- [ ] T020 [US3] Extract shared message-dispatch logic from three duplicate patterns in `src/core/server.c` into `mu_server_dispatch_message()` in `src/core/dispatch.c` (CQ8, FR-019)
- [ ] T021 [P] [US3] Guard `#include "subscription.h"` behind `#ifdef MUC_OPCUA_SERVICE_SUBSCRIPTION` in `src/core/server_internal.h` (AR3, FR-020)
- [ ] T022 [P] [US3] Fix `handle_history_update()` to return actual error codes instead of always `GOOD` in `src/services/history.c` (AR4, FR-021)
- [ ] T023 [P] [US3] Fix strict aliasing violation — replace type-punning pointer cast with `memcpy` or union in `src/encoding/binary_string.c` (UB3, FR-022)
- [ ] T024 [P] [US3] Document C11 requirement in `AGENTS.md` or `README.md` (UB4, FR-023)
- [ ] T025 [P] [US3] Guard `int32_t` ← `uint32_t` casts against impl-defined behavior in affected source files (UB5, FR-024)
- [ ] T026 [P] [US3] Fix `mu_binary_write_expanded_nodeid` to preserve NamespaceUri/ServerIndex flags in `src/encoding/binary_nodeid.c` (UB6, FR-025)
- [ ] T027 [P] [US3] Replace or gate `calloc` usage that pulls in heap allocator on embedded targets in affected source files (UB7, FR-026)

**Checkpoint**: Code compiles cleanly with strict flags; no UB warnings; portable CTZ works.

---

## Phase 5: US4 — Security Hardening (P2)

**Goal**: Fix 4 security-sensitive issues
**Independent Test**: Nonce round-trip with 128-byte nonces; RSA decrypt with 4096-bit key; trust list timing test

- [ ] T028 [P] [US4] Fix OPN nonce length — accommodate nonces up to 128 bytes (KDF_MAX_SEED=64 is insufficient) in `src/security/` (SE1, FR-027)
- [ ] T029 [P] [US4] Size password decrypt buffer for 4096-bit RSA keys in `src/security/` (SE2, FR-028)
- [ ] T030 [P] [US4] Replace non-constant-time trust list `memcmp()` with `mu_constant_time_memcmp()` in `src/security/trust_list.c` (SE3, FR-029)
- [ ] T031 [P] [US4] Add `MU_EVENT_FILTER_MAX_SELECT_COUNT` cap (100) to `read_event_filter_body()` in `src/services/event_filter.c` — return `Bad_EventFilterInvalid` if exceeded (SE4, FR-030)

**Checkpoint**: All security fixes applied; security tests pass.

---

## Phase 6: US5 — Test Coverage & Interop Hardening (P3)

**Goal**: Replace placeholder tests, add missing tests, audit/harden interop tests
**Independent Test**: No placeholder-only tests remain; interop smoke covers Subscribe/Publish and Call

- [ ] T032 [P] [US5] Replace placeholder tests in `tests/unit/test_transfer_subscriptions.c` with behavioral tests: valid transfer, 16-item cap overflow, negative count → `Bad_DecodingError`, response encoding (TQ9)
- [ ] T033 [P] [US5] Replace placeholders in `tests/unit/test_time_sync.c` and `tests/unit/test_reverse_connect.c` with behavioral tests or `#warning "STUB"` markers (TQ7)
- [ ] T034 [P] [US5] Replace placeholders in `tests/unit/test_aggregate_full.c`, `tests/unit/test_audit_events.c`, `tests/unit/test_complex_types.c` with behavioral tests or `#warning "STUB"` markers (TQ7)
- [ ] T035 [P] [US5] Create `tests/unit/test_service_message.c` with ≥3 test cases for `mu_parse_service_prefix` and `mu_write_service_prefix` round-trip (TQ7)
- [ ] T036 [P] [US5] Fix circular verification in response encoding tests — verify against independent binary fixtures instead of encode-then-decode-self in `tests/unit/` (TQ8)
- [ ] T037 [P] [US5] Audit all `*_encode` unit tests for mandatory field verification and add missing field checks in `tests/unit/`
- [ ] T038 [P] [US5] Generate binary fixture for WriteResponse and add round-trip test in `tests/fixtures/` and `tests/unit/`
- [ ] T039 [P] [US5] Verify `reader->position == expected_length` in all response decode tests in `tests/unit/`
- [ ] T040 [US5] Add Subscribe/Publish and Call service tests to `tests/interop/interop_smoke.py` using existing asyncua client infrastructure

**Checkpoint**: All placeholder tests replaced; interop tests cover Subscribe/Publish and Call service sets.

---

## Phase 7: US6 — Documentation & Spec Grounding (P3)

**Goal**: Add ADRs for key design decisions; add spec grounding comments to source files
**Independent Test**: `ls docs/adr/` shows 4 ADRs; `grep "OPC-10000"` returns hits in each specified file

- [ ] T041 [P] [US6] Create ADRs in `docs/adr/`: `0001-zero-heap-design.md`, `0002-adapter-pattern.md`, `0003-profile-tier-system.md`, `0004-poll-model.md` (D4, FR-039)
- [ ] T042 [P] [US6] Add spec grounding comments (OPC-10000-6 citations) to `src/encoding/binary_nodeid.c`, `src/encoding/binary_datavalue.c`, `src/core/uasc.c` (D2, FR-040)
- [ ] T043 [P] [US6] Add spec grounding comments (OPC-10000 part citations) to `include/muc_opcua/encoding.h` and `include/muc_opcua/server.h` (D3, FR-041)

**Checkpoint**: All ADRs created; spec grounding verified via grep.

---

## Phase 8: Polish & Verification

- [ ] T044 Run full test suite: `cd build && ctest --output-on-failure` — verify all tests pass, no regressions
- [ ] T045 Verify TODO.md is updated — remove all 43 completed items from the "Remaining Active Backlog" section
- [ ] T046 Verify ASAN zero leaks on full test suite
- [ ] T047 Build and test with standard profile: `cmake -DMUC_OPCUA_PROFILE=standard .. && cmake --build . && ctest --output-on-failure`
- [ ] T048 Run size measurement: compare flash/RAM before and after to confirm ≤1% regression
- [ ] T049 Run `git push` to trigger CI and verify all CI jobs pass

---

## Dependencies

```
T001 (baseline)
 ├─ US1 (T002-T004) ── independent
 ├─ US2 (T005-T017) ── independent, within phase: T005-T013 mostly [P]; T013 depends on T007 (same file)
 ├─ US3 (T018-T027) ── independent, within phase: all [P] (different files)
 ├─ US4 (T028-T031) ── independent, all [P]
 ├─ US5 (T032-T040) ── independent, all [P]
 ├─ US6 (T041-T043) ── independent, all [P]
 └─ T044-T049 (polish) ── depends on all above
```

All user story phases (US2-US6) can run in parallel after T001 baseline check. US1 requires care since T003 and T004 touch the same file (`service_dispatch.c`) — those two must be sequential but are only 2 tasks in the critical path.

## Parallel Execution

```
# Batch 1 — baseline check:
T001

# Batch 2 — ALL user stories in parallel (different files):
# US1: T002, T003, T004 (sequential within US1 due to same-file constraint)
# US2: T005, T006, T007, T008, T009, T010, T011, T012, T013, T014, T015, T016, T017
# US3: T018, T019, T020, T021, T022, T023, T024, T025, T026, T027
# US4: T028, T029, T030, T031
# US5: T032, T033, T034, T035, T036, T037, T038, T039, T040
# US6: T041, T042, T043

# Batch 3 — polish:
T044, T045, T046, T047, T048, T049
```

## Task Summary

| Phase | Tasks | Story | Priority | Count |
|-------|-------|-------|----------|-------|
| 1: Setup | T001 | — | P0 | 1 |
| 2: Bug Fixes | T002-T004 | US1 | P0 | 3 |
| 3: Hot-Path | T005-T017 | US2 | P1 | 13 |
| 4: Code Quality/UB | T018-T027 | US3 | P2 | 10 |
| 5: Security | T028-T031 | US4 | P2 | 4 |
| 6: Tests/Interop | T032-T040 | US5 | P3 | 9 |
| 7: Documentation | T041-T043 | US6 | P3 | 3 |
| 8: Polish | T044-T049 | — | P0 | 6 |
| **Total** | | | | **49** |

---

## Implementation Strategy

**MVP (US1 only)**: T001 → T002-T004 → T044 → done. Fixes the 3 verified bugs in ~5 tasks.

**Fast delivery**: T001 → all US1-6 in parallel → T044-T049. ~49 tasks, high parallelism.

**Per AGENTS.md rules**:
- Each task is coherent and atomic — one fix per task
- Tasks can be assigned individually to single agents
- OPC UA spec grounding citations included for protocol tasks
- `speckit-analyze` must run after task generation
