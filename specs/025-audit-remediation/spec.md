# Feature Specification: Five-Lens Audit Remediation

**Feature Branch**: `025-audit-remediation`
**Created**: 2026-07-01
**Status**: Draft
**Input**: User description: "Remediate the 12 findings from the five-lens audit (security ×2, complexity/Big-O, embedded readiness, architecture, Cortex-M0+/RP2040): harden authentication and certificate validation, make the RP2040 platform adapter deployable, fix embedded stack/soft-float hot-path issues, remove the address-space scaling cliff, and clean up profile-config and dispatch-file maintainability debt."

## User Scenarios & Testing *(mandatory)*

This feature is a remediation of an internal security/architecture audit. The
"users" are: (a) an OPC UA **client/integrator** connecting to a secured server,
(b) a **firmware integrator** deploying the stack to an RP2040-class target, and
(c) a **maintainer** extending the codebase. Stories are grouped by audit
severity — P1 = critical + high (deployment/security blockers), P2 = medium,
P3 = low. Each story is an independently shippable, independently testable slice.

### User Story 1 - A secured session cannot be activated without proving key possession (Priority: P1)

An OPC UA client opens a secured (`Sign`/`SignAndEncrypt`) SecureChannel, creates
a session, and calls ActivateSession. Today the server decodes the client's
`ClientSignature` and throws it away, so any party that can present a certificate
(one it does not own) is activated, and the per-session `ServerNonce` provides no
anti-replay guarantee. After this story, the server verifies the client's
signature over `(ServerCertificate ‖ ServerNonce)` against the certificate the
client supplied at CreateSession, and rejects activation on any mismatch.

**Why this priority**: This is the OPC 10000-4 §5.7/§7.38 mandatory
application-instance authentication. Without it the "secured" profiles
(Embedded/full) provide no client authentication and no replay protection — the
single most serious audit finding on the wire side.

**Independent Test**: With a Sign channel, send an ActivateSession carrying a
signature computed with the wrong key (or a stale ServerNonce) and assert the
response is `Bad_SecurityChecksFailed` / `Bad_ApplicationSignatureInvalid`; send
a correctly-signed request and assert activation succeeds. Verifiable without any
other story shipping.

**Acceptance Scenarios**:

1. **Given** a Sign/SignAndEncrypt channel and a valid session, **When** the
   client sends an ActivateSession whose ClientSignature verifies over
   `serverCert ‖ serverNonce`, **Then** the session activates (existing behavior
   for the happy path is preserved).
2. **Given** the same channel, **When** the ClientSignature does not verify
   (wrong key, wrong algorithm, or tampered data), **Then** the server rejects
   activation with the cited OPC UA StatusCode and does not activate the session.
3. **Given** the same channel, **When** the ClientSignature is computed over a
   ServerNonce from a previous activation (replay), **Then** verification fails
   because the current session's ServerNonce differs.
4. **Given** SecurityPolicy None, **When** ActivateSession is sent, **Then**
   behavior is unchanged (no signature required for None).

---

### User Story 2 - Certificates are validated for trust and validity period (Priority: P1)

A client presents an application-instance certificate to open a secured channel
and to create a session. Today `mu_certificate_validate` checks only that the
certificate parses and that its RSA key size is in range; expired certificates
are accepted, and trust-list matching runs *only when a trust list is configured*
(fail-open). After this story, a non-None policy fails **closed**: the
certificate's validity period (`notBefore`/`notAfter`) is enforced, and trust is
mandatory. The CreateSession certificate is retained on the session and validated
against the trust list and bound to the OPN channel certificate.

**Why this priority**: Certificate acceptance is the gate for every secured
connection. Fail-open trust and missing expiry checks (findings 3 and 5) let an
attacker-chosen certificate through end-to-end when combined with Story 1.

**Independent Test**: Present (a) an expired but otherwise valid certificate and
(b) a syntactically valid certificate not in the trust list, each on a non-None
policy, and assert both are rejected with the cited StatusCode; present a trusted,
in-validity certificate and assert acceptance.

**Acceptance Scenarios**:

1. **Given** a non-None policy and no configured trust list, **When** a channel
   open is attempted, **Then** it is rejected (fail-closed) rather than accepted.
2. **Given** a configured trust list, **When** a client presents an expired
   certificate, **Then** validation fails with a certificate-validity StatusCode.
3. **Given** a session, **When** the CreateSession certificate differs from the
   certificate that opened the SecureChannel, **Then** the mismatch is detected
   and the session/activation is rejected.
