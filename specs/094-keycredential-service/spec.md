# Feature Specification: KeyCredential Service Server Facet

**Feature Branch**: `094-keycredential-service`
**Created**: 2026-07-20
**Status**: Draft
**Input**: KeyCredential Service Server Facet (PG18, CU 2113) — credential management for broker auth. OPC-10000-12 §8.5-8.6, OPC-10000-7 v1.05.02. Allows the OPC UA server to manage KeyCredentials (X.509 certs or symmetric keys) that OAuth2/OIDC brokers use to authenticate with external Authorization Servers.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — GetEncryptingKey Method (Priority: P1)

A client requests the server's encryption public key so it can encrypt a KeyCredential before submitting it. The server returns its RSA or EC public key in DER format along with the key ID.

**Why this priority**: GetEncryptingKey is the mandatory CU and the prerequisite for all credential write operations. Without it, no credential can be securely transmitted.

**Independent Test**: Call GetEncryptingKey on a configured KeyCredentialConfiguration instance, verify the returned public key is a valid DER-encoded SubjectPublicKeyInfo.

**Acceptance Scenarios**:

1. **Given** the server has a KeyCredentialConfiguration instance with a configured encryption key, **When** a client calls GetEncryptingKey with a valid ResourceUri, **Then** the response contains Good status, a valid public key, and a non-empty keyId.
2. **Given** the server has no encryption key configured, **When** GetEncryptingKey is called, **Then** the response is Bad_NoData.

---

### User Story 2 — CreateCredential Method (Priority: P2)

A client creates a new KeyCredential on the server by submitting the encrypted credential blob. The server stores it and returns success.

**Why this priority**: CreateCredential is the write path for credential lifecycle. It depends on GetEncryptingKey (P1).

**Independent Test**: Submit a test credential blob to a configured KeyCredentialConfiguration instance, verify Good status and the credential is retrievable.

**Acceptance Scenarios**:

1. **Given** a KeyCredentialConfiguration instance, **When** a client calls CreateCredential with a valid encrypted credential, **Then** the response is Good.
2. **Given** a full credential store, **When** CreateCredential is called, **Then** the response is Bad_OutOfMemory or Bad_ResourceUnavailable.

---

### User Story 3 — UpdateCredential and DeleteCredential (Priority: P3)

A client updates or deletes an existing KeyCredential.

**Why this priority**: Completes the credential lifecycle. Lower priority than create.

**Independent Test**: Create a credential, then update it, then delete it. Verify each returns Good.

**Acceptance Scenarios**:

1. **Given** an existing credential, **When** UpdateCredential is called with a new encrypted blob, **Then** the response is Good.
2. **Given** an existing credential, **When** DeleteCredential is called, **Then** the response is Good and subsequent retrieval fails.
3. **Given** no existing credential with the given ID, **When** UpdateCredential or DeleteCredential is called, **Then** the response is Bad_NoEntryExists.

---

### Edge Cases

- GetEncryptingKey with NULL ResourceUri → Bad_InvalidArgument
- CreateCredential with unencrypted credential → rejected (credentials must be encrypted with the server's key)
- Method called on a node that is not a KeyCredentialConfiguration → Bad_NoMatch
- Credential store adapter not configured → methods return Bad_NotSupported
- Concurrent CreateCredential calls → integrator's adapter handles serialization

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Server MUST implement the KeyCredential Service Server Facet per OPC-10000-12 §8.5-8.6, gated on `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE`.
- **FR-002**: Server MUST expose `GetEncryptingKey` Method (OPC-10000-12 §8.5.4 Table 142) on KeyCredentialConfigurationType instances.
- **FR-003**: Server MUST expose `CreateCredential`, `UpdateCredential`, `DeleteCredential` Methods (OPC-10000-12 §8.6) on KeyCredentialConfigurationFolderType instances.
- **FR-004**: The credential storage backend MUST be provided by the integrator via a `mu_key_credential_adapter_t` interface. When no adapter is configured, all methods return `Bad_NotSupported`.
- **FR-005**: KeyCredentialConfigurationType and KeyCredentialConfigurationFolderType address-space nodes MUST be exposed when the CU is enabled and `MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION` is on.
- **FR-006**: All KeyCredential methods MUST be gated on `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE`. When undefined, method calls return `Bad_ServiceUnsupported`.
- **FR-007**: The GetEncryptingKey response MUST include the DER-encoded public key and a keyId string.
- **FR-008**: Credentials MUST be passed as ByteStrings (encrypted blobs). The server does not decrypt — it stores opaque blobs.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target profile: Full 2022 UA Server with KeyCredential Service Server Facet (CU 2113). OPC-10000-7 §4.2.
- **OPC-002**: Method NodeIds per OPC-10000-12 §8.5-8.6 and NodeIds.csv (17516 GetEncryptingKey, 17519 UpdateCredential, 17521 DeleteCredential, 17522 CreateCredential).
- **OPC-003**: KeyCredentialConfigurationType (17533) and KeyCredentialConfigurationFolderType (17496) type definitions per OPC-10000-12 §8.5-8.6.
- **OPC-004**: StatusCodes per OPC-10000-4 §7.38.2 (Bad_NoData, Bad_NoEntryExists, Bad_InvalidArgument, Bad_NotSupported).
- **OPC-005**: Method call dispatch per OPC-10000-4 §5.11 Call service.

### Scope Boundaries *(mandatory)*

- **In Scope**: GetEncryptingKey (P1), CreateCredential (P2), UpdateCredential/DeleteCredential (P3), KeyCredential adapter interface, type-system InstanceDeclarations, Kconfig gating.
- **Out of Scope**: Credential decryption (server stores opaque blobs). Push/Pull model notification flows. KeyCredentialAuditEvent types (audit logging deferred). Full GDS server. Online certificate enrollment with a CA. Key rotation automation.
- **Compatibility Claim**: Full 2022 UA Server Profile gains KeyCredential Service Server Facet conformance claim when the CU is enabled.
- **Application Headroom Goal**: ≤3 KB `.text`. Credential storage is integrator-provided (no static buffers in the library).

### Key Entities

- **KeyCredentialConfigurationType**: An ObjectType that stores per-service configuration: ResourceUri, ProfileUri, EndpointUrls, ServiceStatus. Has GetEncryptingKey Method.
- **KeyCredentialConfigurationFolderType**: An ObjectType that groups KeyCredentialConfiguration instances. Has CreateCredential, UpdateCredential, DeleteCredential Methods.
- **mu_key_credential_adapter_t**: Integrator-provided callback interface for credential storage: `get_encrypting_key`, `create_credential`, `update_credential`, `delete_credential`.
- **KeyCredential**: An opaque ByteString blob (typically an encrypted PKCS#12 or similar) stored by the adapter.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A client can call GetEncryptingKey and receive a valid DER public key.
- **SC-002**: A client can create, update, and delete credentials through the method interface.
- **SC-003**: Missing adapter returns Bad_NotSupported for all methods.
- **SC-004**: KeyCredential type nodes appear in the address space when the CU is enabled.
- **SC-005**: Code compiles out entirely when `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE` is undefined.
- **SC-006**: `.text` grows ≤3 KB for the credential management code.

## Assumptions

- The existing Method Server Facet (`MUC_OPCUA_CU_METHOD_SERVER`) is available for method dispatch.
- The integrator provides a credential storage adapter. The library does not implement persistent storage.
- KeyCredentialConfiguration type nodes are in namespace 0 (standard UA) since they are part of the GDS type system imported into the base address space.
- The GetEncryptingKey public key is the server's existing application instance certificate key or a dedicated key provided by the integrator.
