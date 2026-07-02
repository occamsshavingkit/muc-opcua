# Feature Specification: Conformance-Test Hardening

**Feature Branch**: `028-conformance-test-hardening`
**Created**: 2026-07-02
**Status**: Draft
**Input**: User description: "Make the internal test suite a genuine per-profile conformance-evidence harness (no external CTT available for a long time): per-profile CI + claim→test enforcement, fix the audited bugs/claim-mismatches, add real-crypto security acceptance tests, and force the implemented-but-untested StatusCodes and encoding negative paths. Acts on the five-domain conformance-test gap audit."

## User Scenarios & Testing *(mandatory)*

The "users" are: (a) a **conformance auditor / integrator** who must trust that a
claimed profile behaves as OPC-10000-7 says — with **no external CTT available for
a long time, the internal test suite IS the conformance evidence**; and (b) a
**maintainer** who needs the build to fail when a claim isn't actually verified.
Stories are grouped by the audit's tiers; Tier 0 is the structural keystone and
must come first. Each story is an independently shippable, independently verifiable
slice.

### User Story 1 - Per-profile conformance evidence exists (Priority: P1) 🎯 keystone

Today no test ever runs in a Nano/Micro/Embedded build — the build forces the
`full` profile whenever tests are compiled, so every claim is validated only in the
one build where all features are ON, and the per-profile test gates are dead code.
After this story, the suite builds and runs the tests against each profile's actual
library, and a claimed conformance unit that has no test running in that profile
fails the build (a machine-checkable claim→test map), replacing the current
markdown-string-matching "traceability."

**Why this priority**: This is the foundation of "internal tests are the
conformance evidence." Without per-profile execution, every other test still only
proves behavior in the `full` build; with it, the remaining tiers accrue real
per-profile value. Highest leverage.

**Independent Test**: Build+run the suite with each of `nano`, `micro`, `embedded`
(and `full`); confirm tests actually execute in each and that removing a claimed
unit's backing test (or the test's profile gate) makes that profile's build fail.

**Acceptance Scenarios**:

1. **Given** the CI/build tooling, **When** the suite is built for `nano`,
   `micro`, and `embedded`, **Then** tests compile and `ctest` runs against each
   profile's library (the per-profile gates become live), not just `full`.
2. **Given** the claim→test map, **When** a conformance unit is claimed for a
   profile but no test that runs in that profile backs it, **Then** the check
   fails and names the unmapped unit.
