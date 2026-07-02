# Implementation Plan: Conformance-Test Hardening

**Branch**: `028-conformance-test-hardening` | **Date**: 2026-07-02 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/028-conformance-test-hardening/spec.md`

## Summary

Turn the internal test suite into genuine per-profile conformance evidence (no CTT
for a long time). Five tiers: (US1/Tier0) run the suite against each profile build
and enforce a claim→test map; (US2/Tier1) fix the audited defects and reconcile
false claims; (US3/Tier2) add real-crypto security accept/reject tests; (US4/Tier3)
force the implemented-but-untested StatusCodes and negative paths; (US5/Tier4)
close encoding/transport coverage. Mostly tests + CI + doc reconciliation, with a
handful of tiny code fixes (Browse StatusCode; possibly a Micro ServerProfileArray
URI and/or `Bad_NodeIdExists` on AddNodes). Tier 0 lands first because it makes
every later test count per-profile.

**Technical approach**: The profile matrix already exists in CMake
(`MUC_OPCUA_PROFILE` presets); the only structural blocker is the "force `full`
when tests are built" rule (`CMakeLists.txt:16-20`). US1 scopes that so an explicit
`-DMUC_OPCUA_PROFILE=<p>` with tests is honored, adds `nano|micro|embedded|full`
CI jobs + `make` targets, and adds a machine-checkable claim→test map (a checked
table validated against registered/passing tests per profile) to replace markdown
substring matching. Later tiers are almost entirely new tests using the existing
harnesses (host/mbedtls/wolfssl crypto adapters, the secure-handshake and
service-builder test helpers), plus small code/doc fixes where the audit found a
real defect or over-claim.

## Technical Context

**Language/Version**: Freestanding C11 core (unchanged); CMake + ctest + Unity; CI is GitHub Actions
**Primary Dependencies**: None new. Existing: CMake, Unity, host/OpenSSL + mbedTLS + wolfSSL crypto adapters, libFuzzer
**Storage**: N/A
**Testing**: This feature IS testing — new unit/integration/fuzz tests, run per-profile; existing suite stays green
**Target Platform**: RP2040/Cortex-M0+, Arduino, host (unchanged)
**Project Type**: Portable C library with thin platform adapters (unchanged)
**Performance Goals**: N/A (tests/CI); the few code fixes are not hot-path
**Constraints**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap; all four ARM profiles + ASAN/UBSan stay green; conformance status stays profile-targeting
**Scale/Scope**: 5 tiers; ~1 CI/build change + claim→test map, ~6 code/doc reconciliations, ~25+ new/expanded tests
**OPC UA Normative References**: OPC-10000-7 (profiles/conformance units), OPC-10000-4 §5.9.2/§7.38.2 (Browse limit), §5.6/§5.7 (Session/NodeManagement), OPC-10000-5 / OPC-10000-7 Core Server Facet (Base Info), OPC-10000-6 §5.2/§6.7/§7.1 (encoding/transport)
**Target OPC UA Profile/Conformance Units**: Nano/Micro/Embedded 2017 + full (unchanged)
**Conformance Status Target**: profile-targeting (unchanged; this strengthens the evidence, adds no claim)

## Embedded Size Budget

**Flash Impact**: ≈0. Almost all work is tests/CI/docs (not linked into the library). The few code fixes are tiny: Browse returns a different constant (net 0); an optional Micro ServerProfileArray URI string (~40 B rodata, micro profile only) and/or an `Bad_NodeIdExists` branch in AddNodes (a few bytes, full/nodemanagement only) IF those claims are reconciled by implementing rather than doc-correcting.
**RAM Impact**: 0 (no new static/heap/buffers).
**Heap Use**: None (unchanged).
**Static Tables Added**: None in the library. The claim→test map is a build-time/test artifact, not runtime.
**Transport Buffers**: Unchanged.
**Crypto Dependency Impact**: None new — the real-crypto tests use the existing adapters.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS — the whole point is to bind claims to cited OPC UA sections via tests; the Browse fix returns the cited code; reconciled claims stop over-claiming. No invented StatusCodes.
- **Embedded C Core**: PASS — no OS/platform leakage; code fixes are in the portable core; CI/test-only otherwise.
- **Memory Model**: PASS — no new heap/buffers; any code fix is bounded and static.
- **Minimal OPC UA Surface**: PASS — no new service/policy/transport; corrects one StatusCode and reconciles claims.
- **Profile Research**: PASS — this feature MAKES the profile claims verifiable; profiles unchanged.
- **Correctness Gates**: PASS — this is the correctness-gate feature: test-first, per-profile, negative paths, fuzz for wire decoders; explicitly strengthens Constitution IV enforcement.
- **Security Honesty**: PASS — replaces mock-only security coverage with real-crypto accept/reject; keeps profile-targeting labeling and anti-over-claim guards.
- **Toolchain Discipline**: PASS — CMake, ctest, ASAN, four ARM profiles; adds per-profile CI, aligning with "CI MUST run ... at least one embedded cross-compile" (now per-profile).
- **Size Discipline**: PASS — impact ≈0, re-measured per profile in tasks.

**Result: PASS (no violations). Complexity Tracking not required.**

## Project Structure

### Documentation (this feature)

```text
specs/028-conformance-test-hardening/
├── plan.md              # This file
├── research.md          # Phase 0 — per-profile execution, claim→test map, real-crypto fixtures, reconcile-vs-implement decisions
├── data-model.md        # Phase 1 — the claim→test map + profile matrix entities
├── quickstart.md        # Phase 1 — how to build/run per profile and verify the gates
├── contracts/
│   ├── per-profile-build.md      # profile build/run + gate-enforcement contract
│   └── claim-to-test-map.md      # the machine-checkable claim→test mapping contract
├── checklists/requirements.md    # from /speckit-specify (all pass)
└── tasks.md             # Phase 2 — /speckit-tasks (the roadmap; NOT created here)
```

### Source Code (repository root)

```text
CMakeLists.txt            # US1: scope the "force full" rule so explicit profile+tests is honored
Makefile                  # US1: test-nano / test-micro / test-embedded / test-full targets
.github/workflows/ci.yml  # US1: per-profile build+ctest jobs
tests/
├── conformance/          # US1: claim→test map + a checker test that fails on an unmapped claimed unit
├── unit/                 # US2/US4/US5 new+expanded unit tests; CMakeLists gate review (make gates live)
├── integration/          # US3 real-crypto handshake/activation accept+reject; US4 CLO/close, capacity
├── fixtures/             # US3 expired / not-yet-valid / bad-key-size certs; US5 malformed-encoding fixtures
└── fuzz/                 # US5 ExpandedNodeId fuzz target
src/services/browse.c     # US2: Browse overflow → Bad_TooManyOperations
src/address_space/base_nodes.c   # US2: (if implementing) Micro ServerProfileArray URI
src/services/node_management.c   # US2: (if implementing) AddNodes duplicate → Bad_NodeIdExists
docs/conformance/*, docs/traceability/*   # US2/US5: reconcile any claim corrected rather than implemented
```

**Structure Decision**: Existing layout kept. New: a `tests/conformance/` home for the
claim→test map + its checker (US1). All other work extends existing test files/
harnesses and the four existing profile presets. Per-item in US2, "reconcile" may
be *implement+test* or *correct the doc* — decided in research per the size/scope
tradeoff, not forced to always add code.

## Complexity Tracking

> No Constitution Check violations — intentionally empty.
