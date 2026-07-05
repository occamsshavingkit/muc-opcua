# Implementation Plan: Backlog Critical Fixes (Round 1)

**Branch**: `038-backlog-critical-fixes` | **Date**: 2026-07-05 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/038-backlog-critical-fixes/spec.md`

## Summary

Fix 23 CRITICAL and HIGH-severity issues from the comprehensive review, grouped into seven workstreams: CI/CD pipeline integrity (7), code quality decomposition (5), architecture cleanup (2), undefined behavior fixes (2), test coverage closure (6), and version tracking (1). All changes are contained within existing modules - no new architectural patterns introduced.

## Technical Context

**Language/Version**: C11 (freestanding)
**Primary Dependencies**: CMake >= 3.16, Unity test framework
**Storage**: Static-only; no heap allocation changes
**Testing**: Unity (unit), CTest, asyncua Python client (interop)
**Target Platform**: Host (GCC/Clang) for tests; ARM Cortex-M0+ for embedded
**Project Type**: Portable C library (embedded OPC UA server)
**Constraints**: Zero `.data`/`.bss`/heap regression; all existing tests must pass
**Scale/Scope**: 23 atomic fixes across 15 source files + 8 test files + CI config

## Project Structure

```text
specs/038-backlog-critical-fixes/
├── plan.md              # This file
├── spec.md              # Feature specification
├── tasks.md             # Phase 2 output (/speckit-tasks)
└── checklists/
    └── requirements.md  # Spec quality checklist
```

## Workstreams

### WS1: CI/CD Pipeline Integrity (FR1-FR7)

**Files**: `.github/workflows/ci.yml`, `cmake/MucOpcUaStaticAnalysis.cmake`
**Risk**: Medium — changing CI behavior may surface pre-existing failures that need fixing
**Strategy**: Remove `|| true` from silent-failure steps, wire sanitizer flags, add coverage job, add `standard` profile to matrix. Each CI fix is independently testable via CI run.

### WS2: Code Quality — Function Decomposition (FR8-FR10)

**Files**: `src/core/dispatch_session.c`, `src/core/server.c`, `src/core/service_dispatch.c`
**Risk**: Medium — decomposition must preserve exact behavior; verified by existing tests
**Strategy**: Extract helper functions from long functions. Each extraction is a pure refactor (no behavior change). Run full test suite after each extraction.

### WS3: Code Quality — Stub/Session Fixes (FR11-FR12)

**Files**: `src/services/session.c`, `src/services/session.h`, `src/core/server.c`
**Risk**: Low — `mu_session_create` deprecation is purely advisory; session timeout flag is a simple rename
**Strategy**: Add `__attribute__((deprecated))` to `mu_session_create`. Replace `#ifdef MUC_OPCUA_MULTI_CHUNK` with `#ifdef MUC_OPCUA_SESSION_TIMEOUT` on session timeout logic.

### WS4: Architecture Layer Cleanup (FR13-FR14)

**Files**: `src/core/server_internal.h`, `src/core/service_context.h` (new), `CMakeLists.txt`
**Risk**: Medium — extracting service_context.h touches include dependencies
**Strategy**: Create `service_context.h` with only the fields services need from `struct mu_server`. Update service files to include it instead of `server_internal.h`. Add compile-time guard for profile contradiction.

### WS5: Undefined Behavior Fixes (FR15-FR16)

**Files**: `src/core/safe_mem.h`, `src/encoding/binary_nodeid.c`
**Risk**: Low — straightforward guard add (safe_mem) and null-check add (nodeid read)
**Strategy**: Add `if (dst_offset > dst_cap) return NULL;` to `mu_checked_memcpy_off`. Add `if (!reader->buffer) return ...` guard to `mu_binary_read_expanded_nodeid`.

### WS6: Test Coverage — New Tests (FR17-FR19)

**Files**: `tests/unit/test_transfer_subscriptions.c`, `tests/unit/test_time_sync.c`, `tests/unit/test_reverse_connect.c`, `tests/unit/test_service_message.c` (new)
**Risk**: Low — adding tests, not changing production code
**Strategy**: Replace placeholder tests with behavioral tests. Create `test_service_message.c` with parse/write round-trip tests.

### WS7: Test Coverage — CI Coverage + Fuzz + Interop (FR20-FR22)

**Files**: `.github/workflows/ci.yml`, `tests/interop/interop_smoke.py`
**Risk**: Medium — coverage and fuzz CI jobs may need Clang runtime; interop needs asyncua in CI
**Strategy**: Add coverage job (gcovr for Clang). Add fuzz CI step (60s per target). Extend interop_smoke.py with Subscribe/Publish and Call tests.

### WS8: Documentation — Version Tracking (FR23)

**Files**: `CHANGELOG.md` (new), `include/muc_opcua/version.h` (new), `CMakeLists.txt`
**Risk**: Low — pure documentation + CMake-generated header
**Strategy**: Add `CHANGELOG.md` with Unreleased section. Add `version.h.in` CMake template. Tag `v0.1.0`.

## Size Budget

**Flash Impact**: Negligible — mostly refactoring, CI changes, new tests (test code not shipped)
**RAM Impact**: Zero — no new static allocations. `service_context.h` reuses existing storage.
**Regression Risk**: Covered by existing test suite. All 121 standard tests must continue passing.

## Success Verification

1. CI run on PR shows red when cppcheck/clang-tidy finds issues (was green)
2. CI run shows sanitizer step fails when ASan/UBSan detect errors
3. `standard` profile appears in CI matrix and passes
4. Coverage report generated in CI logs
5. `handle_activate_session` line count checked: ≤3 helper functions, each <150 lines
6. Session timeout fires in micro profile build
7. `mu_checked_memcpy_off(NULL, 10, 11, NULL, 0)` returns NULL
8. `test_transfer_subscriptions` no longer only checks constants
9. `test_service_message.c` exists with ≥3 test cases
10. `interop_smoke.py` has Subscribe/Publish test
11. `version.h` exports `MUC_OPCUA_VERSION_MAJOR/MINOR/PATCH`
12. `git diff main -- src/` shows no regression in existing tests
