# Research: Authorization Service Server Facet

## R1: JWT Library Selection

**Decision**: Write a minimal, freestanding JWT parser in `src/cu/core_2022_server/authorization/jwt.c` — not link an external library.

**Rationale**: The server only needs JWT *validation* (signature verify, claims check), not token creation or JWKS fetching. A minimal parser for the three-segment JWS compact format with Base64url decoding, JSON claim extraction via a small streaming key-value scanner, and RSA/ECDSA signature verification calling the existing platform crypto adapter is ~300-400 lines of C. Bringing in libjansson or cJSON would violate the embedded-first principle (Principle II) and add ~15-20 KB of code for a task that needs a handful of string fields. The platform crypto adapter already provides `mbedtls_pk_verify()` / `EVP_DigestVerify()` / equivalent — the JWT layer just needs to reconstruct the signing input buffer.

**Alternatives considered**:
- cJSON + mbedtls — cJSON is already in the codebase for PubSub JSON, but requires heap and is ~8 KB. JWT claims are flat and few (6-8 fields), making a dedicated scanner leaner.
- Full libjwt / cjose — production-quality but ~50-100 KB, heap-heavy, not freestanding.
- Reuse the existing OPC UA Binary reader — not applicable (JWT is text/Base64url, not OPC UA Binary).

## R2: Key Material Configuration

**Decision**: Trusted issuer public keys are provided as PEM strings or raw DER blobs in the server config structure, parsed at init time into the platform crypto adapter's internal key representation.

**Rationale**: The OPC UA `AuthorizationServiceConfigurationType` stores keys as ByteStrings (DER). The integrator provides the public key — no filesystem dependency, no TLS/X.509 trust chain validation, no dynamic JWKS fetching. The server init function calls `mu_jwt_load_key()` which wraps the crypto adapter's key import. RSA public keys are ~200 bytes DER; ECDSA public keys are ~90 bytes. Both fit in static buffers.

**Alternatives considered**:
- JWKS URI auto-fetch — requires HTTP client, TLS, DNS; violates embedded-first.
- X.509 certificate chain validation — overkill for a Resource Server; the Authorization Server's public key is directly trusted.
- Raw modulus+exponent (JWK format) — more fields to parse vs PEM/DER which the crypto adapter already handles.

## R3: Claims Processing

**Decision**: Extract claims via a minimal streaming scanner that walks the JWT payload JSON looking for specific known keys (`iss`, `sub`, `aud`, `exp`, `nbf`, `iat`). Unknown claims are skipped. No full JSON parser.

**Rationale**: The JWT payload is a flat JSON object with ~6-8 known keys. A dedicated scanner that finds `"keyname":<value>` with basic string/number/boolean value handling is ~100 lines of C and avoids heap allocation. Claim values are stored in fixed-size stack buffers (e.g., 128-byte strings, 64-bit integers). The OPC UA role claims extension (if present) uses a simple array of NodeId strings that can be parsed with the same scanner.

**Alternatives considered**:
- Full JSON parser — unnecessary for flat payload.
- Fixed-offset extraction — fragile; claim order varies by issuer.

## R4: ActivateSession Integration

**Decision**: Hook into the existing `handle_activate_session` user token dispatch. When a `UserIdentityToken` with `tokenType` URI matching `urn:ietf:params:oauth:token-type:jwt` is presented, extract the raw JWT string from `tokenData`, call `mu_jwt_validate()`, and if valid, extract `sub` as the user identity.

**Rationale**: The existing ActivateSession handler already dispatches by user token type (Anonymous, UserName, X509). Adding a JWT case in the same switch block is the natural extension. The JWT token body is a simple ByteString (the raw three-segment JWT). No new service request structures needed.

## R5: Kconfig and Feature Gating

**Decision**: Two new Kconfig symbols under the `MUC_OPCUA_FACET_CORE_2022_SERVER` group:
- `MUC_OPCUA_CU_USER_TOKEN_JWT` (CU 1697) — enables JWT token validation at ActivateSession
- `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` (CU 1629) — enables the AuthorizationServiceConfigurationType address-space nodes

`CU_AUTHORIZATION_SERVICE_SERVER` depends on `CU_USER_TOKEN_JWT`. When `CU_USER_TOKEN_JWT` is undefined, JWT tokens at ActivateSession return `Bad_IdentityTokenRejected`. When defined but no issuer keys are configured, same behavior.

Default `y` for full profile, `n` for all others.

**Rationale**: Separating token validation (CU 1697) from configuration exposure (CU 1629) allows integrators who only need JWT auth without the address-space overhead. This matches the OPC UA spec's CU separation.

## R6: Crypto Algorithm Support

**Decision**: Minimum supported algorithm is RS256 (RSA PKCS#1 v1.5 SHA-256). RS384, RS512, ES256, ES384, ES512 are compile-time optional via Kconfig `MUC_OPCUA_JWT_ALG_RS384`, etc.

**Rationale**: RS256 is the most widely deployed JWT signing algorithm and the mandatory-to-implement algorithm per RFC 7518 §3.1. RSA signature verification is already available in all three platform crypto backends (mbedTLS, OpenSSL, wolfSSL). ECDSA support (ES256) depends on `MUC_OPCUA_CU_SECURITY_ECC` being enabled.

## Validation Strategy

- **Unit tests**: JWT parser/scanner (Base64url decode, claim extraction), `mu_jwt_validate()` with static RSA key pairs (valid, expired, wrong key, wrong audience, wrong issuer, no signature, malformed).
- **Integration tests**: ActivateSession with JWT UserIdentityToken against a real server instance with a pre-configured trusted key.
- **Negative tests**: Every rejection path returns the correct StatusCode.
- **Compile-out tests**: Nano/micro profiles verify zero growth when JWT symbols are undefined.
