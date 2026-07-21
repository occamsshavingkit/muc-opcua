# Implementation Plan: GDS Certificate Pull Management

**Branch**: `097-gds-certificate-pull` | **Date**: 2026-07-20 | **Spec**: [spec.md](./spec.md)

## Technical Context

| Item | Decision |
|------|----------|
| Language | C11 (freestanding, no libc in core paths) |
| Build | CMake, Kconfig-gated via `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` |
| Target | muc-opcua full profile; nano/micro/embedded compile-out |
| Test Framework | Unity (host tests) |
| Crypto | No new crypto dependency; adapter delegates CSR/signing to integrator |
| Size Budget | ≤3.0 KB `.text`, ≤0.5 KB `.rodata` above full baseline |

## Constitution Check

- **I. Spec Fidelity**: OPC-10000-12 §7.8-7.9 citations for every type node, Method, BrowseName, and conformance claim.
- **II. Embedded-First C**: Static type-node tables, no heap allocation, adapter abstraction for platform operations.
- **III. Minimal Surface**: Certificate Manager Pull Model only; Push, TrustList, CRL, audit events deferred.
- **IV. Correctness Gates**: Method handlers validate input before delegation; unit tests for every StatusCode path.
- **V. Security Honesty**: No silent weakening; adapter callback model keeps crypto implementor-provided.
- **VI. Reproducible Build**: CMake gating, clang-format, compiler warnings as errors.
- **VII. Size Discipline**: Nano compile-out verified; type-node tables measured via `test_size_report`.

## Design Artifacts

- Internal object design: N/A — procedural C dispatch; no class hierarchy beyond the OPC UA type system. Adapter is a single function-pointer struct.
- Service sequences: `./contracts/sequences.md`
- Data model: `./data-model.md`
- Validation path: `./quickstart.md`

## Scope & Complexity Tracking

| Decision | Rationale |
|----------|-----------|
| Omit ECC subtypes | RsaSha256 + RsaMin cover the embedded use case. ECC adds 6+ ObjectTypes. Deferred. |
| Omit TrustListType children | TrustList management is a separate CU requiring CRL parsing. Deferred. |
| Omit CertificateDirectoryType Methods (StartSigningRequest, etc.) on the Directory object | Per spec, Methods live on CertificateType/GroupType instances, not the Directory root. |
| Adapter is a single struct | Simpler than per-method adapter granularity. Matches PG18/PG19 precedent. |

## File Layout

```
include/muc_opcua/services/certificate_manager.h   # public adapter + NodeId constants
src/cu/core_2022_server/certificate_manager/
  cert_manager.h                                      # internal dispatch + registration
  cert_manager.c                                      # Method handler implementations
src/address_space/base_nodes.c                        # type-system InstanceDeclarations
src/CMakeLists.txt                                    # compile-definitions gating
src/Kconfig                                           # Kconfig symbol
tests/unit/test_certificate_manager.c                # unit tests
profiles/opcua-profile-manifest.yaml                  # manifest entry
```
