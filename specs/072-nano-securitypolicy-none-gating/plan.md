<!-- markdownlint-disable MD013 -->

# Implementation Plan: Nano SecurityPolicy-None Crypto Gating

**Branch**: `072-nano-securitypolicy-none-gating` | **Date**: 2026-07-15 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/072-nano-securitypolicy-none-gating/spec.md`

## Summary

Introduce a dedicated `MUC_OPCUA_SECURE_CHANNEL_CRYPTO` build gate that separates secure-channel message cryptography (symmetric/asymmetric chunk crypto, certificate handling, trustlist) from the `MUC_OPCUA_FACET_CORE_2022_SERVER` service umbrella, which today does double duty as both the service-enabling facet and the compile-time crypto guard. The gate defaults **off for nano only** (the sole SecurityPolicy-None-only profile) and **on for micro/embedded/standard/full** (security-capable profiles), driven by per-profile Kconfig defaults — FR-005 resolved to *profile intent, not backend presence*. When off, the heavy crypto translation units are excluded from compilation and no linked call site references their symbols; the server retains full SecurityPolicy-None behaviour and still rejects any non-None SecurityPolicy with an explicit OPC UA StatusCode. Secured profiles are byte-identical to their pre-change baseline. Prototype (isolated worktree, since removed) validated ~5.8–6.0 KB nano ARM `.text` recovery with all SecurityPolicy-None and secured tests passing.

## Technical Context

**Language/Version**: C11 core with C99-compatible style where practical; Python 3 for manifest generation and validation.
**Primary Dependencies**: CMake, Kconfig-based profile gating, Unity test harness, repository profile-manifest generator/validator, mbedTLS/wolfSSL crypto backends (secured profiles only).
**Storage**: Static/caller-owned secure-channel and session state; no persistent storage introduced.
**Testing**: Unit + integration tests (SecurityPolicy-None flow with crypto gated off; full crypto handshake/chunk/user-auth suite with the gate on), profile-gating tests, size measurement, sanitizer/static-analysis CI.
**Target Platform**: Host library tests plus ARM Cortex-M0+ size measurement for the nano profile.
**Project Type**: Embedded OPC UA C library.
**Performance Goals**: No change to the protocol hot path; pure subtraction of dead code for nano; zero delta for secured profiles.
**Constraints**: The gate MUST decouple crypto from the facet umbrella; when off it MUST exclude `sym_chunk.c`, `asym_chunk/wrap.c`, `asym_chunk/unwrap.c`, `certificate.c`, `trustlist.c` and reference none of their symbols; SecurityPolicy-None behaviour and explicit non-None rejection MUST be preserved; secured profiles MUST be byte-identical in ARM `.text`; small always-needed utilities (`mu_secure_zero`) MUST remain available to no-crypto builds.
**Scale/Scope**: One build-gating refactor across `src/CMakeLists.txt`, `/Kconfig`, the profile manifest, ~6 crypto call-site files, and the `mu_secure_zero` relocation. No new protocol, service, or crypto algorithm.
**OPC UA Normative References**: OPC-10000-6 §6.7.2 (MessageChunk / SecureChannel framing), §7.2 (OPC UA TCP), §6.7 (SecurityPolicy None plaintext UA Secure Conversation); OPC-10000-4 §5.5 (Discovery), §5.6 (SecureChannel), §5.7 (Session), §5.11.2 (Read), §5.9 (View); OPC-10000-7 §4.3 (NanoEmbeddedDevice profile — SecurityPolicy None, Anonymous).
**Target OPC UA Profile/Conformance Units**: Nano `NanoEmbeddedDevice` (SecurityPolicy None only); message-crypto CUs (`opc_cu_3080` and Sign/Encrypt SecurityPolicy CUs) remain unclaimed for nano and consistent with the gated build. Secured profiles keep their existing security CUs.
**Conformance Status Target**: Profile-targeting, not CTT-verified.

## Embedded Size Budget

**Flash Impact**: Nano ARM `.text` **−5.8 to −6.0 KB** (validated prototype: archive 29,442→23,660; linked ELF 27,220→21,236). Secured profiles: **0 bytes** (byte-identical). Relocating `mu_secure_zero` out of `key_derivation.c` recovers a few hundred additional bytes on nano.
**RAM Impact**: None. No BSS change on nano (SC-001).
**Heap Use**: None introduced.
**Static Tables Added**: None. One new Kconfig symbol + per-profile defaults; generated artifact rows.
**Transport Buffers**: Unchanged.
**Crypto Dependency Impact**: This is the point — nano stops linking the crypto stack. Secured profiles keep mbedTLS/wolfSSL exactly as before. No new backend or algorithm.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity (I)**: PASS. Nano's SecurityPolicy-None-only scope is grounded in OPC-10000-7 §4.3; non-None requests still fail with an explicit OPC UA StatusCode (no silent/partial behaviour). Every gated call site keeps its section citation.
- **Embedded C Core (II)**: PASS. Change is confined to CMake source selection, Kconfig, and C compile-time guards; no platform leakage into protocol code. `mu_secure_zero` moves to an always-compiled utility TU.
- **Memory Model (III)**: PASS. No hot-path or memory-model change; pure code subtraction for nano.
- **Minimal OPC UA Surface (III)**: PASS. Narrows the nano surface to exactly what SecurityPolicy None needs; adds nothing.
- **Profile Research (I)**: PASS. Nano = SecurityPolicy None only per profile definition; FR-005 keeps the gate tied to profile intent, not toolchain.
- **Correctness Gates (IV)**: PASS. Test-first: SecurityPolicy-None flow (OPN None, session, read, browse) proven with crypto off; secured crypto suite proven unchanged with crypto on; profile-gating asserts the gate resolves off for nano and on for secured.
- **Security Honesty (V)**: **PASS — the load-bearing gate.** Crypto is removed only where it is provably dead (nano: no backend, `crypto_adapter == NULL`, SecurityPolicy None only). Secured profiles are byte-identical. Non-None SecurityPolicy is still rejected explicitly — security is NOT weakened silently for size. The manifest is updated so nano's advertised security posture matches the gated build (conformance honesty).
- **Toolchain Discipline (VI)**: PASS. CMake + Kconfig gating; generated-artifact drift checks; profile tests; size ledger; format/static-analysis in CI.
- **Size Discipline (VII)**: PASS. Size impact is measured (SC-001) and recorded in the ledger; the reduction removes only dead code — no status handling, bounds checks, traceability, or tests are removed.

## Project Structure

### Documentation (this feature)

```text
specs/072-nano-securitypolicy-none-gating/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
└── contracts/
    └── crypto-gate-contracts.md
