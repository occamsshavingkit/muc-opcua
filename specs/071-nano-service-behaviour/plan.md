<!-- markdownlint-disable MD013 -->

# Implementation Plan: Server Nano Core Service Behaviour CUs

**Branch**: `071-nano-service-behaviour` | **Date**: 2026-07-14 | **Spec**: [spec.md](./spec.md)  
**Input**: Feature specification from `specs/071-nano-service-behaviour/spec.md`
**Propagated**: 2026-07-14 — Updated from spec.md refinement for dedicated-symbol behavior gating and the approved `opc_cu_3147` deferral.  

## Summary

Promote the roadmap item 006 service-behaviour conformance units from unimplemented manifest entries into honest CU-level claims where existing or newly completed service behaviour supports them. The plan is manifest-first and behavior-gated: audit existing Discovery, View, Write, Session, and Diagnostics behaviour; add named `MUC_OPCUA_CU_*` symbols and backing tests only for satisfied behaviour; wire every claimed symbol through generated configuration into compiled or dispatched behavior; close true gaps with test-first changes; keep nano-default claims limited to CUs whose manifest defaults are true; and preserve deferred `opc_cu_3147` as explicitly unsupported and unclaimed.

## Technical Context

**Language/Version**: C11 core with C99-compatible style where practical; Python 3 for manifest generation and validation.  
**Primary Dependencies**: CMake, Unity test harness, repository profile-manifest generator/validator, OPC UA Binary encoding utilities.  
**Storage**: Static address-space tables, caller/server-owned session and diagnostics state; no persistent storage.  
**Testing**: Unit tests, integration tests, fixture round trips, profile-gating tests, sanitizer/static-analysis/fuzz CI.  
**Target Platform**: Host library tests plus embedded-oriented builds for constrained server profiles.  
**Project Type**: Embedded OPC UA C library.  
**Performance Goals**: No heap requirement on protocol hot path; preserve bounded service arrays and operation limits.  
**Constraints**: Every claimed dedicated CU symbol independently controls its compiled or dispatched behavior; optional and micro+ CUs compile out or reject only their disabled behavior when not enabled; broad aggregate aliases cannot substitute for CU gating; nano-default flash growth stays near the roadmap estimate of 0.5-2 KB.  
**Scale/Scope**: Ten service-behaviour CUs in one Core 2022 Server Facet slice.  
**OPC UA Normative References**: OPC-10000-7 section 4.2; OPC-10000-4 sections 5.5.1, 5.5.2, 5.5.4, 5.7.2, 5.7.3, 5.9.2, 5.9.3, 5.9.4, 5.11.4, 7.32, 7.38; OPC-10000-5 sections 6.3.1, 6.3.3, 8.3.2, 12.9; OPC-10000-6 section 5.2.1.  
**Target OPC UA Profile/Conformance Units**: Claim candidates `opc_cu_2317`, `opc_cu_2328`, `opc_cu_2352`, `opc_cu_2389`, `opc_cu_2400`, `opc_cu_2936`, `opc_cu_3192`, `opc_cu_3530`, and `opc_cu_3983`; intentionally unclaimed `opc_cu_3147` remains in audit scope under follow-up `S071-D1`.  
**Conformance Status Target**: Profile-targeting, not CTT-verified.

## Embedded Size Budget

