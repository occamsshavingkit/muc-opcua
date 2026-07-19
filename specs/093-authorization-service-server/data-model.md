# Data Model: Authorization Service Server Facet

## Entities

### mu_jwt_config_t

Server-level JWT configuration, embedded in `mu_server_config_t`.

| Field | Type | Description |
|-------|------|-------------|
| `issuer_count` | `uint8_t` | Number of trusted issuers (0 = JWT disabled) |
| `issuers` | `const mu_jwt_issuer_t *` | Array of trusted issuer records |

### mu_jwt_issuer_t

A single trusted OAuth2 issuer configuration.

| Field | Type | Description |
|-------|------|-------------|
| `issuer_url` | `const char *` | The `iss` claim value to match (e.g., "https://auth.example.com") |
| `public_key` | `const uint8_t *` | DER-encoded public key (RSA or EC) |
| `public_key_len` | `size_t` | Length of `public_key` |
| `expected_audience` | `const char *` | The `aud` claim value to match |
| `clock_skew_seconds` | `uint32_t` | Acceptable clock skew for `exp`/`nbf` validation |
| `alg` | `mu_jwt_alg_t` | Expected signing algorithm (RS256, RS384, RS512, ES256, …) |

### mu_jwt_alg_t (enum)

| Value | Description |
|-------|-------------|
| `MU_JWT_ALG_RS256` | RSA PKCS#1 v1.5 with SHA-256 |
| `MU_JWT_ALG_RS384` | RSA PKCS#1 v1.5 with SHA-384 |
| `MU_JWT_ALG_RS512` | RSA PKCS#1 v1.5 with SHA-512 |
| `MU_JWT_ALG_ES256` | ECDSA P-256 with SHA-256 |
| `MU_JWT_ALG_ES384` | ECDSA P-384 with SHA-384 |
| `MU_JWT_ALG_ES512` | ECDSA P-521 with SHA-512 |

### mu_jwt_claims_t

Extracted claims from a validated JWT.

| Field | Type | Description |
|-------|------|-------------|
| `sub` | `char[128]` | Subject (user identity) |
| `iss` | `char[128]` | Issuer URL |
| `aud` | `char[128]` | Audience |
| `exp` | `int64_t` | Expiration time (Unix timestamp) |
| `nbf` | `int64_t` | Not-before time (0 if absent) |
| `iat` | `int64_t` | Issued-at time (0 if absent) |

### mu_jwt_result_t (enum)

| Value | Description |
|-------|-------------|
| `MU_JWT_OK` | Token valid |
| `MU_JWT_ERR_MALFORMED` | Not three dot-separated segments |
| `MU_JWT_ERR_BASE64` | Invalid Base64url encoding |
| `MU_JWT_ERR_SIGNATURE` | Signature verification failed |
| `MU_JWT_ERR_EXPIRED` | `exp` claim is in the past |
| `MU_JWT_ERR_NOT_YET_VALID` | `nbf` claim is in the future |
| `MU_JWT_ERR_ISSUER` | `iss` claim doesn't match any trusted issuer |
| `MU_JWT_ERR_AUDIENCE` | `aud` claim doesn't match expected audience |
| `MU_JWT_ERR_NO_SUB` | `sub` claim missing or empty |
| `MU_JWT_ERR_UNSUPPORTED_ALG` | Signing algorithm not supported |
| `MU_JWT_ERR_NO_CONFIGURED_ISSUERS` | JWT feature enabled but no issuers configured |

## Kconfig Symbols

| Symbol | CU | Default | Depends |
|--------|-----|---------|---------|
| `MUC_OPCUA_CU_USER_TOKEN_JWT` | 1697 | `y` (full) | `MUC_OPCUA_CU_USER_AUTH` |
| `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` | 1629 | `y` (full) | `MUC_OPCUA_CU_USER_TOKEN_JWT && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION` |

## File Map

| File | Purpose |
|------|---------|
| `src/cu/core_2022_server/authorization/jwt.h` | Public JWT API (validate, claim accessors) |
| `src/cu/core_2022_server/authorization/jwt.c` | JWT parser, scanner, validator |
| `src/cu/core_2022_server/authorization/base64url.c` | Base64url decode (no padding variant) |
| `src/core/service_dispatch/activate_session.c` | Hook JWT validation into user token dispatch |
| `include/muc_opcua/server.h` | Add `mu_jwt_config_t` to server config |
| `Kconfig` | Add `MUC_OPCUA_CU_USER_TOKEN_JWT`, `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` |
| `tests/unit/test_jwt.c` | JWT parser unit tests |
| `tests/integration/test_jwt_activate_session.c` | ActivateSession with JWT integration test |