4. **Given** SecurityPolicy None, **When** connecting, **Then** no certificate is
   required and behavior is unchanged.

---

### User Story 3 - The RP2040 example runs safely on real hardware (Priority: P1)

A firmware integrator flashes the Pico example to a physical RP2040. Today the
platform adapter is stubbed: the entropy source returns all-zero bytes (so every
nonce and derived session key is zero), the time source is frozen at 0 (so idle
and session timeouts never fire), and TCP read/write are no-ops. After this
story, the adapter draws real entropy and real time from the RP2040, and the
example either has a working transport or is explicitly and safely constrained
until one exists. Additionally, the ~2 KB single-frame stack buffers on the
secured handshake path are moved into the existing static secure-scratch arena so
the handshake does not overflow the RP2040 default stack.

**Why this priority**: Without real entropy the secured profiles are
cryptographically broken on device (finding 1), and the oversized handshake
stack frames (finding 4) HardFault on the RP2040 default 2 KB stack. Together
these block any real embedded deployment with security enabled.

**Independent Test**: On the RP2040 build/target (or host emulation of the
adapter hooks), assert the entropy hook returns non-constant, non-zero output and
the time source advances monotonically; assert the CreateSession/ActivateSession
handlers use the shared scratch arena (no >1 KB single-frame stack locals remain
on that path per the stack-usage check).

**Acceptance Scenarios**:

1. **Given** the RP2040 adapter, **When** entropy is requested twice, **Then**
   the two outputs differ and are not all-zero.
2. **Given** the RP2040 adapter, **When** time is read across a delay, **Then**
   the tick value has advanced (timeouts can fire).
3. **Given** a secured handshake, **When** CreateSession/ActivateSession run,
   **Then** the large signing/verification buffers live in the server scratch
   arena and the measured stack frame for those handlers is within the project's
   stack-usage budget.
4. **Given** the Pico example without a working TCP transport, **When** it is
   built, **Then** it is either wired to a real transport or restricted to
   SecurityPolicy None with a documented required `PICO_STACK_SIZE`.

---

### User Story 4 - The address space scales past 64 nodes without a silent cliff (Priority: P2)

An integrator registers a static address space larger than
`MU_MAX_ADDRESS_SPACE_NODES` (64 nodes). Today the sort index silently disables
itself above the cap, so every node lookup degrades to a linear scan (amplified
by per-request node loops) **and** Query returns nothing at all. After this
story, the behavior above the cap is either a loud build-time assertion or a
supported binary-search path over a sorted user address space — never a silent
correctness/performance cliff.

**Why this priority**: A silent cliff at exactly 64 nodes produces both a CPU-DoS
amplifier on an MCU and a wrong-answer bug (empty Query), but it only triggers on
larger-than-default address spaces, so it is medium rather than blocking.

**Independent Test**: Build/register an address space of 65+ nodes; assert either
a compile-time/registration error is raised, or that node lookups and Query
return correct results with lookup remaining better-than-linear.

**Acceptance Scenarios**:

1. **Given** an address space of `> MU_MAX_ADDRESS_SPACE_NODES` nodes, **When**
   the server is built/initialized, **Then** the outcome is a clear diagnostic or
   a correctly-indexed space — not a silent fallback.
2. **Given** a supported large address space, **When** a client issues Read /
   Browse / Query, **Then** results are correct and Query is non-empty.

---

### User Story 5 - Subscription sampling avoids soft-float on integer data (Priority: P2)

An Embedded/full build runs subscriptions with absolute deadband and aggregate
filters over integer-typed sensor values. Today the deadband comparison and
aggregate math run in `double`, pulling soft-float emulation into the per-tick
sampling loop on the no-FPU core — contradicting the project's own stated
integer-only policy. After this story, integer variant types are compared and
aggregated in integer (int64) arithmetic, with `double` used only for genuine
`Float`/`Double` values.

**Why this priority**: A measurable flash + per-sample cycle cost on the hottest
subscription path, but confined to Embedded/full (Nano/Micro do not compile it),
so medium.

**Independent Test**: Feed integer-typed monitored items through the deadband and
aggregate paths and assert identical reported/suppressed decisions as before
(golden values), while the integer path is taken (no soft-float symbols on the
integer branch).

**Acceptance Scenarios**:

1. **Given** an integer-typed monitored item with an absolute deadband, **When**
   samples arrive, **Then** the same values are reported/suppressed as the prior
   double implementation (equivalent decisions on the golden fixtures).
