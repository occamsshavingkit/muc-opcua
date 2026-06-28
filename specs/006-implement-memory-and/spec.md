# Feature Specification: Memory and Performance Optimizations

**Feature Branch**: `006-implement-memory-and`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "Implement the optimizations in the optimization audit report (reusing/unionizing secure scratch buffer, session timeout integer representation, asymmetric unwrap stack reduction)"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Session Timeout 4-byte Integer Representation (Priority: P1)

The server session state footprint is reduced by storing the revised session timeout as a 4-byte integer in milliseconds instead of an 8-byte raw double bits field. The double conversions are handled at the encoding/decoding boundary.

**Why this priority**: Directly reduces static memory (`mu_session_t`) by 8 bytes per session slot, contributing to a smaller server state RAM footprint without altering external behavior.

**Independent Test**: Create a session with a client, request a timeout, verify the server clamps and returns the correct revised timeout in the ResponseHeader, and check that the size of `mu_session_t` is reduced.

**Acceptance Scenarios**:

1. **Given** a CreateSessionRequest with a requested timeout, **When** the server processes it, **Then** the timeout is clamped correctly and returned as a double on the wire, and `sizeof(mu_session_t)` is verified to be 16 bytes (instead of 24) on 32-bit platforms.

---

### User Story 2 - Asymmetric Unwrap Stack Footprint Reduction (Priority: P2)

Stack usage during connection handshakes is heavily reduced by removing large stack buffers (`plain[2048]` and `verify_buf[4096]`) from `mu_asym_chunk_unwrap`. Instead, the function uses the caller-provided `secure_scratch` buffer or a designated scratch buffer passed as a parameter.

**Why this priority**: Avoids stack overflow on microcontrollers (like RP2040) with limited stack space by replacing 6 KB of stack allocation with static/scratch reuse.

**Independent Test**: Run a secure connection handshake, verify the OpenSecureChannel service completes successfully, and verify via stack inspection scripts that stack consumption has dropped by ~6 KB.

**Acceptance Scenarios**:

1. **Given** a secure handshake request, **When** `mu_asym_chunk_unwrap` is executed, **Then** it performs decryption and signature verification without allocating large arrays on the stack, and returns `MU_STATUS_GOOD` on a valid request.

---

### User Story 3 - Default Cipher Context Size Reduction (Priority: P3)

The default compiled size of `MU_CIPHER_CTX_SIZE` is reduced for configurations that use dynamic/pointer-based crypto adapters, allowing the server to save nearly 1 KB of RAM out-of-the-box.

**Why this priority**: Reclaims significant static RAM automatically for common builds without requiring manual integrator tuning.

**Independent Test**: Build the server using the host crypto adapter and verify the static size of `struct mu_server` drops by 960 bytes.

**Acceptance Scenarios**:

1. **Given** a build using the default pointer-based `host_crypto_adapter`, **When** the server is compiled, **Then** `MU_CIPHER_CTX_SIZE` defaults to a smaller value (e.g., 32 bytes) that accommodates a pointer, reducing BSS footprint.

---

### Edge Cases

- **Session Timeout Limits**: Verifying that converting the timeout from a 32-bit integer millisecond representation back to double bits does not lose precision and respects the OPC UA spec bounds.
- **Asymmetric unwrapping on corrupt chunks**: Verifying that reusing caller-provided scratch space still zeroes out decrypted keys/payloads correctly on validation failure, avoiding security info leaks.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `mu_session_t` MUST represent the revised session timeout internally using a 4-byte `opcua_uint32_t` representing milliseconds, instead of an 8-byte raw double bits field.
- **FR-002**: `mu_asym_chunk_unwrap` MUST NOT allocate large stack buffers (such as 2 KB or 4 KB arrays). It MUST accept a scratch buffer pointer and size from the caller for all temporary plain-text decryption and signature verification.
- **FR-003**: `MU_CIPHER_CTX_SIZE` default value MUST be reduced to 32 bytes (or `sizeof(void*)` plus minor safety margin) when compile options indicate pointer-based adaptors are active, while remaining overridable.
- **FR-004**: All modifications MUST compile without warnings and pass all integration and unit tests.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Secure channel asymmetric unwrap behavior MUST strictly follow OPC-10000-6 §6.7.2 asymmetric message chunk processing.
- **OPC-002**: Session timeout clamping and negotiation MUST follow OPC-10000-4 §5.6.2.2.

### Scope Boundaries *(mandatory)*

- **In Scope**: Reordering/refactoring of `mu_session_t`, stack usage reduction in `mu_asym_chunk_unwrap`, and safe narrowing of default cipher contexts.
- **Out of Scope**: Major changes to the underlying cryptography algorithms or shifting from static memory to dynamic heap allocations.
- **Application Headroom Goal**: Reclaim at least **800 bytes of static RAM (BSS)** and **6 KB of stack space** during service execution.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `sizeof(mu_session_t)` is 16 bytes (instead of 24) on 32-bit ARM targets.
- **SC-002**: `mu_asym_chunk_unwrap` uses less than 256 bytes of stack frame space (verified via stack analysis or static check).
- **SC-003**: All unit and integration tests pass successfully with the optimized structs.
- **SC-004**: The default `struct mu_server` BSS size drops by at least **800 bytes** when compiled with pointer-based crypto.

## Assumptions

- **Integrator custom adaptors**: We assume custom crypto adaptors that store a full key schedule in `cipher_ctx` will continue to compile by manually defining `-DMU_CIPHER_CTX_SIZE=512`.
