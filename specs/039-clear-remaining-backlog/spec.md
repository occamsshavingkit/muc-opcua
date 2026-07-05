# Feature Specification: Clear Remaining Backlog

**Feature ID**: 039-clear-remaining-backlog  
**Status**: Draft  
**Created**: 2026-07-05  
**Source**: TODO.md remaining items after Spec 038 completion

## Summary

Fixes all 43 remaining backlog items in TODO.md: 3 verified bugs, 13 hot-path
performance issues, 17 medium-severity review findings (code quality,
architecture, security, undefined behavior, test gaps, documentation gaps), 5
interop test hardening tasks, and 2 documentation tasks. This is the final
cleanup pass after Spec 038's P0 fixes.

## User Stories

### US1: Fix Verified Bugs (Priority: P0)

**As a** server operator, **I want** the 3 active bugs fixed, **so that** the
server behaves correctly per OPC UA specification for protocol versioning,
monitored item sampling intervals, and timestamp handling.

**Why this priority**: These are confirmed bugs with visible behavioral impact.

**Independent Test**: Tests can verify each fix independently -- protocol version
constant correctly emitted, sampling interval=-1 resolves to publishing interval,
timestampsToReturn is validated and applied.

**Acceptance Scenarios**:

1. **Given** a TCP connection, **When** the server sends a protocol version, **Then**
   it uses `MU_OPC_UA_TCP_PROTOCOL_VERSION` instead of hardcoded `0`.
2. **Given** a ModifyMonitoredItems request with sampling interval -1, **When**
   processed, **Then** the sampling interval resolves to the subscription's
   publishing interval, not 50ms.
3. **Given** a ModifyMonitoredItems request with timestampsToReturn specified,
   **When** processed, **Then** the timestampsToReturn is validated against known
   values and applied (not decoded and silently discarded).

---

### US2: Fix Hot-Path Performance Issues (Priority: P1)

**As a** developer integrating micro-opcua on resource-constrained hardware,
**I want** hot-path memory leaks, redundant work, and O(N) inefficiencies fixed,
**so that** the server runs reliably under sustained load without memory growth
or polling latency spikes.

**Why this priority**: HP1 (variant array leak) causes unbounded memory growth;
HP2 (read cache disabled) causes ~20% throughput penalty; HP3 (redundant
get_tick_ms) wastes CPU on every poll cycle.

**Independent Test**: Each hot-path fix is independently verifiable -- memory
leak detectable with ASAN; read cache hit rate measurable in benchmarks;
redundant calls enumerable via profiler.

**Acceptance Scenarios**:

1. **Given** a Read service call with a variant array, **When** encoding
   completes, **Then** the allocated variant buffer is freed (no ASAN leak).
2. **Given** a Read service call for a cached attribute, **When** the value
   hasn't changed, **Then** the cache returns the stored value without
   re-reading from the address space.
3. **Given** a server poll cycle, **When** multiple time-dependent operations
   execute, **Then** `get_tick_ms()` is called at most once per poll.
4. **Given** an ActivateSession request, **When** auth token extraction occurs,
   **Then** the NodeId is not re-parsed in full (HP4).
5. **Given** a MessageChunk parse, **When** MessageSize is consumed, **Then** it
   is parsed only once (HP12).

---

### US3: Fix Code Quality, Architecture, and Undefined Behavior (Priority: P2)

**As a** maintainer porting micro-opcua to new platforms, **I want** code
duplication removed, layer violations fixed, and undefined behavior remediated,
**so that** the codebase is portable, maintainable, and safe under all compilers.

**Why this priority**: UB items could produce silently wrong results on
non-GCC compilers; duplicate logic inflates maintenance burden.

**Independent Test**: Each fix independently verifiable -- CTZ has fallback path
testable on MSVC; duplicate helpers consolidated then tests still pass; strict
aliasing violations detectable with `-fstrict-aliasing`.

**Acceptance Scenarios**:

1. **Given** `__builtin_ctz()` is not available, **When** compiling on a
   non-GCC/non-Clang compiler, **Then** a portable fallback is used (CQ6).
2. **Given** LE uint32 write, **When** any encoder writes a little-endian
   uint32, **Then** a single helper function is used across all encoding files
   (CQ7).
3. **Given** message dispatch, **When** any service message is handled, **Then**
   dispatch logic is not duplicated across functions in server.c (CQ8).
4. **Given** `server_internal.h`, **When** subscription services are compiled
   out, **Then** `subscription.h` is not unconditionally included (AR3).
