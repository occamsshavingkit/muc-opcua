# Feature Specification: Implement Custom Methods, Aes256_Sha256_RsaPss, Server Diagnostics, and Dynamic Node Management

**Feature Branch**: `011-add-opcua-features`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "Implement custom method calls, Aes256_Sha256_RsaPss security policy, server diagnostics nodes, and dynamic node management"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Custom Method Calls (Priority: P1) [MVP]

An integrator building an embedded control application (e.g. on an RTOS) wants to expose executable actions on the OPC UA server (like "Reboot Device", "Start Batch", or "Trigger Calibration") and map them to their local C application handler functions.

**Why this priority**: Crucial for industrial control systems that require command-and-control interactions over OPC UA beyond basic tag reading/writing.

**Independent Test**: Connect an OPC UA client, browse the Address Space, find the custom method, execute it with input arguments, and receive the correct output parameters and status code.

**Acceptance Scenarios**:

1. **Given** a custom method node configured with a callback handler, **When** a client invokes the `Call` service with correct parameters, **Then** the server executes the handler, returns a status of `Good`, and returns the expected output arguments.
2. **Given** a custom method node, **When** a client invokes it with missing or invalid argument types, **Then** the server rejects the invocation and returns `Bad_InvalidArgument` or `Bad_ArgumentsMissing`.

---

### User Story 2 - Aes256_Sha256_RsaPss Security Policy (Priority: P2)

A security auditor or client deployment environment requires the server to support the modern, secure `Aes256_Sha256_RsaPss` security policy to comply with current cybersecurity standards.

**Why this priority**: RSA-PSS provides stronger cryptographic security and resistance to signature verification attacks than older PKCS1-v1_5 schemes.

**Independent Test**: Run a secure connection test with an OPC UA client configured for `Aes256_Sha256_RsaPss` and verify a successful cryptographic handshake completes.

**Acceptance Scenarios**:

1. **Given** a server supporting the `Aes256_Sha256_RsaPss` policy, **When** a client opens a secure channel using this policy, **Then** the asymmetric signature is verified using RSA-PSS and the asymmetric payload is decrypted using RSA-OAEP with SHA-256.

---

### User Story 3 - Standard Server Diagnostics Node Set (Priority: P3)

A SCADA system operator wants to monitor the health and performance of the OPC UA server using standard diagnostic variables (e.g., active session count, rejected request counts).

**Why this priority**: Necessary for enterprise manageability and remote diagnostics in manufacturing networks.

**Independent Test**: Read the standard variables under `Server -> ServerDiagnostics` via an OPC UA client and verify they count events correctly.

**Acceptance Scenarios**:

1. **Given** a running server, **When** a client connects and creates a new session, **Then** the `CurrentSessionCount` and `AccumulatedSessionCount` variables increase accordingly.

---

### User Story 4 - Dynamic Node Management (Priority: P4)

An integrator wants their application (or authorized clients) to add or delete variables and objects in the server's Address Space at runtime as new sensors are discovered or removed.

**Why this priority**: Required for flexible, self-configuring IIoT gateways that dynamically scan local IO buses.

**Independent Test**: Call the `AddNodes` or `DeleteNodes` service and verify the nodes are added to or removed from the Address Space.

**Acceptance Scenarios**:

1. **Given** a running server, **When** a client calls `AddNodes` to insert a new variable node, **Then** the node is added and can immediately be read, written, or browsed.

---

### Edge Cases

- **Argument mismatch**: The Call service receives arguments that don't match the signature defined by the method's input arguments. It must return `Bad_InvalidArgument` or `Bad_TypeMismatch`.
- **Zero-heap constraints**: Dynamic node insertion/deletion must respect memory footprints and avoid heap fragmentation on resource-constrained microcontrollers.
- **PSS Padding Integrity**: RSA-PSS signatures must be parsed and verified with salt length equal to the hash length (32 bytes) or using the maximum salt length (OPC-10000-7 §6.2), avoiding padding oracle vulnerabilities.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST support user-defined custom method callback registration.
- **FR-002**: The `Call` service MUST decode input arguments, dispatch to user handlers, and encode output arguments.
- **FR-003**: The system MUST support the `Aes256_Sha256_RsaPss` security policy URI.
- **FR-004**: The asymmetric signatures for RSA-PSS MUST be verified using RSASSA-PSS padding.
- **FR-005**: The system MUST instantiate standard OPC UA diagnostics nodes under `Root -> Objects -> Server -> ServerDiagnostics`.
- **FR-006**: Diagnostic counters MUST update atomically when network, session, or subscription events occur.
- **FR-007**: The system MUST support `AddNodes` and `DeleteNodes` service sets.
- **FR-008**: Nodes added dynamically MUST be fully integrated into browse, read, and write services.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: The Method Call service conforms to OPC-10000-4 §5.12.2.
- **OPC-002**: The `Aes256_Sha256_RsaPss` policy conforms to OPC-10000-7 §6.2 (signatures use RSASSA-PSS with SHA-256; asymmetric encryption uses RSA-OAEP with SHA-256).
- **OPC-003**: Server Diagnostics variables conform to OPC-10000-5 §6.x.
- **OPC-004**: Dynamic Node Management conforms to OPC-10000-4 §5.5 (AddNodes/DeleteNodes).

### Scope Boundaries *(mandatory)*

- **In Scope**:
  - `Call` service implementation for custom user methods.
  - `Aes256_Sha256_RsaPss` security policy support in host, mbedtls, and wolfssl adapters.
  - A subset of ServerDiagnostics variables (Session, Channel, and Subscription counters).
  - `AddNodes`/`DeleteNodes` service implementations.
- **Out of Scope**:
  - Full GDS (Global Discovery Server) integration.
  - Remote client-initiated `AddNodes` without authorization.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of custom method unit and integration tests pass.
- **SC-002**: Clients can connect using the `Aes256_Sha256_RsaPss` policy.
- **SC-003**: Diagnostic variables are accurately updated and reflect current session/channel/subscription counts.
- **SC-004**: Dynamic variable nodes can be created and deleted at runtime without memory leaks.
- **SC-005**: Flash and RAM memory impact remains within standard profile guidelines.

## Assumptions

- The target microcontroller has sufficient hardware/software support for RSA-PSS and SHA-256.
- Dynamic node management utilizes a fixed-size node pool to guarantee zero runtime heap allocation.
