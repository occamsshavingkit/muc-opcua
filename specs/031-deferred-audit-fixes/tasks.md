# Tasks: Deferred Audit Findings

**Input**: Design documents from `specs/031-deferred-audit-fixes/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md
**Audit Source**: `docs/review/five-lens-audit-2026-07-04.md`

**Tests**: Tests are mandatory per Constitution IV for service-dispatch behavior,
session state, security, and wire-visible changes. Each protocol-facing task below
includes a test-first step.

**Organization**: Tasks are grouped by user story. Each task is atomic (one finding
per task where coherent) and cites grounding OPC UA spec references.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1-US6)
- File paths are exact. OPC UA part/section citations are required for protocol changes.

---

## Phase 1: Tests for User Story 2 (T27, T31) — RED Phase

**Purpose**: Write failing tests for deadband and publish-timer fixes before implementation (Constitution IV).

**CRITICAL**: These tests must be written and verified RED before any implementation tasks.

- [x] T001 [P] [US2] Add unit test for deadband NONE semantics: verify `monitored_item_change_reportable` returns false when `deadband_type == MU_DEADBAND_TYPE_NONE` and value unchanged, returns true when value changed. Grounding: OPC-10000-4 §7.22.2 (DataChangeFilter.deadbandType). File: `tests/unit/test_subscription_deadband.c`.

- [x] T002 [P] [US2] Add unit test for publish timer loop bound: verify `advance_publish_timer` terminates within 100 iterations when `publishing_interval` is 0. Grounding: OPC-10000-4 §5.13.1.2 (Publish Service Set). File: `tests/unit/test_subscription_publish.c`.

**Checkpoint**: Both tests fail (RED). Proceed to implementation.

---

## Phase 2: User Story 1 — Subscription Hot-Path (Priority: P1) [MVP]

**Goal**: Reduce publish-cycle CPU by ~3x, remove `<math.h>` (~-12 KB ROM), avoid 64-bit division on M0+. All in `src/services/subscription.c`.

**Independent Test**: Publish-cycle benchmark shows reduced CPU per item; `<math.h>` no longer linked; `__aeabi_uldivmod` absent from `advance_sample_timer`.

**Files**: `src/services/subscription.c` (1,692 lines; single file avoids merge conflicts).

### Tests for User Story 1

> Existing test suite covers subscription publish path (105 tests). No new unit tests required — the existing `test_subscription.c` and integration tests validate publish-cycle behavior. Verify existing tests continue to pass after each implementation task.

### Implementation for User Story 1

- [x] T003 [US1] Implement single-pass publish scan with bitmap in `mu_subscriptions_tick` in `src/services/subscription.c:1604`. Replace the three-pass pattern: collect reportable item indices into a `uint32_t` bitmap during the sampling loop (line 1674), then iterate only set bits for encoding (`write_data_change_notification` at line 1024) and cleanup (`clear_reported_items` at lines 1207-1209). All three helper functions remain intact but are called only for bitmap entries, not full array scans. **Edge case**: With STANDARD profile enabled, a 4th pass for triggered items runs — triggered items share the same MU_MAX_MONITORED_ITEMS pool; verify bitmap sizing accommodates the maximum across all profiles. Grounding: OPC-10000-4 §5.13.1.2. Size: ~+100 B flash, ~+32 B RAM (bitmap within MU_MAX_MONITORED_ITEMS headroom).

- [x] T004 [US1] Remove `#include <math.h>` (line 6) and replace `fabs()` with inline conditional negation in `monitored_item_change_reportable` in `src/services/subscription.c:131`. Replace `fabs((double)numeric - (double)item->last_reported_numeric)` with `double diff = numeric - last_reported; if (diff < 0) diff = -diff;`. Verify no other `math.h` dependency exists in subscription.c before removing include. Grounding: OPC-10000-4 §7.22.2. Size: ~-12 KB ROM (removes double-precision math library).

- [x] T005 [US1] Remove 64-bit division from `advance_sample_timer` common path in `src/services/subscription.c:373`. Keep existing `elapsed < interval` fast-path guard; replace catch-up `elapsed / interval` with repeated subtraction bounded to 10 steps. Grounding: OPC-10000-4 §5.13.1.2 (sampling interval behavior). Size: ~+20 B flash.

**Checkpoint**: Hot-path fixes compile. All subscription tests pass. `<math.h>` removed. Run `ctest -R subscription`.

---

## Phase 3: User Story 2 — Subscription Correctness (Priority: P1)

**Goal**: Fix deadband NONE always-reporting and add publish-timer loop bound. Both in `src/services/subscription.c`.

