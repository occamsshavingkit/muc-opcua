# Implementation Plan: Memory and Performance Optimizations

**Branch**: `006-implement-memory-and` | **Date**: 2026-06-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/006-implement-memory-and/spec.md`

## Summary

This feature implements the key memory (RAM/SRAM) and performance improvements discovered during the codebase optimization audit. This includes refactoring the session timeout representation from an 8-byte double to a 4-byte millisecond integer, and eliminating stack allocations and memcpy operations in the asymmetric unwrap code by utilizing caller-provided scratch memory.

## Technical Context

**Language/Version**: C11  
**Primary Dependencies**: None (freestanding)  
**Storage**: N/A  
**Testing**: ctest (unit and integration tests)  
**Target Platform**: ARM Cortex-M0+ (RP2040) / POSIX host  
**Project Type**: library  
**Performance Goals**: Sub-microsecond execution time improvements in hot-paths  
**Constraints**: Zero dynamic heap allocations, zero mutable globals  
**Scale/Scope**: Reclaims ~800 bytes of static RAM and ~6 KB of peak stack usage  
**OPC UA Normative References**: OPC-10000-4 §5.6.2.2, OPC-10000-6 §6.7.2  
**Target OPC UA Profile/Conformance Units**: Standard DataChange Subscription 2017 Server Facet  
**Conformance Status Target**: Profile-targeting

## Embedded Size Budget

**Flash Impact**: -50 to +100 bytes (minor changes in logic)  
**RAM Impact**: Reclaims **800 bytes of static BSS RAM** (via session timeout packing) and **6 KB of stack space** (via unwrap stack reduction).  
**Heap Use**: None  
**Static Tables Added**: N/A  
**Transport Buffers**: N/A  
**Crypto Dependency Impact**: Integrator can now build default pointer-based crypto adaptors with `MU_CIPHER_CTX_SIZE = 32` bytes instead of 512, saving **960 bytes of RAM**.  

## Constitution Check

- **Spec Fidelity**: Yes. Timeout clamping and asymmetric chunk unwrapping follow the exact specifications in OPC-10000-4 and OPC-10000-6.
- **Embedded C Core**: Yes. Written in standard C11 for microcontrollers.
- **Memory Model**: Yes. No heap allocations; uses caller-provided or static buffers.
- **Size Discipline**: Yes. All size reductions are measured and verified against cross-compilation target sizes.

## Project Structure

### Documentation (this feature)

```text
specs/006-implement-memory-and/
├── plan.md              # This file
├── spec.md              # Feature specification
└── tasks.md             # Implementation tasks
```

### Source Code (repository root)

```text
include/
└── micro_opcua/         # Public C API
src/
├── core/                # Platform-independent protocol core (server.h, server_internal.h)
├── encoding/            # OPC UA Binary encoders/decoders
├── services/            # session.c, session.h, secure_channel.c, secure_channel.h
├── address_space/       # Static address-space model
└── security/            # asym_chunk.c, asym_chunk.h
```

**Structure Decision**: Code changes are localized within `src/services/session.*`, `src/security/asym_chunk.*`, and `src/core/server.*`.

## Complexity Tracking

No violations of the Constitution.