2. **Given** a `Float`/`Double`-typed monitored item, **When** samples arrive,
   **Then** the double path is used and behavior is unchanged.

---

### User Story 6 - Mis-specified custom profiles fail loudly at build time (Priority: P2)

A maintainer configures a `custom` profile and enables a feature whose dependency
is not enabled (e.g. `SUBSCRIPTIONS_STANDARD` without `SUBSCRIPTIONS`, or `EVENTS`
without `SUBSCRIPTIONS`). Today the service is silently dropped by an `#if` gate,
producing a half-wired binary. After this story, a single feature-configuration
header provides safe defaults for every gate and `#error`s on illegal
combinations.

**Why this priority**: Prevents shipping a silently-broken profile; medium
because it affects only non-preset (custom) configurations.

**Independent Test**: Compile a custom profile with a known-illegal feature
combination and assert the build fails with a descriptive `#error`; compile each
shipped preset (Nano/Micro/Embedded/full) and assert all still build clean.

**Acceptance Scenarios**:

1. **Given** a custom profile enabling a feature without its dependency, **When**
   the project is compiled, **Then** compilation stops with a descriptive error
   naming the missing dependency.
2. **Given** any of the four shipped presets, **When** compiled, **Then** the
   build succeeds unchanged and the same services are wired as before.

---

### User Story 7 - Residual hardening, latent-defect, and maintainability cleanup (Priority: P3)

A maintainer addresses the remaining low-severity items so they do not become
future live bugs: username-token ServerNonce anti-replay is enforced
unconditionally (not only when the password is encrypted) using a constant-time
compare; the two latent integer-handling defects (uninitialized QueryFirst filter
operand; unchecked negative ExtensionObject length in HistoryUpdate) are fixed;
the dispatch file is de-tangled by extracting the inlined session/channel state
machines and monitored-item/filter decoders into their owning translation units;
and the build/config debt is paid down (Pico size flags, duplicated
`MU_SERVER_STORAGE_BYTES`, per-SecurityPolicy switch table, `ref_type_is_subtype_of`
iteration bound).

**Why this priority**: Individually low-severity or purely internal, but they
close latent defects and materially improve long-term maintainability.

**Independent Test**: Unit tests for the username-nonce path and the two decoder
defects; a build check confirming the refactor produces byte-identical wire
behavior and stays within the resource gates.

**Acceptance Scenarios**:

1. **Given** a username identity token without token-level encryption, **When**
   ActivateSession is processed, **Then** the ServerNonce anti-replay check still
   runs and uses a constant-time comparison.
2. **Given** a QueryFirst with an unsupported filter operand, **When** decoded,
   **Then** no uninitialized operand type is left readable (rejected or
   zero-initialized).
3. **Given** a HistoryUpdate with a negative ExtensionObject length, **When**
   decoded, **Then** it is rejected with `Bad_DecodingError`.
4. **Given** the dispatch/security refactors, **When** the suite runs, **Then**
   wire behavior is unchanged and all StatusCodes are identical to pre-refactor.

---

### Edge Cases

- **Signature/nonce boundary**: ActivateSession with a zero-length or oversized
  ClientSignature, or a ServerNonce shorter than expected, must be rejected
  cleanly (not read out of bounds — the existing bounds-checked reader still
  governs).
- **Missing crypto hook**: a build whose crypto adapter lacks the new
  validity-period hook must degrade to a defined, safe behavior (fail-closed for
  secured policies) rather than skip the check.
- **Address space exactly at the cap** (64 nodes) vs one over: the boundary must
  be handled deterministically with no off-by-one silent fallback.
- **Deadband on mixed/edge numeric types** (e.g. UInt64 near `INT64_MAX`): the
  integer path must not overflow when computing `|new − last|`.
- **Refactor parity**: moving handlers/decoders across translation units must not
  change `--gc-sections` dead-stripping such that a profile gains/loses code.
- **Flash/RAM headroom**: every change must keep each profile within the resource
  gates; the refactor (Story 7) must be net-neutral on `.text`/RAM.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The server MUST verify the ActivateSession `ClientSignature` over
  `(ServerCertificate ‖ ServerNonce)` using the configured RSA-SHA256 verify
  primitive against the client's CreateSession certificate, for every non-None
  security policy, and MUST reject activation on mismatch.
- **FR-002**: The server MUST bind the current session's `ServerNonce` into the
  signature check so a signature over a prior nonce (replay) fails.
