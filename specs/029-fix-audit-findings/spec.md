# Feature Specification: Five-Lens Audit Findings Cleanup

**Feature Branch**: `029-fix-audit-findings`
**Created**: 2026-07-04
**Status**: Draft
**Input**: User description: "clean up all the issues found in @docs/review/five-lens-audit-2026-07-04.md"
**Audit Source**: `docs/review/five-lens-audit-2026-07-04.md` (42 findings across security, memory, correctness, performance)

## User Scenarios & Testing

### User Story 1 — Critical security fixes (Priority: P1)

A deployer of muc-opcua on a real network must not have the ActivateSession replay
protection silently degraded when the platform entropy source is unavailable, and
decrypted user passwords must not remain on the stack for later recovery.

**Why this priority**: Exploitable security gaps — zero-nonce enables replay of
ActivateSession tokens; uncleared password buffer is the only zeroization gap in
the entire security layer.

**Independent Test**: Drive ActivateSession through the secure handshake path with a
mock crypto adapter whose entropy hook returns failure; assert the session is
rejected with a security checks failed StatusCode. Run the password-decrypt path
with valgrind-or-stack-canary instrumentation and confirm no plaintext password
bytes survive past the function return.

**Acceptance Scenarios**:

1. **Given** a secured channel with entropy adapter returning failure, **When** ActivateSession is requested, **Then** the server rejects the activation with `MU_STATUS_BAD_SECURITYCHECKSFAILED` (T1).
2. **Given** a secured channel with working entropy, **When** ActivateSession is requested, **Then** the server generates a non-zero ServerNonce and completes activation (no regression).
3. **Given** an ActivateSession with an encrypted username-token password, **When** the decrypted password has been verified and the request is complete, **Then** no plaintext password bytes remain in the stack frame (T2).

---

### User Story 2 — OPC UA conformance correctness fixes (Priority: P1)

A client browsing a node with more references than `requested_max_references_per_node`,
deleting a node with `deleteTargetReferences=false`, reading the Value attribute of
a node with a non-standard built-in type, or using deadband-type None must receive
the correct OPC UA StatusCode and wire content per specification.

**Why this priority**: Silent data loss (T3 Browse drops collected refs), spec
non-compliance (T9 ignores deleteTargetReferences, T11 rejects valid types, T27
spams notifications, T29 accepts invalid encoding mask), dangling state (T8 session
leak on encode failure), and type-validation bypass (T10 write may proceed on read
failure).

**Independent Test**: Each fix has a corresponding RED test that passes after the
fix. The full ctest suite plus the new tests stays green across all four ARM
profiles.

**Acceptance Scenarios**:

1. **Given** a node with more references than `requested_max_references_per_node`, **When** Browse is requested, **Then** the response returns the number of references that fit plus `BAD_NOCONTINUATIONPOINTS`, not zero references for that node (T3).
2. **Given** a DeleteNodes request with `deleteTargetReferences=false`, **When** the node is deleted, **Then** only the node is removed; references from other nodes pointing to it remain intact (T9).
3. **Given** a session-creation that proceeds past the ID allocation but fails on response encoding, **When** the error is returned, **Then** no dangling session remains consuming a session slot (T8).
4. **Given** a value-source-read failure during Write processing, **When** the type check is reached, **Then** the write is aborted (not allowed to proceed with an unknown type) (T10).
5. **Given** a static variable with a built-in type (Byte, Double, UInt16, Int64, etc.), **When** its Value attribute is read, **Then** the correct value is returned (not `BAD_NOTREADABLE`) (T11).
6. **Given** a monitored item with deadband-type None, **When** the value has not changed, **Then** NO data-change notification is generated (T27).
7. **Given** a HistoryRead with ExtensionObject encoding mask 0x03, **When** the decoder processes the mask, **Then** `BAD_NOTSUPPORTED` is returned (T29).

---

### User Story 3 — Subscription hot-path optimization (Priority: P1)

On Cortex-M0+ targets where every CPU cycle matters, the subscription publish cycle
must not waste CPU on redundant full-array scans, software double-precision math
library calls, or 64-bit integer division in the common no-catchup case, while
producing identical wire output.

**Why this priority**: The publish cycle is the single most expensive recurring path
on Cortex-M0+ and removing `<math.h>` saves ~12 KB ROM — the single largest flash
win in the audit. Wire output must be identical.

