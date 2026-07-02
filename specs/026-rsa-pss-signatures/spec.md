# Feature Specification: SecurityPolicy-aware RSA-PSS Signatures

**Feature Branch**: `026-rsa-pss-signatures`
**Created**: 2026-07-02
**Status**: Draft
**Input**: User description: "Make OPC UA asymmetric signature handling SecurityPolicy-aware so the Aes256_Sha256_RsaPss policy uses RSA-PSS-SHA256 signatures (conformance fix). All three asymmetric-signature sites currently use PKCS#1.5 regardless of policy; the PSS policy needs RSA-PSS-SHA256 + its own algorithm URI. Zero change for None/Basic256Sha256/Aes128."

## User Scenarios & Testing *(mandatory)*

The "users" are: (a) an OPC UA **client** connecting with the `Aes256_Sha256_RsaPss`
SecurityPolicy (including the OPC Foundation reference client and the CTT), and
(b) a **conformance auditor** who needs the server's advertised policy to actually
behave as OPC-10000-7 defines it. Today the server negotiates
`Aes256_Sha256_RsaPss` but signs and verifies with RSA-PKCS#1.5 and advertises the
PKCS#1.5 algorithm URI — so a strict client rejects the ServerSignature and the
server would reject a correctly-PSS-signed ClientSignature. This feature makes the
signature scheme and its declared algorithm URI follow the negotiated policy.

### User Story 1 - A client using Aes256_Sha256_RsaPss completes the handshake (Priority: P1)

A client opens a `Aes256_Sha256_RsaPss` SecureChannel, creates a session, and
activates it. After this feature, the ServerSignature is produced with
RSA-PSS-SHA256 and carries the PSS algorithm URI, and the server verifies the
client's PSS-signed ClientSignature — so the full secured handshake succeeds with
a conformant peer.

**Why this priority**: Without it, the `Aes256_Sha256_RsaPss` policy the server
advertises is not actually usable/conformant — a P1 correctness gap and a blocker
for claiming that policy or passing CTT.

**Independent Test**: Drive a full `Aes256_Sha256_RsaPss` handshake where the
client signs with PSS; assert CreateSession returns a ServerSignature whose
algorithm URI is the PSS URI and whose bytes verify under PSS, and that
ActivateSession succeeds. Verifiable on its own.

**Acceptance Scenarios**:

1. **Given** an `Aes256_Sha256_RsaPss` channel, **When** CreateSession is handled,
   **Then** the ServerSignature is computed with RSA-PSS-SHA256 over
   `(clientCertificate ‖ clientNonce)` and its `algorithm` field is the PSS URI.
2. **Given** the same channel and a session, **When** ActivateSession presents a
   ClientSignature computed with RSA-PSS-SHA256 over `(serverCert ‖ serverNonce)`,
   **Then** the server verifies it under PSS and activates.
3. **Given** the same channel, **When** the ClientSignature is (incorrectly) signed
   with PKCS#1.5 or carries the PKCS#1.5 algorithm URI, **Then** the server rejects
   activation.

### User Story 2 - Basic256Sha256 and Aes128_Sha256_RsaOaep are unchanged (Priority: P1)

A client using `Basic256Sha256` or `Aes128_Sha256_RsaOaep` sees byte-for-byte the
same handshake as before: PKCS#1.5 signatures and the
`http://www.w3.org/2001/04/xmldsig-more#rsa-sha256` algorithm URI.

**Why this priority**: These policies are already interoperable and CTT-relevant;
a regression here would be worse than the bug being fixed.

**Independent Test**: The existing Basic256Sha256 and Aes128 handshake tests pass
unchanged, and the ServerSignature algorithm URI and bytes are identical to the
pre-feature build on the regression fixtures.

**Acceptance Scenarios**:

1. **Given** a `Basic256Sha256` or `Aes128_Sha256_RsaOaep` channel, **When** the
   handshake runs, **Then** signatures use PKCS#1.5 and the xmldsig algorithm URI
   exactly as before.
