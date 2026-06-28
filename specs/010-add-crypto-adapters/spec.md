# Feature Specification: Implement Mbed TLS and wolfSSL platform adapters

**Feature Branch**: `010-add-crypto-adapters`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "Implement Mbed TLS and wolfSSL platform adapters"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Mbed TLS Platform Adapter (Priority: P1)

An integrator compiling the server for an ARM Cortex-M micro-controller wants to use ARM's official `Mbed TLS` library for cryptographic operations instead of OpenSSL (since OpenSSL is too large/incompatible for embedded targets). The integrator wants a standard, drop-in `mu_crypto_adapter_t` implementation for Mbed TLS that compiles under `-ffreestanding` and does not use any dynamic heap allocation.

**Why this priority**: Highly critical for embedded deployment constraints since Mbed TLS is the standard for Cortex-M targets.

**Independent Test**: Build the server library linking against Mbed TLS, start the server, and verify a secure channel handshake completes successfully using an OPC UA client.

**Acceptance Scenarios**:

1. **Given** a server configuration with the Mbed TLS crypto adapter, **When** a client initiates a secure channel handshake using Basic256Sha256, **Then** all signature verification and decryption operations succeed and the session activates.
2. **Given** a server configuration with the Mbed TLS crypto adapter, **When** a client sends a message with an invalid signature, **Then** the adapter rejects the signature and the connection is closed.

---

### User Story 2 - wolfSSL Platform Adapter (Priority: P2)

An integrator using `wolfSSL` in their RTOS project wants to use wolfSSL for the cryptographic adapter. They want a standard, drop-in `mu_crypto_adapter_t` implementation for wolfSSL that compiles under freestanding settings.

**Why this priority**: wolfSSL is a popular choice for commercial embedded projects and RTOS environments.

**Independent Test**: Build the server library linking against wolfSSL, start the server, and verify a secure channel handshake completes successfully.

**Acceptance Scenarios**:

1. **Given** a server configuration with the wolfSSL crypto adapter, **When** a client initiates a secure channel handshake using Basic256Sha256, **Then** all signature verification and decryption operations succeed and the session activates.

---

### User Story 3 - Configurable Build and Selection (Priority: P2)

An integrator wants to select which adapter to build (OpenSSL, Mbed TLS, or wolfSSL) via CMake options (e.g. `MICRO_OPCUA_HAVE_MBEDTLS=ON` or `MICRO_OPCUA_HAVE_WOLFSSL=ON`) and compile the selected adapter source files without symbol collisions.

**Why this priority**: Enables clean builds and gates unused dependencies out of the target binary.

**Independent Test**: Build the project with various CMake options (e.g., `-DMICRO_OPCUA_HAVE_MBEDTLS=ON`) and confirm only the selected adapter is compiled and linked.

**Acceptance Scenarios**:

1. **Given** `MICRO_OPCUA_HAVE_MBEDTLS` is enabled, **When** compiling the library, **Then** the Mbed TLS adapter is compiled and linked, and OpenSSL is not required.

---

### Edge Cases

- **Bad padding/keys**: How does the adapter handle malformed input buffers, mismatched key sizes, or incorrect RSA padding blocks? It must return `MU_STATUS_BAD_DECODINGERROR` or `MU_STATUS_BAD_SECURITYCHECKSFAILED` instead of crashing or leaking memory.
- **Zero heap constraint**: Mbed TLS and wolfSSL must be configured/used in a way that respects the `-ffreestanding` and zero-heap hot-path constraints.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Mbed TLS adapter MUST implement all functions of `mu_crypto_adapter_t` (RSA decryption, signing, verification, symmetric encryption/decryption, HMAC, key derivation) using the Mbed TLS APIs.
- **FR-002**: The wolfSSL adapter MUST implement all functions of `mu_crypto_adapter_t` using the wolfSSL APIs.
- **FR-003**: The adapters MUST not perform any dynamic memory allocations (no `malloc`/`free`) on the hot path (handshake, encrypt, decrypt).
- **FR-004**: The compile-time flags `MICRO_OPCUA_HAVE_MBEDTLS` and `MICRO_OPCUA_HAVE_WOLFSSL` MUST gate the compilation and inclusion of the respective adapter source files.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Cryptographic algorithms MUST match the requirements of OPC-10000-7 §6.2 (Basic256Sha256 and Aes128_Sha256_RsaOaep policies).
- **OPC-002**: Signature verification uses RSASSA-PKCS1-v1_5 with SHA-256.
- **OPC-003**: Asymmetric decryption uses RSA-OAEP with SHA-1.

### Scope Boundaries *(mandatory)*

- **In Scope**: Platform crypto adapters for Mbed TLS and wolfSSL implementing `mu_crypto_adapter_t` under a freestanding C11 environment.
- **Out of Scope**: Packaging/downloading wolfSSL or Mbed TLS source code (assumed to be pre-installed/provided by the system or target build environment).
- **Compatibility Claim**: Fully compatible with Basic256Sha256 and Aes128_Sha256_RsaOaep client connections.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of integration and unit tests pass when compiled with the Mbed TLS adapter.
- **SC-002**: 100% of integration and unit tests pass when compiled with the wolfSSL adapter.
- **SC-003**: Thumb binary size addition for either adapter is under 4 KB.
- **SC-004**: Zero heap allocations are verified during active secure channel communication.

## Assumptions

- Mbed TLS / wolfSSL libraries are available on the compilation path when their respective CMake flags are enabled.
