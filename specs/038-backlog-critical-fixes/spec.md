# Feature Specification: Backlog Critical Fixes (Round 1)

**Feature ID**: 038-backlog-critical-fixes  
**Status**: Draft  
**Created**: 2026-07-05  
**Source**: TODO.md comprehensive review findings (2026-07-05)

## Summary

Fix the highest-priority issues discovered in the 8-agent comprehensive review of the micro-opcua codebase. This round addresses CRITICAL and HIGH-severity findings across CI/CD pipeline integrity, code quality, architecture, security hardening, undefined behavior, test coverage gaps, and documentation.

## Motivation

The comprehensive review exposed 71 backlog items across 7 categories. This specification targets the 23 most impactful:

- **CI/CD**: Silent failures in 7 pipeline steps mean CI is providing false confidence
- **Code Quality**: 5 monolithic functions (200-375 lines) impede testing and maintenance
- **Architecture**: 2 layer-inversion issues create tight coupling between core and services
- **Undefined Behavior**: 2 buffer-safety bugs could crash the server under malicious input
- **Test Coverage**: 6 critical modules have zero or placeholder-only tests
- **Documentation**: No version tracking blocks change management and conformance tooling

## User Stories

### US1: CI/CD Pipeline Integrity (P0)
**As a** developer contributing to micro-opcua,  
**I want** CI to fail when static analysis, sanitizers, or fuzzing detect issues,  
**So that** I can trust the CI signal and catch regressions before review.

### US2: Monolithic Function Decomposition (P0)
**As a** developer maintaining the codebase,  
**I want** long functions (>200 lines) decomposed into testable helpers,  
**So that** I can understand, test, and modify service handlers without fear of breaking unrelated logic.

### US3: Architecture Layer Cleanup (P1)
**As a** developer working on the service layer,  
**I want** clean separation between core platform and service logic,  
**So that** I can compile and test services independently without circular includes.

### US4: Undefined Behavior Remediation (P0)
**As a** downstream user integrating micro-opcua,  
**I want** buffer-safety bugs fixed,  
**So that** the server is robust against crafted OPC-UA messages.

### US5: Test Coverage Gap Closure (P0)
**As a** project maintainer,  
**I want** all service handlers and hot-path code to have meaningful tests,  
**So that** I can validate correctness before release and prevent regressions.

### US6: Version Tracking Establishment (P1)
**As a** downstream consumer or conformance tool,  
**I want** semantic versioning (CHANGELOG.md, version.h, git tags),  
**So that** I can pin a specific version and track breaking changes.

## Functional Requirements

### CI/CD (7 items)

- **FR1**: Remove `|| true` from `format-check`, `cppcheck`, and `clang-tidy` CI steps (CI1)
- **FR2**: Remove `|| true` from sanitizer ctest step (CI2)
- **FR3**: Remove `|| true` from fuzz build step (CI3)
- **FR4**: Wire `MUC_OPCUA_SANITIZERS` cache variable to inject `-fsanitize=` flags (CI4)
- **FR5**: Add `standard` profile to CI test matrix (CI5)
- **FR6**: Add code coverage collection (gcovr) and reporting to CI (CI6)
- **FR7**: Set `CMAKE_BUILD_TYPE=Debug` for CI host-build and test jobs (CI7)

### Code Quality (5 items)

- **FR8**: Decompose `handle_activate_session` (375 lines) into per-token-type helpers (CQ1)
- **FR9**: Extract common logic from `mu_server_poll`'s duplicated `#ifdef` branches (CQ2)
- **FR10**: Decompose `handle_create_monitored_items` (208 lines) into filter-resolution, node-validation, item-allocation, and response-encode helpers (CQ3)
- **FR11**: Mark `mu_session_create` as deprecated or remove, documenting that `mu_session_create_with_identifiers` is the canonical path (CQ4)
- **FR12**: Gate session timeout on a dedicated flag (e.g., `MUC_OPCUA_SESSION_TIMEOUT`) instead of `MUC_OPCUA_MULTI_CHUNK` (CQ5)

### Architecture (2 items)

