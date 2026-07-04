# Implementation Plan: Deferred Audit Findings

**Branch**: `031-deferred-audit-fixes` | **Date**: 2026-07-04 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/031-deferred-audit-fixes/spec.md`
**Source**: `docs/review/five-lens-audit-2026-07-04.md` (15 deferred findings)

## Summary

Fix 13 deferred audit findings and 12 remaining style fixes across 3 files
(subscription.c, service_dispatch.c, base_nodes.c) and 3 platform crypto
adapters. Subscription hot-path gets single-pass publish scan with bitmap, fabs removal
(net -12 KB ROM), and 64-bit division avoidance. Service dispatch gets session
ordering, cert token ifdef consolidation, nonce zeroization, dispatch table
optimization, and profile URI caching. Base nodes get guard consolidation.
Platform crypto adapters get 10 brace fixes; subscription.c gets 2 type fixes.

**Technical approach**: All fixes are surgical single-function changes with
clear spec citations. The subscription bitmap uses existing
MU_MAX_MONITORED_ITEMS headroom. Dispatch table uses existing metadata from
g_supported_services[]. Style fixes are mechanical with zero behavioral impact.

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged)
**Primary Dependencies**: None new. Existing: CMake, Unity, host/OpenSSL + mbedTLS + wolfSSL crypto adapters
**Storage**: N/A
**Testing**: Existing ctest suite (105 tests). Add 3 new unit tests: T27 deadband NONE (`test_subscription_deadband.c`), T31 publish timer guard (`test_subscription_publish.c`), T8 session ordering (`test_dispatch_session_order.c`). All existing tests stay green.
**Target Platform**: RP2040/Cortex-M0+, Arduino, host (unchanged)
**Project Type**: Portable C library with thin platform adapters (unchanged)
**Performance Goals**: Publish cycle CPU reduced ~3x; dispatch lookup to O(1); `<math.h>` removed (net -12 KB ROM)
**Constraints**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap; all four ARM profiles + ASAN/UBSan green
**Scale/Scope**: 13 findings + 12 style fixes; 3 source files + 3 platform adapters; 3 new tests
**OPC UA Normative References**: OPC-10000-4 §5.7.3 (CreateSession/ActivateSession), §5.13.1.2 (Publish), §7.22.2 (DataChangeFilter), §5.4.3.2 (GetEndpoints), §7.1 (Service Sets); OPC-10000-3 §5.2 (AddressSpace TypeSystem nodes); OPC-10000-6 §6.4 (SecurityPolicy key derivation)
**Target OPC UA Profile/Conformance Units**: Nano/Micro/Embedded 2017 + full (unchanged)
**Conformance Status Target**: profile-targeting (unchanged; this strengthens evidence, adds no claim)

## Embedded Size Budget

**Flash Impact**: **Net ~-12 KB** (T5/T24 removes `<math.h>` double emulation library, ~-12 KB; bitmap + guards ~+300 B; dispatch table ~+50 B; net reduction)
**RAM Impact**: **~+32 B** (T4 bitmap uses existing MU_MAX_MONITORED_ITEMS headroom; no new allocations)
**Heap Use**: None (unchanged)
**Static Tables Added**: Dispatch indexing table (~50 B in .rodata). No new tables in subscription.c.
**Transport Buffers**: Unchanged
**Crypto Dependency Impact**: None new

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **I. Spec Fidelity**: PASS — all 13 findings cite exact OPC UA sections from the audit. Fixes preserve or correct spec compliance. Style fixes have no wire-visible impact.
- **II. Embedded-First C Core**: PASS — no OS/platform leakage; all changes in existing portable C11 source files and platform adapters. Bitmap uses static allocation. No new heap allocation; bitmap is static-sized to MU_MAX_MONITORED_ITEMS. Dispatch table is const .rodata.
- **III. Minimal OPC UA Surface**: PASS — no new services, policies, transports, or encodings. Fixes existing behavior to spec. Profiles unchanged; no new conformance claims.
- **IV. Protocol Correctness Gates**: PASS — T27 (deadband) and T31 (timer guard) have test-first coverage via new unit tests. T8 (session ordering) has test-first coverage. Other fixes are correctness restorations with existing test coverage.
- **V. Security and Conformance Honesty**: PASS — T12 (cert token ifdef) closes a potential decode-without-verify path. T13 (nonce zeroize) closes the last nonce zeroization gap. No security trade-offs for size.
- **VI. Fixed Toolchain and Reproducible Builds**: PASS — CMake/ctest/CI unchanged. Platform adapter style fixes are mechanical.
- **VII. Size Discipline**: PASS — net ROM reduction (T5 dominates). All profiles stay within +3%/+5% gates. Bitmap fits existing headroom.

**Result: PASS (no violations). Complexity Tracking not required.**

## Project Structure

### Documentation (this feature)

```text
specs/031-deferred-audit-fixes/
├── plan.md              # This file
├── research.md          # Phase 0 — decision table for optimization choices
├── data-model.md        # Phase 1 — entities affected by fixes
├── quickstart.md        # Phase 1 — how to verify each fix
├── contracts/           # Phase 1 — fix contracts
└── tasks.md             # Phase 2 (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
src/services/
├── subscription.c       # T4 (single-pass scan with bitmap), T5/T24 (fabs→inline), T6 (64-bit div),
│                        #   T27 (deadband NONE), T31 (publish timer guard)
│                        #   MISRA 10.4 fixes (lines 1576, 1610)
src/core/
├── service_dispatch.c   # T8 (session create order), T12 (cert token ifdef),
│                        #   T13 (nonce stack zeroize), T23 (dispatch table),
│                        #   T25 (profile URI cache)
src/address_space/
├── base_nodes.c         # T28 (guard consolidation)
src/platform/
├── host_crypto_adapter.c    # MISRA 15.6 braces (~4 sites)
├── mbedtls_crypto_adapter.c # MISRA 15.6 braces (~3 sites)
├── wolfssl_crypto_adapter.c # MISRA 15.6 braces (~3 sites)
tests/
├── unit/
│   ├── test_subscription_deadband.c    # New: T27 deadband NONE semantics (T001)
│   ├── test_subscription_publish.c     # New: T31 publish timer guard (T002)
│   ├── test_dispatch_session_order.c   # New: T8 session ordering (T008)
│   └── (existing tests)                # No changes expected
```

**Structure Decision**: Existing layout kept. All fixes are in their natural source files. New tests for T27 and T31 go in `tests/unit/` following existing naming conventions. The audit report at `docs/review/five-lens-audit-2026-07-04.md` serves as the traceability source. Platform crypto adapters are modified only for mechanical brace additions.

## Complexity Tracking

> No Constitution Check violations — intentionally empty.
