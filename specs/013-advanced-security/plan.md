# Implementation Plan: Advanced Security Policies

**Branch**: `013-advanced-security` | **Date**: 2026-06-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/013-advanced-security/spec.md`

## Summary

Implement `Aes128-Sha256-RsaOaep` and `Aes256-Sha256-RsaPss` security policies for symmetric and asymmetric cryptography, and introduce basic TrustList management to validate and reject unknown client certificates during the `OpenSecureChannel` handshake, matching exactly byte-for-byte.

## Technical Context

**Language/Version**: Freestanding C11
**Primary Dependencies**: mbedTLS (embedded targets), OpenSSL (host tests)
**Storage**: N/A
**Testing**: Unity or CMocka for unit tests, plus integration testing with external clients (e.g., UaExpert)
**Target Platform**: Microcontrollers (e.g., RP2040, Arduino) and Linux test host
**Project Type**: C Library
**Performance Goals**: Avoid heap allocation in hot path
**Constraints**: Zero-heap allocations for crypto primitives, exact byte-for-byte certificate matching
**Scale/Scope**: Support exact certificate matching for a small static array of certificates
**OPC UA Normative References**: OPC-10000-4 5.5, 5.6.2 (Application Authentication), OPC-10000-6 UASC Security Headers, OPC-10000-7 (Profiles)
**Target OPC UA Profile/Conformance Units**: Micro Embedded Device Server Profile
**Conformance Status Target**: profile-targeting

## Embedded Size Budget

**Flash Impact**:
- nano: 16493 bytes
- micro: 23774 bytes
- embedded: 39630 bytes
- full-featured: 39956 bytes
**RAM Impact**: Small stack usage during cryptographic operations. No static RAM increase.
**Heap Use**: None (crypto adapters will use caller-provided or static buffers).
**Static Tables Added**: `mu_trust_list_t` (few bytes, caller-provided).
**Transport Buffers**: No changes needed; sizes accommodated by existing design.
**Crypto Dependency Impact**: Moderate flash impact from bringing in additional hashing and padding functions in mbedTLS.

## Constitution Check

*GATE: Passed.*
- **Spec Fidelity**: Security implementations cite exact OPC-10000-7 profiles.
- **Embedded C Core**: Freestanding C11 is used, keeping heap-free constraints.
- **Memory Model**: No heap allocation; TrustList uses caller-provided buffers.
- **Minimal OPC UA Surface**: Scoped to the required security policies.
- **Size Discipline**: Added cryptography dependencies are tracked.

## Project Structure

### Documentation (this feature)

```text
specs/013-advanced-security/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output
```

### Source Code (repository root)

```text
include/
└── micro_opcua/         
    └── security.h       # Updated for new policies and TrustList API
src/
├── core/                
├── encoding/            
├── services/            
├── address_space/       
├── security/            # TrustList logic and policy handling
├── platform/            # Updated host_crypto_adapter.c and mbedtls_crypto_adapter.c
└── generated/           
platform/
├── pico/                
└── arduino/             
examples/
└── minimal_server/      # Update to initialize TrustList
tests/
├── unit/
│   └── test_security.c
├── fixtures/
├── fuzz/
└── integration/
docs/
├── adr/
└── traceability/
```

**Structure Decision**: The logic will reside within the existing `src/security/` and `src/platform/` directories. TrustList configuration will be added to the public API in `include/micro_opcua/server.h` or `security.h`.

## Complexity Tracking

*(No violations)*
