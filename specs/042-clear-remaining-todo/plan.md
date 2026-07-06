# Implementation Plan: Clear Remaining TODO

**Branch**: `042-clear-remaining-todo` | **Date**: 2026-07-05

## Summary

Fix 23 remaining TODO items: 9 CodeRabbit findings, 6 parameter reductions,
5 duplicate code consolidations, 3 nesting improvements. No behavioral changes.

## Technical Context

**Language/Version**: C11 (freestanding)
**Testing**: ctest on default, micro, standard profiles
**Constraints**: Zero behavioral changes, zero public API changes where avoidable

## Constitution Check

- **Spec Fidelity**: PASS. No wire-visible changes.
- **Embedded C Core**: PASS. C11 compatible changes only.
- **Memory Model**: PASS. No allocation changes.
- **Minimal OPC UA Surface**: PASS. No new APIs.
- **Correctness Gates**: PASS. Existing tests as regression gate.
- **Security Honesty**: PASS. No security surface change.
- **Toolchain Discipline**: PASS. clang-format, cppcheck required.
- **Size Discipline**: PASS. Context structs for parameters may add minimal struct overhead but eliminate parameter-passing code.

No violations.
