# Feature Specification: Optimization Audit Remediation

**Feature Branch**: `004-optimization-fixes`
**Created**: 2026-06-27
**Status**: Draft
**Input**: User description: "implement fixes for the docs/review/optimization-audit-2026-06-27.md make sure that we maintain fidelity to the OPC-UA spec that is available through the OPC Foundation MCP."

## Overview

This feature remediates the findings of the consolidated optimization audit
(`docs/review/optimization-audit-2026-06-27.md`), produced by seven independent
reviewers. The findings fall into three themes — **robustness/crash-safety**,
**runtime performance**, and **flash/RAM footprint** — plus a secret-hygiene
concern. Every change MUST preserve the server's observable OPC UA behaviour
(or move it *closer* to the specification where the audit found a deviation),
verified against the OPC Foundation specifications via the OPC UA reference.
No change may introduce dynamic allocation or break the existing test suite or
conformance documentation.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A connected client cannot crash or desynchronize the server with edge-case-but-legal or malformed input (Priority: P1)

An OPC UA client (or a faulty/hostile peer on the network) sends messages that
are unusual but permitted by the specification — a non-empty
`clientSoftwareCertificates` array on ActivateSession, a non-trivial
UserIdentityToken, a NodeId of an unexpected identifier type — or that are
outright malformed. The server must reject or correctly consume them without
reading past buffer bounds, overflowing its stack, or losing framing
synchronization for subsequent fields.

**Why this priority**: Crash-safety and parser correctness on the untrusted
network path is the highest-value, lowest-regret outcome. The audit's
top-ranked finding (≈12.8 KB worst-case stack on the pre-auth secure path,
flagged by four reviewers) is a real stack-overflow risk on a Cortex-M0+ with
no guard page, and the ActivateSession decoder desync (T22) is a confirmed
spec-fidelity defect.

**Independent Test**: Add malformed/edge-case input tests (ActivateSession with
a non-empty cert array and a populated identity token; NodeId type confusion;
truncated/oversized length fields) and a worst-case stack-usage measurement on
the secured OpenSecureChannel path. Passes when the server returns the
specification-defined StatusCode (or processes the message correctly) and the
measured worst-case stack on the secure path is at or below the agreed budget.

**Acceptance Scenarios**:

1. **Given** an active session, **When** a client sends ActivateSession with a
   non-empty `clientSoftwareCertificates` array (a SignedSoftwareCertificate
   list, "Reserved for future use" per OPC-10000-4 §5.7.3.2), **Then** the
   server consumes and ignores the array, continues to decode `localeIds` and
   `userIdentityToken` from the correct offsets, and activates the session.
2. **Given** an inbound OpenSecureChannel exchange, **When** the request is
   processed end-to-end, **Then** the peak stack consumption of that call chain
   does not exceed the agreed budget and no buffer is read or written out of
   bounds.
3. **Given** a request whose leading NodeId / identity-token type id is not the
   expected ns=0 numeric form, **When** it is decoded, **Then** the server does
   not interpret the wrong union member and returns the appropriate StatusCode.
4. **Given** an OpenSecureChannel request, **When** its `securityPolicyUri` /
   `securityMode` combination is inconsistent with the server's active security
   capability, **Then** the server responds per OPC-10000-4 §5.6.2.2 rather than
   silently accepting it.

---

### User Story 2 - Sensitive cryptographic material does not linger in memory (Priority: P2)

A device is deployed in the field. After a secure channel closes (or its struct
is reused for the next connection), no derived session keys, key-derivation
intermediates, nonces, or decrypted plaintext remain readable in RAM on the
unprotected (no-MMU) device.

**Why this priority**: Security hygiene with no functional risk. Independent of
the performance and size work, and quick to land.

**Independent Test**: A test that drives a secure channel to close/reset and
asserts that the key storage and known scratch regions are zeroed; review that
each crypto helper scrubs its stack temporaries before returning.

**Acceptance Scenarios**:

1. **Given** an established secure channel, **When** the channel is closed or
   its slot is reused, **Then** the derived client/server key sets are zeroized
   before reuse.
2. **Given** any key-derivation or symmetric/asymmetric operation, **When** it
   returns, **Then** its stack-resident secret intermediates have been cleared
   with a means not removed by the optimizer.

---

### User Story 3 - The server spends fewer CPU cycles per request and per timer tick (Priority: P2)

A client performs Read/Browse/TranslateBrowsePaths against an address space, and
subscriptions sample monitored items on a recurring timer. The per-request and
per-tick cost should not grow with the product of request size and address-space
size, and per-message cryptographic setup should not repeat work that is
constant for the lifetime of a secure channel.

