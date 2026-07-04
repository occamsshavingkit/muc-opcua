# Feature Specification: Deferred Audit Findings — subscription.c + service_dispatch.c

**Feature Branch**: `031-deferred-audit-fixes`  
**Created**: 2026-07-04  
**Status**: Draft  
**Input**: User description: "Fix deferred five-lens audit findings in subscription.c (T4, T5/T24, T6, T27, T28, T31), service_dispatch.c (T8, T12, T13, T23, T25), and remaining style fixes"
**Parent Feature**: 029-fix-audit-findings (22 fixed, 15 deferred)
**Source**: `docs/review/five-lens-audit-2026-07-04.md`

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Subscription hot-path runtime reduction (Priority: P1)

On Cortex-M0+ (no hardware FPU), each publish cycle does three full scans of
monitored items, calls `fabs(double)` pulling in 12 KB of software double-precision
emulation, and uses 64-bit division (~300 cycles each). These combined make the
publish hot-path unnecessarily expensive for resource-constrained microcontrollers.

**Fix**: T4 (single-pass scan with bitmap), T5/T24 (remove `fabs` / `<math.h>`),
T6 (avoid 64-bit division on sample timer advance).

**Why this priority**: Largest embedded impact — ~12 KB ROM savings plus ~3x CPU
reduction per publish cycle. When combined, these are the most impactful remaining
fixes in the audit.

**Independent Test**: Publish-cycle benchmark shows reduced CPU cycles per item.
All existing subscription and publishing tests continue to pass. Profile size
checks confirm `<math.h>` no longer linked.

**Acceptance Scenarios**:

1. **Given** a subscription with monitored items, **When** `mu_subscriptions_tick` runs, **Then** the publish cycle performs a single pass collecting reportable items into a bitmap, then iterates only the bitmap for encoding and cleanup.
2. **Given** a Cortex-M0+ build, **When** the subscription module is compiled, **Then** `<math.h>` is not pulled in; `fabs()` is replaced by inline conditional negation.
3. **Given** a sample timer tick where `elapsed < interval` (common case), **When** `advance_sample_timer` runs, **Then** 64-bit division is avoided entirely.
4. **Given** a sample timer tick where `elapsed >= interval` (catch-up), **When** `advance_sample_timer` runs, **Then** a bounded iteration or 32-bit fallback handles step counting without 64-bit `__aeabi_uldivmod`.
5. **Given** 105 existing tests, **When** the test suite runs, **Then** all tests pass with no regressions.

---

### User Story 2 — Subscription correctness fixes (Priority: P1)

The deadband NONE condition always reports changes regardless of whether the value
actually changed, generating unnecessary network traffic. The publish timer advance
loop has no upper bound, risking near-infinite spin when the interval is 0 or
extremely small.

**Fix**: T27 (deadband NONE falls through to value equality check), T31 (publish
timer max-iteration guard).

**Why this priority**: T27 is a correctness bug — unnecessary publish notifications
on every sample when deadband is None violate expected monitoring behavior. T31 is
a robustness guard against an edge case that could hang the server.

**Independent Test**: Unit test verifying deadband NONE reports only on actual
value changes. Unit test confirming publish timer loop terminates within bounded
iterations even with 0-interval.

**Acceptance Scenarios**:

1. **Given** a monitored item with `deadband_type == MU_DEADBAND_TYPE_NONE`, **When** the value has not changed, **Then** `monitored_item_change_reportable` returns false.
2. **Given** a monitored item with `deadband_type == MU_DEADBAND_TYPE_NONE`, **When** the value has changed, **Then** `monitored_item_change_reportable` returns true.
3. **Given** `advance_publish_timer` with a 0-interval, **When** the loop executes, **Then** it terminates within ~100 iterations.

---

### User Story 3 — Service dispatch correctness and security (Priority: P2)

Session creation writes the response prefix after the session is committed, leaving
a dangling session slot on encode failure. Certificate user token decoding and
verification are split across separate `#ifdef` blocks, risking a build
misconfiguration where tokens are decoded but not verified. ServerNonce copies in
CreateSession/ActivateSession are left on the stack without zeroization.

