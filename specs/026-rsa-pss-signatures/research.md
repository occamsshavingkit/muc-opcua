# Phase 0 Research: SecurityPolicy-aware RSA-PSS Signatures

No `[NEEDS CLARIFICATION]` remained after specify. The open questions are design
decisions about how to route the signature scheme by policy within the
constitution's constraints.

## Decision 1 — Centralize scheme selection in adapter-aware wrappers

**Decision**: Add `mu_asym_signature_sign(cr, policy, data, len, sig, &sig_len)` and
`mu_asym_signature_verify(cr, policy, cert, cert_len, data, len, sig, sig_len)` in
`certificate.c`. Each inspects the policy: for `Aes256_Sha256_RsaPss` it calls
`cr->rsa_pss_sha256_sign/verify`; for `Basic256Sha256`/`Aes128_Sha256_RsaOaep` it
calls `cr->rsa_sha256_sign/verify`. Also add `mu_security_policy_asym_signature_uri(policy)`
in `security_policy.c` returning the matching URI.

**Rationale**: One source of truth (FR-003) — the three call sites become
`mu_asym_signature_sign(...)` / `_verify(...)` with no per-site ternary, and the
fail-closed rule (FR-004) lives in exactly one spot. The wrappers belong in
`certificate.c` because that unit already takes `const mu_crypto_adapter_t *`; the
pure URI/metadata belongs in `security_policy.c` next to the existing policy table.

**Alternatives considered**:
- *Inline ternary at each of the 3 sites*: rejected — triplicates the
  scheme+URI+fail-closed logic; drift risk (the exact bug this feature fixes).
- *Add a new `asym_signature.c` unit*: viable but adds a translation unit for two
  small functions; deferred unless the wrappers grow.

## Decision 2 — Algorithm URIs are exact constants keyed by policy

**Decision**: PKCS#1.5 policies → `http://www.w3.org/2001/04/xmldsig-more#rsa-sha256`
(unchanged); `Aes256_Sha256_RsaPss` → `http://opcfoundation.org/UA/security/rsa-pss-sha2-256`;
None/Invalid → NULL (no signature). The accessor returns the string; callers use its
`strlen` for the SignatureData length.

**Rationale**: These are the OPC-10000-7 SecurityPolicy-defined URIs. Emitting the
correct URI is required for a conformant peer to accept the ServerSignature.

**Alternatives considered**: none — the URIs are normative.

## Decision 3 — Validate the incoming ClientSignature algorithm URI against the policy

**Decision**: In ActivateSession, before verifying, compare the decoded
`ClientSignature.algorithm` (already read as `algorithm`) to
`mu_security_policy_asym_signature_uri(channel policy)` (exact length + bytes).
Mismatch → reject (`Bad_SecurityChecksFailed`). An empty/absent algorithm on a
secured policy is likewise rejected (existing empty-signature rejection already
covers the empty case).

**Rationale**: Closes an algorithm-confusion gap and yields a clear rejection
rather than relying solely on the crypto verify failing. Exact comparison avoids
OOB reads (length checked first).

**Alternatives considered**:
- *Skip URI validation, rely on crypto verify to fail*: rejected — weaker and
  less diagnostic; the spec (FR-005) calls for explicit validation.

## Decision 4 — User-identity-token (x509) signature uses the channel policy selector, with a noted boundary

**Decision**: Route the x509 user-token signature verify (site 3) through the same
`mu_asym_signature_verify(cr, policy, ...)` using the **SecureChannel** policy, matching
how the current code already operates in the channel's security context.

**Rationale**: Keeps all three sites consistent and fixes the PSS case for the
common deployment where the UserTokenPolicy's SecurityPolicy matches the channel's.
Strictly, OPC-10000-4 keys the UserIdentityToken signature off the
`UserTokenPolicy.securityPolicyUri` advertised for that token, which *may* differ
from the channel policy. This server advertises user-token policies aligned with
the channel, so channel-policy selection is correct here.

**Boundary noted**: if a future feature advertises a user-token policy whose
SecurityPolicy differs from the channel, site 3 must key off the UserTokenPolicy
URI instead. Out of scope now; recorded so it isn't silently wrong later.

## Decision 5 — Test approach: switch the PSS handshake to real PSS, add a mismatch test

**Decision**: The existing `Aes256_Sha256_RsaPss` handshake test currently passes
because both sides used PKCS#1.5. After the server switches to PSS for that policy,
the test client must sign the ClientSignature with `rsa_pss_sha256_sign` and expect
the PSS ServerSignature. Add a negative test: on the PSS channel, a ClientSignature
carrying the PKCS#1.5 algorithm URI (or PKCS#1.5 bytes) is rejected. Basic256Sha256
and Aes128 tests are unchanged and act as the byte-identical regression guard.

**Rationale**: Test-first per Constitution IV; the negative test locks FR-005.

**Alternatives considered**: none.
