# Implementation Plan: Implement Mbed TLS and wolfSSL platform adapters

**Branch**: `010-add-crypto-adapters` | **Date**: 2026-06-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/010-add-crypto-adapters/spec.md`

## Summary

This plan outlines the design and implementation of pluggable cryptographic platform adapters for the `Mbed TLS` and `wolfSSL` libraries. These adapters will implement the `mu_crypto_adapter_t` interface, allowing micro-opcua to run securely on resource-constrained embedded systems (like ARM Cortex-M) where OpenSSL is not available.

## Technical Context

**Language/Version**: C11  
**Primary Dependencies**: None (freestanding core); Mbed TLS or wolfSSL when their respective CMake option is enabled.  
**Storage**: N/A  
**Testing**: Unity (host), CTest  
**Target Platform**: Arm Cortex-M (e.g., RP2040, STM32), Linux/POSIX host  
**Project Type**: library  
**Performance Goals**: Handshake and message encryption/decryption processing times consistent with the underlying library performance.  
**Constraints**: Zero dynamic heap allocations on the hot path (all buffers and context objects are stack-allocated or static); stack usage under 512 bytes per function call.  
**OPC UA Normative References**: OPC-10000-7 §6.2 (Security Policies), OPC-10000-6.  
**Target OPC UA Profile/Conformance Units**: Base Server Behavior, Security Policies.  
**Conformance Status Target**: profile-targeting  

## Embedded Size Budget

**Flash Impact**: Under 2 KB of ARM Thumb code with all options enabled.
- Measured host (x86_64) text segment sizes:
  - `mbedtls_crypto_adapter.c.o`: 4,600 bytes
  - `wolfssl_crypto_adapter.c.o`: 5,058 bytes
- Typically ~1.5 KB to 2 KB of Thumb-2 code for each adapter on ARM Cortex-M.
**RAM Impact**: 0 bytes of static RAM (all crypto contexts are stack-allocated or held in the caller-supplied session context).  
**Heap Use**: 0 bytes (zero heap allocations).  
**Static Tables Added**: N/A  
**Transport Buffers**: N/A (reuses existing transport buffers).  
**Crypto Dependency Impact**: Integrates with external Mbed TLS or wolfSSL library linked by the application build system.  

## Constitution Check

- **Spec Fidelity**: Yes. Adapters strictly adhere to asymmetric and symmetric cryptographic schemes defined by OPC UA Security Policies (Basic256Sha256 and Aes128_Sha256_RsaOaep).
- **Embedded C Core**: Yes. Free of OS dependencies; standard C11 core.
- **Memory Model**: Yes. No dynamic allocations; all contexts are stack-allocated or supplied by the caller.
- **Minimal OPC UA Surface**: Yes. Gated behind CMake flags `MICRO_OPCUA_HAVE_MBEDTLS` and `MICRO_OPCUA_HAVE_WOLFSSL`.
- **Profile Research**: Yes.
- **Correctness Gates**: Yes. The adapters will be validated via unit tests verifying parity with standard test vectors.
- **Security Honesty**: Yes. Proven cryptographic libraries are wrapped without custom math.
- **Toolchain Discipline**: Yes. Managed via CMake and testable locally on the host.
- **Size Discipline**: Yes. Flash and RAM overhead is minimal and documented.

## Project Structure

### Documentation (this feature)

```text
specs/010-add-crypto-adapters/
├── plan.md              # This file
├── research.md          # Research findings
├── data-model.md        # Wire and memory layout design
├── quickstart.md        # Integration guide
└── tasks.md             # Implementation tasks
```

### Source Code (repository root)

```text
src/
└── platform/
    ├── mbedtls_crypto_adapter.c # Mbed TLS adapter implementation
    └── wolfssl_crypto_adapter.c # wolfSSL adapter implementation
tests/
└── unit/
    ├── test_mbedtls_adapter.c   # Mbed TLS adapter tests
    └── test_wolfssl_adapter.c   # wolfSSL adapter tests
```

**Structure Decision**: Code additions will be integrated directly into the `src/platform/` directory and gated using CMake configuration flags.