**Fix**: T8 (reorder session create vs. response prefix), T12 (consolidate cert
token `#ifdef` block), T13 (zeroize nonce stack copies).

**Why this priority**: T8 is a correctness bug (dangling session); T12 is a
defense-in-depth security measure; T13 is the last nonce zeroization gap. All are
in the same file and benefit from simultaneous refactoring.

**Independent Test**: Force encode failure after session creation; verify session
slot is cleaned up. Verify cert token paths compile correctly under
`MUC_OPCUA_SECURITY` on/off. Verify valgrind reports no uninitialized or secret
bytes on nonce stack copies.

**Acceptance Scenarios**:

1. **Given** `handle_create_session`, **When** `write_response_prefix` succeeds but a subsequent encode step fails, **Then** the created session is cleaned up or creation is deferred until all encode steps succeed.
2. **Given** the cert token decode and verify code, **When** `MUC_OPCUA_SECURITY` is disabled, **Then** the cert token path is fully excluded (no decode-only path exists).
3. **Given** local `nonce_buf[32]` in `handle_create_session` and `handle_activate_session`, **When** the nonce has been copied to the session slot, **Then** the stack buffer is zeroized before function return.

---

### User Story 4 — Service dispatch performance optimizations (Priority: P2)

Every OPC UA request performs a linear scan through ~30 entries in
`g_supported_services[]` to find the dispatcher. On GetEndpoints, profile URIs are
parsed from the wire, discarded, then re-parsed for each endpoint (N+1 times).
These are low-cost but unnecessary overhead on every message.

**Fix**: T23 (direct-index dispatch table), T25 (profile URI parse once, cache
results).

**Why this priority**: P2 because these are optimization-only — no correctness or
security impact. Simple, measurable wins that compound with message volume.

**Independent Test**: Benchmark shows reduced comparisons per request dispatch.
Profile URI parse count matches number of distinct profiles, not N*endpoints.

**Acceptance Scenarios**:

1. **Given** a service request, **When** the dispatch table is consulted, **Then** the lookup is direct-index or log-N, reducing from ~15 to ~1-2 comparisons per message.
2. **Given** a GetEndpoints request with N profiles, **When** profiles are parsed, **Then** each distinct profile URI is parsed at most once.

---

### User Story 5 — Base nodes guard consolidation (Priority: P3)

`base_nodes.c` uses 35+ individual `#if`/`#endif` blocks wrapping each type-system
node definition. This is error-prone and reduces code clarity without any compile
benefit since all guards use the same feature flags.

**Fix**: T28 (consolidate into single `#ifdef` block).

**Why this priority**: Purely cosmetic — no runtime impact. Improves
maintainability for future profile-specific node work.

**Independent Test**: All profiles compile and link correctly. No change in ROM or
RAM footprint.

**Acceptance Scenarios**:

1. **Given** `base_nodes.c`, **When** compiled for any profile, **Then** the type-system node table is guarded by a single `#ifdef` block instead of 35+ individual guards.
2. **Given** the consolidated guards, **When** a profile that excludes type-system nodes is built, **Then** no linker errors occur for missing nodes.

---

### User Story 6 — Remaining MISRA style fixes (Priority: P3)

10 MISRA 15.6 violations remain in platform crypto adapters (wolfssl,
host_crypto, mbedtls) where single-statement bodies lack braces. 2 MISRA 10.4
violations in `subscription.c` compare `size_t` against `0u` (should be `0` or
`(size_t)0`).

**Fix**: Add braces to 10 single-statement bodies in platform crypto files. Fix 2
`size_t` vs `0u` type inconsistencies.

**Why this priority**: These are the last style issues from the 3,598 Codacy
findings. Low effort, closes the remaining style gap without changing behavior.

**Independent Test**: Codacy scan shows 0 MISRA 15.6 and 10.4 findings in the
affected files. All existing tests continue to pass.

**Acceptance Scenarios**:

