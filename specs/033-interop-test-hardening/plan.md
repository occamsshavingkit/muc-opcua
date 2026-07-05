# Implementation Plan: Interop Test Hardening

**Branch**: `033-interop-test-hardening` | **Date**: 2026-07-04 | **Spec**: [spec.md](./spec.md)

## Summary

Audit and harden all response encoder unit tests to verify complete field coverage.
Add write interop test to the smoke suite. Verify existing interop coverage for all
supported service sets. No protocol changes — tests only.

## Technical Context

**Language/Version**: C11 + Python (interop)
**Primary Dependencies**: asyncua/opcua-asyncio (Python), Unity (C tests)
**Testing**: ctest + Python interop runner
**Target Platform**: Host only (interop tests don't run on embedded)
**Scale/Scope**: ~10 encoder test files audited + 1 new interop test

## Constitution Check

- **I. Spec Fidelity**: PASS — tests validate spec compliance, no spec changes.
- **II. Embedded-First C Core**: PASS — tests only, no core changes.
- **IV. Protocol Correctness Gates**: PASS — directly strengthens the correctness gate by catching missing fields.
- **VII. Size Discipline**: PASS — no size impact (tests only).

**Result: PASS.**
