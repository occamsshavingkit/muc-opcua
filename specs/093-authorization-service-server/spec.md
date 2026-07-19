# Feature Specification: Authorization Service Server Facet

**Feature Branch**: `093-authorization-service-server`  
**Created**: 2026-07-19  
**Status**: Draft  
**Input**: Authorization Service Server Facet — OAuth2/JWT access token support for OPC UA ActivateSession. Required for v1.05.02 profile compliance (Standard/Full). Enables the server to act as an OAuth2 Resource Server, validating JWT bearer tokens presented by clients during session activation.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — JWT Bearer Token Session Activation (Priority: P1)

An OPC UA client with a valid JWT access token (obtained from an external OAuth2 Authorization Server) connects to the server and calls ActivateSession with the JWT as the user token. The server validates the JWT signature, issuer, audience, and expiry, then establishes the session with the claims-identified user.

**Why this priority**: This is the core functionality — without it, the Authorization Service facet has no value. Every other story depends on successful JWT validation.

**Independent Test**: Configure the server with a known good RSA public key, send an ActivateSession with a valid signed JWT, verify session activates with Good status. Send with an expired JWT, verify Bad_IdentityTokenInvalid. Can be tested with a scripted JWT or a statically-keyed test JWT.

**Acceptance Scenarios**:

1. **Given** the server has a configured trusted issuer public key, **When** a client sends ActivateSession with a valid JWT (correct signature, not expired, matching issuer/audience), **Then** the session is activated with StatusCode Good and the user identity matches the JWT claims (subject + roles).
2. **Given** the server has a configured trusted issuer public key, **When** a client sends ActivateSession with an expired JWT, **Then** the response is Bad_IdentityTokenInvalid.
3. **Given** the server has a configured trusted issuer public key, **When** a client sends ActivateSession with a JWT signed by an untrusted key, **Then** the response is Bad_IdentityTokenInvalid.
4. **Given** the server has a configured trusted issuer, **When** a client sends ActivateSession with a valid JWT whose `aud` claim does not match the server's expected audience, **Then** the response is Bad_IdentityTokenInvalid.

---

### User Story 2 — Multiple Issuer Trust Configuration (Priority: P2)

An integrator configures the server with one or more trusted OAuth2 issuers. Each issuer record includes: the issuer URL, the JWKS URI or a pinned public key, the expected audience value, and an optional clock skew tolerance. The server fetches or loads issuer keys at startup and can refresh them.

**Why this priority**: Production deployments need at least one configurable issuer. Multi-issuer support is standard for OAuth2 and matches the OPC UA AuthorizationServiceConfiguration type.

**Independent Test**: Configure the server with two different issuer records, test that ActivateSession accepts a JWT from either issuer with the correct audience per issuer.

**Acceptance Scenarios**:

1. **Given** the server is configured with issuer A (key K_A, audience "opcua-server") and issuer B (key K_B, audience "opcua-server"), **When** a client sends ActivateSession with a JWT signed by K_A with audience "opcua-server", **Then** session activates successfully.
2. **Given** the configuration above, **When** a client sends ActivateSession with a JWT signed by K_B with audience "other-app", **Then** the response is Bad_IdentityTokenInvalid (audience mismatch for issuer B).
3. **Given** the server has a configured issuer, **When** the issuer's key is updated (rotation), **Then** the server can validate tokens signed by the new key without restart.

---

### User Story 3 — JWT Claims to User Identity Mapping (Priority: P3)

The server extracts user identity and role information from validated JWT claims. The `sub` claim becomes the user identity. Standard OPC UA role claims (if present) are mapped to the server's RoleSet to grant access to role-restricted nodes.

**Why this priority**: Identity mapping is needed for authorization enforcement, but the role subset can ship after basic token validation.

**Independent Test**: Send a JWT with `sub: "operator1"` and a custom claim containing role NodeIds, verify the session reports the correct user identity and the roles are visible in the session diagnostics.

**Acceptance Scenarios**:

1. **Given** a validated JWT with `sub: "operator1"`, **When** the session is activated, **Then** the session's user identity reports "operator1".
2. **Given** a validated JWT with no `sub` claim, **When** the session is activated, **Then** the response is Bad_IdentityTokenInvalid.

---

### Edge Cases