1. **Given** `host_crypto_adapter.c`, **When** scanned, **Then** 0 MISRA 15.6 single-statement body violations.
2. **Given** `mbedtls_crypto_adapter.c`, **When** scanned, **Then** 0 MISRA 15.6 single-statement body violations.
3. **Given** `wolfssl_crypto_adapter.c`, **When** scanned, **Then** 0 MISRA 15.6 single-statement body violations.
4. **Given** `subscription.c:1576,1610`, **When** compiled with MISRA checking, **Then** no `size_t` vs `0u` type mismatch warnings.

---

### Edge Cases

- **Bitmap overflow**: T4 uses a bitmap sized to `MU_MAX_MONITORED_ITEMS`. With STANDARD profile enabled (4th pass for triggered items), the bitmap must be sized to accommodate the maximum count across all profiles.
- **Zero-interval publish**: T31 must handle the case where `publishing_interval` is 0 — the guard must bound iterations without underflowing the timer.
- **fabs removal on host**: T5 must work when `double` has native HW FPU support. The inline negation must produce identical results to `fabs()` for all edge cases (NaN, -0.0, subnormals).
- **Session cleanup ordering**: T8 must not introduce a double-free if the error path triggers before session allocation versus after.
- **Profile URI empty**: T25 must handle the case where an endpoint has no profile URIs or an empty array.
- **Dispatch table sizing**: T23 must accommodate the largest service ID (max NodeId value in `g_supported_services[]`) without over-allocating flash.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `mu_subscriptions_tick` MUST collect reportable item indices during the sampling pass and iterate only those indices for notification encoding and cleanup (T4).
- **FR-002**: The subscription module MUST NOT include `<math.h>`; `fabs(double)` MUST be replaced by inline conditional negation: `diff = (numeric > last_reported) ? numeric - last_reported : last_reported - numeric` (T5/T24).
- **FR-003**: `advance_sample_timer` MUST avoid 64-bit division via the existing `elapsed < interval` fast-path guard and a bounded catch-up path using 32-bit arithmetic or repeated subtraction (T6).
- **FR-004**: `monitored_item_change_reportable` MUST fall through to value equality comparison when `deadband_type == MU_DEADBAND_TYPE_NONE` instead of always returning true (T27).
- **FR-005**: `advance_publish_timer` MUST include an upper iteration bound to prevent near-infinite spin when interval is 0 or extremely small (T31).
- **FR-006**: `handle_create_session` MUST call `write_response_prefix` before `mu_session_create_with_identifiers`, and MUST call `mu_session_close` on the newly created session if any subsequent response-encode step fails (T8).
- **FR-007**: Certificate user token (type 327) decode and verification code MUST be consolidated under a single `#ifdef MUC_OPCUA_SECURITY` block so no decode-without-verify path exists (T12).
- **FR-008**: Local `nonce_buf[32]` stack copies in `handle_create_session` and `handle_activate_session` MUST be zeroized with `mu_secure_zero` after the nonce is copied to the session slot (T13).
- **FR-009**: Service dispatch table lookup MUST use direct indexing or range-based lookup to reduce from linear scan to O(1) or O(log N) comparisons (T23).
- **FR-010**: GetEndpoints profile URI parsing MUST parse each distinct URI at most once, caching results for endpoint comparison (T25).
- **FR-011**: Type-system node `#if`/`#endif` guards in `base_nodes.c` MUST be consolidated into a single `#ifdef` block (T28).
- **FR-012**: All single-statement `if`/`for`/`while` bodies in `host_crypto_adapter.c`, `mbedtls_crypto_adapter.c`, and `wolfssl_crypto_adapter.c` MUST use brace-enclosed compound statements per MISRA 15.6.
- **FR-013**: `size_t` comparisons in `subscription.c` at lines 1576 and 1610 MUST not compare `size_t` variables against `0u` literal; use `0` or `(size_t)0` per MISRA 10.4.
- **FR-014**: All four ARM profiles + ASAN/UBSan MUST stay green with zero test regressions.
- **FR-015**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap. T5/T24 removes `<math.h>`, which should net reduce ROM.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Subscription publish behavior cites OPC-10000-4 §5.13.1.2 (Publish Service Set — notification message generation).
- **OPC-002**: Deadband filter semantics cite OPC-10000-4 §7.22.2 (DataChangeFilter deadbandType enumeration).
- **OPC-003**: Session creation ordering cites OPC-10000-4 §5.7.3 (CreateSession request processing).
- **OPC-004**: Certificate user token verify cites OPC-10000-4 §5.7.3 (ActivateSession identity token types).
- **OPC-005**: GetEndpoints profile enumeration cites OPC-10000-4 §5.4.3.2 (GetEndpoints — transport profile URIs).
- **OPC-006**: Service dispatch (discovery/service set selection) cites OPC-10000-4 §7.1 (Service Sets).