- **FR13**: Extract a `service_context.h` interface with only the fields services require, removing bidirectional core↔services includes from `server_internal.h` (AR1)
- **FR14**: Add compile-time error when `MUC_OPCUA_STANDARD_PROFILE` and `MUC_OPCUA_PROFILE` are set inconsistently (AR2)

### Undefined Behavior (2 items)

- **FR15**: Add `if (dst_offset > dst_cap)` guard to `mu_checked_memcpy_off` in `safe_mem.h` (UB1)
- **FR16**: Add null-buffer guard to `mu_binary_read_expanded_nodeid`'s raw buffer peek (UB2)

### Test Coverage (6 items)

- **FR17**: Add behavioral unit tests for `transfer_subscriptions` handler (count parsing, 16-item cap, response encoding) (TQ1)
- **FR18**: Replace placeholder-only tests for `time_sync` and `reverse_connect` with behavioral tests or explicit stub markers with `#warning` (TQ2)
- **FR19**: Add unit tests for `mu_parse_service_prefix` and `mu_write_service_prefix` in `service_message.c` (TQ3)
- **FR20**: Add code coverage CI job (gcovr) (TQ4)
- **FR21**: Add CI job that runs each of the 7 fuzz targets for 60 seconds (TQ5)
- **FR22**: Add interop tests for Subscribe/Publish and Call service sets (TQ6)

### Documentation (1 item)

- **FR23**: Add CHANGELOG.md, `include/muc_opcua/version.h` (CMake-generated), and initial git tag `v0.1.0` (D1)

## Success Criteria

- **SC1**: CI pipeline fails (non-green) when cppcheck, clang-tidy, or format-check finds issues
- **SC2**: CI pipeline fails when sanitizer tests detect memory errors
- **SC3**: `standard` profile appears in CI test matrix and all tests pass
- **SC4**: Code coverage report is generated and visible in CI output
- **SC5**: `handle_activate_session` is split into ≤3 functions, each < 150 lines
- **SC6**: Session timeout fires in micro profile (gated on session flag, not MULTI_CHUNK)
- **SC7**: `mu_checked_memcpy_off` with `dst_offset > dst_cap` returns NULL instead of overflowing
- **SC8**: 6 placeholder-only tests are replaced with behavioral tests or stub warnings
- **SC9**: `test_service_message.c` exists with ≥3 test cases for parse/write functions
- **SC10**: Interop smoke tests cover CreateSubscription → Publish cycle
- **SC11**: `CHANGELOG.md` exists and `version.h` exposes `MUC_OPCUA_VERSION_*` macros
- **SC12**: All existing tests continue to pass (no regressions)

## Key Entities

- **CI configuration**: `.github/workflows/ci.yml`
- **Core server loop**: `src/core/server.c`
- **Session dispatch**: `src/core/dispatch_session.c`
- **Service dispatch core**: `src/core/service_dispatch.c`
- **Architecture layer**: `src/core/server_internal.h`, `src/services/`
- **Buffer safety**: `src/core/safe_mem.h`
- **NodeId encoding**: `src/encoding/binary_nodeid.c`
- **Service message layer**: `src/core/service_message.c`
- **Version tracking**: `CHANGELOG.md` (new), `include/muc_opcua/version.h` (new)
- **Test files**: 6 placeholder tests to replace, 2 new test files needed

## Assumptions

- The project uses Semantic Versioning 2.0.0 for version.h
- Decomposing functions will not change external behavior — verified by existing tests
- CI runner has Clang available for sanitizer and fuzz builds
- gcovr chosen for coverage (Clang + GCC compatible)
- asyncua Python client is available in CI for interop test expansion
- `mu_session_create` deprecation is non-breaking (internal API only)

## Out of Scope

- MEDIUM and LOW severity items from the comprehensive review (deferred to Round 2)
- Original hot-path audit items (HP1-HP13) — deferred to dedicated performance round
- Deferred audit findings (T006, ModifyMonitoredItems bugs) — already in backlog for Round 2
- Interop hardening binary fixtures and audit — already in backlog for Round 2
- Security MEDIUM items (SE1-SE6) — deferred to dedicated security hardening round