**Independent Test**: T001 and T002 pass (go GREEN). Deadband NONE monitors produce 0 notifications when value is unchanged. Publish timer terminates within 100 iterations for any interval.

### Implementation for User Story 2

- [x] T006 [US2] Fix `monitored_item_change_reportable` deadband NONE semantics in `src/services/subscription.c:144-145`. When `deadband_type == MU_DEADBAND_TYPE_NONE`, fall through to value equality comparison instead of early-returning true. Preserve absolute and percentage deadband paths below. Grounding: OPC-10000-4 §7.22.2 (DataChangeFilter — "DeadbandType NONE: None (no deadband). Data changes are always reported." — i.e., report only ON change, not always). Size: ~-8 B flash (removes unconditional return).

- [x] T007 [US2] Add publish timer loop iteration bound in `advance_publish_timer` in `src/services/subscription.c:1132`. Add local counter to `do...while(0)` loop; terminate after 100 iterations. Guard against 0-interval or extremely small interval near-infinite spin. Grounding: OPC-10000-4 §5.13.1.2 (publish interval). Size: ~+12 B flash.

**Checkpoint**: T001 and T002 pass (GREEN). All subscription tests pass. Run `ctest -R subscrip`.

---

## Phase 4: User Story 3 — Dispatch Correctness & Security (Priority: P2)

**Goal**: Fix session create ordering (dangling slot), consolidate cert token `#ifdef`, zeroize nonce stack copies. All in `src/core/service_dispatch.c`.

**Independent Test**: Encode failure after session creation cleans up session. Cert token path excluded when `MUC_OPCUA_SECURITY` is off. Valgrind reports no nonce bytes on stack after function return.

### Tests for User Story 3

- [x] T008 [US3] Add unit test for session ordering: create session, force response encode failure, verify session slot is reclaimed (no leaked slot in `mu_max_sessions`). Grounding: OPC-10000-4 §5.7.3 (CreateSession — server shall return the CreateSession response; encode failure after session allocation must not silently leak a session slot). File: `tests/unit/test_dispatch_session_order.c`.

- [x] T009 [P] [US3] [VERIFICATION] Add compile-time guard check for cert token ifdef: build without `MUC_OPCUA_SECURITY` and confirm cert token handler (type 327) is fully excluded. This is a verification step, not a code change — it validates T011. Run: `cmake -DMUC_OPCUA_SECURITY=OFF .. && cmake --build .`.

### Implementation for User Story 3

- [x] T010 [US3] Reorder session creation vs. response prefix in `handle_create_session` in `src/core/service_dispatch.c:780-811`. Move `write_response_prefix(MU_STATUS_GOOD)` before `mu_session_create_with_identifiers`. Add cleanup path: if subsequent encode step fails, call `mu_session_close` on the newly created session. **Edge case**: Guard cleanup with a check that session creation succeeded (e.g., `session_id > 0` or check return value) — if encode failure occurs before `mu_session_create_with_identifiers` is reached, skip cleanup to avoid double-free or invalid close. Grounding: OPC-10000-4 §5.7.3 (CreateSession — response encoding failure must not leak session slot). Size: ~+30 B flash.

- [x] T011 [US3] Consolidate certificate user token (type 327) decode and verify under single `#ifdef MUC_OPCUA_SECURITY` block in `handle_activate_session` in `src/core/service_dispatch.c:1029-1223`. Move decode logic inside the same `#ifdef` guard as verify logic. Grounding: OPC-10000-4 §5.7.3 (ActivateSession — identity token types). Size: ~+0 B (preprocessor restructure only).

- [x] T012 [US3] Zeroize `nonce_buf[32]` stack copies in `handle_create_session` and `handle_activate_session` in `src/core/service_dispatch.c:772,1240`. Add `mu_secure_zero(nonce_buf, sizeof(nonce_buf))` after `memcpy` of nonce into session slot, at both locations. Grounding: OPC-10000-4 §5.7.3 (CreateSession/ActivateSession — ServerNonce is part of the session establishment and is subsequently used as KDF input for derived session keys per OPC-10000-6 §6.4; stack remnants after copy constitute leaked key material). Constitution V requires zeroization discipline, T2 (password zeroize, completed 030) establishes the pattern. Size: ~+24 B flash.

**Checkpoint**: T008-T009 pass. Session cleanup works. Cert token path guarded. Valgrind clean on nonce. Run `ctest -R dispatch_services`.

---

## Phase 5: User Story 4 — Dispatch Performance (Priority: P2)

