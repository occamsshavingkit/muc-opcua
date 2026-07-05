# Feature Specification: Clear Remaining TODO

**Feature ID**: 042-clear-remaining-todo
**Status**: Draft
**Created**: 2026-07-05
**Source**: TODO.md active backlog (CodeRabbit + complexity audit)

## Summary

Fix all remaining TODO.md items: 9 CodeRabbit findings (CR1-CR9), 6
parameter-heavy functions (CP1-CP6), 5 duplicate code patterns (CD1-CD6
excluding CD4), and 3 deep-nesting sites (CN1-CN3). 23 items across 4
categories. No behavioral changes.

## User Stories

### US1: Fix CodeRabbit Bugs and Quality Items (Priority: P1)

**As a** developer, **I want** the 9 CodeRabbit-flagged issues fixed,
**so that** minor correctness, style, and documentation gaps are closed.

### US2: Reduce Parameter Counts (Priority: P2)

**As a** developer reading function signatures, **I want** functions with
>5 parameters refactored to use context structs, **so that** signatures
are comprehensible.

### US3: Eliminate Duplicate Code (Priority: P2)

**As a** maintainer, **I want** duplicated logic consolidated, **so that**
fixes in one place don't require fixing the same bug elsewhere.

### US4: Reduce Deep Nesting (Priority: P3)

**As a** developer debugging filter dispatch, **I want** deeply nested
if/else chains replaced with dispatch tables or extracted functions,
**so that** control flow is clear.

## Requirements

### US1 — CodeRabbit Items (9)

- **FR-001**: `test_write_response.c` MUST have explicit skip placeholder when `MUC_OPCUA_SERVICE_WRITE` is undefined (CR1)
- **FR-002**: `asym_chunk.c` MUST reuse `mu_safe_int32_from_size_t` for cert/thumb length clamping (CR2)
- **FR-003**: `ctz.h` MUST unify `mu_ctz_u32` to `static inline` for both GCC/Clang and fallback paths (CR3)
- **FR-004**: `safe_mem.h` MUST move `#include <stdint.h>` to top of file (CR4)
- **FR-005**: `test_dispatch_subscription.c` helper functions MUST be extracted to shared test utility or moved from `test_transfer_subscriptions.c` (CR5)
- **FR-006**: Spec doc count mismatch in `specs/039/spec.md` MUST be reconciled (CR6)
- **FR-007**: CTZ lookup table MUST be accounted in `specs/039/plan.md` size budget (CR7)
- **FR-008**: Placeholder verification in `specs/039/quickstart.md` MUST handle `#warning "STUB"` markers (CR8)
- **FR-009**: Full verification block in `specs/039/quickstart.md` MUST be self-contained (CR9)

### US2 — Parameter Reduction (6)

- **FR-010**: `mu_sym_chunk_wrap` (13 params) MUST use a context struct (CP1)
- **FR-011**: `mu_asym_chunk_wrap` (12 params) MUST use a context struct (CP2)
- **FR-012**: `mu_asym_chunk_unwrap` (9 params) MUST use a context struct (CP3)
- **FR-013**: `mu_uasc_finalize_symmetric` (8 params) MUST use a context struct (CP4)
- **FR-014**: `drive_subscription_id_status_array` (8 params) MUST use a context struct (CP5)
- **FR-015**: `mu_read_process_with_user_index` (8 params) MUST use a context struct (CP6)

### US3 — Duplicate Code (5)

- **FR-016**: ns0-numeric NodeId check duplicated 13× MUST consolidate to `mu_nodeid_is_ns0_numeric()` (CD1)
- **FR-017**: MbedTLS crypto adapter 8 near-identical AES/OAEP functions MUST consolidate to parameterized helpers (CD2)
- **FR-018**: Host crypto adapter 6 near-identical cipher/OAEP functions MUST consolidate to parameterized helpers (CD3)
- **FR-019**: Duplicate signature verification in `dispatch_session.c` MUST consolidate (CD5)
- **FR-020**: Poll helper duplication in `server.c` across `#ifdef` branches MUST unify (CD6)

### US4 — Deep Nesting (3)

- **FR-021**: Filter-type dispatch chain in `service_dispatch.c` MUST replace nested if/else with dispatch table (CN1)
- **FR-022**: Per-item filter-update logic MUST reduce nesting depth (CN2)
- **FR-023**: `read_event_filter_body` string-comparison chain MUST use lookup table (CN3)

## Success Criteria

- **SC-001**: All 23 items addressed and removed from TODO.md
- **SC-002**: All existing tests pass unchanged on default, micro, standard profiles
- **SC-003**: clang-format, cppcheck clean
- **SC-004**: No behavioral changes, no new public API symbols where avoidable

## Scope Boundaries

- **In Scope**: 23 items from TODO.md active backlog
- **Out of Scope**: Binary size measurement, new features, test additions
