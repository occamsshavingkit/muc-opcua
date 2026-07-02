# Phase 1 Data Model: SecurityPolicy-aware RSA-PSS Signatures

No runtime data structures change. The "entities" are the selector functions and
the policy metadata they read.

## 1. SecurityPolicy signature metadata (src/security/security_policy.c/.h) — EXTENDED

Extend the existing policy table / accessors with the asymmetric signature URI, and
add a PSS predicate:

```c
/* The SignatureData.algorithm URI for the policy's asymmetric signatures, or NULL
   for None/Invalid. OPC-10000-7 SecurityPolicy definitions. */
const char *mu_security_policy_asym_signature_uri(mu_security_policy_id_t policy);

/* True iff the policy's asymmetric signatures use RSA-PSS-SHA256
   (Aes256_Sha256_RsaPss); false for the PKCS#1.5 policies and None. */
opcua_boolean_t mu_security_policy_uses_pss(mu_security_policy_id_t policy);
```

- **Invariants**: `Basic256Sha256`, `Aes128_Sha256_RsaOaep` → xmldsig URI, `uses_pss=false`;
  `Aes256_Sha256_RsaPss` → PSS URI, `uses_pss=true`; None/Invalid → URI NULL,
  `uses_pss=false`.
- **Consumers**: the wrappers below and the ClientSignature URI validation.

## 2. Asymmetric-signature wrappers (src/security/certificate.c/.h) — NEW

```c
/* Sign `data` with the policy's asymmetric signature scheme. Fails closed
   (Bad_SecurityChecksFailed) if the required primitive is absent. */
opcua_statuscode_t mu_asym_signature_sign(const mu_crypto_adapter_t *crypto,
                                          mu_security_policy_id_t policy,
                                          const opcua_byte_t *data, size_t data_len,
                                          opcua_byte_t *signature, size_t *signature_len);

/* Verify `signature` over `data` using `certificate` under the policy's scheme.
   Fails closed if the required primitive is absent. */
opcua_statuscode_t mu_asym_signature_verify(const mu_crypto_adapter_t *crypto,
                                            mu_security_policy_id_t policy,
                                            const opcua_byte_t *certificate, size_t certificate_len,
                                            const opcua_byte_t *data, size_t data_len,
                                            const opcua_byte_t *signature, size_t signature_len);
```

- **Dispatch**: `mu_security_policy_uses_pss(policy)` → `rsa_pss_sha256_*` else
  `rsa_sha256_*`.
- **Fail-closed**: NULL crypto adapter or NULL required fn → `Bad_SecurityChecksFailed`
  (never a silent PKCS#1.5 fallback for a PSS policy).
- **Signatures match the existing adapter hooks** so no argument-shape surprises:
  the sign/verify hooks already take exactly these parameters.

## 3. Call-site changes (src/core/service_dispatch.c) — LOGIC ONLY, no struct change

- ServerSignature emit (~L851): `cr->rsa_sha256_sign(...)` + hardcoded `SIG_URI` →
  `mu_asym_signature_sign(cr, policy, ...)` + `mu_security_policy_asym_signature_uri(policy)`.
- ClientSignature verify (~L402, `verify_activate_client_signature`): add
  algorithm-URI validation vs policy; `rsa_sha256_verify(...)` →
  `mu_asym_signature_verify(cr, policy, ...)`.
- x509 user-token verify (~L1171): `rsa_sha256_verify(...)` →
  `mu_asym_signature_verify(cr, policy, ...)`.
- No buffer sizes change (`sig[512]`, `verify_buf[1536]`, `to_sign[1536]` unchanged).

## 4. Test data (tests/integration/test_secure_handshake_modern.c) — CHANGED

- The `Aes256_Sha256_RsaPss` client path signs the ClientSignature with
  `rsa_pss_sha256_sign` and expects the PSS ServerSignature algorithm URI.
- New negative case: PSS channel + PKCS#1.5-algorithm ClientSignature → rejected.