- **FR-003**: Certificate validation MUST enforce the certificate validity period
  (`notBefore`/`notAfter`) via a crypto-adapter hook for non-None policies.
- **FR-004**: Trust-list validation MUST be mandatory (fail-closed) for any
  non-None security policy; a missing/empty trust list MUST cause secured
  connections to be rejected rather than accepted.
- **FR-005**: The server MUST retain the CreateSession client certificate on the
  session and MUST validate it against the trust list and confirm it matches the
  certificate that opened the SecureChannel.
- **FR-006**: The username-token `ServerNonce` anti-replay check MUST run for all
  username tokens (not only RSA-encrypted passwords) and MUST use a constant-time
  comparison.
- **FR-007**: The RP2040 platform adapter MUST provide a real hardware entropy
  source (non-constant, non-zero output) and a monotonic real time/tick source.
- **FR-008**: The Pico example MUST either use a working TCP transport or be
  explicitly restricted to SecurityPolicy None until one exists, and MUST
  document the required `PICO_STACK_SIZE`.
- **FR-009**: The large signing/verification buffers in the CreateSession and
  ActivateSession handlers MUST be relocated from single-function stack frames
  into the existing server secure-scratch arena.
- **FR-010**: Behavior above `MU_MAX_ADDRESS_SPACE_NODES` MUST be deterministic —
  a build-time/registration diagnostic or a supported indexed path — never a
  silent linear-scan fallback; Query MUST return correct (non-empty) results for
  supported address spaces of any size.
- **FR-011**: Absolute-deadband and aggregate computations MUST use integer
  arithmetic for integer variant types, reserving floating point for genuine
  `Float`/`Double` values, with identical report/suppress decisions to the prior
  implementation.
- **FR-012**: A single feature-configuration header MUST provide safe defaults
  for every feature gate and MUST `#error` on illegal feature combinations.
- **FR-013**: The QueryFirst unsupported-operand path MUST NOT leave an operand
  type uninitialized (reject or zero-initialize), and the HistoryUpdate
  ExtensionObject length MUST be rejected when negative.
- **FR-014**: The inlined session/channel handlers and monitored-item/filter
  decoders MUST be moved out of the dispatch translation unit into their owning
  units, with no change to wire behavior.
- **FR-015**: Build/config debt MUST be resolved: Pico targets built with size
  flags (`-Os`/`--gc-sections`), the duplicated `MU_SERVER_STORAGE_BYTES`
  definition collapsed, per-SecurityPolicy parameter switches replaced by a single
  static table, and `ref_type_is_subtype_of` given an iteration bound.
- **FR-016**: Every change except the explicit conformance fixes MUST be
  wire-behavior-neutral; the conformance fixes (FR-001–006, FR-013) MUST change
  only the reject/accept decision for previously-invalid inputs, never the
  encoding of valid responses.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target role/profile is unchanged — Nano/Micro/Embedded 2017 plus
  the full-featured superset, per OPC-10000-7; conformance status remains
  **profile-targeting** (this feature does not claim CTT verification).
- **OPC-002**: Services touched: ActivateSession and CreateSession
  (OPC-10000-4 §5.7, §7.38, §5.6), certificate/SecureChannel handling
  (OPC-10000-4 §5.5, OPC-10000-6 §6.7), Subscription/MonitoredItem deadband &
  aggregate (OPC-10000-4 §7.22, OPC-10000-13), Query (OPC-10000-4 §5.9),
  HistoryUpdate (OPC-10000-11). No new service is added.
