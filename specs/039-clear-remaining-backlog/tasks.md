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

- [ ] T002 [P] [US1] Replace hardcoded protocol version `0`/`0u` with `MU_OPC_UA_TCP_PROTOCOL_VERSION` in `src/core/tcp_connection.c` (FR-001, OPC-10000-6 §7.1.2.3 Hello/Acknowledge)
- [ ] T003 [P] [US1] Fix ModifyMonitoredItems sampling interval=-1 to resolve to subscription's `publishing_interval_ms` in `src/core/service_dispatch.c` (FR-002, OPC-10000-4 §5.13.3 ModifyMonitoredItems)
- [ ] T004 [US1] Fix ModifyMonitoredItems `timestampsToReturn` — validate against known values and apply (not `(void)` discard) in `src/core/service_dispatch.c` (FR-003, OPC-10000-4 §5.13.3)

**Checkpoint**: All 3 bugs fixed and tests pass.

---

## Phase 3: US2 — Hot-Path Performance (P1)

**Goal**: Fix 13 hot-path items; eliminate memory leak, enable read cache, reduce redundant work
**Independent Test**: ASAN zero leaks (HP1); cache hit rate >0% (HP2); profiler shows ≤1 get_tick_ms per poll (HP3)

- [ ] T005 [US2] Add `free()` to variant array encoding callers — found at `src/services/read.c` read_result path where allocated variant buffer must be freed after encoding (HP1, FR-004, OPC-10000-6 §5.2.2.16 Variant)
- [ ] T006 [US2] Fix read cache lookup timestamp comparison in `src/services/read.c` — cache hits must return stored values; measure and record cache hit rate >0% (HP2, FR-005, SC-003)
- [ ] T007 [US2] Capture `get_tick_ms()` once at poll entry and pass result through poll chain in `src/core/server.c` — verify via instrumentation that ≤1 call occurs per poll cycle (HP3, FR-006, SC-004)
- [ ] T008 [P] [US2] Avoid re-parsing full NodeId during auth token extraction — reuse already-parsed NodeId fields in `src/core/dispatch_session.c` (HP4, FR-007, OPC-10000-4 §5.7.4 ActivateSession)
- [ ] T009 [P] [US2] Batch per-primitive `ensure_bytes()` bounds checks — validate total payload size once per read op instead of per-primitive in `src/encoding/binary_reader.c` (HP5, FR-008)
- [ ] T010 [P] [US2] Batch per-primitive `ensure_space()` bounds checks — validate total write space once per write op instead of per-primitive in `src/encoding/binary_writer.c` (HP6, FR-009)
- [ ] T011 [P] [US2] Remove full DataValue `memset(0)` per result; memset only the fields actually written in `src/services/read.c` (HP7, FR-010)
- [ ] T012 [P] [US2] Avoid `memmove()` of unconsumed recv buffer — track read position and defer shift until buffer is fully consumed in `src/core/server.c` (HP8, FR-011)
- [ ] T013 [US2] Add `next_session_timeout_ms` field to server struct; poll loop checks this before scanning session slots in `src/core/server.c` (HP9, FR-012)
- [ ] T014 [P] [US2] Write type-check in `src/core/dispatch_attribute.c` must use stack-local copy of current type instead of re-reading from address space (HP10, FR-013)
- [ ] T015 [P] [US2] Replace O(N^2) startup duplicate node check with sort-then-scan approach (qsort node IDs then linear scan) in `src/address_space/address_space.c` — use `qsort` on host; embedded targets without stdlib must provide a local sort implementation (HP11, FR-014)
- [ ] T016 [P] [US2] Parse MessageSize once and store in chunk context; remove second parse site in `src/core/message_chunk.c` (HP12, FR-015, OPC-10000-6 §6.7.2 MessageChunk)
- [ ] T017 [P] [US2] Gate `listen()` in `mu_server_init()` behind `#ifdef MUC_OPCUA_LISTEN_IN_INIT` in `src/core/server.c` (HP13, FR-016)

**Checkpoint**: All hot-path fixes applied; ASAN clean; read cache functional.

---

## Phase 4: US3 — Code Quality, Architecture & UB (P2)

**Goal**: Fix 10 items — eliminate code duplication, fix layer violations, remediate undefined behavior
**Independent Test**: Compiles with `-fstrict-aliasing -Wconversion` on GCC; passes on non-GCC compiler or fallback tested