**Independent Test**: The existing subscription deadband and integer-equivalence unit
tests pass identically before and after. The `hotpath_benchmark` publish-cycle rows
show throughput improvement. The fabs-removal yields identical deadband decisions
for all existing test vectors. ROM measurement confirms `<math.h>` no longer linked.

**Acceptance Scenarios**:

1. **Given** a subscription with 20 monitored items, **When** the publish tick runs, **Then** items are scanned once (not 3-4 times) for reportable changes — identical publish output, reduced CPU (T4).
2. **Given** a deadband computation for Float/Double types, **When** the absolute difference is computed, **Then** the result is identical to the prior `fabs()` result without linking `<math.h>` (T5, T24).
3. **Given** a sample timer advance in the common case where `elapsed < interval`, **When** the check runs, **Then** no 64-bit division is executed (T6).

---

### User Story 4 — Tier 2 hardening (Priority: P2)

A deployer using the OpenSSL host adapter must have explicit OAEP hash/MGF
parameters regardless of OpenSSL defaults, the RP2040 Pico platform must document
or implement its TCP networking status, and certificate trust configuration should
be clearly documented.

**Why this priority**: Real risks with lower exploitability or complexity. The OAEP
default fix prevents silent interop breaks on OpenSSL upgrades. The Pico
documentation prevents deployment surprises. The certificate and channel-ID items
improve defense-in-depth.

**Independent Test**: Each fix is testable in isolation. OAEP: verify the explicit
parameter calls compile and the handshake still passes. Pico: verify the README
documents the current stub status. Channel ID: verify entropy adapter is called.

**Acceptance Scenarios**:

1. **Given** the OpenSSL host adapter, **When** RSA-OAEP encryption is performed, **Then** the OAEP hash and MGF hash are explicitly set to SHA-1 (not relying on OpenSSL defaults) (T7).
2. **Given** the ActivateSession certificate identity token path, **When** MUC_OPCUA_SECURITY is disabled, **Then** the token decode and reject paths are under a single `#ifdef` block (T12).
3. **Given** a CreateSession or ActivateSession request, **When** the ServerNonce is copied into the session slot, **Then** the local stack copy is zeroized before the function returns (T13).
4. **Given** the RP2040 Pico platform adapter, **When** a developer reads its documentation, **Then** the current TCP stub status and required stack size are clearly stated (T14).
5. **Given** the trust model documentation, **When** a deployer reads the security section, **Then** DER-exact comparison and the absence of chain/CRL validation are clearly documented (T16).
6. **Given** the SecureChannel open path, **When** a channel ID is assigned, **Then** the entropy adapter is used to generate a random channel ID instead of hardcoded `1` (T17).
7. **Given** the mbedTLS PSS verify path, **When** a signature is verified, **Then** the caller-provided `signature_length` is validated against the RSA context's expected length before the PSS call (T18).

---

### User Story 5 — Tier 3 cleanup & test coverage (Priority: P3)

All remaining low-severity findings must be addressed: documentation gaps filled,
test coverage added for the nine coverage gaps identified, defense-in-depth
improvements applied (const-correctness, loop bounds, time source conditioning
guidance), and dead-code/inefficiency items resolved where trivial.

**Why this priority**: No immediate risk but improves codebase quality, reduces
future bug surface, and provides evidence for compliance audits.

**Independent Test**: Each item is independently verifiable. Const-correctness is
verified by a compile check that the arrays moved to `.rodata`. Test coverage is
verified by `ctest` runs. Documentation updates are verified by review.

**Acceptance Scenarios**:

1. **Given** the TranslateBrowsePaths response, **When** only a partial path matched, **Then** `remainingPathIndex` reflects the actual unmatched count, not 0xFFFFFFFF (T30).
2. **Given** `g_supported_services[]` and `POLICY_TABLE[]`, **When** compiled for ARM Cortex-M0+, **Then** both arrays reside in `.rodata` (flash), not `.data` (RAM) (T26).
3. **Given** the nine test coverage gaps in the audit report, **When** `ctest` runs on the full profile, **Then** each gap has a corresponding green test (T1, T2, T3, T8, T9, T10, T19, T20, T29).
4. **Given** the `advance_publish_timer` loop with a zero-interval subscription, **When** the loop would iterate unbounded, **Then** it is capped at a maximum of 100 iterations (T31).
5. **Given** the HistoryRead encoding mask, **When** a mask with reserved bits set (0xFE) is received, **Then** `BAD_NOTSUPPORTED` is returned (T29 - shared with US2).

