# Implementation Plan: [FEATURE]

**Branch**: `[###-feature-name]` | **Date**: [DATE] | **Spec**: [link]
**Input**: Feature specification from `/specs/[###-feature-name]/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

[Extract from feature spec: primary requirement + technical approach from research]

## Technical Context

**Language/Version**: C11
**Primary Dependencies**: None (libc only)
**Storage**: N/A
**Testing**: ctest, Unity
**Target Platform**: Embedded ARM Cortex-M, Linux, Windows, macOS
**Project Type**: Embedded C Library
**Performance Goals**: N/A
**Constraints**: Zero-heap (no malloc/free), bounded memory, low ROM overhead
**Scale/Scope**: Up to hundreds of dynamic nodes and references on microcontrollers
**OPC UA Normative References**: OPC 10000-4, Section 5.7 (NodeManagement Service Set); Sections 5.7.2, 5.7.3, 5.7.4, 5.7.5.
**Target OPC UA Profile/Conformance Units**: Node Management Services (Add Nodes, Add References)
**Conformance Status Target**: Profile-targeting

## Embedded Size Budget

**Flash Impact**: ~2.5 kB estimated for Add/Delete Nodes and References logic and serialization routines.
**RAM Impact**: `sizeof(mu_node_t) * MU_MAX_DYNAMIC_NODES` + `sizeof(mu_reference_t) * MU_MAX_DYNAMIC_REFERENCES`. E.g., ~4.5 kB for 32 nodes and 64 references.
**Heap Use**: None (pre-allocated from server block)
**Static Tables Added**: N/A
**Transport Buffers**: No change to transport buffers required.
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
└── micro_opcua/         # Public C API
    ├── server.h         # Update with mu_server_add_node, mu_server_add_reference
    ├── config.h         # Add MU_MAX_DYNAMIC_NODES, MU_MAX_DYNAMIC_REFERENCES
src/
├── core/                # Platform-independent protocol core
│   ├── server.c         # Dynamic state initialization
│   ├── service_dispatch.c # Dispatch AddNodes, DeleteNodes, etc.
├── services/            # NodeManagement services
│   ├── node_management.c # Implement AddNodes, DeleteNodes, AddReferences
├── address_space/       # Static and dynamic address-space model
│   ├── address_space.c  # Update to search dynamic nodes as well
tests/
├── unit/
│   ├── test_node_management.c
```

**Structure Decision**: A new `node_management.c` under `src/services/` will contain the serialization and processing logic for the four NodeManagement services. `address_space.c` will be updated to merge searches across both the ROM address space array and the RAM dynamic node array.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., bounded heap in cold-path setup] | [specific protocol/profile need] | [why caller-provided storage is insufficient] |
| [e.g., additional service set] | [cited OPC-10000-7 profile requirement] | [why smaller profile/surface is insufficient] |
| [e.g., generated type table] | [specific size/correctness reason] | [why hand-maintained compact table is insufficient] |
