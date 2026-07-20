# Feature Specification: Global Certificate Management Server Facet

**Created**: 2026-07-20 | **Status**: Draft  
**Input**: PG20 — Certificate enrollment/renewal via GDS. Minimal implementation: type nodes + adapter + Kconfig gating. Full GDS Push/Pull model deferred.

## User Scenarios

### US1 — Browse Certificate Management types (P1)
When the CU is enabled, clients can browse the CertificateGroupType and CertificateType node hierarchies in the address space.

**Independent Test**: Browse BaseObjectType→CertificateGroupType forward ref exists.

### US2 — Certificate store adapter (P2)
Integrator provides a certificate storage adapter. The library exposes the type system; actual cert lifecycle operations are delegated.

**Edge Cases**: Missing adapter → type nodes exist but are non-functional. CU undefined → zero code.

## Requirements
- **FR-001**: CertificateGroupType and CertificateType InstanceDeclarations per OPC-10000-12 §7.5-7.6, gated on `MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT`.
- **FR-002**: `mu_certificate_management_adapter_t` — integrator-provided cert lifecycle callbacks.
- **FR-003**: Kconfig: depends on `METHOD_SERVER && BASE_INFO_TYPE_INFORMATION`. Full default.
- **FR-004**: Type nodes only — Methods (CreateSigningRequest, GetRejectedList, FinishRequest) deferred.

## Scope
- **In**: CertificateGroupType, CertificateType InstanceDeclarations. Adapter interface.
- **Out**: Full GDS server (Push/Pull), Certificate enrollment workflow, CRL management, trust list update.
- **Size**: ≤2 KB `.text`.
