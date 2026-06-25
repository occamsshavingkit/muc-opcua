# Implementation Plan: [FEATURE]

**Branch**: `[###-feature-name]` | **Date**: [DATE] | **Spec**: [link]
**Input**: Feature specification from `/specs/[###-feature-name]/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

[Extract from feature spec: primary requirement + technical approach from research]

## Technical Context

<!--
  ACTION REQUIRED: Replace the content in this section with the technical details
  for the project. The structure here is presented in advisory capacity to guide
  the iteration process.
-->

**Language/Version**: [e.g., Python 3.11, Swift 5.9, Rust 1.75 or NEEDS CLARIFICATION]  
**Primary Dependencies**: [e.g., FastAPI, UIKit, LLVM or NEEDS CLARIFICATION]  
**Storage**: [if applicable, e.g., PostgreSQL, CoreData, files or N/A]  
**Testing**: [e.g., pytest, XCTest, cargo test or NEEDS CLARIFICATION]  
**Target Platform**: [e.g., Linux server, iOS 15+, WASM or NEEDS CLARIFICATION]
**Project Type**: [e.g., library/cli/web-service/mobile-app/compiler/desktop-app or NEEDS CLARIFICATION]  
**Performance Goals**: [domain-specific, e.g., 1000 req/s, 10k lines/sec, 60 fps or NEEDS CLARIFICATION]  
**Constraints**: [domain-specific, e.g., <200ms p95, <100MB memory, offline-capable or NEEDS CLARIFICATION]  
**Scale/Scope**: [domain-specific, e.g., 10k users, 1M LOC, 50 screens or NEEDS CLARIFICATION]
**OPC UA Normative References**: [exact OPC-10000 part/section citations required]
**Target OPC UA Profile/Conformance Units**: [profile or conformance-unit set from OPC-10000-7]
**Conformance Status Target**: [experimental/profile-targeting/profile-compliant/CTT-verified]

## Embedded Size Budget

<!--
  ACTION REQUIRED: Replace with measured or justified estimates. All feature
  plans must state how much room remains for application logic on the chosen
  microcontroller class.
-->

**Flash Impact**: [estimated bytes or NEEDS CLARIFICATION]
**RAM Impact**: [static RAM + peak stack + buffers or NEEDS CLARIFICATION]
**Heap Use**: [none/caller-provided/justified bounded heap or NEEDS CLARIFICATION]
**Static Tables Added**: [tables and approximate bytes or N/A]
**Transport Buffers**: [sizes, ownership, and OPC UA section basis]
**Crypto Dependency Impact**: [backend, code size/RAM impact, or N/A]

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
<!--
  ACTION REQUIRED: Replace the placeholder tree below with the concrete layout
  for this feature. Delete unused options and expand the chosen structure with
  real paths (e.g., apps/admin, packages/something). The delivered plan must
  not include Option labels.
-->

```text
include/
└── micro_opcua/         # Public C API
src/
├── core/                # Platform-independent protocol core
├── encoding/            # OPC UA Binary encoders/decoders
├── services/            # Discovery/SecureChannel/Session/View/Attribute services
├── address_space/       # Static address-space model
├── security/            # Pluggable crypto/security policy interfaces
├── platform/            # Adapter interfaces only; no board-specific code
└── generated/           # Reproducible compact spec/type tables if used
platform/
├── pico/                # Pico SDK adapter/example integration
└── arduino/             # Arduino/PlatformIO adapter/example integration
examples/
└── minimal_server/
tests/
├── unit/
├── fixtures/
├── fuzz/
└── integration/
docs/
├── adr/
└── traceability/
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