```

### Source Code (repository root)

```text
Kconfig                                  # new MUC_OPCUA_SECURE_CHANNEL_CRYPTO symbol + per-profile defaults
profiles/opcua-profile-manifest.yaml     # gate symbol + nano security-posture reconciliation
scripts/profile_manifest/{generate,validate}.py
src/CMakeLists.txt                       # exclude crypto TUs when the gate is off; relocate mu_secure_zero
src/cu/core_2022_server/security/
├── sym_chunk.c                          # excluded when gate off
├── asym_chunk/{wrap,unwrap}.c           # excluded when gate off
├── certificate.c                        # excluded when gate off
├── trustlist.c                          # excluded when gate off
└── key_derivation.c                     # mu_secure_zero split out to an always-compiled utility
src/…/{data_chunk,process_message,osc_handler,secure_channel}.c   # crypto call-site guards → new gate
src/core/service_dispatch/{create_session,activate_session}.c     # crypto call-site guards → new gate
tests/{unit,integration}/                # None-path + secured-path + gating coverage
scripts/test_profile_gating.sh           # assert gate off@nano / on@secured
docs/{build-and-gating.md,size/feature-size-ledger.md,conformance/}
```

**Structure Decision**: Keep the existing CU-aligned tree and manifest-generated Kconfig→CMake flow. Add exactly one new gate symbol; do not introduce a parallel crypto abstraction. Crypto TUs are excluded at the CMake source-selection layer when the gate is off; each crypto call site switches its guard from `MUC_OPCUA_FACET_CORE_2022_SERVER` to `MUC_OPCUA_SECURE_CHANNEL_CRYPTO`, preserving its existing SecurityPolicy-None fallback. `mu_secure_zero` relocates to an always-compiled utility unit so no-crypto builds keep it without pulling the crypto stack back in. The facet umbrella continues to enable nano's mandatory services; only its crypto-guard duty is removed.

## Design Artifacts

- Research (coupling map + decisions): [research.md](./research.md)
- Data model (gate symbol, TU membership, call-site map): [data-model.md](./data-model.md)
- Behaviour/interface contracts: [contracts/crypto-gate-contracts.md](./contracts/crypto-gate-contracts.md)
- Validation path (build, measure, run None e2e, run secured suite): [quickstart.md](./quickstart.md)

## Phase 0: Research

See [research.md](./research.md). The coupling map (crypto TUs, call-site guards, `mu_secure_zero` callers, non-None rejection path) is verified against the current tree; FR-005 is resolved (profile intent). No open clarifications remain.

## Phase 1: Design & Contracts

See [data-model.md](./data-model.md) and [contracts/crypto-gate-contracts.md](./contracts/crypto-gate-contracts.md). Contracts formalize: (C1) nano build excludes the crypto TUs and defines no `MUC_OPCUA_SECURE_CHANNEL_CRYPTO`; (C2) SecurityPolicy-None connect→OPN→session→read→browse succeeds with crypto off; (C3) non-None SecurityPolicy is rejected with an explicit StatusCode and no partial channel; (C4) secured builds define the gate, compile all crypto TUs, pass the full crypto suite, and are byte-identical in ARM `.text`.

## Post-Design Constitution Check

PASS (pending Phase 1 completion). The design removes only provably-dead code, preserves explicit non-None rejection, keeps secured profiles byte-identical, and reconciles the manifest so the advertised posture matches the build. No constitution waiver required.

## Complexity Tracking

- **Facet macro double-duty split**: `MUC_OPCUA_FACET_CORE_2022_SERVER` currently gates both nano's mandatory services and the crypto stack. The simpler alternative — leave the crypto linked and rely on runtime `crypto_adapter == NULL` — was rejected because it violates Principle VII (ships ~6 KB of dead flash on the smallest profile) and Principle V (advertised posture diverges from the build). Splitting the crypto guard into a dedicated gate is the minimal correct change.
- **`mu_secure_zero` relocation**: Moving a small utility out of a crypto TU risks a subtle build break if a no-crypto caller is missed. Mitigation: enumerate all callers in research.md and relocate to an always-compiled utility unit, verified by a nano build + link with the gate off.