3. **Given** a green `full` build, **When** the per-profile jobs run, **Then** any
   claim/build mismatch (a unit claimed for a profile but absent from that
   profile's build) is surfaced as a failure, not hidden.

---

### User Story 2 - Audited defects and false claims are corrected (Priority: P1)

The audit surfaced real behavior bugs and claims the code doesn't back. After this
story each is either fixed to the conformant behavior or the claim is corrected
honestly, with a test pinning the result.

**Why this priority**: These are correctness/honesty defects (wrong StatusCode,
mandatory nodes absent from a claimed profile, StatusCodes/types claimed but never
produced). They actively mislead an integrator relying on the docs.

**Independent Test**: For each item, a test asserts the corrected behavior; for
each corrected claim, the doc no longer over-claims and a build/test confirms the
reconciled state.

**Acceptance Scenarios**:

1. **Given** a Browse request exceeding the node cap, **When** it is decoded,
   **Then** the server returns `Bad_TooManyOperations` (not `Bad_InternalError`),
   matching Read/Write/Query.
2. **Given** the Nano and Micro profiles, **When** their build is inspected/tested,
   **Then** either the mandatory Server object / ServerStatus / ServerProfileArray /
   NamespaceArray are present, or `profile-nano.md`/`profile-micro.md` no longer
   claim the Core Server Facet behaviors that aren't built.
3. **Given** the RegisterNodes/UnregisterNodes Nano claim, **When** reconciled,
   **Then** either those services are built in the Nano profile or the Nano claim
   is corrected.
4. **Given** the Micro profile, **When** its ServerProfileArray is read, **Then**
   the advertised URI matches what the profile actually implements (or the Micro
   URI claim is dropped).
5. **Given** a StatusCode claimed in the docs, **When** the suite runs, **Then**
   either a test forces the server to emit it, or it is removed from the claimed
   set (no code claims a StatusCode it never produces).
6. **Given** the built-in-type claims, **When** reconciled, **Then** any type
   claimed as supported has an encode/decode test, or the claim is scoped to where
   the type is actually handled.

---

### User Story 3 - Security acceptance paths are proven with real crypto (Priority: P1)

The security-critical *acceptance* paths (a valid client is admitted) are currently
proven only against mock verifiers — the exact pattern that hid a real bug. After
this story, the accept and reject sides are exercised against a real crypto backend.

**Why this priority**: A mock that mirrors a bug proves nothing; these paths gate
who is admitted to a secured server. This is the weakest point of the
"tests-are-evidence" posture.

**Independent Test**: End-to-end handshakes/activations against the real host
crypto adapter (and, where feasible, mbedTLS/wolfSSL) drive each accept and reject
case to the correct outcome.

**Acceptance Scenarios**:

1. **Given** a real crypto backend, **When** an X509 (i=327) user-token
   ActivateSession is presented with a correctly-signed token, **Then** it is
   accepted; with a bad signature, **Then** it is rejected.
2. **Given** a secured channel and a real backend, **When** a valid UserName/
   password identity is presented, **Then** activation reaches Good; with a wrong
   password, **Then** it is rejected.
3. **Given** an encrypted password token, **When** the ServerNonce is correct,
   **Then** it is accepted; when the ServerNonce is wrong/stale (replay), **Then**
   it is rejected.
4. **Given** a real expired certificate and a real not-yet-valid certificate,
   **When** presented on a secured policy, **Then** each is rejected for its
   validity period.
5. **Given** a certificate whose RSA key size is out of the profile bounds,
   **When** validated, **Then** it is rejected.
6. **Given** `allow_untrusted_clients=false` and no matching trust-list entry,
   **When** an untrusted client certificate opens a secured channel, **Then** the
   handshake is rejected (fail-closed verified end-to-end).

---

### User Story 4 - Implemented behaviors get their forcing tests (Priority: P2)

Several StatusCodes and service negative paths are implemented but never triggered
by a test. After this story each has a test that constructs the condition and
asserts the emitted code / outcome.

**Why this priority**: Cheap, high-value coverage that backs headline profile
claims (session count, request limits) and closes the "implemented but unverified"
category — but the behavior already exists, so lower risk than US1–US3.

**Independent Test**: Each condition is driven and the exact StatusCode / state is
asserted.

**Acceptance Scenarios**:

1. **Given** CreateSession beyond `MU_MAX_SESSIONS`, **Then** `Bad_TooManySessions`.
2. **Given** an oversized request/OPN, **Then** `Bad_RequestTooLarge`.
3. **Given** a Read exceeding the node cap, **Then** `Bad_TooManyOperations`.
4. **Given** the relevant conditions, **Then** `Bad_HistoryOperationUnsupported`,
   `Bad_MessageNotAvailable`, and `Bad_NotFound` are each emitted and asserted.
5. **Given** a CloseSecureChannel (CLO) message driven through the poll loop,
   **Then** the channel is observably closed.
6. **Given** SetMonitoringMode / SetPublishingMode with an invalid id, **Then**
   the per-item invalid-id StatusCode is returned.

---

### User Story 5 - Encoding/transport gaps are covered (Priority: P2)

Wire-reachable decoders and transport limits that lack malformed-input or
round-trip coverage get tests. After this story every production wire decoder has
at least a malformed-input test, and the primitives lacking round-trip tests get
them.

**Why this priority**: Robustness of the wire surface (malformed input is
attacker-controllable), but the highest-risk decoders already have coverage, so
this rounds out breadth at P2.

**Independent Test**: Malformed/boundary inputs to each named decoder assert
`Bad_DecodingError` (or defined handling); primitives round-trip.

**Acceptance Scenarios**:

1. **Given** a malformed ExpandedNodeId (bad NamespaceUri/ServerIndex flag +
   truncation), **When** decoded, **Then** `Bad_DecodingError` — with unit and
   fuzz coverage.
2. **Given** a HELLO with an over-length EndpointUrl, **When** processed, **Then**
   it is rejected.
3. **Given** NodeId identifier types Guid (0x04) and Opaque/ByteString (0x05),
   **When** decoded, **Then** they are handled or explicitly rejected (no silent
   misparse).
4. **Given** each primitive lacking a dedicated test (SByte, Int16, UInt16, Int64,
   UInt64, Double, DateTime), **When** round-tripped, **Then** values are exact;
   DataValue timestamp/picosecond mask bits round-trip.
5. **Given** a QualifiedName / LocalizedText with a truncated field, **Then**
   `Bad_DecodingError`.
6. **Given** the RSA-PSS algorithm URI advertised on a PKCS#1.5 policy (the reverse
   of the existing case), **Then** the signature is rejected.