- **OPC-003**: Newly-enforced checks MUST return the OPC-UA-cited StatusCodes:
  `Bad_ApplicationSignatureInvalid` / `Bad_SecurityChecksFailed` for a failed
  ClientSignature or certificate check, `Bad_CertificateTimeInvalid` (or the
  project's mapped equivalent) for expiry, `Bad_DecodingError` for the negative
  HistoryUpdate length.
- **OPC-004**: Wire encoding/transport (OPC-10000-6: Binary encoding,
  MessageChunk, OPC UA TCP) is unchanged; no message layout, header, or chunking
  behavior is modified.
- **OPC-005**: SecurityPolicy set is unchanged (None, Basic256Sha256,
  Aes128_Sha256_RsaOaep, Aes256_Sha256_RsaPss); this feature only tightens the
  acceptance decisions for secured policies. Status: profile-targeting.

### Scope Boundaries *(mandatory)*

- **In Scope**: Verification/validation logic for ActivateSession and
  certificates; RP2040 entropy/time (and transport wiring or safe restriction);
  relocation of handshake scratch buffers; address-space cap behavior; integer
  deadband/aggregate; a feature-config header with dependency guards; the two
  latent decoder fixes; the dispatch/security-policy refactors; and the build/config
  cleanup. All four profiles.
- **Out of Scope**: New OPC UA services or SecurityPolicies; any change to the
  binary wire encoding of valid messages; multi-connection/multi-core support;
  a production-grade lwIP integration beyond what the example needs to be safe;
  external CTT conformance certification.
- **Compatibility Claim**: After this feature, the server may claim the same
  Nano/Micro/Embedded 2017 profile-targeting surface as before, now with
  OPC-10000-4-compliant ActivateSession client authentication and certificate
  validity/trust enforcement for secured policies.
- **Application Headroom Goal**: Each profile stays within the existing resource
  gates — `.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap, normalized
  throughput ≥ 0.85× baseline. The Story 7 refactor is expected net-neutral.

### Key Entities *(include if feature involves data)*

- **ClientSignature (SignatureData)**: algorithm + signature bytes proving the
  client holds the private key of its application-instance certificate; verified
  over `serverCert ‖ serverNonce`.
- **Application-instance certificate**: the client's X.509 certificate, now
  retained on the session, validated for trust and validity period, and bound to
  the SecureChannel certificate.
- **ServerNonce**: per-session random value binding activation liveness; requires
  real hardware entropy on the target.
- **Secure-scratch arena**: the existing static server-owned buffer that large
  handshake signing/verification data must use instead of the stack.
- **Feature-configuration header**: single source of feature-gate defaults and
  dependency `#error` guards.
- **Address-space index**: the sorted lookup structure whose cap behavior must
  become deterministic.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of ActivateSession requests on a secured policy with an
  invalid, mismatched, or replayed ClientSignature are rejected; 100% of
  correctly-signed requests still succeed.
- **SC-002**: On a non-None policy, a secured connection is rejected when no trust
  list is configured and when the presented certificate is expired or untrusted;
  a trusted, in-validity certificate connects successfully.
- **SC-003**: On real RP2040 hardware (or the adapter-hook test double), two
  successive entropy reads differ and are non-zero, and the time source advances
  across a measured delay — so timeouts fire.
- **SC-004**: The CreateSession and ActivateSession handlers hold no single-frame
  stack local larger than 1 KB on the secured path (verified by the stack-usage
  check), and the documented `PICO_STACK_SIZE` is sufficient for the handshake.
- **SC-005**: All previously-specified malformed inputs — invalid signature,
  expired/untrusted cert, negative HistoryUpdate length, unsupported QueryFirst
  operand — return their cited OPC UA StatusCodes with no out-of-bounds access.
- **SC-006**: Every profile (Nano/Micro/Embedded/full) stays within the resource
  gates (`.text` ≤ +3%, `.data + .bss` ≤ +5%, no new heap, throughput ≥ 0.85×
  baseline), and all four presets plus at least one illegal custom combination
  behave as specified at build time.
- **SC-007**: An address space of more than 64 nodes yields correct Read/Browse
  results and non-empty Query, or a clear build/registration diagnostic — never a
  silent linear-scan fallback.
- **SC-008**: The full existing `ctest` suite stays green, and the
  dispatch/security-policy refactor produces byte-identical responses to the
  pre-refactor build on the regression fixtures.

## Assumptions

- The three shipped crypto backends (mbedtls, wolfssl, host/OpenSSL) can expose a
  certificate validity-period check; where a backend cannot, the secured path
  fails closed rather than skipping the check.
- The RP2040 entropy source is the on-chip ROSC-based TRNG and the time source is
  the SDK timer (`time_us_64()` / `to_ms_since_boot()`); a full lwIP transport is
  not required for this feature — a safe restriction to SecurityPolicy None is an
  acceptable interim for the example if transport work is deferred.
- The existing secure-scratch arena has (or can be sized to) sufficient capacity
  for the relocated CreateSession/ActivateSession buffers, consistent with the
  existing `_Static_assert` sizing discipline.
- "Integer variant types" for the deadband/aggregate fix are the signed/unsigned
  8–64-bit integer built-ins; `Float`/`Double` retain floating-point handling.
- The dispatch and SecurityPolicy refactors are behavior-preserving reorganizations
  validated by the existing regression/interop fixtures, not redesigns.
- The audit findings and their file locations (from the completed five-lens audit)
  are accepted as the authoritative problem statement; this spec does not re-derive
  them.