**Why this priority**: Direct latency/throughput improvement on the busiest
paths; matters most as integrators grow their address space and monitored-item
counts. Behaviour-preserving, so lower risk than P1 but higher value than size.

**Independent Test**: Behaviour-preserving regression (existing Read/Browse/
subscription tests stay green) plus assertions/measurements that node resolution
is sub-linear, that monitored-item sampling performs no per-tick node lookup,
and that symmetric key setup happens once per channel rather than per message.

**Acceptance Scenarios**:

1. **Given** an address space of N nodes, **When** a node is resolved by NodeId,
   **Then** the lookup cost is sub-linear (not a full O(N) scan).
2. **Given** M monitored items on a recurring sample timer, **When** the timer
   ticks, **Then** no per-item address-space lookup is performed (the resolved
   node is cached at create time).
3. **Given** a secured session exchanging many MSG chunks, **When** chunks are
   processed, **Then** the symmetric key schedule is computed once per channel,
   not once per chunk, and Read/Browse/subscription responses are byte-for-byte
   identical to the pre-change output.

---

### User Story 4 - The compiled library is smaller on flash-constrained targets (Priority: P3)

An integrator builds the Nano/Micro profile for a small MCU. The compiled core
is meaningfully smaller than the current baseline recorded in the size ledger,
with no loss of supported functionality and no new build breakage when optional
services are disabled.

**Why this priority**: Footprint headroom is valuable but the lowest-risk-to-
defer of the four; it is pure size/maintainability with no behavioural change.

**Independent Test**: Rebuild each profile, compare against
`docs/size/feature-size-ledger.md` baseline, and run the full suite for every
profile permutation (including service-disabled builds that previously risked a
`-Werror` failure).

**Acceptance Scenarios**:

1. **Given** the Micro profile, **When** it is rebuilt after the size changes,
   **Then** the core flash figure is reduced versus the recorded baseline and
   the size ledger is updated.
2. **Given** any single optional service disabled at build time, **When** the
   library is compiled with warnings-as-errors, **Then** the build succeeds
   (no unreferenced-helper failures) and the disabled service's code is absent
   from the binary.
3. **Given** any profile, **When** the full test suite runs, **Then** all tests
   pass exactly as before the size changes.

---

### Edge Cases

- **ActivateSession with reserved fields populated**: non-empty
  `clientSoftwareCertificates` and/or a populated UserIdentityToken body must be
  consumed at the correct lengths so following fields stay aligned (OPC-10000-4
  §5.7.3.2).
- **Oversized / boundary length fields**: a length read from the wire that meets
  or exceeds the remaining buffer must be rejected with the cited decoding
  StatusCode, with the bound check immune to integer overflow.
- **NodeId identifier-type confusion**: a non-numeric or non-ns0 NodeId where
  the code path assumes ns0-numeric must not be mis-read as the wrong union
  member.
- **Browse node that overflows the reference pool**: the single-pass rewrite
  must preserve the existing all-or-nothing-per-node semantics (no partially
  emitted node).
- **Disabled-service build**: every optional-service `#ifdef` permutation must
  compile cleanly under warnings-as-errors.
- **Secure-channel reuse**: keys from a prior channel must not be observable in
  the next channel's lifetime.
- **Headroom**: all changes must keep (and ideally increase) flash/RAM headroom
  available to the integrator's application on the target MCU class.

## Requirements *(mandatory)*

### Functional Requirements

**Robustness & crash-safety (US1)**

- **FR-001**: The system MUST process an inbound OpenSecureChannel exchange with
  a worst-case stack consumption at or below the agreed budget (see SC-001),
  by relocating large secure-path scratch buffers out of the call-chain stack
  into server-owned storage and/or right-sizing the asymmetric scratch buffers.
- **FR-002**: The ActivateSession decoder MUST consume the
  `clientSoftwareCertificates` array and the UserIdentityToken body at their
  encoded lengths so that all subsequent fields are decoded from correct
  offsets, even when those fields are non-empty (OPC-10000-4 §5.7.3.2).
- **FR-003**: The system MUST validate the length of every variable-length field
  (string, byte string, extension-object body, array) read from the wire
  against the actual remaining buffer using an overflow-safe comparison before
  any copy or skip, per the OPC UA Binary encoding rules (OPC-10000-6 §5.2),
  returning `Bad_DecodingError` (OPC-10000-4 §7.38.2) on a malformed/overflowing
  length.