**Goal**: Reduce dispatch table lookup to O(log N), cache profile URIs in GetEndpoints. Both in `src/core/service_dispatch.c`.

**Independent Test**: Dispatch comparisons reduced from ~15 to ≤5 per request. Profile URI parsed once per distinct URI.

### Implementation for User Story 4

- [x] T013 [US4] Replace linear scan service dispatch lookup with binary search in `src/core/service_dispatch.c:295-302`. Sort `g_supported_services[]` by `request_id` at compile time (static const array already sorted for link-time ordering), then binary search at runtime. Grounding: OPC-10000-4 §7.1 (Service Sets — request dispatch by NodeId). Size: ~+50 B flash (binary search code; no new table needed).

- [x] T014 [US4] Cache profile URIs in GetEndpoints handler in `src/core/service_dispatch.c:1644-1648`. Parse profile URI array once into local index array, then compare each endpoint against cached indices instead of re-parsing the wire UInt32Array per endpoint. **Edge case**: If the profile URI array on the wire is empty (array count == 0), skip caching and treat all endpoints as having no matching profiles. Grounding: OPC-10000-4 §5.4.3.2 (GetEndpoints — transport profile URI matching). Size: ~+20 B flash.

**Checkpoint**: Dispatch benchmark confirms <5 comparisons. Profile URIs parsed once. Run `ctest -R dispatch`.

---

## Phase 6: User Story 5 — Base Nodes Guard Consolidation (Priority: P3)

**Goal**: Consolidate 35+ individual `#if`/`#endif` guards into single `#ifdef` block. File: `src/address_space/base_nodes.c`.

**Independent Test**: All profiles compile and link; no linker errors; ROM/RAM unchanged.

### Implementation for User Story 5

- [x] T015 [US5] Consolidate type-system node guards in `src/address_space/base_nodes.c:263-731`. Replace 35+ individual `#if`/`#endif` blocks wrapping each node definition with a single `#ifdef` block covering the entire type-system node table section. All guards use the same profile feature flag; no subset is ever excluded independently. Grounding: OPC-10000-3 §5.2 (AddressSpace — TypeSystem node hierarchy definitions). No wire-visible behavior change — preprocessor restructure only. Size: 0 bytes.

**Checkpoint**: All 4 profiles compile. No linker errors. Run `ctest -R address_space`.

---

## Phase 7: User Story 6 — MISRA Style Fixes (Priority: P3)

**Goal**: Fix remaining 12 MISRA violations: 10 single-statement brace violations in platform crypto adapters, 2 `size_t` vs `0u` type issues in subscription.c.

**Independent Test**: Codacy scan shows 0 MISRA 15.6 findings in platform crypto files; 0 MISRA 10.4 in subscription.c. All tests pass.

### Implementation for User Story 6

- [x] T016 [P] [US6] Add braces to single-statement bodies in `src/platform/host_crypto_adapter.c`. Locate all single-statement `if`/`for`/`while` bodies lacking braces (~4 sites). Add compound statement braces per MISRA-C:2012 Rule 15.6. Manual inspection required — do not use automated fix scripts (known corruption risk per lesson 6 in HANDOFF.md). Size: ~+60 B flash (brace emission only; no behavioral change).

- [x] T017 [P] [US6] Add braces to single-statement bodies in `src/platform/mbedtls_crypto_adapter.c`. Locate all single-statement `if`/`for`/`while` bodies lacking braces (~3 sites). Add compound statement braces per MISRA-C:2012 Rule 15.6. Size: ~+45 B flash (no behavioral change).

- [x] T018 [P] [US6] Add braces to single-statement bodies in `src/platform/wolfssl_crypto_adapter.c`. Locate all single-statement `if`/`for`/`while` bodies lacking braces (~3 sites). Add compound statement braces per MISRA-C:2012 Rule 15.6. Size: ~+45 B flash (no behavioral change).

- [x] T019 [US6] Fix `size_t` vs `0u` type inconsistencies in `src/services/subscription.c:1576,1610`. Replace `0u` literal with `0` or `(size_t)0` where compared against `size_t` variables, per MISRA-C:2012 Rule 10.4. Size: 0 bytes. **WARNING**: Lines 1576,1610 are in `mu_subscriptions_tick` (same function as T003). Run this task AFTER T003-T007 to avoid merge conflicts.

**Checkpoint**: T016-T019 done. Style clean on affected files. Run `ctest` — all 105 tests pass (no behavioral change).

---

## Phase 8: Polish & Cross-Cutting Validation

**Purpose**: Full regression, size measurement, and CI readiness across all profiles.