7. **Given** a message spanning more than `max_chunk_count` chunks, **Then** it is
   rejected.

### Edge Cases

- A per-profile job must fail loudly if a profile can't build at all (not skip).
- The claim→test map must handle a unit legitimately covered only in a superset
  profile vs. one genuinely missing from its own profile — these are different and
  must not be conflated.
- Reconciling a claim by *removing* it must keep the anti-over-claim guards
  intact (the status must stay profile-targeting; no unqualified compliance or
  external-tool-verified wording without external evidence).
- New real-crypto tests must run only where the crypto backend is available
  (OpenSSL/mbedTLS/wolfSSL present), and skip cleanly otherwise — never silently
  pass as if covered.
- Fixing the Browse StatusCode must not change behavior for in-bounds requests.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The build/CI MUST compile and run the test suite against the `nano`,
  `micro`, and `embedded` profiles (in addition to `full`), so the per-profile test
  gates are exercised and each profile's claimed behavior runs against that
  profile's actual library.
- **FR-002**: A machine-checkable claim→test mapping MUST fail the build when a
  conformance unit claimed for a profile has no test that compiles and runs in that
  profile. Markdown substring matching alone MUST NOT satisfy this.
- **FR-003**: Browse with more than the supported `nodesToBrowse` MUST return
  `Bad_TooManyOperations` (consistent with Read/Write/Query), with a test.
- **FR-004**: For each mandatory Base-Information behavior claimed by a profile,
  the profile build MUST either provide it (with a profile-runnable test) or the
  claim MUST be corrected; this applies to the Server object/ServerStatus/
  ServerProfileArray/NamespaceArray, RegisterNodes/UnregisterNodes, and the Micro
  ServerProfileArray URI.
- **FR-005**: No StatusCode or built-in type may be listed as supported/claimed
  without either a test that exercises it or removal of the claim.
- **FR-006**: The X509 user-token, UserName-accept, encrypted-password + nonce,
  certificate validity, key-size, and fail-closed-trust behaviors MUST each have a
  test exercising the real crypto backend for both accept and reject outcomes;
  mock-only coverage does not satisfy this.
- **FR-007**: Each implemented-but-untested StatusCode/negative path in US4 MUST
  have a test that constructs the triggering condition and asserts the outcome.
- **FR-008**: Each wire-reachable decoder in US5 MUST have malformed-input coverage
  (unit and, for those on the fuzz surface, fuzz); primitives lacking round-trip
  tests MUST get them.
- **FR-009**: All changes MUST keep the existing `ctest`, ASAN/UBSan, and the four
  ARM profile builds green and within the resource gates.
- **FR-010**: Where a claim is corrected rather than implemented, the conformance/
  traceability docs MUST be updated honestly, and conformance status MUST remain
  labeled **profile-targeting**; the stronger conformance claims stay out of scope
  until external CTT evidence exists.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target role/profiles unchanged — Nano/Micro/Embedded 2017 + full,
  per OPC-10000-7; conformance status remains **profile-targeting**. This feature
  strengthens the *evidence* for those claims, it does not add profiles.
- **OPC-002**: Behaviors touched by the few code fixes: Browse operation limit
  (OPC-10000-4 §5.9.2, §7.38.2), Base Information nodes (OPC-10000-5 / OPC-10000-7
  Core Server Facet), AddNodes duplicate handling (OPC-10000-4 §5.7 NodeManagement)
  where `Bad_NodeIdExists` is implemented. No new service is added.