---

### Edge Cases

- What happens when the entropy adapter is present but returns all-zeros? ActivateSession and all auth paths must reject, not accept with a zero nonce.
- What happens when `MU_SECURE_SCRATCH_SIZE` is at its minimum bound and the password decrypt buffer is moved from stack to the session scratch sub-region? The existing `_Static_assert` must still pass.
- How does the Browse fix handle the case where `requested_max_references_per_node` is zero (unlimited)? No drop — all references returned.
- What happens when a monitored-item deadband type is None and both value and status are unchanged? No notification must be generated (was spamming).
- How does removing `fabs()` affect Float vs Double deadband comparison? Identical — `diff = a - b; if (diff < 0) diff = -diff;` produces the exact same result as `fabs(a - b)`.

## Requirements

### Functional Requirements

- **FR-001**: `fill_server_nonce()` MUST return a StatusCode; `handle_activate_session` MUST reject with `BAD_SECURITYCHECKSFAILED` when nonce generation fails (T1).
- **FR-002**: The password decrypt buffer `decrypt_buf[256]` MUST be zeroized via `mu_secure_zero()` before the function exits or any `goto` target (T2).
- **FR-003**: Browse response MUST assign `res->references` and `res->num_references` from the collected pool when a node hits the reference limit, preserving the references that fit (T3).
- **FR-004**: Subscription publish cycle MUST aggregate reportable-item indices in a single pass and iterate only over those indices for encoding and cleanup (T4).
- **FR-005**: The deadband absolute-value comparison MUST use inline arithmetic (`diff = a - b; if (diff < 0) diff = -diff;`) instead of `fabs()`, and the `<math.h>` include MUST be removed (T5, T24).
- **FR-006**: Sample timer advance MUST avoid 64-bit division in the common `elapsed < interval` case; the catch-up path MUST use repeated subtraction or 32-bit operations (T6).
- **FR-007**: `rsa_oaep()` in the host crypto adapter MUST explicitly set OAEP MD and MGF1 MD via `EVP_PKEY_CTX_set_rsa_oaep_md()` and `EVP_PKEY_CTX_set_rsa_mgf1_md()` (T7).
- **FR-008**: `handle_create_session` MUST write the response prefix before creating the session, or MUST clean up the session on subsequent encoding failure (T8).
- **FR-009**: DeleteNodes MUST check `items[i].delete_target_references` and skip reference cleanup when the flag is false (T9).
- **FR-010**: Write type validation MUST set `result = BAD_INTERNALERROR` when `mu_value_source_read` fails (T10).
- **FR-011**: `mu_value_source_read` MUST return the actual value for ALL built-in scalar types, not only Boolean/Int32/UInt32/Float/String (T11).
- **FR-012**: Certificate token decode and verification paths MUST be under a single consolidated `#ifdef MUC_OPCUA_SECURITY` block (T12).
- **FR-013**: Local `nonce_buf[32]` copies in CreateSession and ActivateSession MUST be zeroized after `memcpy` into the session slot (T13).
- **FR-014**: Pico platform README MUST document the current TCP stub status and required LWIP/CYW43 integration for real networking (T14).
- **FR-015**: Trust model documentation MUST describe DER-exact certificate comparison as the intended security model (T16).
- **FR-016**: SecureChannel ID MUST use the entropy adapter for at least 4 bytes of randomness (T17).
- **FR-017**: mbedTLS PSS verify MUST validate `signature_length` against `mbedtls_rsa_get_len(rsa)` before the PSS call (T18).
- **FR-018**: Monitored-item change reportability with deadband-type NONE MUST emit notifications ONLY when value or status actually changed (T27).
- **FR-019**: HistoryRead ExtensionObject encoding mask MUST reject any value where bits other than bit 0 are set (T29).
- **FR-020**: `g_supported_services[]` and `POLICY_TABLE[]` MUST be declared `const` (T26).
- **FR-021**: The nine test coverage gaps from the audit MUST each have a corresponding green test (T1, T2, T3, T8, T9, T10, T19, T20, T29).
- **FR-022**: `advance_publish_timer` MUST have a maximum iteration guard of 100 iterations (T31).
- **FR-023**: Each fix MUST NOT increase heap usage (.data, .bss must remain zero) and MUST keep `.text` within +3% of the pre-change baseline across all four ARM profiles.

### OPC UA Normative Scope

