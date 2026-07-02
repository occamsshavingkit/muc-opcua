# Implementation Plan: Five-Lens Audit Remediation

**Branch**: `025-audit-remediation` | **Date**: 2026-07-01 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/025-audit-remediation/spec.md`

## Summary

Remediate the twelve findings of the completed five-lens audit across three
concern areas: (1) **wire-security correctness** — verify the ActivateSession
ClientSignature, enforce certificate validity-period and mandatory (fail-closed)
trust for secured policies, bind the CreateSession certificate to the OPN channel
certificate, and make the username-token nonce check unconditional and
constant-time; (2) **embedded readiness** — give the RP2040 adapter real entropy
and time, relocate ~2 KB handshake stack buffers into the existing secure-scratch
arena, remove the double soft-float from the subscription hot path, and eliminate
the silent 64-node address-space cliff; (3) **maintainability** — a single feature
gate header with dependency `#error`s, extraction of the inlined session/channel
handlers and monitoring decoders out of `service_dispatch.c`, a table-driven
SecurityPolicy parameter set, and assorted build/config cleanup.

**Technical approach**: The three wire-security fixes are test-first (RED fixtures
before code) per Constitution IV. The ClientSignature verification reuses the
existing `rsa_sha256_verify` adapter primitive against the certificate that
opened the SecureChannel — so no per-session certificate copy is stored (RAM
neutral on the 2-session pool); binding CreateSession cert == OPN cert (FR-005)
is what makes the channel cert the correct verification key. Certificate validity
adds one new optional crypto-adapter hook (`verify_certificate_validity`) with a
fail-closed default when absent. Everything else is behavior-preserving
refactoring validated against the existing regression/interop fixtures and the
size/speed gates.

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged), C99-compatible style where practical
**Primary Dependencies**: None new. Existing: CMake, Unity (host tests), mbedTLS/wolfSSL/OpenSSL (optional crypto backends), Pico SDK (RP2040), libFuzzer (decoder fuzz)
**Storage**: N/A (no data storage; static/caller-provided memory model preserved)
**Testing**: Existing `ctest` host suite MUST stay green; new RED fixtures for ClientSignature verify, certificate validity/trust, username-nonce, integer deadband, the two decoder defects, and the feature-guard `#error`s; decoder fuzz smoke unchanged
**Target Platform**: RP2040/Cortex-M0+ (Pico SDK) primary, Arduino second, host for dev/test/interop
**Project Type**: Portable C library with thin platform adapters and examples (unchanged)
**Performance Goals**: No throughput regression beyond the gate; integer deadband should *reduce* per-sample cost on Embedded/full; batch Read/Write unaffected
**Constraints**: `.text` ≤ +3%, `.data + .bss` ≤ +5% per profile, no new heap, normalized throughput ≥ 0.85× baseline; no wire-visible change to valid-message encoding
**Scale/Scope**: ~12 findings across `src/core`, `src/services`, `src/security`, `src/encoding`, `src/address_space`, `include/muc_opcua`, `platform/pico`, `cmake/`, plus new tests/fixtures
**OPC UA Normative References**: OPC-10000-4 §5.6 (CreateSession), §5.7/§7.38 (ActivateSession + SignatureData), §5.5 (SecureChannel/certificates), §7.22 (MonitoredItem deadband), §5.9 (Query), OPC-10000-6 §6.7 (MessageChunk/SecureChannel framing), OPC-10000-11 (HistoryUpdate), OPC-10000-13 (Aggregates)
**Target OPC UA Profile/Conformance Units**: Nano/Micro/Embedded 2017 + full-featured superset (unchanged)
**Conformance Status Target**: profile-targeting (unchanged; this feature makes the secured profiles *actually* enforce the auth they claim, but does not add a CTT-verified claim)

## Embedded Size Budget