- **OPC-003**: Newly-forced conditions MUST return their OPC-UA-cited StatusCodes
  (`Bad_TooManyOperations`, `Bad_TooManySessions`, `Bad_RequestTooLarge`,
  `Bad_DecodingError`, certificate/security codes, etc.) — reusing existing codes.
- **OPC-004**: Wire encoding/transport (OPC-10000-6) behavior is unchanged except
  the Browse StatusCode correction and any decoder that currently misparses an
  identifier type; no valid-message encoding changes.
- **OPC-005**: SecurityPolicy set unchanged; the security work adds *tests*, not
  new policy behavior (except reconciled claims). Status: profile-targeting.

### Scope Boundaries *(mandatory)*

- **In Scope**: Per-profile build/CI + claim→test enforcement; the specific
  audited code fixes and doc reconciliations; real-crypto security acceptance/
  reject tests; forcing tests for implemented-but-untested StatusCodes and negative
  paths; encoding/transport malformed/round-trip coverage. All four profiles.
- **Out of Scope**: External CTT execution; new OPC UA services, SecurityPolicies,
  transports, or encodings; performance work; the deferred UserTokenPolicy-keyed
  signature scheme (feature 026 boundary); any change to valid-message wire
  encoding beyond the Browse StatusCode correction.
- **Compatibility Claim**: After this feature the same Nano/Micro/Embedded 2017
  profile-targeting surface may be claimed, now backed by per-profile test evidence
  and honest, reconciled docs — still profile-targeting pending external CTT.
- **Application Headroom Goal**: Within existing gates — `.text` ≤ +3%,
  `.data + .bss` ≤ +5%, no new heap. Most work is tests/CI (zero runtime cost); the
  few code fixes are tiny.

### Key Entities *(include if feature involves data)*

- **Conformance unit ↔ test map**: the machine-checkable mapping from each claimed
  conformance unit/profile to the test(s) that verify it in that profile's build.
- **Profile build matrix**: nano / micro / embedded / full, each with its feature-
  macro set and the tests that run in it.
- **Claim record**: an entry in the conformance/traceability docs; must be backed
  by a test or removed.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The suite builds and runs under all four profiles; per-profile jobs
  exist in CI and `make` targets, and the per-profile test gates are demonstrably
  live (a gated test runs in its profile and is absent in others).
- **SC-002**: 100% of claimed conformance units have a backing test that runs in
  the claimed profile, enforced by a failing check when one is missing.
- **SC-003**: Every StatusCode and built-in type still listed as supported has a
  test that exercises it; the previously-unbacked claims are either implemented+
  tested or removed.
- **SC-004**: Browse overflow returns `Bad_TooManyOperations`; the Nano/Micro base-
  node, RegisterNodes, and Micro-URI claims are reconciled (built+tested or
  corrected).
- **SC-005**: The six US3 security behaviors each pass an accept-and-reject test on
  a real crypto backend (skipping cleanly only where no backend is present).
- **SC-006**: Every implemented-but-untested StatusCode/negative path in US4 and
  every wire decoder/primitive in US5 has its test; the full suite, ASAN/UBSan, and
  four ARM profiles stay green within the resource gates.
- **SC-007**: Conformance docs contain no over-claim; status remains
  profile-targeting and the anti-over-claim guards still pass.

## Assumptions

- Adding per-profile build/CI jobs is feasible with the existing CMake profile
  presets; the `full`-forcing logic in the build can be scoped so an explicit
  profile + tests is honored.
- The crypto backends (host/OpenSSL, mbedTLS, wolfSSL) can generate/sign with real
  keys in tests; a real expired/not-yet-valid cert can be produced (generated with
  past/future validity) or supplied as a fixture.
- Reconciling a claim may legitimately mean *correcting the doc* rather than adding
  code (e.g., if including base nodes in Nano would blow the size budget, the Nano
  claim is narrowed) — the audit's intent is honesty, not maximal features.
- The claim→test map can be expressed as a checked artifact (e.g., a table the
  build validates against registered/passing tests) rather than prose.
- This is remediation of an existing audit; the audit findings and their locations
  are the authoritative problem statement and are not re-derived here.
- The deliverable of planning is an ordered, dependency-aware task list; Tier 0
  (US1) precedes the rest because it makes the other tiers' tests count per-profile.