### Scope Boundaries *(mandatory)*

- **In Scope**: T4, T5/T24, T6, T27, T31 (subscription.c); T8, T12, T13, T23, T25 (service_dispatch.c); T28 (base_nodes.c); 10 MISRA 15.6 (platform crypto adapters); 2 MISRA 10.4 (subscription.c).
- **Out of Scope**: T17 (secure_channel.c channel ID entropy — blocked on test refactoring); T22 (TypeDefinition cache — structural change to mu_node_t); T1, T2, T7, T44 (completed in 030); T3, T9-T11, T14-T16, T18-T21, T26, T29-T30, T32-T42 (completed in 029/030).
- **Compatibility Claim**: Nano/Micro/Embedded 2017 + full profiles unchanged. No new conformance claims.
- **Application Headroom Goal**: Net ROM reduction expected (~-12 KB from math.h removal). RAM change ≤ 0 bytes (bitmap uses existing MU_MAX_MONITORED_ITEMS headroom).

### Key Entities

- **Monitored item bitmap**: A bitmap sized to MU_MAX_MONITORED_ITEMS tracking which items are reportable during the sampling pass, consumed by the encoding and cleanup passes (T4).
- **g_supported_services**: Dispatch table mapping request NodeIds to handler functions. Changed from linear-scan array to direct-index or range-based structure (T23).
- **ServerNonce**: 32-byte random value stored in session slot for ActivateServerNonce verification. Stack copies are now zeroized after use (T13).
- **Deadband filter**: DataChangeFilter parameter controlling notification suppression. NONE type now correctly requires value change (T27).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Publish cycle CPU per monitored item reduced by ≥ 3x versus current triple-scan implementation.
- **SC-002**: `<math.h>` removed from subscription module link dependencies; ROM reduced by ~12 KB on ARM profiles.
- **SC-003**: 64-bit `__aeabi_uldivmod` calls eliminated from `advance_sample_timer` common path.
- **SC-004**: Deadband NONE monitors produce 0 notifications when value is unchanged.
- **SC-005**: Publish timer loop terminates within 100 iterations for any interval value.
- **SC-006**: Dangling session slot cannot occur when encode fails after session creation.
- **SC-007**: Certificate user token decode path does not compile when MUC_OPCUA_SECURITY is disabled.
- **SC-008**: Valgrind reports 0 uninitialized or secret bytes on nonce stack copies.
- **SC-009**: Service dispatch lookup ≤ 3 comparisons per request (down from ~15).
- **SC-010**: 0 remaining MISRA 15.6 and 10.4 Codacy findings in affected files.
- **SC-011**: 105/105 tests pass; all four ARM profiles + ASAN/UBSan green.
- **SC-012**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap.

## Assumptions

- The existing `MU_MAX_MONITORED_ITEMS` macro bounds the bitmap size and fits within existing RAM headroom.
- The `elapsed < interval` fast-path guard in `advance_sample_timer` handles the overwhelming majority of calls; catch-up is rare enough that repeated subtraction is acceptable.
- The inline `fabs` replacement `(numeric > last_reported) ? numeric - last_reported : last_reported - numeric` produces bit-identical results for all normal double values seen in deadband calculations.
- Dispatch table direct indexing uses the NodeId numeric identifier as a sparse array index, with a small lookup table for non-contiguous IDs.
- `mu_secure_zero` is available at the nonce copy sites in service_dispatch.c (already verified to compile under relevant profiles).
- Platform crypto adapter style fixes (braces) are mechanical changes with zero behavioral impact.
