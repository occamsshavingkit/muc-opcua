# Implementation Plan: KeyCredential Service Server Facet

**Feature Branch**: `094-keycredential-service`
**Spec**: [spec.md](./spec.md)

## Technical Context

- **Language**: C11 freestanding
- **Build**: CMake + Kconfig, `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE` (depends on `MUC_OPCUA_CU_METHOD_SERVER`)
- **Target profiles**: full (default), standard (opt-in)
- **Dependencies**: Method Server Facet (method dispatch), base address space
- **Size budget**: ≤3 KB `.text`

## Constitution Check

| Principle | Status |
|-----------|--------|
| I. Spec Fidelity | ✅ OPC-10000-12 §8.5-8.6, CU 2113 |
| II. Embedded-First | ✅ No heap, integrator-provided adapter |
| III. Minimal Surface | ✅ 4 method callbacks, gated CU |
| IV. Protocol Correctness | ✅ Correct StatusCodes per spec |
| V. Security Honesty | ✅ Credentials are opaque blobs; server doesn't decrypt |
| VI. Reproducible | ✅ Kconfig-gated |
| VII. Size Discipline | ✅ ≤3 KB budget |

## Phases

1. Kconfig + adapter interface + CMake gating
2. Method handlers (GetEncryptingKey, CreateCredential, UpdateCredential, DeleteCredential)
3. Address-space type nodes
4. Unit tests
5. Polish (size, format, conformance doc)

## Design Artifacts

- Data model: `./data-model.md`
- Validation: `./quickstart.md`
- Sequences: N/A — single method dispatch, no cross-boundary flows