- **FR-004**: Where a decoded NodeId is consumed as an ns=0 numeric identifier,
  the system MUST verify the identifier type (and namespace) — per the NodeId
  binary encoding (OPC-10000-6 §5.2.2.9) — before reading the numeric union
  member, returning `Bad_DecodingError` (OPC-10000-4 §7.38.2) for a malformed
  type, and for the ActivateSession UserIdentityToken type specifically
  `Bad_IdentityTokenInvalid` (OPC-10000-4 §7.38.2; ActivateSession §5.7.3).
- **FR-005**: The system MUST handle an OpenSecureChannel `securityPolicyUri` /
  `securityMode` that is inconsistent with the server's active security
  capability per OPC-10000-4 §5.6.2.2, rather than silently accepting it.
- **FR-006**: Subscription per-operation request loops MUST be bounded by an
  explicit operation cap before emitting response array lengths, consistent with
  the existing `MU_DISPATCH_MAX_*` pattern used by Read/Browse, returning
  `Bad_TooManyOperations` (OPC-10000-4 §7.38.2) when the cap is exceeded.

**Secret hygiene (US2)**

- **FR-007**: The system MUST zeroize derived client/server key sets when a
  secure channel is closed or its storage is reused.
- **FR-008**: Key-derivation and symmetric/asymmetric crypto routines MUST clear
  their stack-resident secret intermediates (key material, MACs, signatures,
  decrypted plaintext, nonces) before returning, using a zeroization that the
  compiler will not optimize away.

**Runtime performance (US3)**

- **FR-009**: Node resolution by NodeId MUST be sub-linear in the address-space
  size (e.g. an index built once at initialization), without introducing dynamic
  allocation.
- **FR-010**: Each MonitoredItem MUST cache its resolved node at creation so the
  recurring sample timer performs no per-tick address-space lookup.
- **FR-011**: Browse MUST resolve each node's references in a single pass while
  preserving the existing all-or-nothing-per-node emission semantics.
- **FR-012**: Symmetric session keys MUST be prepared once per secure channel
  and reused for every MSG chunk (no per-message key schedule recomputation).
- **FR-013**: Recurring timer arithmetic MUST avoid 64-bit division on the
  common in-time path, and inbound buffer compaction MUST avoid per-message
  whole-remainder copies under multi-message reads.
- **FR-014**: All performance changes MUST be behaviour-preserving: encoded
  responses for Read, Browse, and subscription services MUST be byte-for-byte
  identical to the pre-change output for equivalent inputs.

**Footprint (US4)**

- **FR-015**: The system MUST reduce duplicated and redundant code (consolidated
  encode/decode error handling, unified service dispatch, shared per-id array
  handlers, shared little-endian pack/unpack, table-driven element sizing)
  without behavioural change.
- **FR-016**: Diagnostic-only and unreferenced code (status-name strings, the
  stale/dead unsupported-service table) MUST be gated out of or removed from
  embedded builds.
- **FR-017**: The build MUST offer an opt-in link-time-optimization setting,
  default off for distributed archives and available for firmware-from-source.
- **FR-018**: Every optional-service build permutation MUST compile under
  warnings-as-errors with no unreferenced-helper failures, and disabled
  services MUST be absent from the resulting binary.

**Cross-cutting constraints**

- **FR-019**: No change may introduce heap allocation; the no-heap, statically
  allocated, bounded-execution design MUST be preserved.
- **FR-020**: The existing unit, integration, and fuzz test suites MUST continue
  to pass for every build profile; new tests MUST cover each behavioural change
  (FR-002, FR-004, FR-005) and the stack budget (FR-001).
- **FR-021**: Any observable behavioural change MUST be justified against, and
  cited to, the OPC Foundation specification via the OPC UA reference; where the
  audit and the specification disagree with current code, the specification
  governs.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target role/profile is unchanged by this feature — it remains the
  Nano/Micro Embedded Device Server profile targeting already in the repository
  (OPC-10000-7); this feature is remediation, not new conformance scope.
- **OPC-002**: Services/encodings touched behaviourally are ActivateSession
  (OPC-10000-4 §5.7.3.2; UserIdentityToken §5.7.3), OpenSecureChannel (OPC-10000-4
  §5.6.2.2), Browse (OPC-10000-4 §5.9.2), the subscription per-operation limit
  (`Bad_TooManyOperations`, OPC-10000-4 §7.38.2), NodeId binary decoding
  (OPC-10000-6 §5.2.2.9), and the general Binary decoding length/overflow rules
  (OPC-10000-6 §5.2 → `Bad_DecodingError`, OPC-10000-4 §7.38.2). Read,
  CreateMonitoredItems/subscription response encoding, and the binary encoders are
  otherwise touched only in a behaviour-preserving manner (byte-identical output,
  SC-003).
