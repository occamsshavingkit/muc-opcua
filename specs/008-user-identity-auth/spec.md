# Feature Specification: UserName/Password User Identity Token Authentication

**Feature Branch**: `008-user-identity-auth`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "Implement UserName/Password User Identity Token authentication for server sessions"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Anonymous User Authentication (Priority: P1)

Allow clients to authenticate anonymously when Anonymous login is enabled.

**Why this priority**: Preserves backward compatibility and supports simple/testing setups.

**Independent Test**: Configure the server with an anonymous user policy, connect a client, activate the session using `AnonymousIdentityToken` (NodeId 321), and verify successful connection.

**Acceptance Scenarios**:

1. **Given** a server configured with Anonymous policy enabled, **When** a client connects and sends an `ActivateSessionRequest` with `AnonymousIdentityToken`, **Then** the session is successfully activated and returns `Good`.
2. **Given** a server configured with Anonymous policy disabled, **When** a client connects and sends an `ActivateSessionRequest` with `AnonymousIdentityToken`, **Then** the session activation fails and returns `Bad_IdentityTokenRejected`.

---

### User Story 2 - UserName/Password Authentication (Priority: P1) [MVP]

Allow clients to authenticate using plain-text Username and Password credentials when connecting over security policy None.

**Why this priority**: Essential for basic access control in micro-device networks.

**Independent Test**: Configure the server with UserName policy and a validation handler, connect a client over security policy None, activate the session with valid credentials, and verify it succeeds.

**Acceptance Scenarios**:

1. **Given** a server configured with a UserName policy and validation handler, **When** a client sends an `ActivateSessionRequest` with a valid Username/Password, **Then** the session is successfully activated and returns `Good`.
2. **Given** a server configured with a UserName policy and validation handler, **When** a client sends an `ActivateSessionRequest` with an invalid Username or Password, **Then** the session activation fails and returns `Bad_IdentityTokenRejected`.

---

### User Story 3 - Encrypted UserName/Password Authentication (Priority: P2)

Allow clients to authenticate using encrypted Username and Password credentials when standard security is active.

**Why this priority**: Required for secure channels to prevent credential sniffing on the wire.

**Independent Test**: Connect a client over a secure channel (e.g. Basic256Sha256), send encrypted credentials, and verify successful validation and session activation.

**Acceptance Scenarios**:

1. **Given** a secure channel connection, **When** a client sends an `ActivateSessionRequest` containing an encrypted UserNameIdentityToken matching server private key, **Then** the server decrypts the credentials, validates them against the handler, and returns `Good`.
2. **Given** a secure channel connection, **When** a client sends an `ActivateSessionRequest` containing an encrypted UserNameIdentityToken that cannot be decrypted or has invalid credentials, **Then** the server returns `Bad_IdentityTokenRejected` or `Bad_IdentityTokenInvalid`.

---

### Edge Cases

- **Malformed Token Body**: The `UserNameIdentityToken` body contains malformed encoding (e.g. string lengths exceeding the extension object boundaries). The server MUST reject it with `Bad_DecodingError`.
- **Empty Username/Password**: The client sends empty/null strings for username or password. The server authentication handler receives them and MUST return `Bad_IdentityTokenRejected`.
- **Unsupported Token Type**: The client sends a user identity token type that is neither Anonymous nor UserName (e.g. X509 or IssuedToken). The server MUST reject it with `Bad_IdentityTokenInvalid`.
- **Headroom Preservation**: Implementing decoding/handling of UserIdentityTokens MUST NOT use dynamic memory allocations and must limit transient stack usage under 256 bytes.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The server configuration MUST support registering a user authentication callback/handler with signature:
  `opcua_statuscode_t (*mu_user_auth_handler_t)(void *handle, const mu_string_t *username, const mu_bytestring_t *password, const mu_string_t *policy_id)`
- **FR-002**: The server MUST support advertising multiple `UserTokenPolicy` entries (Anonymous and/or UserName) in its `EndpointDescription` in response to `GetEndpoints`.
- **FR-003**: The server MUST decode the `UserNameIdentityToken` body (citing NodeId 324 binary encoding) during `ActivateSession` request processing.
- **FR-004**: When the client sends `UserNameIdentityToken` (NodeId 324), the server MUST invoke the registered user authentication handler.
- **FR-005**: If the user authentication handler returns an error (or is not configured), the server MUST reject session activation and return `Bad_IdentityTokenRejected`.
- **FR-006**: Plaintext and encrypted password formats MUST be supported depending on the security policy of the channel.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: UserIdentityTokens and UserNameIdentityToken are defined in OPC-10000-4 §7.36.
- **OPC-002**: UserTokenPolicy configuration is defined in OPC-10000-4 §7.37.
- **OPC-003**: ActivateSession service parameter requirements are defined in OPC-10000-4 §5.7.3.
- **OPC-004**: Binary encoding rules for UserNameIdentityToken are defined in OPC-10000-6 §7.38.3.

### Scope Boundaries *(mandatory)*

- **In Scope**:
  - `AnonymousIdentityToken` (NodeId 321) default binary decoding and validation.
  - `UserNameIdentityToken` (NodeId 324) default binary decoding and validation.
  - Plaintext and encrypted password processing.
  - Endpoint advertising of user policies.
- **Out of Scope**:
  - `X509IdentityToken` (NodeId 327) and `IssuedIdentityToken` (NodeId 293).
  - UserTokenSignature validation for federated identity (WS-Security).
- **Compatibility Claim**:
  - User Token-UserName Server Facet (`http://opcfoundation.org/UA-Profile/Server/UserToken-UserName`).
- **Application Headroom Goal**:
  - Target code size impact under 1.5 KB flash; 0 bytes static RAM (BSS) growth.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of unit and integration tests validate correct credentials allow session activation, while invalid credentials return `Bad_IdentityTokenRejected`.
- **SC-002**: Verification with AddressSanitizer and UndefinedBehaviorSanitizer shows no memory leaks, buffer overflows, or undefined behaviors.
- **SC-003**: Thumb `-Os` flash memory footprint overhead is under 1.5 KB.
- **SC-004**: Static memory footprint (BSS) overhead is 0 bytes.

## Assumptions

- Standard OPC UA clients can fall back to plain-text password transmission over security policy None if the endpoint specifies it.
- RSA decryption of the password string (when encrypted) uses the existing security policy decrypt helpers and does not require new cryptographic primitives.
