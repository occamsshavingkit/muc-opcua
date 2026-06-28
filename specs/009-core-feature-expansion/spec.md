# Feature Specification: Core Feature Expansion

**Feature Branch**: `009-core-feature-expansion`  
**Created**: 2026-06-28  
**Status**: Draft  
**Input**: User description: "Implement Write service, multiple concurrent TCP connections, Aes128_Sha256_RsaOaep and Aes256_Sha256_RsaPss security policies, X.509 certificate user authentication, alarms and conditions (events), and template platform adapters."

## User Scenarios & Testing

### User Story 1 - Write Service for Control (Priority: P1)

Integrators want to control device actuators or change configuration parameters by writing values to target variables in the address space.

**Why this priority**: Core service required for bidirectional interaction (control and configuration), which is essential for standard field devices.

**Independent Test**: Connect an OPC UA client, issue a `WriteRequest` to a mutable variable, and verify that the value changes and the server returns `Good` status code.

**Acceptance Scenarios**:

1. **Given** a variable node with write access enabled, **When** the client sends a `WriteRequest` with a valid `Variant` value matching the data type, **Then** the value is updated and the server returns `Good` (0x00000000).
2. **Given** a variable node, **When** the client sends a `WriteRequest` with an invalid data type, **Then** the server rejects it and returns `Bad_TypeMismatch` (0x80740000).
3. **Given** a read-only variable node, **When** the client sends a `WriteRequest`, **Then** the server rejects the write and returns `Bad_NotWritable` (0x803B0000).

---

### User Story 2 - Multiple Concurrent Clients (Priority: P1)

Field technicians want to connect UAExpert to view server status while the main SCADA/HMI panel is actively collecting telemetry.

**Why this priority**: Required for field diagnostic workflows without disrupting primary SCADA/HMI connections.

**Independent Test**: Establish two concurrent TCP connections to the server, create sessions on both, and verify both can read data simultaneously.

**Acceptance Scenarios**:

1. **Given** a server initialized with multiple connection slots, **When** a second TCP client connects while the first is active, **Then** both connections are maintained concurrently.
2. **Given** the maximum number of connections is reached, **When** a new client attempts to connect, **Then** the server rejects the connection gracefully.

---

### User Story 3 - Modern Security Policies (Priority: P2)

Compliance officers require the OPC UA server to run with modern cryptographic algorithms because `Basic256Sha256` is legacy or deprecated in their security policy.

**Why this priority**: Essential for enterprise security compliance.

**Independent Test**: Connect UaExpert using `Aes128_Sha256_RsaOaep` / `Aes256_Sha256_RsaPss` policies and complete a secure session handshake.

**Acceptance Scenarios**:

1. **Given** the server is configured with a secure endpoint, **When** a client connects using `Aes128_Sha256_RsaOaep` or `Aes256_Sha256_RsaPss`, **Then** the server validates the security policy and successfully completes the secure channel handshake.

---

### User Story 4 - X.509 Certificate User Authentication (Priority: P2)

Integrators want to authenticate users using client-side X.509 certificates to support role-based access control without usernames/passwords.

**Why this priority**: Standard corporate deployment requirement for secure machine-to-machine integrations.

**Independent Test**: Connect using a client certificate, sign the server nonce, and activate the session successfully.

**Acceptance Scenarios**:

1. **Given** a trust list containing the client certificate, **When** the client sends an `ActivateSessionRequest` with a valid `CertificateIdentityToken` and signature, **Then** the server verifies the signature and activates the session.
2. **Given** an untrusted client certificate, **When** the client sends an `ActivateSessionRequest`, **Then** the server rejects it with `Bad_IdentityTokenRejected` (0x80210000).

---

### User Story 5 - Alarms & Conditions (Events) (Priority: P3)

Operators want to receive immediate alarm notifications when a machine variable goes out of bounds (e.g., Temperature > 100°C) without polling the value.

**Why this priority**: Reduces network bandwidth and reaction time for critical alerts.

**Independent Test**: Set up a subscription on the event notifier, trigger an alarm condition, and verify that the client receives the event notification immediately.

**Acceptance Scenarios**:

1. **Given** an event subscription, **When** the temperature variable exceeds the limit, **Then** the server generates an event notification and delivers it via the active publish loop.

### Edge Cases

- **Write Buffer Overflow**: A client sends a write request with a payload larger than the server's receive buffer size. The server must reject it with `Bad_RequestTooLarge` (0x80B80000).
- **Session Timeout on Inactive Connection**: An inactive connection times out. The server must close the secure channel and free resources immediately.
- **Malformed Write Payload**: A client sends a malformed value array or invalid string length during Write. The server must reject with `Bad_DecodingError` (0x80070000).
- **Event Queue Saturation**: Multiple alarms trigger simultaneously. The server must handle overflow by dropping the oldest events or returning an overflow flag.

## Requirements

### Functional Requirements

- **FR-001**: The server MUST implement the `Write` service set (OPC-10000-4 §5.10.4) to write variable values and attributes.
- **FR-002**: The server MUST return `Bad_NotWritable` (0x803B0000) when a write is attempted on read-only attributes or read-only nodes.
- **FR-003**: The server MUST validate write values against the target variable's data type, returning `Bad_TypeMismatch` (0x80740000) on mismatch.
- **FR-004**: The server MUST support a configurable number of concurrent TCP connections (`MU_MAX_CONNECTIONS`) without dynamic heap allocation.
- **FR-005**: The server MUST gracefully reject new connection requests when `MU_MAX_CONNECTIONS` is exceeded.
- **FR-006**: The server MUST implement `Aes128_Sha256_RsaOaep` and `Aes256_Sha256_RsaPss` security policy URIs (OPC-10000-7 §6.2).
- **FR-007**: The server MUST support `CertificateIdentityToken` (NodeId 327) user authentication (OPC-10000-4 §7.36.4).
- **FR-008**: The server MUST verify the user certificate signature during `ActivateSession` using the configured crypto adapter.
- **FR-009**: The server MUST implement event notification generation and publishing (OPC-10000-4 §5.12.1) to support Alarms & Conditions.
- **FR-010**: All new features MUST be gated behind compile options (e.g. `MICRO_OPCUA_SERVICE_WRITE`, `MICRO_OPCUA_MULTIPLE_CONNECTIONS`, `MICRO_OPCUA_EVENTS`) to preserve memory overhead when not used.

### OPC UA Normative Scope

- **OPC-001**: Implemented services/features are Write (OPC-10000-4 §5.10.4), Secure Channel Policies (OPC-10000-7 §6.2), Certificate User Authentication (OPC-10000-4 §7.36.4), and Event/Alarm Notifications (OPC-10000-9).
- **OPC-002**: Unsupported write request features MUST return `Bad_ServiceUnsupported` (0x800B0000) or `Bad_WriteNotSupported` (0x803C0000).
- **OPC-003**: Wire encoding of the Write service structure cites OPC-10000-6 §5.10.4.

### Scope Boundaries

- **In Scope**: Write service (value attribute only), concurrent connections limit management, AES-128/256 security policies, certificate user token validation, simple alarm event generation.
- **Out of Scope**: Writing non-value attributes (except AccessLevel), WS-Security tokens, historic alarm event history.
- **Compatibility Claim**: http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2017
- **Application Headroom Goal**: The example application with Write, multiple connections, and events enabled must fit within 45 KiB flash and 48 KiB RAM on the RP2040.

## Success Criteria

### Measurable Outcomes

- **SC-001**: Client write operations to mutable variables complete successfully in under 5 milliseconds.
- **SC-002**: The server maintains up to the configured number of concurrent TCP connections without dynamic heap allocations.
- **SC-003**: Client authentication using X.509 user certificates succeeds on valid signatures and fails on invalid/untrusted ones.
- **SC-004**: Alarm notifications are delivered to subscribers within 50 milliseconds of the condition trigger.
- **SC-005**: Gating compile options OFF successfully excludes the new feature code, ensuring the baseline Nano profile footprint remains at 16.1 KiB.

## Assumptions

- The platform's network adapter supports accepting and tracking multiple network sockets concurrently.
- The crypto adapter provides signature verification functions for client user certificates.
- Events are queued in static, pre-allocated event queues within the subscription state structure.