- [ ] T018 [P] [US3] Add `mu_ctz_u32()` in `src/core/ctz.h` — use `__builtin_ctz` on GCC/Clang, de Bruijn 32-entry lookup table fallback for other compilers; update all 9 call sites (CQ6, FR-017)
- [ ] T019 [P] [US3] Consolidate three duplicate LE uint32 write helpers — standardize on `mu_binary_write_u32_le` in `src/encoding/binary_encoding.h`; migrate call sites in `src/encoding/binary_reader.c`, `src/encoding/binary_nodeid.c` (CQ7, FR-018)
- [ ] T020 [US3] Extract shared message-dispatch logic — create `mu_server_dispatch_message()` as static function in `src/core/server.c` replacing three duplicate if/else chains (CQ8, FR-019)
- [ ] T021 [P] [US3] Guard `#include "subscription.h"` behind `#ifdef MUC_OPCUA_SERVICE_SUBSCRIPTION` in `src/core/server_internal.h` (AR3, FR-020)
- [ ] T022 [P] [US3] Fix `handle_history_update()` to propagate actual error codes instead of always returning `GOOD` in `src/services/history.c` (AR4, FR-021)
- [ ] T023 [P] [US3] Fix strict aliasing violation — replace type-punning pointer cast in `src/encoding/binary_string.c` with `memcpy`-based extraction (UB3, FR-022)
- [ ] T024 [P] [US3] Document C11 requirement in `README.md` (UB4, FR-023)
- [ ] T025 [P] [US3] Guard `int32_t` ← `uint32_t` casts in `src/core/safe_mem.h` and callers — add explicit range check before narrowing assignment (UB5, FR-024)
- [ ] T026 [P] [US3] Fix `mu_binary_write_expanded_nodeid` — preserve NamespaceUri (bit 2) and ServerIndex (bit 3) flags when encoding in `src/encoding/binary_nodeid.c` (UB6, FR-025)
- [ ] T027 [P] [US3] Audit `calloc` usage: `grep` for calloc callers in `src/`; replace each site with static/stack buffers where possible; gate remaining calloc behind `MUC_OPCUA_ALLOW_HEAP` flag (UB7, FR-026)

**Checkpoint**: Code compiles cleanly with strict flags; no UB warnings; portable CTZ works.

---

## Phase 5: US4 — Security Hardening (P2)

**Goal**: Fix 4 security-sensitive issues
**Independent Test**: Nonce round-trip with 128-byte nonces; RSA decrypt with 4096-bit key; trust list timing test

- [ ] T028 [P] [US4] Fix OPN nonce length — raise `KDF_MAX_SEED` to 128 or use per-operation nonce size in `src/security/`; verify 128-byte nonce round-trip (SE1, FR-027, OPC-10000-6 §6.7.6 OpenSecureChannel)
- [ ] T029 [P] [US4] Size password decrypt buffer for 4096-bit RSA keys in `src/security/`; verify decrypt succeeds with max key size (SE2, FR-028, OPC-10000-4 §5.7.4 ActivateSession)
- [ ] T030 [P] [US4] Replace non-constant-time trust list `memcmp()` with `mu_constant_time_memcmp()` in `src/security/trust_list.c` (SE3, FR-029, OPC-10000-6 §5.2.2 constant-time operations)
- [ ] T031 [P] [US4] Add `MU_EVENT_FILTER_MAX_SELECT_COUNT` cap (100) to `read_event_filter_body()` in `src/services/event_filter.c` — return `Bad_EventFilterInvalid` if exceeded (SE4, FR-030, OPC-10000-4 §7.22 EventFilter)

**Checkpoint**: All security fixes applied; security tests pass.

---

## Phase 6: US5 — Test Coverage & Interop Hardening (P3)

**Goal**: Replace placeholder tests, add missing tests, audit/harden interop tests
**Independent Test**: No placeholder-only tests remain; interop smoke covers Subscribe/Publish and Call

