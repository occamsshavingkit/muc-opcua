# Feature Specification: GDS Certificate Pull Management

**Feature Branch**: `097-gds-certificate-pull`  
**Created**: 2026-07-20  
**Status**: Draft  
**Input**: D-GDS — Full Certificate Pull Model (OPC-10000-12 §7). Implement the CertificateManager type system and Pull management Methods for certificate enrollment/renewal.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Browse CertificateManager Type Hierarchy (Priority: P1)

An OPC UA client browses the server address space and discovers the full Certificate Management type hierarchy: CertificateDirectoryType organizing CertificateGroupType instances, each containing CertificateType subtypes that describe what certificate kinds are available. This makes the server's certificate infrastructure discoverable via standard Browse.

**Why this priority**: Type-system nodes are the foundation — every Method call depends on a browsable, spec-conformant type hierarchy. Without this, no certificate management workflow can begin.

**Independent Test**: Browse BaseObjectType→CertificateDirectoryType subtype refs. Verify CertificateDirectoryType has HasComponent refs to CertificateGroupType instances (DefaultApplicationGroup + DefaultHttpsGroup + DefaultUserTokenGroup). Verify each CertificateGroupType has HasOrderedComponent refs to at least one CertificateType.

**Acceptance Scenarios**:

1. **Given** MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL is enabled, **When** a client browses subtypes of BaseObjectType, **Then** CertificateDirectoryType and CertificateGroupType and CertificateType are discoverable via HasSubtype references.
2. **Given** the CU is enabled, **When** a client browses the CertificateGroups Folder, **Then** DefaultApplicationGroup(15625), DefaultHttpsGroup(15626), and DefaultUserTokenGroup(15627) are visible as Organized objects of CertificateGroupType.
3. **Given** the CU is disabled, **When** the binary is linked for nano/micro/embedded, **Then** zero Certificate Management bytes are in the final image.

---

### User Story 2 — StartSigningRequest Method (Priority: P2)

An application client requests a new certificate from the CertificateManager by calling StartSigningRequest on a CertificateType node. The server returns a unique requestId used to poll and fetch the completed certificate.

**Why this priority**: StartSigningRequest is the entry gate for the Pull workflow — every certificate enrollment begins here.

**Independent Test**: Call StartSigningRequest on RsaSha256ApplicationCertificateType with a valid CSR, verify a non-zero requestId is returned and StatusCode is Good. Call with a zero-length CSR, verify Bad_InvalidArgument.

**Acceptance Scenarios**:

