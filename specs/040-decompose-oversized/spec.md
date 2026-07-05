# Feature Specification: Decompose Oversized Functions

**Feature ID**: 040-decompose-oversized
**Status**: Draft
**Created**: 2026-07-05
**Source**: Code complexity audit — TODO.md CX1-CX16

## Summary

Split 16 functions exceeding 100 lines into focused helper functions. No
behavioral changes — each decomposition extracts logical sub-concerns into
file-static helpers that the original function calls. Existing tests must
continue passing unchanged as the regression gate.

Scope is limited to functions >100 lines. Parameter-heavy functions, nested
logic, and duplicate code patterns are deferred to separate specs.

## Motivation

16 functions in the core protocol path exceed the 100-line maintainability
threshold identified in the complexity audit. The worst cases are
`handle_create_session` (263 lines, 6 concerns) and `handle_activate_session`
(246 lines, 8 concerns). Long functions are harder to test, harder to
understand, and invite future bloat.

## User Stories

### US1: Session Dispatch Decomposition (Priority: P1)

**As a** developer debugging session lifecycle issues,
**I want** `handle_create_session`, `handle_activate_session`, and
`read_create_session_request` broken into named, single-purpose helpers,
**so that** I can reason about each step of session creation and activation
independently, and write targeted tests for each step.

**Why this priority**: These are the two largest functions in the codebase
(263 + 246 lines) and touch critical security-sensitive session state.

**Independent Test**: Existing `test_dispatch_session_order`,
`test_dispatch_services`, `test_user_auth_*` tests continue to pass.

**Acceptance Scenarios**:

1. **Given** `handle_create_session`, **When** decomposed, **Then** each helper
   handles one concern (encode routing info, encode server endpoints, encode
   server signature, build discovery URL, etc.) and the parent function is
   under 100 lines.
2. **Given** `handle_activate_session`, **When** decomposed, **Then** helpers
   cover per-token-type activation (anonymous, username, certificate) and the
   parent is under 100 lines.
3. **Given** `read_create_session_request`, **When** decomposed, **Then**
   helpers cover session name decode, endpoint URL decode, and the parent is
   under 100 lines.

---

### US2: Service Dispatch Decomposition (Priority: P1)

**As a** developer working on OPC UA service handlers,
**I want** `handle_modify_monitored_items` (210 lines) and
`handle_open_secure_channel` (149 lines) split into helpers,
**so that** the per-filter and per-security-policy branch logic is isolated
and independently testable.

**Why this priority**: These are the core service handlers for
ModifyMonitoredItems and OpenSecureChannel — two of the most complex OPC UA
services.

**Independent Test**: `test_service_dispatch`, `test_subscriptions`,
`test_secure_channel` tests continue to pass.

**Acceptance Scenarios**:

1. **Given** `handle_modify_monitored_items`, **When** decomposed, **Then**
   filter resolution, node validation, item update, and response encoding each
   have dedicated helpers.
2. **Given** `handle_open_secure_channel`, **When** decomposed, **Then**
   security-policy selection, nonce validation, and response encoding each
   have dedicated helpers.

---

### US3: Server Core Decomposition (Priority: P2)

**As a** developer maintaining the server event loop,
**I want** `process_message` (183 lines, 5 concerns), `handle_data_chunk_secure`
(134 lines), `handle_data_chunk_plaintext` (123 lines), and `mu_server_poll`
(102 lines) split into named helpers,
**so that** message processing, chunk handling, and poll logic are
comprehensible and testable in isolation.

**Why this priority**: These functions are the server's main execution path.
Clean separation here improves all downstream debugging.

**Independent Test**: `test_server_handshake`, `test_message_chunk`,
`test_message_chunk_errors` tests continue to pass.

**Acceptance Scenarios**:

1. **Given** `process_message`, **When** decomposed, **Then** Hello, OPN/MSG,
   multi-chunk continuation, and multi-chunk final assembly each have
   dedicated helpers.
2. **Given** `handle_data_chunk_secure`, **When** decomposed, **Then** OPN
   path and MSG path have dedicated helpers.
3. **Given** `handle_data_chunk_plaintext`, **When** decomposed, **Then**
   plaintext OPN setup, sequence check, and dispatch are separated.
4. **Given** `mu_server_poll`, **When** decomposed, **Then** connection
   acceptance, read processing, and timeout maintenance have dedicated helpers.

---

### US4: Crypto / Subscription Decomposition (Priority: P3)

**As a** developer extending the crypto adapter or subscription engine,
**I want** `mu_asym_chunk_wrap` (166 lines), `mu_asym_chunk_unwrap` (182),
`write_data_change_notification` (168), `publish_due` (139), and
`build_publish_response` (117) split into helpers,
**so that** asymmetric crypto operations and subscription publishing are
comprehensible.

**Why this priority**: These functions are complex but less frequently
modified than session/service dispatch.

**Independent Test**: `test_asym_chunk`, `test_sym_chunk`, `test_subscriptions`,
`test_subscription_deadband` tests continue to pass.

**Acceptance Scenarios**:

1. **Given** `mu_asym_chunk_wrap`, **When** decomposed, **Then** policy
   dispatch, cleartext header assembly, padding, signing, and encryption
   each have dedicated helpers.
2. **Given** `mu_asym_chunk_unwrap`, **When** decomposed, **Then**
   decryption, signature verification, and payload extraction each have
   dedicated helpers.
3. **Given** `write_data_change_notification`, **When** decomposed, **Then**
   notification header, payload per-monitored-item, and diagnostics
   building are separated.
