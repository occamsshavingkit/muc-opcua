# Tasks: Clear Remaining TODO

**Feature**: 042-clear-remaining-todo

## Format: `- [ ] [ID] [P?] [Story?] Description with file path`

---

## Phase 1: Setup

- [ ] T001 Verify baseline: `cd build && ctest --output-on-failure` — all pass

---

## Phase 2: US1 — CodeRabbit Items (P1)

- [ ] T002 [P] [US1] Add explicit skip placeholder to `tests/unit/test_write_response.c` when `MUC_OPCUA_SERVICE_WRITE` undefined — use `#ifndef MUC_OPCUA_SERVICE_WRITE` with `TEST_PASS_MESSAGE("requires MUC_OPCUA_SERVICE_WRITE")` (FR-001, CR1)
- [ ] T003 [P] [US1] Replace manual cert/thumb length clamping in `src/security/asym_chunk.c` with `mu_safe_int32_from_size_t()` calls (FR-002, CR2)
- [ ] T004 [P] [US1] Unify `mu_ctz_u32` in `src/core/ctz.h` to `static inline` for both GCC/Clang and fallback (FR-003, CR3)
- [ ] T005 [P] [US1] Move `#include <stdint.h>` to top of file in `src/core/safe_mem.h` (FR-004, CR4)
- [ ] T006 [US1] Extract duplicate test helpers from `tests/unit/test_dispatch_subscription.c` and `tests/unit/test_transfer_subscriptions.c` into `tests/support/test_helpers.h` (FR-005, CR5)
- [ ] T007 [P] [US1] Reconcile backlog count in `specs/039-clear-remaining-backlog/spec.md` — change "43 remaining" to "40 remaining" or add missing 3 items (FR-006, CR6)
- [ ] T008 [P] [US1] Add CTZ lookup table (128 bytes) to size budget in `specs/039-clear-remaining-backlog/plan.md` (FR-007, CR7)
- [ ] T009 [P] [US1] Fix placeholder verification grep in `specs/039-clear-remaining-backlog/quickstart.md` to also match `#warning "STUB"` markers (FR-008, CR8)
- [ ] T010 [P] [US1] Make Full Verification block in `specs/039-clear-remaining-backlog/quickstart.md` self-contained — change `cd build` to work from any directory (FR-009, CR9)

---

## Phase 3: US2 — Parameter Reduction (P2)

- [ ] T011 [US2] Refactor `mu_sym_chunk_wrap` (13 params) in `src/security/sym_chunk.c` — group params into `mu_sym_wrap_params_t` struct (FR-010, CP1)
- [ ] T012 [US2] Refactor `mu_asym_chunk_wrap` (12 params) in `src/security/asym_chunk/wrap.c` — group into `mu_asym_wrap_params_t` (FR-011, CP2)
- [ ] T013 [US2] Refactor `mu_asym_chunk_unwrap` (9 params) in `src/security/asym_chunk/unwrap.c` — group into `mu_asym_unwrap_params_t` (FR-012, CP3)
- [ ] T014 [US2] Refactor `mu_uasc_finalize_symmetric` (8 params) in `src/core/uasc.c` — group into `mu_uasc_sym_finalize_params_t` (FR-013, CP4)
- [ ] T015 [US2] Refactor `drive_subscription_id_status_array` (8 params) in `src/core/service_dispatch/subscription_helpers.c` — group into context struct (FR-014, CP5)
- [ ] T016 [US2] Refactor `mu_read_process_with_user_index` (8 params) in `src/services/read/read_process.c` — group into `mu_read_process_params_t` (FR-015, CP6)

---

## Phase 4: US3 — Duplicate Code (P2)

- [ ] T017 [US3] Create `mu_nodeid_is_ns0_numeric()` in `src/encoding/binary_nodeid.h` — replace all 13 inline uses across `service_dispatch/`, `dispatch_session/`, `server/` (FR-016, CD1)
- [ ] T018 [US3] Consolidate MbedTLS AES/OAEP functions in `src/platform/mbedtls_crypto_adapter.c` — parameterize by key size and hash type (FR-017, CD2)
- [ ] T019 [US3] Consolidate host crypto cipher/OAEP functions in `src/platform/host_crypto/` — parameterize by key size and hash type (FR-018, CD3)
- [ ] T020 [US3] Consolidate duplicate signature verification in `src/core/service_dispatch/activate_session.c` — extract shared `verify_buf` construction (FR-019, CD5)
- [ ] T021 [US3] Unify poll helper duplication across `#ifdef` branches in `src/core/server/poll.c` (FR-020, CD6)

---

## Phase 5: US4 — Deep Nesting (P3)

- [ ] T022 [US4] Replace nested if/else filter-type dispatch in `src/core/service_dispatch/filter_reader.c` with dispatch table (FR-021, CN1)
- [ ] T023 [US4] Reduce per-item filter-update nesting in `src/core/service_dispatch/monitored_items.c` — extract helper functions (FR-022, CN2)
- [ ] T024 [US4] Replace string-comparison chain in `read_event_filter_body` in `src/core/service_dispatch/filter_reader.c` with lookup table (FR-023, CN3)

---

## Phase 6: Polish

- [ ] T025 Run clang-format on all modified files
- [ ] T026 Run cppcheck — zero warnings
- [ ] T027 Run ctest on default, micro, standard profiles — all pass
- [ ] T028 Update TODO.md — mark all 23 items completed

---

## Dependencies

All US phases are independent (different files). Within US3, T017 (shared helper) should precede T020-T021 which use it. Within US2, each function is in a different file and independent.

## Task Summary

| Phase | Tasks | Count |
|-------|-------|-------|
| Setup | T001 | 1 |
| US1: CodeRabbit | T002-T010 | 9 |
| US2: Parameters | T011-T016 | 6 |
| US3: Duplicates | T017-T021 | 5 |
| US4: Nesting | T022-T024 | 3 |
| Polish | T025-T028 | 4 |
| **Total** | | **28** |

## Implementation Strategy

Per AGENTS.md: single-task subagents. Independent [P] tasks dispatched in parallel.
Sequential tasks (same file) run one at a time.