**Flash Impact**: Expected nano-default growth approximately 0.5-2 KB; optional/micro+ write/session/diagnostics code must compile out when disabled.  
**RAM Impact**: No new mandatory heap; additional diagnostics/session state must remain server-owned and bounded.  
**Heap Use**: None required.  
**Static Tables Added**: Generated Kconfig/manifest metadata and optional claim-map/doc rows; no large new address-space static table expected.  
**Transport Buffers**: Existing request/response buffer ownership remains caller/server-provided; no increase without measured justification.  
**Crypto Dependency Impact**: N/A; this slice does not add security policy or cryptography backends.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS. The spec cites exact OPC-10000 sections for each service family and requires task-level section grounding for protocol-visible changes.
- **Embedded C Core**: PASS. Work stays in the existing C11 core, generated manifest artifacts, and host tests.
- **Memory Model**: PASS. Requirements prohibit mandatory heap and keep arrays/diagnostics/session state bounded.
- **Minimal OPC UA Surface**: PASS. Scope is limited to ten existing roadmap CUs; security, subscriptions, NodeManagement, Query, and History remain out of scope.
- **Profile Research**: PASS. Manifest profile defaults are treated as canonical; nano claims are limited to `profile_defaults.nano: true`.
- **Correctness Gates**: PASS. Test-first service behaviour coverage is required for Discovery, View, Write, Session, Diagnostics, malformed inputs, and disabled CUs.
- **Security Honesty**: PASS. No new cryptography claim; profile-targeting status is explicit.
- **Toolchain Discipline**: PASS. Plan includes manifest validation, generated artifact drift checks, profile tests, formatting/static analysis, and CI-equivalent checks.
- **Size Discipline**: PASS. Size impact must be measured and recorded before implementation completion.

## Project Structure

### Documentation (this feature)

```text
specs/071-nano-service-behaviour/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── class-diagram.md
├── quickstart.md
├── behavior/
├── checklists/
├── contracts/
└── roadmap-reviews/
```

### Source Code (repository root)

```text
profiles/
└── opcua-profile-manifest.yaml
scripts/profile_manifest/
├── generate.py
└── validate.py
src/cu/core_2022_server/
├── discovery/
├── view_basic_translate/
├── attribute_write/
└── diagnostics/
src/core/service_dispatch/
├── activate_session.c
├── attribute_handler.c
└── dispatch_core.c
tests/
├── unit/
├── integration/
└── fixtures/
docs/
├── build-and-gating.md
├── conformance/
└── size/
```

**Structure Decision**: Use the existing CU-aligned source tree and manifest generator. Do not introduce a parallel service layer. Dedicated symbols flow from the manifest-generated Kconfig into CMake source selection and compile definitions; shared service files may compile when any dependent CU is enabled, but dispatch or operation handling must independently reject behavior whose dedicated CU is disabled. Existing aggregate aliases may remain for compatibility but cannot be the sole gate for an implemented CU claim.

## Design Artifacts

- Internal object design: [class-diagram.md](./class-diagram.md)
- Service sequences: [contracts/sequences.md](./contracts/sequences.md)
- Behavior draft: [behavior/bdd.draft.feature](./behavior/bdd.draft.feature)
- BDD contracts: [contracts/bdd/service-behaviour.feature](./contracts/bdd/service-behaviour.feature)
- Expected UIF contracts: [contracts/uif/expected-uif.json](./contracts/uif/expected-uif.json)
- Behavior contracts: [contracts/behavior/scenario-instances.json](./contracts/behavior/scenario-instances.json)
- Data model: [data-model.md](./data-model.md)
- Interface contracts: [contracts/service-contracts.md](./contracts/service-contracts.md)
- Validation path: [quickstart.md](./quickstart.md)

## Phase 0: Research

See [research.md](./research.md). All planning unknowns are resolved without requiring user clarification.

## Phase 1: Design & Contracts

See [data-model.md](./data-model.md), [class-diagram.md](./class-diagram.md), and [contracts/](./contracts/). Behavior contracts formalize all required case types from [checklists/behavior-testability.md](./checklists/behavior-testability.md).

## Post-Design Constitution Check

PASS. The selected design remains manifest-first, bounded, test-first, and profile-honest. No constitution waiver is required.

## Complexity Tracking

- **Aggregate-to-dedicated gate migration**: Shared Discovery, View, Write, Session, and Diagnostics handlers currently use broader aggregate guards. The simpler alternative of metadata-only dedicated symbols was rejected because it violates FR-011 and Constitution Principles I/V. Per-CU tests and bounded dispatch/operation gates are required before claims.
- **Approved deferral**: `opc_cu_3147` partial IndexRange writes are not implemented. It remains unclaimed with explicit `MU_STATUS_BAD_WRITENOTSUPPORTED` behavior and follow-up `S071-D1`; no conformance waiver is inferred.
