# Implementation Plan: Split Oversized Source Files

**Branch**: `041-split-oversized-files` | **Date**: 2026-07-05 | **Spec**: [spec.md](./spec.md)

## Summary

Mechanical split: 11 monolithic `.c` files → 11 directories with 2-5 sub-files
each. No logic changes. CMake build updated. Total ~40 new `.c` files.

## Technical Context

**Language/Version**: C11 (freestanding, C99-compatible subset)
**Primary Dependencies**: CMake, clang-format
**Testing**: `ctest` after each split as regression gate
**Performance Goals**: No impact (same compilation units, same code)
**Constraints**: Zero behavioral change, zero public API change, zero test change

## Constitution Check

*GATE: Must pass before implementation.*

- **Spec Fidelity**: PASS. No wire-visible change.
- **Embedded C Core**: PASS. No platform leakage, same C11.
- **Memory Model**: PASS. No allocation changes.
- **Minimal OPC UA Surface**: PASS. No API changes.
- **Correctness Gates**: PASS. Existing tests gate regression.
- **Security Honesty**: PASS. No security surface change.
- **Toolchain Discipline**: PASS. CMake, clang-format.
- **Size Discipline**: PASS. Same code, same flash.

No violations.