1. **Given** a CertificateType node with the StartSigningRequest Method, **When** a client calls it with a valid certificate signing request (DER-encoded PKCS#10), **Then** Good is returned with a unique integer requestId.
2. **Given** a CertificateType node with StartSigningRequest, **When** a client calls with an invalid/empty request, **Then** Bad_InvalidArgument is returned.

---

### User Story 3 — FinishRequest Method (Priority: P2)

After the CertificateManager has signed the certificate (delegated to integrator adapter), the client calls FinishRequest with the requestId to retrieve the completed certificate, private key, and issuer certificate chain.

**Why this priority**: FinishRequest completes the Pull workflow — without it the client can never receive the signed certificate.

**Independent Test**: Submit a request via StartSigningRequest, have the mock adapter mark it as completed, call FinishRequest with the correct requestId, verify Good with the certificate bytes. Call FinishRequest with an unknown requestId, verify Bad_NotFound.

**Acceptance Scenarios**:

1. **Given** a pending certificate request, **When** the adapter completes it, **Then** FinishRequest returns Good with the signed certificate, private key, and issuer certificates.
2. **Given** no knowledge of a requestId, **When** FinishRequest is called with an arbitrary integer, **Then** Bad_NotFound.

---

### User Story 4 — GetRejectedList Method (Priority: P3)

The client calls GetRejectedList on a CertificateGroupType node to discover which certificate requests were rejected by the Certificate Manager and why, enabling retry or operator intervention.

**Why this priority**: Error-path visibility. Essential for production certificate lifecycle management but can ship after the happy path.

**Independent Test**: Create a rejected request via mock adapter, call GetRejectedList on the group, verify the list includes the rejected request's requestId and reason.

**Acceptance Scenarios**:

1. **Given** one or more rejected requests in a certificate group, **When** GetRejectedList is called, **Then** the response contains each rejected requestId with its associated error code.

---

### User Story 5 — StartNewKeyPairRequest Method (Priority: P3)

When an application needs the CertificateManager to generate a new asymmetric key pair (rather than providing its own CSR), it calls StartNewKeyPairRequest. The server delegates key generation to the adapter and returns a requestId for polling.

**Why this priority**: Convenience alternative to StartSigningRequest. Lower priority because applications providing their own CSR is the more common embedded pattern.

**Independent Test**: Call StartNewKeyPairRequest with a valid key specification, verify a non-zero requestId. Call with unsupported key parameters, verify Bad_InvalidArgument.

**Acceptance Scenarios**:

1. **Given** an RsaSha256 CertificateType, **When** StartNewKeyPairRequest is called with a valid key size request, **Then** Good with a unique requestId.
2. **Given** a null key specification, **When** StartNewKeyPairRequest is called, **Then** Bad_InvalidArgument.

---

### Edge Cases

- Missing adapter: type nodes exist and are browsable, but Method calls return Bad_InternalError or Bad_NotImplemented.
- CertificateType with both Methods and no Methods: Browse should show StartSigningRequest/StartNewKeyPairRequest as Optional (only present when adapter provides implementation callbacks).
- RequestId lifecycle: adapter owns cleanup; stale requestIds from prior server restarts are silently unknown.
- Concurrent requests: two clients calling StartSigningRequest on the same CertificateType must yield distinct requestIds.
- Certificate format validation: adapter validates CSR format; server delegates without parsing DER structure.
- CU disabled at compile time: the code compiles out entirely — no type nodes, no Methods, no adapter struct field, zero binary contribution.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST expose CertificateDirectoryType(i=15594) as a subtype of BaseObjectType when `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` is enabled.
- **FR-002**: System MUST expose CertificateGroupType(i=12555) with HasSubtype references, plus the three standard instances (DefaultApplicationGroup, DefaultHttpsGroup, DefaultUserTokenGroup) organized by the CertificateGroups Folder.
- **FR-003**: System MUST expose CertificateType(i=12556) and its subtypes: ApplicationCertificateType(12557), HttpsCertificateType(12558), UserCertificateType(15017), RsaSha256ApplicationCertificateType(12559), and RsaMinApplicationCertificateType(15421) per OPC-10000-12 §7.8.4.
- **FR-004**: Each CertificateType subtype instance MUST have the StartSigningRequest Method (i=12482) and StartNewKeyPairRequest Method (i=12483) as Optional components, present only when the adapter provides implementation callbacks.
- **FR-005**: CertificateGroupType instances MUST have GetRejectedList Method(i=12747) as an Optional component.
- **FR-006**: `mu_certificate_manager_adapter_t` MUST provide callbacks for: `start_signing_request`, `start_new_key_pair_request`, `finish_request`, `get_rejected_list`.
- **FR-007**: Kconfig `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` MUST depend on `METHOD_SERVER && BASE_INFO_TYPE_INFORMATION`. Default: enabled in full profile, off in nano/micro/embedded.
- **FR-008**: Method handlers MUST validate input arguments (non-null, size > 0 for byte strings) and return appropriate OPC UA StatusCodes (Bad_InvalidArgument, Bad_NotFound) before delegating to the adapter.
- **FR-009**: Type-system nodes and Method registration MUST compile out entirely when the CU Kconfig symbol is disabled, verified by nano-profile compile-out.
- **FR-010**: All new OPC UA type-system nodes, BrowseNames, NodeIds, reference arrays, and Has* edge declarations MUST cite exact sections in OPC-10000-12 §7.8-7.9.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target conformance unit: "GDS Certificate Manager Pull Model" (OPC-10000-12 §7, CU 1631: Global Certificate Management 2022 Server Facet). Without external GDS connectivity the server cannot pass CTT for the full CU but provides the type-system compliance foundation.
- **OPC-002**: Implemented types and Methods per OPC-10000-12: §7.8.3.1 (CertificateGroupType), §7.8.4.1-§7.8.4.9 (CertificateType hierarchy), §7.9.2 (CertificateDirectoryType), §7.9.6 (StartSigningRequest), §7.9.7 (StartNewKeyPairRequest), §7.9.9 (FinishRequest), §7.9.10 (GetRejectedList).
- **OPC-003**: Unsupported: Push Model (§7.10 ServerConfigurationType), TrustList update, CertificateExpired/CertificateUpdated audit events, CRL management — all deferred.
- **OPC-004**: Wire encoding per OPC-10000-6 Binary encoding; Method input/output arguments use standard OPC UA type encodings (ByteString for DER, UInt32 for requestId, String for groupId).
- **OPC-005**: Conformance status: profile-targeting. Declared as profile-compliant only after CTT evidence.

### Scope Boundaries *(mandatory)*

- **In Scope**: Full CertificateManager type hierarchy (CertificateDirectoryType → CertificateGroupType → CertificateType with subtypes), Method nodes for Pull workflow (StartSigningRequest, StartNewKeyPairRequest, FinishRequest, GetRejectedList), adapter interface with per-method callbacks, Kconfig compile-gating, unit tests for Method dispatch and argument validation.
- **Out of Scope**: Push Model (OPC-10000-12 §7.10 ServerConfigurationType), TrustList integration (§7.8.2), CertificateExpirationAlarmType, audit event types, external GDS connectivity, actual CSR/DER parsing, cryptographic key generation, CRL management.
- **Compatibility Claim**: StandardUA2022/Facets/OpcUaGlobalCertificateManagementServerFacet after CTT verification.
- **Application Headroom Goal**: ≤3.0 KB .text, ≤0.5 KB .rodata above baseline full profile.

### Key Entities

- **CertificateGroupType** (i=12555): ObjectType organizing certificates by context (application vs HTTPS vs user). Contains TrustList and CertificateType instances.
- **CertificateType** (i=12556): Base ObjectType for certificate instances. Subtyped into ApplicationCertificateType, HttpsCertificateType, UserCertificateType, and algorithm-specific variants (RsaSha256, RsaMin).
- **CertificateDirectoryType** (i=15594): Root ObjectType for the Certificate Manager, organizing CertificateGroups and Methods.
- **requestId** (UInt32): Opaque integer handle returned by StartSigningRequest/StartNewKeyPairRequest, consumed by FinishRequest to retrieve the completed certificate.
- **mu_certificate_manager_adapter_t**: Integrator-provided callbacks. All callbacks receive the method call context and return a mu_status_code_t.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: When the CU is enabled, a Browse of subtypes of BaseObjectType returns CertificateDirectoryType, CertificateGroupType, and at least 5 CertificateType subtypes.
- **SC-002**: All 4 Method node definitions pass argument validation: invalid input returns Bad_InvalidArgument, unknown requestId returns Bad_NotFound.
- **SC-003**: 10 unit tests exercise Method dispatch and argument validation, passing on host with mock adapter.
- **SC-004**: Nano-profile compile-out verified: `cmake -B b_nano -DMUC_OPCUA_PROFILE=nano && cmake --build b_nano` builds with no cert manager symbols.
- **SC-005**: All new type-system nodes pass BrowseName length validation (length constant matches declared array size).
- **SC-006**: .text contribution measured at ≤3.0 KB for full profile enabling.

## Assumptions

- The integrator provides the actual signing, key generation, and certificate storage logic via the adapter; the library provides only type-system conformance and Method dispatch.
- No TLS/HTTPS connectivity to an external GDS is required — the adapter can store certificates locally or delegate to an out-of-band process.
- DefaultApplicationGroup, DefaultHttpsGroup, and DefaultUserTokenGroup instance NodeIds follow the OPC UA NodeSet schema (i=15625, 15626, 15627).
- ECC certificate subtypes (EccApplicationCertificateType, EccNistP256ApplicationCertificateType, etc.) are deferred to a future increment.
- TrustListType and trust list management methods (OpenWithMasks, CloseAndUpdate, AddCertificate, RemoveCertificate) are a separate future scope requiring a dedicated spec.