- **OPC-001**: Target profiles remain Nano/Micro/Embedded 2017 + full per OPC-10000-7. Conformance status remains profile-targeting.
- **OPC-002**: Browse overflow fix cites OPC-10000-4 §5.9.2 (Browse service / View Service Set).
- **OPC-003**: ActivateSession nonce/entropy handling cites OPC-10000-4 §5.7.3 (ActivateSession), §7.38.2 (StatusCode set).
- **OPC-004**: DeleteNodes deleteTargetReferences cites OPC-10000-4 §5.8.5.2 (DeleteNodes).
- **OPC-005**: Deadband/DataChange notification semantics cite OPC-10000-4 §7.22.2 (DataChangeTrigger / deadband).
- **OPC-006**: Value attribute readability cites OPC-10000-3 §5.6 (Variable NodeClass / Value Attribute).
- **OPC-007**: BrowseName namespace cites OPC-10000-3 §5.2.4 (QualifiedName).
- **OPC-008**: TimestampsToReturn cites OPC-10000-4 §5.11.2.3 (Read Service / TimestampsToReturn).
- **OPC-009**: ExtensionObject encoding mask cites OPC-10000-6 §5.2.2.15 (ExtensionObject encoding).

### Scope Boundaries

- **In Scope**: All 42 findings from `docs/review/five-lens-audit-2026-07-04.md` across all four tiers plus test coverage gaps. ROM optimization (T5). Documentation updates (T14, T16).
- **Out of Scope**: New OPC UA services, new security policies, new transport protocols, CTT certification. The SHA-1 thumbprint (T15) is spec-mandated and receives documentation only. Chain-of-trust/CRL/OCSP (T16) is a documentation-only clarification of the current trust model.
- **Compatibility Claim**: Wire output identical for subscription publish, Browse, Read, ActivateSession (except the new fail-closed nonce behavior which is a security fix). No profile URI changes.
- **Application Headroom Goal**: Net ROM reduction of ~11 KB (T5 removes `<math.h>` double library). Net RAM +0 bytes. All four ARM profiles within Constitution gates (.text ≤ +3%, data+bss ≤ +5%).

### Key Entities

- **ServerNonce**: Cryptographically random 32-byte value per session activation. Must fail-closed on entropy failure (T1). Local copies must be zeroized (T13).
- **Browse Reference Pool**: Per-node collection of matched references during Browse processing. Must be assigned to the result even when the per-node limit is exceeded (T3).
- **MonitoredItem Reportability**: Bitmap or index list of items that have changed, built during the sampling pass and consumed during encoding/cleanup passes (T4).
- **DeleteTargetReferences Flag**: Per-item boolean from the DeleteNodes wire request that controls whether references from surviving nodes are preserved (T9).
- **Deadband Filter**: Per-item threshold comparison. For None type, must not generate notifications on unchanged values (T27). For Float/Double, uses inline arithmetic instead of `fabs()` (T5).

## Success Criteria

### Measurable Outcomes

- **SC-001**: All 42 audit findings are addressed (fixed, documented, or explicitly deferred with rationale).
- **SC-002**: The nine test coverage gaps each have a corresponding green test in the full profile ctest run.
- **SC-003**: ActivateSession with failed entropy returns `BAD_SECURITYCHECKSFAILED` (not silently completes with zero nonce).
- **SC-004**: The `<math.h>` double-precision library is no longer linked in any ARM profile build, verified by `scripts/measure_size.sh all` showing ~-12 KB ROM delta.
- **SC-005**: All four ARM profiles (nano/micro/embedded/full) + ASAN/UBSan stay green; no regressions in the existing test suite.
- **SC-006**: `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap usage across all profiles.
- **SC-007**: All stated OPC UA normative citations are verifiable against the spec and backed by at least one test.

## Assumptions

- The existing test infrastructure (Unity framework, service builders, fake platform, crypto adapters) is sufficient for all new negative-path tests.
- The session-handshake scratch sub-region (`MU_SECURE_SESSION_MAX = 2048`) added in Feature 025 has enough remaining capacity for the password decrypt buffer relocation (T2).
- The MonitoredItem struct can accommodate a small bitmap or index array (≤ `MU_MAX_MONITORED_ITEMS` bits) without exceeding the RAM budget (T4).
- Entropy adapter failures on real hardware (T1) can be simulated via a mock/host adapter that conditionally returns failure, as already done for the OPN path.
- The RP2040 Pico platform is a compile-only validation target for this feature; full LWIP/CYW43 integration (T14) is out of scope.
