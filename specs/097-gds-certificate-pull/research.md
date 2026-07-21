# Decision Research: GDS Certificate Pull Management

**Date**: 2026-07-20

## D1: Method Location

**Decision**: Place StartSigningRequest + StartNewKeyPairRequest on CertificateType instances;
FinishRequest on CertificateDirectoryType; GetRejectedList on CertificateGroupType.

**Rationale**: Per OPC-10000-12 §7.9 — the specification defines these Methods on specific types.
CertificateType handles CSR/NewKeyPair, CertificateDirectoryType handles certificate retrieval,
CertificateGroupType tracks rejection state.

**Alternatives**: Placing all Methods on CertificateDirectoryType. Rejected: violates normative spec
layout and makes per-certificate-type registration impossible.

## D2: FinishRequest Implementation

**Decision**: FinishRequest adapter callback receives caller-provided output buffers for certificate,
private key, and issuer chain. Adapter fills them.

**Rationale**: Follows embedded-first pre-allocated buffer pattern (Constitution §II). No heap
allocation in the hot path. Caller is the OPC UA stack's response encoder which provides buffers.

**Alternatives**: Adapter allocates internally. Rejected: violates embedded-first constraint.

## D3: Adapter Granularity

**Decision**: Single `mu_certificate_manager_adapter_t` struct with four function pointers.

**Rationale**: Matches PG18 (KeyCredential: 4 methods, 1 adapter) and PG19 (RoleManagement: 4 methods,
1 adapter) patterns. Integrator has one configuration point.

**Alternatives**: Per-CertificateType adapters. Rejected: unnecessary complexity for the scope;
a single backend handles all certificate types.

## D4: Instance NodeIds

**Decision**: Use OPC NodeSet-specified NodeIds for standard instances:
DefaultApplicationGroup=i=15625, DefaultHttpsGroup=i=15626, DefaultUserTokenGroup=i=15627.

**Rationale**: Conformance requires discoverability at known NodeIds. The OPC Foundation NodeSet
provides these as standard identifiers.

## D5: ECC Subtypes Deferral

**Decision**: Omit EccApplicationCertificateType and its 6 algorithm-specific subtypes.

**Rationale**: Embedded servers are RSA-first; ECC certificate types are primarily for high-security
and NIST compliance scenarios. Adding 7 ObjectTypes for a single Method registration is poor
cost/benefit ratio. Can be added in a follow-up spec.

## D6: Test Strategy

**Decision**: Unit tests with mock adapter; no integration tests against real GDS.

**Rationale**: The adapter boundary is a clean seam. Method handler logic (input validation,
argument marshaling, return-code dispatch) is fully testable with mock callbacks. External
GDS connectivity is an integrator concern.