4. **Given** `publish_due`, **When** decomposed, **Then** subscription
   selection, notification building, and response encoding are separated.

---

### US5: Read and MonitoredItem Decomposition (Priority: P3)

**As a** developer extending the Read service,
**I want** `read_attribute` (102 lines) and `handle_create_monitored_items`
(98 lines, borderline) split into helpers,
**so that** attribute-type dispatch and monitored-item setup are separable
concerns.

**Why this priority**: Smallest functions in the list, close to the 100-line
threshold.

**Independent Test**: `test_read_service`, `test_subscriptions` tests
continue to pass.

**Acceptance Scenarios**:

1. **Given** `read_attribute`, **When** decomposed, **Then** attribute-type
   dispatch (Value vs. NodeClass vs. BrowseName etc.) has a dedicated helper.

---

### Edge Cases

- What happens when a helper function is called with invalid parameters at
  runtime? Helpers must propagate existing error handling — returned status
  codes must match the original monolithic function's error behavior exactly.
- What about preprocessor branches (`#ifdef`) that affect control flow in
  the original function? Helpers must respect the same feature gates or be
  conditionally compiled.
- What if a "helper" still exceeds 100 lines? Split further. Target: each
  helper ≤ 50 lines.

## Requirements

### Functional Requirements

- **FR-001**: `handle_create_session` in `src/core/dispatch_session.c` MUST
  be split into helpers under 100 lines each (CX1).
- **FR-002**: `handle_activate_session` in `src/core/dispatch_session.c`
  MUST be split into helpers under 100 lines each (CX2).
- **FR-003**: `read_create_session_request` in `src/core/dispatch_session.c`
  MUST be split into helpers under 100 lines each (CX13).
- **FR-004**: `handle_modify_monitored_items` in `src/core/service_dispatch.c`
  MUST be split into helpers under 100 lines each (CX3).
- **FR-005**: `handle_open_secure_channel` in `src/core/service_dispatch.c`
  MUST be split into helpers under 100 lines each (CX8).
- **FR-006**: `handle_create_monitored_items` in `src/core/service_dispatch.c`
  MUST be split into helpers under 100 lines each (CX16).
- **FR-007**: `process_message` in `src/core/server.c` MUST be split into
  helpers under 100 lines each (CX4).
- **FR-008**: `handle_data_chunk_secure` in `src/core/server.c` MUST be
  split into helpers under 100 lines each (CX10).
- **FR-009**: `handle_data_chunk_plaintext` in `src/core/server.c` MUST be
  split into helpers under 100 lines each (CX11).
- **FR-010**: `mu_server_poll` in `src/core/server.c` MUST be split into
  helpers under 100 lines each (CX14).
- **FR-011**: `mu_asym_chunk_wrap` in `src/security/asym_chunk.c` MUST be
  split into helpers under 100 lines each (CX7).
- **FR-012**: `mu_asym_chunk_unwrap` in `src/security/asym_chunk.c` MUST be
  split into helpers under 100 lines each (CX5).
- **FR-013**: `write_data_change_notification` in `src/services/subscription_publish.c`
  MUST be split into helpers under 100 lines each (CX6).
- **FR-014**: `publish_due` in `src/services/subscription_publish.c` MUST be
  split into helpers under 100 lines each (CX9).
- **FR-015**: `build_publish_response` in `src/services/subscription_publish.c`
  MUST be split into helpers under 100 lines each (CX12).
- **FR-016**: `read_attribute` in `src/services/read.c` MUST be split into
  helpers under 100 lines each (CX15).
- **FR-017**: All extracted helpers MUST be file-static (no new public API).
- **FR-018**: All existing tests MUST continue to pass with zero changes.

### OPC UA Normative Scope

- **OPC-001**: No normative change. OPC-10000-4 (Services), OPC-10000-6
  (Mappings) sections remain the governing references for the affected
  functions. Decomposition does not alter wire behavior or StatusCode returns.
- **OPC-002**: Conformance status unchanged: profile-targeting.

### Scope Boundaries

- **In Scope**: 16 functions across 6 source files. Each function split into
  file-static helpers. Existing tests as regression gate. No new tests
  required (existing suite covers all paths).
- **Out of Scope**: Parameter reduction (CP1-CP6), duplicate code cleanup
  (CD1-CD6), nesting reduction (CN1-CN3), new features, new tests, API
  changes, profile expansions.

### Key Entities

- **Helper function**: A `static` function extracted from a long function,
  handling one logical sub-concern. Target: ≤ 50 lines each.
- **Original function**: The function being decomposed. After decomposition:
  ≤ 100 lines, calling helpers for sub-concerns.
- **Regression gate**: Run `ctest` after each function's decomposition. All
  tests must pass before moving to the next function.

## Success Criteria

- **SC-001**: All 16 functions are ≤ 100 lines after decomposition.
- **SC-002**: Zero behavioral changes — all existing tests pass unchanged.
- **SC-003**: Compilation succeeds with zero new warnings on default,
  micro, and standard profiles.
- **SC-004**: clang-format check passes on all modified files.
- **SC-005**: No new public API symbols (all helpers are file-static).

## Assumptions

- Decomposed helpers are extracted into the same `.c` file as the original
  function as `static` functions.
- No cross-file extraction. Each function's helpers live alongside it.
- Helpers inherit the original function's preprocessor guards where needed.
- The original function's signature is unchanged — it becomes a short
  orchestrator calling helpers.
- Tests do not need modification; the helper extraction is invisible to
  test code.
