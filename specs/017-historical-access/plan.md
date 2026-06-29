# Implementation Plan: [FEATURE]

**Branch**: `[###-feature-name]` | **Date**: [DATE] | **Spec**: [link]
**Input**: Feature specification from `/specs/[###-feature-name]/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement Historical Access (HA) reading and updating via a platform-agnostic persistence adapter interface, adhering to strictly zero-heap allocations.

## Technical Context

<!--
  ACTION REQUIRED: Replace the content in this section with the technical details
  for the project. The structure here is presented in advisory capacity to guide
  the iteration process.
-->

**Language/Version**: C11
**Primary Dependencies**: None (freestanding core)
**Storage**: N/A
**Testing**: Unity Test Framework
**Target Platform**: Any embedded or host system
**Project Type**: Embedded Library
**Performance Goals**: N/A
**Constraints**: Zero-heap allocations
**Scale/Scope**: < 1000 history data points per operation
**OPC UA Normative References**: Part 4, 5.10.3 and 5.10.4; Part 11
**Target OPC UA Profile/Conformance Units**: History Server Facet
**Conformance Status Target**: profile-targeting

## Embedded Size Budget

<!--
  ACTION REQUIRED: Replace with measured or justified estimates. All feature
  plans must state how much room remains for application logic on the chosen
  microcontroller class.
-->

**Flash Impact**: ~2 KB
**RAM Impact**: ~32 bytes for adapter structure.
**Heap Use**: none
**Static Tables Added**: N/A
**Transport Buffers**: Data points copy directly to the chunk buffer without intermediate heap usage.
**Crypto Dependency Impact**: N/A

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: Every wire-visible behavior, StatusCode, service, encoding,
  and conformance claim cites exact OPC UA normative sections. Unsupported
  behavior has a specified OPC UA failure result.
- **Embedded C Core**: Plan uses freestanding C11 with C99-compatible style where
  practical; no OS dependency leaks into the core; platform services are adapters.
- **Memory Model**: Protocol hot path avoids mandatory heap allocation; buffers,
  address-space storage, and transport memory are static or caller-provided.
- **Minimal OPC UA Surface**: Scope is limited to OPC UA TCP over opc.tcp with OPC
  UA Binary unless a cited conformance requirement justifies more.
- **Profile Research**: Smallest target server profile/conformance-unit set is
  justified from OPC-10000-7 before implementation claims are made.
- **Correctness Gates**: Binary encoding/decoding, chunking, state machines,
  StatusCodes, and malformed input have test-first coverage, fixtures, and fuzz or
  property tests where applicable.
- **Security Honesty**: SecurityPolicy None, if used, is permitted by cited profile
  research or explicitly limited to a non-production interoperability phase;
  cryptography remains pluggable.
- **Toolchain Discipline**: CMake, host tests, static analysis, formatting, and at
  least one embedded cross-compile are included.
- **Size Discipline**: Flash/RAM impact, stack/buffer usage, static tables, and
  crypto cost are estimated or measured, with documented application headroom.

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
include/
└── micro_opcua/
    ├── config.h             # Add MICRO_OPCUA_SERVICE_HISTORY
    └── services/
        └── history.h        # mu_history_adapter_t, mu_historical_data_point_t
src/
├── core/
│   ├── server_internal.h    # History context/adapter instance
│   └── service_dispatch.c   # Hook HistoryRead and HistoryUpdate
└── services/
    └── history.c            # HistoryRead and HistoryUpdate parsing and dispatch
tests/
└── unit/
    └── test_history.c       # Tests with mock in-memory adapter
```

**Structure Decision**: [Document the selected structure and reference the real
directories captured above]

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., bounded heap in cold-path setup] | [specific protocol/profile need] | [why caller-provided storage is insufficient] |
| [e.g., additional service set] | [cited OPC-10000-7 profile requirement] | [why smaller profile/surface is insufficient] |
| [e.g., generated type table] | [specific size/correctness reason] | [why hand-maintained compact table is insufficient] |
