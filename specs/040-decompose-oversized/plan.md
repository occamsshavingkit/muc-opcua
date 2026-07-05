# Implementation Plan: Decompose Oversized Functions

**Branch**: `040-decompose-oversized` | **Date**: 2026-07-05 | **Spec**: [spec.md](./spec.md)

## Summary

Split 16 functions exceeding 100 lines into file-static helper functions.
Each function is in one of 6 source files. No behavioral changes — existing
test suite is the regression gate. Work is purely structural: extract logical
sub-concerns from monolithic functions into named `static` helpers.

## Technical Context

**Language/Version**: C11 (freestanding, C99-compatible subset)
**Primary Dependencies**: CMake, Unity/CMocka, clang-format
**Testing**: Run `ctest` after each decomposition. No new tests required.
**Target Platform**: Host (GCC/Clang) for tests; ARM Cortex-M0+ for embedded
**Project Type**: Portable C library — structural refactor only
**Performance Goals**: No performance impact (helpers are file-static, compiler inlines identically)
**Constraints**: Zero behavioral change, zero public API change, zero test changes
**Scale/Scope**: 16 functions × 6 files

## Embedded Size Budget

**Flash Impact**: Zero. File-static helpers compile to the same codegen as the
monolithic function (identical inlining decisions).
**RAM Impact**: Zero. No new data structures, allocations, or stack changes.
**Static Tables**: None.

## Constitution Check

*GATE: Must pass before Phase 0 research.*

- **Spec Fidelity**: PASS. Decomposition does not change wire behavior, StatusCodes, or service semantics. OPC UA compliance is preserved.
- **Embedded C Core**: PASS. Helpers are C11-compatible, file-static, no OS dependency.
- **Memory Model**: PASS. No heap allocations added or changed.
- **Minimal OPC UA Surface**: PASS. No new APIs, services, or transports.
- **Profile Research**: PASS. No profile changes.
- **Correctness Gates**: PASS. Existing test suite (88-124 tests depending on profile) serves as regression gate.
- **Security Honesty**: PASS. No security surface change.
- **Toolchain Discipline**: PASS. clang-format applied after each decomposition.
- **Size Discipline**: PASS. Flash/RAM unchanged.

## Project Structure

```
specs/040-decompose-oversized/
├── plan.md              # This file
├── research.md           # Phase 0 output
├── quickstart.md         # Phase 1 output
├── tasks.md              # Phase 2 output
└── checklists/
```

No new source files. All changes are within existing files:
- `src/core/dispatch_session.c` — CX1, CX2, CX13
- `src/core/service_dispatch.c` — CX3, CX8, CX16
- `src/core/server.c` — CX4, CX10, CX11, CX14
- `src/security/asym_chunk.c` — CX5, CX7
- `src/services/subscription_publish.c` — CX6, CX9, CX12
- `src/services/read.c` — CX15

## Complexity Tracking

No constitution violations.
