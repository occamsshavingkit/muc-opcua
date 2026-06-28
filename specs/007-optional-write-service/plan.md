# Implementation Plan: Optional Attribute Write Service

**Feature Branch**: `007-optional-write-service`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "Implement optional Attribute Write service for Value only with scalar values and callback"

## Technical Context

**Language/Version**: C11  
**Primary Dependencies**: None (freestanding core)  
**Storage**: N/A (application managed)  
**Testing**: Unity (host), Python/asyncua interop tests  
**Target Platform**: RP2040 (Cortex-M0+), Linux/POSIX host  
**Project Type**: library  
**Performance Goals**: Sub-millisecond packet validation and callback dispatch  
**Constraints**: Zero dynamic heap allocations, zero mutable globals  
**Scale/Scope**: < 1.5 KB flash impact  
**OPC UA Normative References**: OPC-10000-4 §5.11.4  
**Target OPC UA Profile/Conformance Units**: Write Value Server Facet (OPC-10000-7 §6.6.6)  
**Conformance Status Target**: profile-targeting  

## Embedded Size Budget

**Flash Impact**: 
- Nano profile with Write enabled: 16,909 bytes (Delta: +408 bytes vs baseline 16,501 bytes)
- Embedded profile with Write enabled: 36,018 bytes (Delta: +400 bytes vs baseline 35,618 bytes)
- Overall flash impact is well within the 1.5 KB budget.
**RAM Impact**: 0 bytes static BSS; transient stack usage under 256 bytes.
**Heap Use**: None (zero dynamic heap allocations).
**Static Tables Added**: N/A  
**Transport Buffers**: N/A  
**Crypto Dependency Impact**: N/A  

## Constitution Check

- **Spec Fidelity**: Yes. Write requests and responses match Part 4 §5.11.4 and Part 6 serialization rules exactly.
- **Embedded C Core**: Yes. Standard C11, no external OS dependencies, fully portable.
- **Memory Model**: Yes. No dynamic allocations; transient buffers parsed in-place.
- **Minimal OPC UA Surface**: Yes. Standard Write service is optional and gated behind `MICRO_OPCUA_SERVICE_WRITE`.
- **Profile Research**: Yes. Targets the Write Value Server Facet (OPC-10000-7 §6.6.6).
- **Correctness Gates**: Yes. Incorporates unit tests, boundary decoding tests, and interop testing.
- **Security Honesty**: Yes. plubgable crypto is preserved.
- **Toolchain Discipline**: Yes. Follows the CMake build toolchain.
- **Size Discipline**: Yes. Estimated code size under 1.5 KB, with zero RAM (BSS) impact.

## Project Structure

### Documentation

```text
specs/007-optional-write-service/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Design research
├── data-model.md        # Wire structures
├── quickstart.md        # Integration guide
└── tasks.md             # Implementation tasks
```

### Source Code

```text
include/
└── micro_opcua/
    └── server.h         # Register write handler callback
src/
├── core/
│   ├── service_dispatch.c  # Dispatch handle_write
│   └── service_dispatch.h  # Private handle_write declaration
└── encoding/
    ├── binary_reader.c     # Decode WriteValue entries
    └── binary_reader.h
```

**Structure Decision**: Code changes are isolated within public headers for the callback registration, decoding files for parsing WriteValues, and the core dispatch table.

## Complexity Tracking

No violations of the Constitution.
