# Implementation Plan: Five-Lens Audit Findings Cleanup

**Branch**: `029-fix-audit-findings` | **Date**: 2026-07-04 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/029-fix-audit-findings/spec.md`
**Audit Source**: `docs/review/five-lens-audit-2026-07-04.md` (42 findings)

## Summary

Remediate all 42 findings from the five-lens audit (security, memory, correctness, performance) across 5 user stories. Six critical fixes (T1-T6) address the highest severity items: ActivateSession nonce fail-closed, password buffer zeroization, Browse reference loss, subscription triple-scan, fabs removal from deadband path, and 64-bit division avoidance. Eight strong fixes (T7-T14) harden crypto, correctness, and embedded trust. Fifteen worthwhile fixes (T15-T29) improve spec compliance and performance. Thirteen low-severity items (T30-T42) provide cleanup and test coverage. Net ROM reduction of ~12 KB (from `<math.h>` removal).

**Technical approach**: All fixes are surgical — single-function, single-file changes with clear spec citations from the audit. Each P1 fix has a RED test written first (Constitution IV). The existing ctest suite plus per-profile CI stays green. No new services, policies, transports, or conformance claims.

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged)
**Primary Dependencies**: None new. Existing: CMake, Unity, host/OpenSSL + mbedTLS + wolfSSL crypto adapters
**Storage**: N/A
**Testing**: New unit tests for 9 coverage gaps; all existing tests stay green; host + per-profile CI
**Target Platform**: RP2040/Cortex-M0+, Arduino, host (unchanged)
**Project Type**: Portable C library with thin platform adapters (unchanged)
**Performance Goals**: Subscription publish-cycle CPU reduced ~3x; `fabs()` removed from hot path; net -12 KB ROM
**Constraints**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap; all four ARM profiles + ASAN/UBSan green
**Scale/Scope**: 42 findings across 5 user stories; ~15 source files touched; ~9 new tests
**OPC UA Normative References**: OPC-10000-4 §5.7.3, §5.8.5.2, §5.9.2, §5.11.2.3, §7.22.2, §7.38.2; OPC-10000-3 §5.2.4, §5.6; OPC-10000-6 §5.2.2.15
**Target OPC UA Profile/Conformance Units**: Nano/Micro/Embedded 2017 + full (unchanged)
**Conformance Status Target**: profile-targeting (unchanged; this strengthens evidence, adds no claim)

## Embedded Size Budget

**Flash Impact**: **Net ~-12 KB** (T5 removes `<math.h>` double-precision emulation library, ~-12 KB; remaining fixes ~+300 B total)
**RAM Impact**: **0 bytes** (T4 bitmap uses ~+32 B of existing `MU_MAX_MONITORED_ITEMS` headroom; T2 uses existing `MU_SECURE_SESSION_MAX` scratch sub-region; all other fixes are zero-RAM)
**Heap Use**: None (unchanged)
**Static Tables Added**: None in the library. Test fixtures as needed.
**Transport Buffers**: Unchanged
**Crypto Dependency Impact**: None new. T7 adds explicit OAEP parameter calls already in OpenSSL but not linked differently.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS — all 42 findings cite exact OPC UA sections verified by the audit. Each fix preserves or corrects spec compliance.
- **Embedded C Core**: PASS — no OS/platform leakage; all changes in existing portable C11 source files; Pico adapter gets documentation only.
- **Memory Model**: PASS — no new heap allocation; T2 password buffer moves from stack to existing static scratch region (MU_SECURE_SESSION_MAX); T4 bitmap uses existing struct headroom.
- **Minimal OPC UA Surface**: PASS — no new services, policies, transports, or encodings. Fixes existing behavior to spec.
- **Profile Research**: PASS — profiles unchanged; no new conformance claims; status remains profile-targeting.
- **Correctness Gates**: PASS — all P1 fixes are test-first; 9 new tests for coverage gaps; existing suite stays green across all profiles.
- **Security Honesty**: PASS — T1 strengthens session auth to fail-closed; T2 closes the last zeroization gap; no security trade-offs for size.
- **Toolchain Discipline**: PASS — CMake/ctest/CI unchanged; `-fstack-protector-strong` is a documentation recommendation only (T38).
- **Size Discipline**: PASS — net ROM reduction; all profiles stay within +3%/.+5% gates; documented headroom preserved.

**Result: PASS (no violations). Complexity Tracking not required.**

## Project Structure

### Documentation (this feature)

```text
specs/029-fix-audit-findings/
├── plan.md              # This file
├── research.md          # Phase 0 — decision table for each finding group
├── data-model.md        # Phase 1 — entities affected by fixes
├── quickstart.md        # Phase 1 — how to verify each fix
├── contracts/           # Phase 1 — fix contracts
└── tasks.md             # Phase 2 (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
src/core/
├── service_dispatch.c    # T1 (fill_server_nonce), T2 (password zeroize), T8 (session create order), T10 (write validation), T12 (cert token ifdef), T13 (nonce zeroize), T21 (Query arrays), T23 (dispatch scan), T25 (profile URI parse)
├── server.c              # T14 (Pico documentation hook)
src/services/
├── browse.c              # T3 (Browse reference assignment), T22 (TypeDef cache)
├── subscription.c        # T4 (single-pass scan), T5 (fabs removal), T6 (64-bit div), T24 (math.h removal), T27 (deadband None), T31 (publish timer guard)
├── node_management.c     # T9 (deleteTargetReferences)
├── read.c                # T19 (BrowseName namespace), T20 (TimestampsToReturn)
├── history.c             # T29 (encoding mask)
├── secure_channel.c      # T17 (channel ID entropy)
├── session.c             # T33 (documentation)
src/address_space/
├── value_source.c        # T11 (all scalar types)
├── base_nodes.c          # T28 (guard consolidation)
├── address_space.c       # T42 (validation)
src/security/
├── certificate.c         # T15 (doc), T32 (self-check)
├── security_policy.c     # T26 (const)
├── trustlist.c           # T16 (doc), T34 (doc)
src/encoding/
├── binary_string.c       # T40 (inline)
├── binary_writer.c       # T40
src/platform/
├── host_crypto_adapter.c   # T7 (explicit OAEP params)
├── mbedtls_crypto_adapter.c # T18 (signature_length check)
platform/pico/
├── mu_pico_adapter.c     # T14 (doc), T39 (ROSC doc)
├── README.md             # T14
tests/
├── unit/                 # 9 new tests (T1, T2, T3, T8, T9, T10, T19, T20, T29)
├── integration/          # Existing tests — no new integration tests expected
docs/
├── review/               # Audit report source
├── traceability/         # Updated for any new claims
└── adr/                  # Documentation-only findings recorded here
```

**Structure Decision**: Existing layout kept. All fixes are in their natural source files. New tests go in `tests/unit/` following existing naming conventions. The audit report at `docs/review/five-lens-audit-2026-07-04.md` serves as the traceability source.

## Complexity Tracking

> No Constitution Check violations — intentionally empty.