- [x] T020 Run full host test suite: `ctest -V`. All 105 tests must pass.

- [x] T021 Run ASAN + UBSan build: compile with `-DMUC_OPCUA_SANITIZE=ON`, run `ctest -V`. Zero sanitizer failures.

- [x] T022 Run ARM profile cross-compile: verify nano, micro, embedded, and full profiles compile with CMake embedded toolchain. Zero warnings.

- [x] T023 Measure flash/RAM impact and performance: compare ARM profile sizes before and after all changes. Verify net ROM reduction (~-12 KB expected from T004), verify net RAM ≤ +32 bytes (bitmap within headroom). Confirm `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap. Run publish-cycle benchmark to verify CPU per monitored item reduced ≥ 3x versus pre-T003 baseline (SC-001). Use `tests/benchmark/hotpath_benchmark.c` or add subscription-specific benchmark.

- [x] T024 Run formatting and static analysis: `clang-format --dry-run` clean, `cppcheck` with existing config, verify no new Codacy findings introduced.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Tests US2)**: No dependencies — write & verify RED immediately.
- **Phase 2 (US1 — hot-path)**: No dependencies. Can start in parallel with Phase 3 after Phase 1.
- **Phase 3 (US2 — correctness)**: Depends on Phase 1 (tests go GREEN). T006 and T007 are independent inside the phase.
- **Phase 4 (US3 — dispatch correctness)**: No dependencies on US1/US2. Different file.
- **Phase 5 (US4 — dispatch performance)**: Can follow US3 in same file, or start in parallel with US3.
- **Phase 6 (US5 — base nodes)**: No dependencies. Different file.
- **Phase 7 (US6 — style fixes)**: No dependencies. Platform files and subscription.c are different from affected areas in US1-US4.
- **Phase 8 (Polish)**: Depends on all user story phases complete.

### Within Each File (Sequential)

```
src/services/subscription.c:
  Phase 1 → Phase 2 (T003 → T004 → T005) → Phase 3 (T006 → T007)
  Note: T019 (US6 type fixes) must NOT collide with T003-T007 — schedule after Phase 3 or verify no conflict.

src/core/service_dispatch.c:
  Phase 4 (T008 test → T010 → T011 → T012) → Phase 5 (T013 → T014)

src/address_space/base_nodes.c:
  Phase 6 (T015) — standalone, no intra-file dependencies

Platform crypto adapters:
  Phase 7 (T016, T017, T018) — all [P], three different files, fully parallel
```

### Parallel Opportunities

| Can run in parallel | Tasks |
|---------------------|-------|
| Phase 1 tests | T001 ∥ T002 |
| Phase 2 + Phase 4 | T003-T005 (subscription.c) ∥ T008-T012 (dispatch.c) — different files |
| Phase 2 + Phase 6 | T003-T005 ∥ T015 — different files |
| Phase 3 + Phase 4 | T006-T007 ∥ T010-T012 — different files |
| Phase 3 + Phase 6 | T006-T007 ∥ T015 |
| Phase 4 tests | T008 ∥ T009 |
| Phase 5 + Phase 6 | T013-T014 ∥ T015 |
| Phase 7 all | T016 ∥ T017 ∥ T018 (different platform files) |
| Phase 8 all after implementation | T020 ∥ T021 ∥ T022 ∥ T024 |

### Suggested MVP Scope

**MVP = Phase 1 + Phase 2 + Phase 3** (US1 + US2: subscription hot-path + correctness). This is the highest-impact work — ~12 KB ROM savings, 3x publish cycle improvement, and deadband/timer correctness. Tests: T001-T002 go GREEN; all subscription tests pass.

---

## Notes

- Tasks T003-T007, T010-T014, T019 must be coordinated for `subscription.c` and `service_dispatch.c` to avoid merge conflicts. Recommended: Phase 2 **before** Phase 3 (within same file), and Phases 2+3 **after** or **parallel with** Phases 4+5 (different files).
- T016-T018 (platform adapters): manual inspection required per HANDOFF.md lesson 6 — automated brace fix scripts CORRUPTED the arduino adapter. Inspect each site individually.
- T008 (session order test): requires ability to force encode failure. May need mock writer adapter if not already available in test framework.
- T013 (binary search dispatch): verify `g_supported_services[]` is sorted by `request_id` at compile time. If not, sort entries or use alternative lookup.
- T004 (fabs removal): the inline `diff = -diff` for negative values must produce identical IEEE 754 double results as `fabs()`. Verify on host with FPU and embedded without.