- **OPC-003**: Malformed or out-of-bounds encodings MUST return the cited
  decoding/encoding StatusCodes (Binary encoding rules per OPC-10000-6);
  unsupported services continue to return their existing specification-defined
  StatusCodes unchanged.
- **OPC-004**: Wire encoding/transport requirements are unchanged: OPC UA Binary
  encoding and MessageChunk/secure-conversation framing per OPC-10000-6; field
  ordering and lengths for the touched services follow OPC-10000-4 as cited.
- **OPC-005**: SecurityPolicy status is unchanged (Basic256Sha256 remains gated
  behind the existing security build option); FR-005/FR-007/FR-008 harden the
  existing implementation without altering its conformance claim.

### Scope Boundaries *(mandatory)*

- **In Scope**: Remediation of the audit findings T1–T22 grouped into the four
  user stories above; new regression/fuzz tests for the behavioural changes; an
  opt-in LTO build setting; updates to the size ledger and any affected
  conformance/traceability docs.
- **Out of Scope**: New OPC UA services or facets; new SecurityPolicies; new
  transports or encodings; raising advertised profile/conformance claims;
  ARM-specific assembly; any redesign that would require dynamic allocation.
- **Compatibility Claim**: After this feature, the server may continue to claim
  exactly its current profile/services/encoding/transport surface — the feature
  preserves the claim while improving correctness, security, performance, and
  size.
- **Application Headroom Goal**: Flash and RAM headroom available to the
  integrator's application MUST not decrease and is expected to increase
  (smaller core flash, lower worst-case stack).

### Key Entities

- **Address-space node index**: a build-/init-time ordering or lookup structure
  over the static node set enabling sub-linear NodeId resolution; statically
  sized, no heap.
- **Secure-channel cryptographic context**: per-channel prepared key state
  (schedule) reused across messages and zeroized on channel teardown/reuse.
- **MonitoredItem resolved-node reference**: a cached pointer/handle to the
  resolved node stored at create time, used by the sampling timer.
- **Server-owned secure scratch storage**: relocated buffers for the secure
  request/response path, replacing large stack arrays.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Worst-case stack consumption on the secured OpenSecureChannel path
  is reduced from the audited ~12.8 KB to ≤ 10 KB (target ~9–10 KB), verified by
  a stack-usage measurement in CI.
- **SC-002**: All specified malformed and edge-case inputs (including a
  non-empty ActivateSession `clientSoftwareCertificates` array and NodeId type
  confusion) return the cited OPC UA StatusCodes or are processed correctly,
  with zero out-of-bounds accesses under the address/UB sanitizers.
- **SC-003**: For equivalent inputs, encoded Read/Browse/subscription responses
  are byte-for-byte identical before and after the performance changes (100% of
  regression vectors match).
- **SC-004**: Address-space NodeId resolution is sub-linear, demonstrated by a
  measured lookup-cost that does not scale linearly as the node count grows, and
  the subscription sample tick performs zero per-item node lookups.
- **SC-005**: The Micro-profile core flash figure is reduced by at least 8%
  versus the current `docs/size/feature-size-ledger.md` baseline, with the
  ledger updated to the new measured figures.
- **SC-006**: The full test suite (unit, integration, fuzz) passes for every
  build profile and every single-service-disabled permutation under
  warnings-as-errors, and no build introduces heap allocation (verified by the
  existing no-heap checks).
- **SC-007**: Derived session keys and crypto scratch are provably zeroized on
  channel teardown/reuse (asserted by test).

## Assumptions

- The four user stories may be delivered and merged independently and in
  priority order (US1 → US2 → US3 → US4); each is an independently shippable
  slice that leaves the server fully functional.
- A non-empty `clientSoftwareCertificates` array is consumed and ignored (the
  field is "Reserved for future use" per OPC-10000-4 §5.7.3.2), not rejected,
  unless the OPC UA reference indicates a specific rejection StatusCode is
  required.
- "Byte-for-byte identical responses" is the chosen definition of
  behaviour-preserving for the performance work; the existing reference-client
  and asyncua interop tests remain the conformance backstop.
- The stack budget target (≤10 KB) reflects the audit's projected ~9–10 KB
  outcome from relocating scratch and right-sizing asymmetric buffers; the exact
  CI threshold will be set during planning.
- The opt-in LTO setting remains OFF by default for distributed `.a` archives to
  avoid toolchain coupling, and is intended for firmware-from-source builds.
- The OPC Foundation specification (accessed via the OPC UA reference) is the
  source of truth for any behavioural decision; current code is corrected toward
  it where they differ.
- No new external dependencies, services, or hardware assumptions are
  introduced; all work stays within the existing CMake/profile build system.