- [ ] T032 [P] [US5] Replace placeholder tests in `tests/unit/test_transfer_subscriptions.c` with behavioral tests: valid transfer, 16-item cap overflow, negative count → `Bad_DecodingError`, response encoding (FR-033, TQ9, OPC-10000-4 §5.7.3 TransferSubscriptions)
- [ ] T032a [P] [US5] Add isolated unit tests for `dispatch_subscription.c` subscription dispatch handler in `tests/unit/test_dispatch_subscription.c` — cover create/modify/delete monitored items dispatch paths (FR-033, TQ9)
- [ ] T033 [P] [US5] Replace placeholders in `tests/unit/test_time_sync.c` and `tests/unit/test_reverse_connect.c` with behavioral tests or `#warning "STUB"` markers (FR-031, TQ7)
- [ ] T034 [P] [US5] Replace placeholders in `tests/unit/test_aggregate_full.c`, `tests/unit/test_audit_events.c`, `tests/unit/test_complex_types.c` with behavioral tests or `#warning "STUB"` markers (FR-031, TQ7)
- [ ] T035 [P] [US5] Create `tests/unit/test_service_message.c` with test cases: prefix write+parse round-trip, boundary message-id values, invalid input → error (FR-031)
- [ ] T036 [P] [US5] Fix circular verification in response encoding tests — replace encode-then-decode-self patterns with comparison against independent binary fixture bytes in `tests/unit/test_write_response.c`, `tests/unit/test_read_response.c`, and any other affected encode test files (FR-032, TQ8)
- [ ] T037 [P] [US5] Catalog gaps in `*_encode` unit tests — audit each encode test file for mandatory field assertions; produce list of missing field checks per test file in `tests/unit/` (FR-035)
- [ ] T037b [US5] Fill encode test gaps identified by T037 — add missing field assertions to each `*_encode` unit test file in `tests/unit/` (FR-035)
- [ ] T037a [P] [US5] Audit interop smoke tests for per-service coverage gaps: list service sets tested vs missing in `tests/interop/interop_smoke.py`; identify Subscribe/Publish and Call gaps for T040 (FR-034)
- [ ] T038 [P] [US5] Generate binary fixture for WriteResponse message in `tests/fixtures/write_response.bin`; add round-trip encode→decode test in `tests/unit/test_write_response.c` verifying each field (FR-036)
- [ ] T039 [P] [US5] Verify `reader->position == expected_length` in response decode tests — add assertion to each `*_decode_response` test in `tests/unit/` (FR-037)
- [ ] T040 [US5] Add Subscribe/Publish and Call service tests to `tests/interop/interop_smoke.py` using existing asyncua client infrastructure (FR-038, OPC-10000-4 §5.13 Subscriptions, OPC-10000-4 §5.6 Method service)

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
 ├─ US2 (T005-T017) ── independent, within phase: mostly [P]
 ├─ US3 (T018-T027) ── independent, within phase: all [P] (different files)
 ├─ US4 (T028-T031) ── independent, all [P]
  ├─ US5 (T032-T040) ── T037 catalogs → T037b fills; T037a audits → T040 fills
 ├─ US6 (T041-T043) ── independent, all [P]
 └─ T044-T049 (polish) ── depends on all above
```

All user story phases (US2-US6) can run in parallel after T001 baseline check. Within US5, T037a (audit) should run before T040 (fill gaps). Within any phase, [P] tasks target different files and are independent.

## Parallel Execution

```
# Batch 1 — baseline check:
T001

# Batch 2 — ALL user stories in parallel (different files):
# US1: T002, T003, T004
# US2: T005, T006, T007, T008, T009, T010, T011, T012, T013, T014, T015, T016, T017
# US3: T018, T019, T020, T021, T022, T023, T024, T025, T026, T027
# US4: T028, T029, T030, T031
# US5: T032, T032a, T033, T034, T035, T036, T037, T037a, T037b, T038, T039, T040
#   (T037 catalogs gaps → T037b fills gaps; T037a audits gaps → T040 fills gaps)
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
| 6: Tests/Interop | T032-T040 (incl. T032a, T037a, T037b) | US5 | P3 | 12 |
| 7: Documentation | T041-T043 | US6 | P3 | 3 |
| 8: Polish | T044-T049 | — | P0 | 6 |
| **Total** | | | | **52** |

---

## Implementation Strategy

**MVP (US1 only)**: T001 → T002-T004 → T044 → done. Fixes the 3 verified bugs in ~5 tasks.

**Fast delivery**: T001 → all US1-6 in parallel → T044-T049. 52 tasks, high parallelism.

**Per AGENTS.md rules**:
- Each task is coherent and atomic — one fix per task
- Tasks can be assigned individually to single agents
- OPC UA spec grounding citations included for protocol tasks
- `speckit-analyze` must run after task generation
