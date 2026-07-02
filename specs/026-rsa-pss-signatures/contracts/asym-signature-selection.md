# Contract: Policy-keyed asymmetric signature selection

**Feature**: 026 · **FRs**: FR-001..FR-007 · **OPC UA**: OPC-10000-7 SecurityPolicy
definitions; OPC-10000-4 §5.6.3/§5.7.3/§7.38.2; OPC-10000-6 §6.7.

## Selection table (single source of truth)

| SecurityPolicy | Signature scheme | Adapter fn (sign / verify) | `SignatureData.algorithm` URI |
|----------------|------------------|----------------------------|-------------------------------|
| None | (none) | — | — (no signature emitted/required) |
| Basic256Sha256 | RSA-PKCS#1.5-SHA256 | `rsa_sha256_sign` / `rsa_sha256_verify` | `http://www.w3.org/2001/04/xmldsig-more#rsa-sha256` |
| Aes128_Sha256_RsaOaep | RSA-PKCS#1.5-SHA256 | `rsa_sha256_sign` / `rsa_sha256_verify` | `http://www.w3.org/2001/04/xmldsig-more#rsa-sha256` |
| Aes256_Sha256_RsaPss | RSA-PSS-SHA256 | `rsa_pss_sha256_sign` / `rsa_pss_sha256_verify` | `http://opcfoundation.org/UA/security/rsa-pss-sha2-256` |

## Wrapper semantics

`mu_asym_signature_sign` / `mu_asym_signature_verify`:

| Condition | Required result |
|-----------|-----------------|
| Policy resolves to a scheme and the matching adapter fn is present | dispatch to that fn; return its status |
| Policy is a secured policy but the required adapter fn (or adapter) is NULL | `MU_STATUS_BAD_SECURITYCHECKSFAILED` (fail closed — **never** silently use PKCS#1.5 for a PSS policy) |
| Policy is None/Invalid | callers do not invoke these (no signature); if invoked, return `MU_STATUS_BAD_SECURITYCHECKSFAILED` |

## ServerSignature emission (CreateSession)

- Sign `(clientCertificate ‖ clientNonce)` via `mu_asym_signature_sign(cr, channelPolicy, ...)`.
- Write `SignatureData.algorithm = mu_security_policy_asym_signature_uri(channelPolicy)`
  and the signature bytes. None → null/null signature (unchanged).

## ClientSignature verification (ActivateSession)

1. Decoded `algorithm` URI MUST exactly equal
   `mu_security_policy_asym_signature_uri(channelPolicy)` (length + bytes); mismatch →
   `Bad_SecurityChecksFailed`.
2. Verify `(serverCertificate ‖ serverNonce)` via
   `mu_asym_signature_verify(cr, channelPolicy, channelClientCert, ...)`.

## x509 user-identity-token signature verification

- Verify via `mu_asym_signature_verify(cr, channelPolicy, userCert, ...)` (see
  research Decision 4 for the channel-policy boundary note).

## Tests (RED first)

| Case | Expected |
|------|----------|
| Aes256_Sha256_RsaPss full handshake, client signs with PSS | succeeds; ServerSignature carries the PSS URI and verifies under PSS |
| Aes256_Sha256_RsaPss, ClientSignature carries PKCS#1.5 URI | rejected (`Bad_SecurityChecksFailed`) |
| Aes256_Sha256_RsaPss, ClientSignature PKCS#1.5 bytes | rejected |
| Basic256Sha256 / Aes128 handshake | byte-identical to pre-feature (xmldsig URI, PKCS#1.5) |
| SecurityPolicy None | no signature; unchanged |
| Secured policy, adapter lacks the required sign/verify fn | fail closed (rejected) |
