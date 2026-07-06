# Implementation Plan: Implement Placeholder Stub Tests

**Branch**: `043-implement-stub-tests` | **Date**: 2026-07-06 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/043-implement-stub-tests/spec.md`

## Summary

Replace 5 placeholder test files with real behavioral tests, adjusted for the
actual state of the codebase discovered during Phase 0 research:
- **test_minimal_server_flow.c** (US4): Fully implementable — full server
  lifecycle integration test using existing mock transport patterns.
- **test_discovery_endpoint_no_session.c** (US5): Fully implementable —
  GetEndpoints/FindServers without session using existing patterns.
- **test_aggregate_full.c** (US1): Reduced scope — only 3 of 42 aggregate
  functions exist. Write edge-case tests for Average, Min, Max via direct
  API calls not covered by existing `test_aggregate.c`.
- **test_audit_events.c** (US2): Minimal scope — `mu_raise_audit_event` is a
  no-op with no callback mechanism. Test current behavior only (no-crash,
  NULL-safety).
- **test_complex_types.c** (US3): Deferred — encoder/decoder
  (`mu_binary_encode_struct`, `mu_binary_decode_struct`) do not exist.
  Registration API already tested by existing stub. Update documentation.

## Technical Context

**Language/Version**: C11 (freestanding)
**Primary Dependencies**: Unity test framework, project-internal API headers
**Storage**: N/A (test-only, no production code changes)
**Testing**: ctest via Unity, `cmake --build . && ctest`
**Target Platform**: Host (Linux/macOS) for tests; embedded profiles cross-compile only
**Project Type**: C library
**Performance Goals**: All tests complete in <5 seconds total
**Constraints**: Tests must gate on same `MUC_OPCUA_*` feature flags as implementation
**Scale/Scope**: 5 test files, ~15-25 test cases total
**OPC UA Normative References**: OPC-10000-13 (Aggregates), OPC-10000-5 §6.5 (Audit),
  OPC-10000-3 §5.6.4 (Complex Types), OPC-10000-6 §7.1 (TCP Transport),
  OPC-10000-4 §5.5.4 (Discovery), OPC-10000-4 §5.6 (Session Services)
**Target OPC UA Profile/Conformance Units**: Nano Server Profile (OPC-10000-7)
  with aggregate/audit/complex-type tests gated on higher-profile feature flags
**Conformance Status Target**: experimental (tests added, no CTT verification)

## Embedded Size Budget

**Flash Impact**: 0 (test-only, no production code changes)
**RAM Impact**: 0 (test-only, no production code changes)
**Heap Use**: 0 (no production code changes)
**Static Tables Added**: 0 (test-only)
**Transport Buffers**: N/A
**Crypto Dependency Impact**: N/A

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Spec Fidelity**: PASS. Every test requirement cites exact OPC UA sections
  (OPC-10000-13, OPC-10000-5 §6.5, OPC-10000-3 §5.6.4, OPC-10000-6 §7.1,
  OPC-10000-4 §5.5.4, OPC-10000-4 §5.6).
- **Embedded C Core**: PASS. Tests are host-only C11 using Unity framework.
  No OS dependencies. Production code unchanged.
- **Memory Model**: PASS. No production code changes. Tests use stack-allocated
  buffers via existing test helpers.
- **Minimal OPC UA Surface**: PASS. Tests exercise existing surface only.
- **Profile Research**: PASS. Tests gate on existing profile feature flags
  (MUC_OPCUA_SUBSCRIPTIONS_STANDARD, MUC_OPCUA_EVENTS, etc.).
- **Correctness Gates**: PASS. All new tests are test-first (tests fail against
  current stub code, pass when implemented). Wire-level tests for complex types
  use round-trip encode/decode with byte-level verification.
- **Security Honesty**: PASS. No security policy changes. Audit event tests
  verify security-relevant dispatch path without modifying crypto.
- **Toolchain Discipline**: PASS. CMake + Unity + ctest. Existing build
  infrastructure unchanged.
- **Size Discipline**: PASS. Zero flash/RAM impact (test-only change).

## Project Structure

### Documentation (this feature)

```text
specs/043-implement-stub-tests/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (N/A — no new interfaces)
└── tasks.md             # Phase 2 output
```

### Source Code (test files only)

```text
tests/
├── unit/
│   ├── test_aggregate_full.c       # STUB1 → real tests
│   ├── test_audit_events.c         # STUB2 → real tests
│   └── test_complex_types.c        # STUB5 → real tests
├── integration/
│   ├── test_minimal_server_flow.c        # STUB6 → real tests
│   └── test_discovery_endpoint_no_session.c  # STUB7 → real tests
└── support/
    └── test_helpers.h              # Existing test infrastructure (reuse)
```

**Structure Decision**: No new files or directories. All work is in-place
replacement of stub test bodies in existing test files.

## Complexity Tracking

No violations. All tests are additive with zero production code impact.
Zero flash/RAM overhead. All constitution principles satisfied.