5. **Given** a HistoryUpdate call, **When** the operation fails, **Then**
   `handle_history_update` returns the actual error code (AR4).
6. **Given** strict aliasing rules enabled, **When** binary_string.c processes
   a string, **Then** no type-punning via incompatible pointer types occurs
   (UB3).
7. **Given** `mu_binary_write_expanded_nodeid`, **When** writing a node with
   NamespaceUri or ServerIndex, **Then** the flags are preserved in output (UB6).

---

### US4: Security Hardening (Priority: P2)

**As a** security-conscious integrator deploying an OPC UA server, **I want**
security-sensitive bugs fixed, **so that** cryptographic operations and input
validation are robust against crafted attacks.

**Why this priority**: Nonce mismatch blocks encrypted sessions; buffer undersize
could cause RSA decryption failures; non-constant-time comparison leaks timing
information; unbounded select_count enables DoS.

**Independent Test**: Each item testable independently -- nonce length test with
various policies; RSA decrypt with maximum key size; trust list comparison with
controlled timing; event filter with excessive select_count.

**Acceptance Scenarios**:

1. **Given** an OPN request with a 128-byte nonce, **When** the KDF processes
   it, **Then** the nonce is handled correctly (not truncated) (SE1).
2. **Given** a password encrypted with a 4096-bit RSA key, **When** the server
   decrypts it, **Then** the decrypt buffer is large enough (SE2).
3. **Given** a trust list comparison, **When** matching a certificate
   thumbprint, **Then** comparison is constant-time or uses a non-secret
   comparison key (SE3).
4. **Given** an EventFilter with select_count > 0, **When** the filter is
   parsed, **Then** a reasonable upper bound is enforced (SE4).

---

### US5: Test Coverage & Interop Hardening (Priority: P3)

**As a** developer validating the server against OPC UA conformance tools, **I**
**want** placeholder tests replaced, missing tests added, and interop tests
hardened, **so that** the test suite provides genuine confidence rather than
false security.

**Why this priority**: Placeholder tests claim coverage without verifying
behavior; missing interop tests leave service-set gaps unvalidated.

**Independent Test**: Each test file independently verifiable -- placeholder
replacement adds real assertions; new tests independently pass/fail; interop
smoke tests pass against the server.

**Acceptance Scenarios**:

1. **Given** 6 placeholder-only test files, **When** test suite runs, **Then**
   all have behavioral assertions or explicit `#warning "STUB"` markers (TQ7).
2. **Given** response encoding tests, **When** round-trip tests execute, **Then**
   results come from independent code paths (not circular verification) (TQ8).
3. **Given** `dispatch_subscription.c`, **When** unit tests run, **Then**
   isolated tests exist for subscription dispatch logic (TQ9).
4. **Given** a WriteResponse encoding, **When** round-trip tested against a
   binary fixture, **Then** encoding/decoding is verified correct.
5. **Given** the interop smoke test, **When** executed against the server,
   **Then** Subscribe/Publish and Call service sets are covered.

---

### US6: Documentation & Spec Grounding (Priority: P3)

**As a** developer navigating the codebase, **I want** architecture decisions
documented and spec citations present in source code, **so that** I can
understand design rationale and verify spec compliance without external research.

**Why this priority**: Documentation defects don't affect runtime behavior but
significantly impact developer velocity and spec compliance audits.

**Independent Test**: Each documentable independently -- ADRs exist as files;
spec grounding comments visible via grep from source files.

**Acceptance Scenarios**:

1. **Given** the project's design patterns, **When** a developer asks "why
   this design?", **Then** ADRs exist for zero-heap design, adapter pattern,
   profile tier system, and poll model (D4).
2. **Given** `binary_nodeid.c`, **When** reading the source, **Then** spec
   grounding comments cite the relevant OPC-10000-6 sections (D2).
3. **Given** public API headers `encoding.h` and `server.h`, **When** reading
   function prototypes, **Then** spec grounding comments cite the relevant
   OPC UA specification sections (D3).

---

### Edge Cases

- What happens when `__builtin_ctz` is called on zero? Portable fallback must
  handle zero input gracefully.
- What happens when `dst_offset + len > dst_cap` in `mu_checked_memcpy_off`?
  Already fixed in Spec 038 (UB1); only the variable naming `dst_offset` guard
  was added.
- How does the Read cache handle invalidation when the underlying value changes?
  Cache must be invalidated on any write to the same node.
- What happens when a fuzz target runs for 60 seconds on a CI runner with limited
  resources? Must not OOM or deadlock.

## Requirements

### Functional Requirements

**Bug Fixes (US1)**:

- **FR-001**: `tcp_connection.c` MUST emit `MU_OPC_UA_TCP_PROTOCOL_VERSION` instead of hardcoded `0`/`0u`.
- **FR-002**: `service_dispatch.c` ModifyMonitoredItems handler MUST resolve sampling interval `-1` to the subscription's `publishing_interval_ms`.
- **FR-003**: `service_dispatch.c` ModifyMonitoredItems handler MUST validate and apply `timestampsToReturn` from the request.

**Hot-Path Performance (US2)**:

- **FR-004**: Callers of `binary_variant.c` variant array encoding MUST free allocated buffers (HP1).
- **FR-005**: Read service cache MUST be enabled -- cache hits must return stored values (HP2).
- **FR-006**: `mu_server_poll` MUST call `get_tick_ms()` at most once per poll iteration (HP3).
- **FR-007**: Auth token extraction MUST NOT re-parse the full NodeId (HP4).
- **FR-008**: Binary reader per-primitive `ensure_bytes()` bounds checks SHOULD be batched where possible (HP5).
- **FR-009**: Binary writer per-primitive `ensure_space()` bounds checks SHOULD be batched where possible (HP6).
- **FR-010**: Read service result construction MUST NOT zero-initialize full DataValue per result (HP7).
- **FR-011**: `memmove()` of unconsumed recv buffer in server.c SHOULD be avoided when not needed (HP8).
- **FR-012**: Session timeout scan MUST NOT iterate all slots when sessions are sparse (HP9).
- **FR-013**: `dispatch_attribute.c` write type-check MUST NOT re-read current value from address space (HP10).
- **FR-014**: O(N^2) duplicate node check at address space startup MUST be reduced to O(N log N) or better (HP11).
- **FR-015**: MessageSize MUST be parsed exactly once per chunk (HP12).
- **FR-016**: `listen()` inside `mu_server_init()` MUST be deferred or made configurable (HP13).

**Code Quality & UB (US3)**:

- **FR-017**: `__builtin_ctz()` MUST have a portable fallback for non-GCC/Clang compilers (CQ6).
- **FR-018**: Duplicate LE uint32 write helpers MUST be consolidated into a single implementation (CQ7).
- **FR-019**: Duplicate message-dispatch logic in server.c MUST be extracted into shared helper(s) (CQ8).
- **FR-020**: `server_internal.h` MUST NOT unconditionally include `subscription.h` (AR3).
- **FR-021**: `handle_history_update` MUST return actual error codes instead of always GOOD (AR4).
- **FR-022**: Strict aliasing violation in `binary_string.c` MUST be eliminated (UB3).
- **FR-023**: C11 requirement MUST be documented in project README/AGENTS.md (UB4).
- **FR-024**: `int32_t` ← `uint32_t` casts MUST be guarded against impl-defined behavior (UB5).
- **FR-025**: `mu_binary_write_expanded_nodeid` MUST preserve NamespaceUri/ServerIndex flags (UB6).
- **FR-026**: `calloc` usage that pulls in heap allocator on embedded targets SHOULD be replaced or gated (UB7).

**Security Hardening (US4)**:

- **FR-027**: OPN nonce handling MUST accommodate nonces up to 128 bytes (SE1).
- **FR-028**: Password decrypt buffer MUST be sized for 4096-bit RSA keys (SE2).
- **FR-029**: Trust list certificate comparison MUST use constant-time `memcmp` or equivalent (SE3).
- **FR-030**: `read_event_filter_body` MUST enforce a reasonable upper bound on `select_count` (SE4).

**Test Coverage & Interop (US5)**:

- **FR-031**: Six placeholder-only test files MUST be replaced with behavioral tests or `#warning "STUB"` markers (TQ7).
- **FR-032**: Response encoding tests MUST NOT use circular verification (encode then decode same output) (TQ8).
- **FR-033**: `dispatch_subscription.c` MUST have isolated unit tests (TQ9).
- **FR-034**: All interop tests MUST be audited for per-service coverage.
- **FR-035**: All `*_encode` unit tests MUST verify mandatory fields in output.
- **FR-036**: A binary fixture for WriteResponse MUST be generated and round-trip tested.
- **FR-037**: All response decode tests MUST verify `reader->position == expected_length`.
- **FR-038**: Interop smoke tests MUST cover Subscribe/Publish and Call service sets.

**Documentation & Spec Grounding (US6)**:

- **FR-039**: ADRs MUST exist for zero-heap design, adapter pattern, profile tier system, and poll model (D4).
- **FR-040**: Spec grounding comments MUST be added to `binary_nodeid.c`, `binary_datavalue.c`, `uasc.c` (D2).
- **FR-041**: Spec grounding comments MUST be added to `encoding.h` and `server.h` (D3).

### OPC UA Normative Scope

- **OPC-001**: Profile targeting remains Standard 2017 UA Server Profile
  (OPC-10000-7 §6.6); no new conformance units added.
- **OPC-002**: Spec grounding comments cite OPC-10000-3 (Address Space),
  OPC-10000-4 (Services), OPC-10000-6 (Mappings) for relevant source files.
- **OPC-003**: Unsupported operations return their existing StatusCode behavior ---
  no new service endpoints introduced.
- **OPC-004**: OPC UA TCP Binary encoding (OPC-10000-6 §7) remains the sole
  transport/encoding; no WireShark/XML/JSON/HTTPS.
- **OPC-005**: Conformance status remains profile-targeting; CTT verification
  deferred to post-cleanup run.

### Scope Boundaries

- **In Scope**: All 43 remaining items from TODO.md across bugs, hot-path,
  code quality, security, undefined behavior, tests, interop hardening, and
  documentation.
- **Out of Scope**: New feature work, new conformance units, CTT verification
  runs, companion spec implementations, XML/JSON/HTTPS transports.
- **Compatibility Claim**: No change to existing profile claims. All fixes are
  behavior-preserving or restoring intended behavior; no breaking API changes.
- **Application Headroom Goal**: Flash must not regress >1% from current
  baseline. RAM unchanged (all hot-path fixes are stack/static).

### Key Entities

- **Hot-path audit items (HP1-HP13)**: Performance and correctness issues in
  the server's most frequently executed code paths.
- **Code quality items (CQ6-CQ8)**: Portability, duplication, and dispatch
  logic issues.
- **Architecture items (AR3-AR4)**: Layer violation in include structure;
  suppressed error returns.
- **Security items (SE1-SE4)**: Cryptographic boundary and input validation
  issues.
- **Undefined behavior items (UB3-UB7)**: C standard violations that may
  produce incorrect results on non-GCC compilers.
- **Test quality items (TQ7-TQ9)**: Gaps where tests misrepresent coverage.
- **Documentation items (D2-D4)**: Missing spec references and architecture
  decision records.
- **Interop hardening tasks**: Test infrastructure improvements for
  service-level validation.

## Success Criteria

### Measurable Outcomes

- **SC-001**: 3 verified bugs produce correct behavior (protocol version,
  sampling interval, timestampsToReturn) confirmed by unit tests.
- **SC-002**: ASAN runs with zero leaks on the variant array encoding test.
- **SC-003**: Read cache hit rate is >0% (cache is functional, not
  always-missing).
- **SC-004**: `get_tick_ms()` is called once per poll cycle (confirmed by
  profiler or instrumentation test).
- **SC-005**: All 43 TODO.md items are removed from the TODO.md file.
- **SC-006**: Code compiles and passes tests on at least one non-GCC compiler,
  or documented known limitations for UB3-UB7 items.
- **SC-007**: All placeholder-only tests are replaced with behavioral assertions
  or explicit stub markers.
- **SC-008**: Interop smoke tests pass with newly added Subscribe/Publish and
  Call service coverage.
- **SC-009**: All existing 121+ tests continue to pass (no regressions).
- **SC-010**: ADRs exist as `.md` files in the project root or `docs/adr/`
  directory.
- **SC-011**: Spec grounding comments are verifiable via `grep` for each
  specified source file.
- **SC-012**: Flash and RAM metrics do not regress more than 1% from current
  baseline.

## Assumptions

- All fixes are backward-compatible -- no breaking changes to existing public
  API signatures.
- The Read cache implementation already exists but is disabled; enabling it
  requires only a logic fix, not new data structures.
- Portable CTZ fallback uses a lookup table or `__builtin_ctz` compilation
  guard.
- Session timeout scanning improvement can use a simple "earliest timeout"
  tracking mechanism rather than a priority queue.
- LE uint32 write consolidation identifies the canonical implementation and
  migrates other call sites to it.
- ADRs are placed in `docs/adr/` following the existing project structure.
- Interop test expansion uses the existing Python asyncua client infrastructure
  in `tests/interop/`.
- All UB fixes preserve correct behavior on existing GCC/Clang targets while
  adding safety for other compilers.
- `calloc` replacement/removal does not break any existing allocations; all
  callers using `calloc`-returned buffers are identified and migrated to
  stack/static alternatives.
