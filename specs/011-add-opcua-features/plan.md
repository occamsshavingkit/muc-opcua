# Implementation Plan: Implement Custom Methods, Aes256_Sha256_RsaPss, Server Diagnostics, and Dynamic Node Management

**Branch**: `011-add-opcua-features` | **Date**: 2026-06-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/011-add-opcua-features/spec.md`

## Summary

This plan outlines the design and implementation of four high-value OPC UA features:
1. **Custom C Method Calls**: Support for user-defined method callbacks in the address space, integrated into the `Call` service dispatch layer.
2. **Aes256_Sha256_RsaPss Security Policy**: Verification of RSASSA-PSS signatures and decryption of RSA-OAEP with SHA-256 in all cryptographic platform adapters (host, mbedtls, wolfssl).
3. **Server Diagnostics**: Instantiation and tracking of standard server variables for monitoring session, channel, and subscription counts.
4. **Dynamic Node Management**: Implementation of `AddNodes`/`DeleteNodes` service sets using a fixed-size dynamic node pool to maintain zero-heap constraints.

## Technical Context

**Language/Version**: C11  
**Primary Dependencies**: None (freestanding core); OpenSSL, Mbed TLS, or wolfSSL depending on CMake options.  
**Storage**: N/A  
**Testing**: Unity (host), CTest  
**Target Platform**: Arm Cortex-M, Linux/POSIX host  
**Project Type**: library  
**Performance Goals**: Sub-millisecond dispatch for method calls and dynamic node modifications.  
**Constraints**: Zero dynamic heap allocations on the hot path (all nodes are allocated from a preallocated pool).  
**OPC UA Normative References**: OPC-10000-4 §5.5, §5.12.2; OPC-10000-5 §6.x; OPC-10000-7 §6.2.  
**Target OPC UA Profile/Conformance Units**: Base Server Behavior, Method Call, Security Policies.  
**Conformance Status Target**: profile-targeting  

## Embedded Size Budget

**Flash Impact**: Under 8 KB of ARM Thumb code with all options enabled.  
**RAM Impact**: Fixed static allocation for dynamic node pools (~2 KB total for 32 dynamic nodes).  
**Heap Use**: 0 bytes.  
**Static Tables Added**: N/A  
**Transport Buffers**: N/A  
**Crypto Dependency Impact**: Integrates with existing linked crypto backends.

## Constitution Check

- **Spec Fidelity**: Yes. All features strictly follow Part 4, 5, and 7 specifications.
- **Embedded C Core**: Yes. Standard C11 core, free of dynamic heap usage.
- **Memory Model**: Yes. No dynamic allocations on the hot path. Bounded static pool used for dynamic nodes.
- **Minimal OPC UA Surface**: Yes. Gated behind standard compile flags.
- **Correctness Gates**: Yes. Supported by comprehensive unit and integration tests.

## Project Structure

### Documentation (this feature)

```text
specs/011-add-opcua-features/
├── spec.md              # Feature specification
├── plan.md              # This file
├── research.md          # Research findings
├── data-model.md        # Wire and memory layout design
├── quickstart.md        # Integration guide
└── checklists/
    └── requirements.md  # Quality checklist
```

### Source Code (repository root)

```text
include/
└── micro_opcua/
    └── server.h                 # Expanded custom callback registrations
src/
├── core/
│   └── service_dispatch.c      # Call service dispatch, AddNodes, DeleteNodes handlers
├── security/
│   └── certificate.c            # RSA-PSS signature verification logic
├── platform/
│   ├── host_crypto_adapter.c    # PSS support (OpenSSL)
│   ├── mbedtls_crypto_adapter.c # PSS support (Mbed TLS)
│   └── wolfssl_crypto_adapter.c # PSS support (wolfSSL)
└── address_space/
    └── address_space.c          # Dynamic node pool allocator
tests/
└── unit/
    ├── test_method_call.c       # Custom method call tests
    ├── test_security_policy.c   # Aes256_Sha256_RsaPss tests
    └── test_node_management.c   # AddNodes / DeleteNodes tests
```