2. **Given** SecurityPolicy None, **When** connecting, **Then** there is no
   signature and behavior is unchanged.

### User Story 3 - The declared signature algorithm is validated against the policy (Priority: P2)

The incoming ClientSignature carries an `algorithm` URI. After this feature the
server checks that URI against the negotiated policy and rejects a signature that
declares the wrong algorithm for the policy, rather than ignoring the field.

**Why this priority**: Hardening that closes an algorithm-confusion gap; medium
because the cryptographic verification itself already fails for a genuinely
mismatched scheme, so this is defense-in-depth plus a clearer StatusCode.

**Independent Test**: On an `Aes256_Sha256_RsaPss` channel, send an ActivateSession
whose ClientSignature.algorithm is the PKCS#1.5 URI; assert rejection with the
cited StatusCode.

**Acceptance Scenarios**:

1. **Given** any secured policy, **When** the ClientSignature.algorithm URI does
   not match the policy's expected signature algorithm, **Then** the server rejects
   activation.

### Edge Cases

- A backend whose `rsa_pss_sha256_sign`/`rsa_pss_sha256_verify` is absent on an
  `Aes256_Sha256_RsaPss` channel must fail closed (reject), not silently fall back
  to PKCS#1.5.
- An empty or zero-length ClientSignature on a secured channel is rejected (existing
  behavior preserved).
- Buffer sizes for the signature and signed data are unchanged; a PSS signature is
  the same length as the PKCS#1.5 signature for the same key, so no buffer growth.
- The algorithm-URI comparison must be exact (length + bytes) and must not read out
  of bounds for a short/oversized URI.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The server MUST select the asymmetric signature scheme by negotiated
  SecurityPolicy: RSA-PKCS#1.5-SHA256 for `Basic256Sha256` and
  `Aes128_Sha256_RsaOaep`; RSA-PSS-SHA256 for `Aes256_Sha256_RsaPss`.
- **FR-002**: The server MUST advertise the matching `SignatureData.algorithm` URI
  for the negotiated policy: the xmldsig `rsa-sha256` URI for the PKCS#1.5 policies
  and `http://opcfoundation.org/UA/security/rsa-pss-sha2-256` for the PSS policy.
- **FR-003**: The policy→(sign, verify, algorithm-URI) selection MUST be defined in
  one place and used consistently at all three asymmetric-signature sites:
  ServerSignature emission (CreateSession), application ClientSignature verification
  (ActivateSession), and x509 user-identity-token signature verification.
- **FR-004**: For the PSS policy, if the required PSS sign/verify primitive is
  unavailable, the server MUST reject the secured operation (fail closed) rather
  than fall back to PKCS#1.5.
- **FR-005**: The server MUST validate the incoming ClientSignature's `algorithm`
  URI against the negotiated policy and reject a mismatch.
- **FR-006**: Behavior for SecurityPolicy None, `Basic256Sha256`, and
  `Aes128_Sha256_RsaOaep` MUST be unchanged, including the exact ServerSignature
  algorithm URI string and signature bytes on identical inputs.
- **FR-007**: The change MUST NOT alter message layout, chunking, or any non-
  signature wire field; the only wire-visible change is the ServerSignature
  algorithm/bytes and the accepted ClientSignature algorithm for the PSS policy.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target role/profiles unchanged (Nano/Micro/Embedded 2017 + full);
  conformance status remains **profile-targeting**. This feature is a prerequisite
  toward claiming the `Aes256_Sha256_RsaPss` SecurityPolicy and toward external CTT.
- **OPC-002**: Services touched: CreateSession ServerSignature and ActivateSession
  ClientSignature (OPC-10000-4 §5.6.3, §5.7.3, §7.38.2); SecureChannel signature
  algorithms (OPC-10000-6 §6.7). No new service.