- JWT without `exp` claim → rejected (Bad_IdentityTokenInvalid)
- JWT with `nbf` claim in the future → rejected
- JWT with an unsupported signing algorithm (e.g., HS256 when only RS256 is configured) → rejected
- Malformed JWT (not three dot-separated segments) → rejected
- JWT with a key ID (`kid`) in the header that doesn't match any known key → rejected
- Server configured with zero trusted issuers → JWT authentication disabled entirely (falls back to existing user token types)
- Very large JWT (approaching or exceeding configured max token size) → rejected with clear diagnostic

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Server MUST support the OAuth2 Resource Server role as defined by OPC-10000-7 §6.6 (Authorization Service Server Facet, CU 1629).
- **FR-002**: Server MUST validate JWT access tokens during ActivateSession when a JWT UserIdentityToken is presented (OPC-10000-4 §5.7.3, OPC-10000-7 CU 1697).
- **FR-003**: JWT validation MUST check: token structure (header.payload.signature), signature against the trusted issuer's public key, expiration (`exp`) claim, not-before (`nbf`) claim if present, issuer (`iss`) claim against configured trusted issuers, and audience (`aud`) claim against the configured audience.
- **FR-004**: Supported JWT signing algorithms MUST include at minimum RS256 (RSA PKCS#1 v1.5 with SHA-256). RS384, RS512, ES256, ES384, and ES512 are optional extensions.
- **FR-005**: Server MUST support configuration of one or more trusted OAuth2 issuers via the `mu_server_config_t`, each with: issuer URL, JWKS URI or pinned public key(s), expected audience, and optional clock skew tolerance.
- **FR-006**: The `sub` (subject) claim from a validated JWT MUST be used as the user identity for audit and authorization purposes.
- **FR-007**: JWT token validation failure at ActivateSession MUST return Bad_IdentityTokenInvalid (OPC-10000-4 §5.7.3, OPC-10000-7 Table 137a).
- **FR-008**: Server MUST gate the JWT token path on the `MUC_OPCUA_CU_USER_TOKEN_JWT` Kconfig symbol. When the symbol is undefined, JWT UserIdentityToken at ActivateSession MUST return Bad_IdentityTokenRejected.
- **FR-009**: Server MUST gate the Authorization Service configuration objects on `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER`. When undefined but `MUC_OPCUA_CU_USER_TOKEN_JWT` is defined, JWT validation still works but no AuthorizationServiceConfiguration nodes are exposed.
- **FR-010**: JWT validation code MUST be freestanding — no dependency on libcurl, OpenSSL X509 validation, or POSIX filesystem. The integrator provides key material as PEM strings or DER blobs through the server config.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target profile: Standard 2017 UA Server with v1.05.02 Authorization Service Server Facet (CU 1629) and User Token — JWT Server Facet (CU 1697). OPC-10000-7 §6.6.
- **OPC-002**: ActivateSession service with JWT UserIdentityToken per OPC-10000-4 §5.7.3 Table 41.
- **OPC-003**: JWT UserIdentityToken encoding per OPC-10000-6 §5.2.3 (simple string token body carried as a ByteString).
- **OPC-004**: JWT validation semantics per RFC 7519 (JWT), RFC 7515 (JWS), RFC 7517 (JWK). JWT best current practices per RFC 8725.
- **OPC-005**: AuthorizationServiceConfigurationType address-space nodes per OPC-10000-12 §9.7.4 (gated on CU 1629). Only the type-system InstanceDeclarations are in scope; the full GDS server-side logic is deferred.
- **OPC-006**: `Bad_IdentityTokenInvalid` StatusCode per OPC-10000-4 §7.38.2.

### Scope Boundaries *(mandatory)*

- **In Scope**: JWT validation at ActivateSession (P1), issuer configuration via server config (P2), `sub` claim → user identity (P3), Kconfig gating for CU 1629 and CU 1697, type-system InstanceDeclarations for AuthorizationServiceConfigurationType.
- **Out of Scope**: Full GDS Authorization Service (token issuance, token introspection endpoint, OAuth2 Client Credentials flow server-side). OAuth2 Authorization Code flow. Dynamic JWKS fetching from issuer URL. Token refresh. The server is an OAuth2 **Resource Server** only, not an Authorization Server. Online token introspection (RFC 7662). Encrypted JWTs (JWE).
- **Compatibility Claim**: Standard 2017 UA Server Profile gains `Authorization Service Server Facet` and `User Token — JWT Server Facet` conformance claims when both Kconfig symbols are enabled.
- **Application Headroom Goal**: JWT validation code adds ≤5 KB `.text`. Cryptographic operations reuse the existing platform crypto adapter (mbedTLS, OpenSSL, wolfSSL). No additional crypto library dependencies.

### Key Entities

- **JWT Access Token**: A Base64-encoded JWS-signed JSON token with header, payload, and signature segments. Payload contains claims (`iss`, `sub`, `aud`, `exp`, `nbf`, `iat`, plus optional OPC UA role claims).
- **Trusted Issuer Configuration**: A record in the server config with: `issuer_url` (string), `jwks_uri` (string, optional), `public_key` (PEM/DER, optional), `expected_audience` (string), `clock_skew_seconds` (uint32).
- **JWT UserIdentityToken**: An OPC UA UserIdentityToken of type `IssuedToken` with `tokenType` URI `urn:ietf:params:oauth:token-type:jwt` and the raw JWT string in the `tokenData` field per OPC-10000-6 §5.2.3.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A client can activate an OPC UA session using a valid RS256-signed JWT with the correct issuer and audience. A standalone integration test demonstrates this against a pre-shared RSA key pair.
- **SC-002**: An expired JWT is rejected with `Bad_IdentityTokenInvalid` (verified by unit test).
- **SC-003**: A JWT signed by an unknown key is rejected with `Bad_IdentityTokenInvalid`.
- **SC-004**: Two independently configured issuers with different keys support concurrent activation from different clients (verified by unit test with two issuer configs).
- **SC-005**: The `sub` claim from a validated JWT is correctly reported in the session's user identity diagnostics.
- **SC-006**: JWT code compiles out entirely when both `MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER` and `MUC_OPCUA_CU_USER_TOKEN_JWT` are undefined — no `.text` or `.data` growth.
- **SC-007**: With JWT support compiled in, `.text` grows ≤5 KB relative to the baseline Standard profile.
- **SC-008**: All profile build targets (nano, micro, embedded, standard, full) compile and pass their test suites without regression.

## Assumptions

- The server's platform crypto adapter already provides SHA-256 and RSA signature verification primitives. JWT validation wraps these, not replaces them.
- The integrator provides at least one public key for JWT validation. Without any configured key, JWT authentication is disabled and `Bad_IdentityTokenRejected` is returned.
- JWT tokens fit within the existing ActivateSession message size limits (no multi-chunk or extended buffer allocation required).
- The OAuth2 Authorization Server is external to the OPC UA server — this spec covers only the Resource Server side.
- Clock synchronization between the Authorization Server and the OPC UA server is the integrator's responsibility. The server applies `clock_skew_seconds` tolerance from issuer config.
- JWT is always sent as a simple string token body (ByteString). No OPC UA Binary-encoded JWT structure or ExtensionObject wrapping is needed at this stage.
