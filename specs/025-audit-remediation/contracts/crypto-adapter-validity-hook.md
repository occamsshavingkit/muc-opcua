# Contract: `verify_certificate_validity` crypto-adapter hook

**Finding**: 3 (certificate validity not checked) · **Spec**: FR-003, FR-004 ·
**OPC UA**: OPC-10000-4 §5.5 certificate validation; StatusCode
`Bad_CertificateTimeInvalid`.

## Signature (added to `mu_crypto_adapter_t`, include/muc_opcua/platform.h)

```c
opcua_statuscode_t (*verify_certificate_validity)(void *context,
        const opcua_byte_t *certificate, size_t length);
```

## Semantics

| Condition | Required return |
|-----------|-----------------|
| Current time within `[notBefore, notAfter]` of the DER cert | `MU_STATUS_GOOD` |
| Current time before `notBefore` or after `notAfter` | `MU_STATUS_BAD_CERTIFICATETIMEINVALID` |
| Certificate unparseable | `MU_STATUS_BAD_CERTIFICATEINVALID` |
| `certificate == NULL` or `length == 0` | `MU_STATUS_BAD_CERTIFICATEINVALID` |

## Caller contract (`mu_certificate_validate`, src/security/certificate.c)

1. For `policy == None` → return `Good` (no cert), unchanged.
2. For any secured policy:
   - If `crypto->verify_certificate_validity == NULL` → **fail closed**: return
     `MU_STATUS_BAD_CERTIFICATEINVALID` (do not skip the check).
   - Else call it; propagate a non-`Good` result.
3. The existing key-size / parse checks remain and run in addition.

## Backend obligations

- **mbedtls**: parse with `mbedtls_x509_crt_parse_der`, compare
  `valid_from`/`valid_to` against the adapter time source.
- **wolfssl**: use `wc_GetDateInfo` / cert date fields.
- **host (OpenSSL)**: `X509_cmp_time` against `notBefore`/`notAfter`.
- All three MUST use the same clock the server uses for timeouts (adapter time),
  not wall-clock divergence.

## Tests (RED first)

- Expired cert (notAfter in the past) → `Bad_CertificateTimeInvalid`.
- Not-yet-valid cert (notBefore in the future) → `Bad_CertificateTimeInvalid`.
- In-window cert → `Good`.
- Adapter with `verify_certificate_validity == NULL` on a secured policy →
  connection rejected (fail-closed).
