# Conformance: User Token — JWT Server Facet (spec 093)

This server implements the OPC UA **User Token — JWT Server Facet**
(OPC-10000-7 PG18, CU 1697) — validation of OAuth2 / OpenID Connect JWT
bearer tokens presented as the `IssuedIdentityToken` body during
ActivateSession. Gated behind **`MUC_OPCUA_CU_USER_TOKEN_JWT`** (default
**ON** for the Full profile; depends on `MUC_OPCUA_CU_USER_AUTH`).

The server acts as an OAuth2 **Resource Server**: it validates JWT signatures
against caller-configured trusted issuers, enforces `exp` / `nbf` / `iss` /
`aud` / `sub` claims, and uses the `sub` claim as the session user identity.
It does **not** issue tokens, refresh tokens, or call the Authorization
Server's introspection endpoint.

Grounded against:

- OPC-10000-4 §5.7.3 (ActivateSession UserIdentityToken dispatch, Table 41)
- OPC-10000-4 §7.38.2 (`Bad_IdentityTokenInvalid` / `Bad_IdentityTokenRejected`)
- OPC-10000-6 §5.2.3 (IssuedIdentityToken encoding — JWT carried as a
  ByteString in `tokenData`)
- OPC-10000-7 v1.05.02 CU 1697 (User Token — JWT Server Facet)
- RFC 7519 (JWT), RFC 7515 (JWS), RFC 7518 §3.1 (alg registry), RFC 8725 §3
  (JWT BCP)

## Supported Algorithms

| Alg | JWS | Hash | Gating Kconfig | Backend |
|---|---|---|---|---|
| RS256 | RSASSA-PKCS1-v1_5 | SHA-256 | always under `MUC_OPCUA_CU_USER_TOKEN_JWT` | platform crypto adapter |
| RS384 | RSASSA-PKCS1-v1_5 | SHA-384 | always | platform crypto adapter |
| RS512 | RSASSA-PKCS1-v1_5 | SHA-512 | always | platform crypto adapter |
| ES256 | ECDSA P-256 | SHA-256 | `MUC_OPCUA_CU_SECURITY_ECC` | platform crypto adapter |
| ES384 | ECDSA P-384 | SHA-384 | `MUC_OPCUA_CU_SECURITY_ECC` | platform crypto adapter |
| ES512 | ECDSA P-521 | SHA-512 | `MUC_OPCUA_CU_SECURITY_ECC` | platform crypto adapter |

The HMAC family (HS256/HS384/HS512) and the `none` algorithm are explicitly
rejected (RFC 8725 §3.1).

## Validation Pipeline

`mu_jwt_validate()` runs the following stages in order; the first failure
short-circuits with the listed result code.

| Stage | Check | Failure Code |
|---|---|---|
| 1 | Token has exactly three dot-separated segments | `MU_JWT_ERR_MALFORMED` |
| 2 | At least one trusted issuer configured | `MU_JWT_ERR_NO_CONFIGURED_ISSUERS` |
| 3 | Header Base64url-decodes and `alg` is recognized | `MU_JWT_ERR_BASE64` / `MU_JWT_ERR_UNSUPPORTED_ALG` |
| 4 | Payload Base64url-decodes; scanner extracts `iss`/`sub`/`aud`/`exp`/`nbf`/`iat` | `MU_JWT_ERR_BASE64` |
| 5 | `exp` claim present and not in the past (per-issuer skew) | `MU_JWT_ERR_EXPIRED` |
| 6 | `iss` matches a configured trusted issuer | `MU_JWT_ERR_ISSUER` |
| 7 | `nbf` not in the future (per-issuer skew) | `MU_JWT_ERR_NOT_YET_VALID` |
| 8 | `aud` matches the issuer's `expected_audience` | `MU_JWT_ERR_AUDIENCE` |
| 9 | `sub` non-empty (overlong subjects are rejected) | `MU_JWT_ERR_NO_SUB` |
| 10 | `alg` matches the issuer's configured algorithm | `MU_JWT_ERR_UNSUPPORTED_ALG` |
| 11 | Signature verifies against the issuer's `public_key` | `MU_JWT_ERR_SIGNATURE` |

A token is accepted iff all eleven stages pass; `out_claims` is then filled
in with the extracted values.

## ActivateSession Integration

The hook in `src/core/service_dispatch/activate_session.c` dispatches by
UserToken Policy `tokenType` URI:

| `tokenType` URI | Handling |
|---|---|
| `urn:ietf:params:oauth:token-type:jwt` | JWT path: extract raw JWT from `tokenData`, call `mu_jwt_validate()`, copy `sub` to `slot->user_identity` |
| (any other) | Existing UserName / X509 / Anonymous dispatch is unchanged |

The ActivateSession result codes are mapped per spec 093 FR-007/FR-008 and
OPC-10000-4 §7.38.2:

| JWT result | ActivateSession result |
|---|---|
| `MU_JWT_OK` | `Good` |
| `MU_JWT_ERR_NO_CONFIGURED_ISSUERS` | `Bad_IdentityTokenRejected` (server config fault) |
| All other `MU_JWT_ERR_*` | `Bad_IdentityTokenInvalid` |

JWT bearer tokens MUST NOT be accepted over SecurityPolicy#None
(spec 093 FR-008 / OPC-10000-4 §7.40.2.1); a JWT presented on an
unencrypted channel is rejected with `Bad_IdentityTokenRejected`.

## Configuration

The trusted-issuer table lives in `mu_server_config_t.jwt`:

```c
static const mu_jwt_issuer_t s_issuers[] = {
    {
        .issuer_url = "https://auth.example.com",
        .public_key = issuer_der_ptr,
        .public_key_len = issuer_der_len,
        .expected_audience = "opcua-server",
        .clock_skew_seconds = 30,
        .alg = MU_JWT_ALG_RS256,
    },
    /* ... up to 255 issuers ... */
};

mu_server_config_t cfg = { ... };
cfg.jwt.issuers = s_issuers;
cfg.jwt.issuer_count = sizeof(s_issuers) / sizeof(s_issuers[0]);
```

When `cfg.jwt.issuer_count == 0`, JWT authentication is disabled and any JWT
UserIdentityToken is rejected with `Bad_IdentityTokenRejected` per spec 093
FR-008.

## Backing Tests

| Claim | Test |
|---|---|
| Valid RS256 token accepted, claims extracted | `test_jwt::test_jwt_valid_rs256_token_is_accepted_and_claims_extracted` |
| Expired token rejected | `test_jwt::test_jwt_expired_token_is_rejected` |
| Wrong signing key rejected | `test_jwt::test_jwt_signed_with_wrong_key_is_rejected` |
| Wrong issuer / audience / missing sub rejected | `test_jwt::test_jwt_wrong_{issuer,audience}_is_rejected`, `test_jwt_missing_sub_is_rejected` |
| Malformed / bad Base64 / unsupported alg rejected | `test_jwt::test_jwt_{not_three_segments,bad_base64,unsupported_alg}_is_rejected` |
| nbf future / no exp / no configured issuers rejected | `test_jwt::test_jwt_{nbf_in_future,no_exp,no_configured_issuers}_is_rejected` |
| `sub` boundary conditions (empty, 127 chars, overlong, escapes) | `test_jwt_claims` |
| Multi-issuer trust, cross-issuer audience mismatch | `test_jwt_multi_issuer` |
| Per-issuer clock-skew boundary | `test_jwt_clock_skew` |
| End-to-end ActivateSession with JWT | `test_jwt_activate_session` |

## Build Gating

```kconfig
config MUC_OPCUA_CU_USER_TOKEN_JWT
    bool "User Token JWT"
    depends on MUC_OPCUA_CU_USER_AUTH
    default y if MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES
```

When undefined, a JWT UserIdentityToken at ActivateSession returns
`Bad_IdentityTokenRejected` (spec 093 FR-008) and no JWT code is linked.

## Crypto Backends

The validator calls `mu_crypto_jwt_verify()` in
`src/cu/core_2022_server/authorization/crypto_jwt.c`, which dispatches to:

- OpenSSL (`EVP_DigestVerify`) when `MUC_OPCUA_HAVE_OPENSSL` is defined.
- mbedTLS (`mbedtls_pk_verify`) when `MUC_OPCUA_HAVE_MBEDTLS` is defined.
- wolfSSL (`wc_SignatureVerify`) when `MUC_OPCUA_HAVE_WOLFSSL` is defined.

ECDSA signatures are converted from the raw r‖s JWS format (RFC 7518 §3.4) to
the DER-encoded `SEQUENCE { INTEGER r, INTEGER s }` form expected by all three
backends before verification.