- **OPC-003**: A ClientSignature that fails PSS verification, or that declares the
  wrong algorithm for the policy, MUST be rejected with `Bad_SecurityChecksFailed`
  (or the project's existing application-signature failure code).
- **OPC-004**: Wire encoding/transport (OPC-10000-6 Binary, MessageChunk, OPC UA
  TCP) is otherwise unchanged.
- **OPC-005**: SecurityPolicy set is unchanged (None, Basic256Sha256,
  Aes128_Sha256_RsaOaep, Aes256_Sha256_RsaPss). This feature makes the PSS policy's
  signature algorithm conformant to OPC-10000-7. Status: profile-targeting.

### Scope Boundaries *(mandatory)*

- **In Scope**: Policy-aware selection of the RSA signature scheme and algorithm
  URI at the three asymmetric-signature sites, plus algorithm-URI validation of the
  incoming ClientSignature. All backends (host/mbedtls/wolfssl) via the existing
  `rsa_pss_sha256_sign`/`rsa_pss_sha256_verify` adapter hooks.
- **Out of Scope**: Any new SecurityPolicy; symmetric (message) signature/HMAC
  handling; RSA-OAEP encryption padding (already policy-correct); new adapter hooks;
  changes to Basic256Sha256/Aes128 behavior; external CTT execution itself.
- **Compatibility Claim**: After this feature the server may honestly claim the
  `Aes256_Sha256_RsaPss` SecurityPolicy's signature algorithm as conformant to
  OPC-10000-7, still profile-targeting pending CTT.
- **Application Headroom Goal**: Within the existing gates — `.text` ≤ +3%,
  `.data + .bss` ≤ +5%, no new heap. Expected impact is negligible (a small
  selection table/branch; the PSS primitives are already linked).

### Key Entities *(include if feature involves data)*

- **Signature scheme selector**: the mapping from SecurityPolicy id to
  {sign function, verify function, algorithm URI}. Single source of truth.
- **SignatureData.algorithm**: the URI string emitted (ServerSignature) and
  validated (ClientSignature) per policy.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A full `Aes256_Sha256_RsaPss` handshake (OPN → CreateSession →
  ActivateSession → a Read) completes end-to-end with PSS signing/verification.
- **SC-002**: The CreateSession ServerSignature on an `Aes256_Sha256_RsaPss` channel
  carries the PSS algorithm URI and verifies under RSA-PSS-SHA256; on Basic256Sha256
  and Aes128 it carries the xmldsig URI and verifies under PKCS#1.5.
- **SC-003**: 100% of `Aes256_Sha256_RsaPss` ActivateSession requests whose
  ClientSignature is wrong-scheme, wrong-algorithm-URI, or invalid are rejected;
  correctly PSS-signed ones succeed.
- **SC-004**: Basic256Sha256, Aes128_Sha256_RsaOaep, and None handshakes are
  byte-identical to the pre-feature build on the regression fixtures.
- **SC-005**: All existing `ctest`, ASAN/UBSan, and the four ARM profile builds stay
  green; each profile remains within the resource gates.

## Assumptions

- All three shipped backends already implement `rsa_pss_sha256_sign` and
  `rsa_pss_sha256_verify` correctly (added in feature 013); this feature only
  routes to them by policy — it does not implement new crypto.
- The PSS algorithm URI is `http://opcfoundation.org/UA/security/rsa-pss-sha2-256`
  and the PKCS#1.5 URI remains `http://www.w3.org/2001/04/xmldsig-more#rsa-sha256`
  (OPC-10000-7 SecurityPolicy definitions).
- A PSS signature occupies the same byte length as a PKCS#1.5 signature for the
  same RSA key, so existing signature/scratch buffers are sufficient (no resizing).
- The existing secured-handshake tests already cover the `Aes256_Sha256_RsaPss`
  policy (currently passing because sign and verify both used PKCS#1.5); this
  feature switches that policy's path to PSS and updates the test client to sign
  with PSS accordingly.