**Flash Impact**: Small net change expected, per profile:
- Security stories (1, 2, username-nonce): ClientSignature verify + validity hook call + trust fail-close add a modest amount of `.text` in the secured profiles (Embedded/full only). Estimate < +1.5% `.text`; must stay ≤ +3% gate.
- Integer deadband (Story 5): expected **flash reduction** on Embedded/full by dropping soft-float (`__aeabi_d*`) from the integer branch.
- Refactor (Story 7 dispatch/security-policy table): expected net-neutral to slightly negative `.text` (table replaces parallel switches).
- Nano/Micro: near-zero (secured paths and subscriptions not compiled).
**RAM Impact**: **0 additional bytes intended.** ClientSignature verify uses the channel certificate (no per-session cert copy). Handshake buffers move *from stack to the existing* `secure_scratch` (RAM already allocated) — this *reduces* peak stack, not increases static RAM. Session struct gains at most a small binding field (thumbprint or channel ref) if design requires it — bounded to ≤ 32 B × `MU_MAX_SESSIONS` and only under `MUC_OPCUA_SECURITY`.
**Heap Use**: None (unchanged; core remains heap-free).
**Static Tables Added**: One `static const` SecurityPolicy parameter table (replaces ~6 switch statements — net neutral/negative). One feature-defaults header (no runtime footprint).
**Transport Buffers**: Unchanged. `secure_scratch` sizing re-partitioned, not enlarged (must keep the `_Static_assert` satisfied; may raise `MU_SECURE_SCRATCH_SIZE` only if the CreateSession/ActivateSession sub-region cannot fit within the existing 12288 B — to be confirmed in research).
**Crypto Dependency Impact**: One new optional adapter hook `verify_certificate_validity` implemented in the three backends; fail-closed when absent. No new library dependency.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS — every wire-visible change cites OPC UA sections (spec OPC-002/003). The three security fixes make behavior *more* conformant (enforce §7.38 ClientSignature, §5.5 certificate validity/trust); no StatusCode is invented — cited codes reused. Unsupported inputs still fail with the correct code.
- **Embedded C Core**: PASS — freestanding C11; the RP2040 entropy/time work lives entirely in `platform/pico/` (adapter layer), no OS/board code enters the core; the dispatch extraction moves code *within* the portable core.
- **Memory Model**: PASS — no new heap; buffers move stack→existing static arena (net RAM neutral / stack-positive). Any session binding field is bounded and static.
- **Minimal OPC UA Surface**: PASS — no new service, transport, or encoding; scope only tightens existing secured-policy acceptance and fixes latent defects.
- **Profile Research**: PASS — profiles unchanged; the feature-guard header makes the existing profile matrix *enforceable* rather than expanding it.
- **Correctness Gates**: PASS — all parsing/StatusCode/SecureChannel/session/security changes are test-first with fixtures; decoder-defect fixes get boundary tests; fuzz smoke retained.
- **Security Honesty**: PASS — this feature *removes* silent security weakening (fail-open trust, unverified signature); SecurityPolicy None behavior is explicitly preserved; the Pico example is either given real transport or documented as None-only interim (Security Honesty §V).
- **Toolchain Discipline**: PASS — CMake, host `ctest`, sanitizers, formatting, and the RP2040 cross-compile all in CI; Pico targets gain the size flags they were missing.
- **Size Discipline**: PASS — per-profile flash/RAM/stack impact estimated above; each task re-runs `measure_size.sh` and `check_stack_usage.sh`; gates enforced.

**Result: PASS (no violations). Complexity Tracking not required.**

## Project Structure

### Documentation (this feature)

```text
specs/025-audit-remediation/
├── plan.md              # This file
├── research.md          # Phase 0 — the load-bearing design decisions (8)
├── data-model.md        # Phase 1 — structs/entities touched
├── quickstart.md        # Phase 1 — how to build/test/verify the remediation
├── contracts/           # Phase 1 — new adapter hook + verification + feature-guard contracts
│   ├── crypto-adapter-validity-hook.md
│   ├── activate-session-verification.md
│   └── feature-guards.md
├── checklists/
│   └── requirements.md  # From /speckit-specify (all pass)
└── tasks.md             # Phase 2 — /speckit-tasks (NOT created here)
```

### Source Code (repository root)

```text
include/muc_opcua/
├── config.h             # collapse duplicated MU_SERVER_STORAGE_BYTES (F12); scratch sizing (F4)
├── features.h           # NEW — feature-gate defaults + illegal-combination #error (F9)
└── platform.h           # add verify_certificate_validity hook to mu_crypto_adapter (F3)
src/core/
├── service_dispatch.c   # verify ClientSignature (F2); move handshake buffers to scratch (F4);
│                        #   extract session/channel handlers + monitoring decoders (F8)
├── server.c / server_internal.h  # secure_scratch sub-region for session handlers (F4)
src/services/
├── session.c/.h         # retain/bind CreateSession cert to OPN cert (F5); username nonce (F10)
├── secure_channel.c/.h  # receive extracted OPN/channel handler logic (F8)
├── subscription.c       # integer deadband + aggregate (F7)
├── history.c            # reject negative ExtensionObject length (F11)
src/security/
├── certificate.c        # validity-period + mandatory trust (F3)
├── security_policy.c    # switch statements → one static const table (F12)
src/encoding/
├── binary_query.c       # QueryFirst operand init/reject (F11)
src/address_space/
├── node_id.c / address_space.c  # 64-node cap: build-time assert or indexed path (F6)
├── browse.c             # ref_type_is_subtype_of iteration bound (F12)
platform/pico/
├── mu_pico_adapter.c    # real ROSC entropy + timer time; TCP wire-or-restrict (F1)
├── CMakeLists.txt       # apply -Os/--gc-sections/LTO to Pico targets (F12)
tests/
├── unit/                # RED fixtures: cert validity/trust, integer deadband, decoder defects, feature #error
├── integration/         # ActivateSession signature verify + username nonce (full handshake)
└── fixtures/            # signed/unsigned ActivateSession byte fixtures, expired/untrusted certs
```

**Structure Decision**: Existing layout is kept verbatim (Constitution "Technology
and Scope Constraints"). The only new files are `include/muc_opcua/features.h`
(config surface, no footprint) and test fixtures; the Story 8 extraction moves
existing code from `service_dispatch.c` into the already-present `session.c` /
`secure_channel.c` / `subscription.c` translation units rather than creating new
architecture.

## Complexity Tracking

> No Constitution Check violations — this section intentionally left empty.
