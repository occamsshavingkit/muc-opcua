# Feature Specification: Nano Client - Discovery + Read

**Feature Branch**: `047-nano-client`
**Created**: 2026-07-07
**Status**: Draft
**Input**: User description: "Start the first spec kit implementation for the OPC UA client — Nano Client: Discovery + Read per the roadmap."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Discover and Connect to a Server (Priority: P1)

An embedded developer integrating the library needs to connect their device to an
OPC UA server so they can read sensor values and browse the server's address space.

The library currently has no client-side connection capability. The developer must
be able to discover a server's endpoints, open a SecureChannel, and establish a
session.

**Why this priority**: P1 — connection is the foundation of every client interaction.
Without it, no further client functionality is possible.

**Independent Test**: `ctest -R test_nano_client_connect` — client connects to the
existing muc-opcua server, opens a SecureChannel, and establishes a session with
Basic256Sha256.

**Acceptance Scenarios**:

1. **Given** a running OPC UA server at a known endpoint URL, **When** the client
   calls GetEndpoints, **Then** the client receives a list of available endpoint
   descriptions with security policy URIs and certificates.
2. **Given** a selected endpoint with SecurityPolicy Basic256Sha256, **When** the
   client opens a SecureChannel, **Then** the client and server negotiate a
   channel with the agreed security policy and a non-zero channel ID.
3. **Given** an open SecureChannel, **When** the client calls CreateSession
   followed by ActivateSession, **Then** the client receives a session ID and
   authentication token, and the session state is established.
4. **Given** an active session, **When** the client calls CloseSession, **Then**
   the server releases the session resources and the client marks the session
   as closed.

### User Story 2 - Read Variable Values (Priority: P1)

A developer needs to read OPC UA variable values from a server to monitor sensor
data or device state in their embedded application.

**Why this priority**: P1 — reading values is the primary purpose of a Nano client.
Without Read, the client cannot deliver useful data.

**Independent Test**: `ctest -R test_nano_client_read` — client reads a known
variable from the server and verifies the value matches expectations.

**Acceptance Scenarios**:

1. **Given** an active session and a known NodeId, **When** the client calls Read
   on that NodeId with an appropriate timestampsToReturn and maxAge, **Then** the
   client receives the variable's value, status code, and source timestamp.
2. **Given** an active session and a NodeId that does not exist, **When** the
   client calls Read, **Then** the client receives Bad_NodeIdUnknown and handles
   it gracefully.
3. **Given** an active session and a NodeId of an Object node (not a Variable),
   **When** the client calls Read, **Then** the client receives
   Bad_NodeIdInvalid.

### User Story 3 - Browse the Address Space (Priority: P2)

A developer needs to navigate the server's address space to discover available
nodes, their types, and their relationships.

**Why this priority**: P2 — browsing is needed for discovery and exploration but
is not required for reading known variables. Developers with prior knowledge of
NodeIds can read without browsing.

**Independent Test**: `ctest -R test_nano_client_browse` — client browses the
server's objects folder and verifies the expected child nodes are returned.

**Acceptance Scenarios**:

1. **Given** an active session and the root node (NodeId 84), **When** the client
   calls Browse with a reasonable nodeClassMask and resultMask, **Then** the
   client receives the child nodes of the root with their BrowseNames and
   NodeClasses.
2. **Given** a Browse result with a continuation point, **When** the client calls
   BrowseNext, **Then** the client receives the next batch of results.
3. **Given** a NodeId and a relative path, **When** the client calls
   TranslateBrowsePathsToNodeIds, **Then** the client receives the target NodeId
   matching the path.

### Edge Cases

- What happens when the server endpoint URL is unreachable or DNS resolution
  fails? The client should time out with a configurable timeout and report an
  appropriate connection error.
- What happens when the server rejects the SecureChannel Open request (e.g.,
  unsupported security policy)? The client returns the server's StatusCode.
- What happens when the session is lost (server restart, network interruption)?
  The client detects the broken SecureChannel and returns an appropriate
  disconnection status.
- What happens when a message chunk exceeds the configured receive buffer size?
  The client rejects the chunk with Bad_DecodingError or Bad_CommunicationError.
- How does the feature preserve application flash/RAM headroom on the target
  microcontroller class? The client uses the same zero-heap, caller-provided
  buffer model as the server. Maximum session count and buffer sizes are
  compile-time constants.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a `mu_client_init` function that initializes a
  client instance with caller-provided storage and configuration.
- **FR-002**: System MUST provide a `mu_client_connect` function that resolves a
  hostname, opens a TCP connection, negotiates an OPC UA TCP Hello/Acknowledge,
  and opens a SecureChannel.
- **FR-003**: System MUST provide a `mu_client_get_endpoints` function that
  sends a GetEndpoints request and parses the response.
- **FR-004**: System MUST provide a `mu_client_create_session` function that
  sends a CreateSession request and handles the response, including the server's
  session ID and authentication token.
- **FR-005**: System MUST provide a `mu_client_activate_session` function that
  sends an ActivateSession request with the user identity token.
- **FR-006**: System MUST provide a `mu_client_read` function that sends a Read
  request for one or more nodes and returns the values with status codes and
  timestamps.
- **FR-007**: System MUST provide a `mu_client_browse` function that sends a
  Browse request and returns the references of the specified node.
- **FR-008**: System MUST provide a `mu_client_browse_next` function that
  retrieves the next batch of browse results when a continuation point is
  returned.
- **FR-009**: System MUST provide a `mu_client_translate_browse_paths_to_node_ids`
  function that resolves a relative path to a NodeId.
- **FR-010**: System MUST provide a `mu_client_close_session` function that sends
  a CloseSession request and transitions the client state appropriately.
- **FR-011**: System MUST provide a `mu_client_disconnect` function that closes
  the SecureChannel and TCP connection.
- **FR-012**: All client functions MUST accept caller-provided buffers for
  request serialization and response parsing — no heap allocation in the
  protocol path.
- **FR-013**: The client MUST support SecurityPolicy Basic256Sha256 and
  SecurityPolicy None for the Nano profile.
- **FR-014**: All client API functions MUST return an OPC UA StatusCode to
  indicate success or failure.
- **FR-015**: The client MUST detect a lost connection (SecureChannel timeout,
  TCP disconnect) and report it via StatusCode.

### OPC UA Normative Scope *(mandatory for protocol features)*

- **OPC-001**: Target OPC UA profile is the Nano Embedded UA Client Profile
  (OPC-10000-7). Conformance units: Discovery Client (GetEndpoints), Base Client
  (SecureChannel, Session), Attribute Client (Read), View Client (Browse,
  BrowseNext, TranslateBrowsePathsToNodeIds).
- **OPC-002**: Implemented services per OPC-10000-4: §5.4.2 GetEndpoints,
  §5.5.2 OpenSecureChannel, §5.5.3 CloseSecureChannel, §5.6.2 CreateSession,
  §5.6.3 ActivateSession, §5.6.4 CloseSession, §5.10.2 Read, §5.8.2 Browse,
  §5.8.3 BrowseNext, §5.8.4 TranslateBrowsePathsToNodeIds.
- **OPC-003**: Unsupported services (Write, Subscribe, Call, HistoryRead, etc.)
  are not compiled into the Nano client profile. If called at the API level,
  they return Bad_NotImplemented.
- **OPC-004**: Wire encoding follows OPC-10000-6 §5 Binary Encoding and §6 OPC
  UA TCP. MessageChunk structure per §6.7.2, Hello/Acknowledge per §7.1.2.3/§7.1.2.4,
  OPC UA TCP per §7.2. SecureChannel framing uses the existing server
  implementation's encoder/decoder.
- **OPC-005**: Conformance status is experimental. Tests verify interop against
  the existing muc-opcua server. No CTT verification in this spec.

### Scope Boundaries *(mandatory)*

- **In Scope**:
  - Client API for connection lifecycle (init, connect, disconnect, destroy)
  - GetEndpoints, OpenSecureChannel, CloseSecureChannel
  - CreateSession, ActivateSession, CloseSession
  - Read (single node and multiple nodes)
  - Browse, BrowseNext, TranslateBrowsePathsToNodeIds
  - SecurityPolicy Basic256Sha256 and None (reusing server's crypto stubs)
  - OPC UA Binary encoding on opc.tcp
  - Host-side tests with interop against the existing muc-opcua server
  - Client state machine (disconnected → connecting → connected → session →
    active → disconnected)
  - Caller-provided buffer model matching the server's existing pattern
- **Out of Scope**:
  - Subscriptions, MonitoredItems (future spec — Micro Client)
  - Write
  - Method Call
  - HistoryRead, HistoryUpdate
  - Events, Alarms, Conditions
  - PubSub
  - JSON or HTTPS transport
  - Dynamic node registration
  - FindServers (multi-server discovery)
  - RegisterServer2
- **Compatibility Claim**: Nano Embedded UA Client Profile conformance unit set
  (experimental, no CTT verification). Interop verified against muc-opcua server
  in host tests.
- **Application Headroom Goal**: Nano client adds ~8-12 KB flash when linked
  alongside the existing server (shared encoding/security). Standalone client
  targets ~6-10 KB flash. Zero additional heap use.

### Key Entities *(include if feature involves data)*

- **mu_client_t**: Client instance struct holding connection state, session
  handle, SecureChannel state, and configuration. Caller-provided storage.
- **mu_endpoint_description_t**: Describes a server endpoint returned by
  GetEndpoints, including endpoint URL, security policy URI, server certificate,
  and security mode.
- **mu_session_handle_t**: Opaque handle representing an active session,
  including the server-assigned session ID and authentication token.
- **mu_read_value_t**: A single read result containing a Variant value,
  StatusCode, and source timestamps.
- **mu_browse_result_t**: A browse result containing references with their
  NodeIds, BrowseNames, DisplayNames, and NodeClasses.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A C client can connect to any standard OPC UA server supporting
  Basic256Sha256, open a SecureChannel and session, read a variable, and browse
  the root node — all in under 5 seconds on host.
- **SC-002**: The client successfully completes get-endpoints → open-channel →
  create-session → activate-session → read → browse → close-session →
  disconnect round-trip against the muc-opcua server without errors.
- **SC-003**: All specified error conditions (unreachable server, bad NodeId,
  unsupported security policy, connection loss) return the correct OPC UA
  StatusCodes.
- **SC-004**: No heap allocation occurs on any client I/O or protocol path.
  All buffers are caller-provided.
- **SC-005**: The client passes all host-side unit and interop tests when
  gated behind `MUC_OPCUA_CLIENT_PROFILE=nano`.
- **SC-006**: Existing server tests continue to pass unchanged — no server
  regression from the shared encoding/transport layer.

## Assumptions

- The client reuses the existing server's encoder/decoder, StatusCode system,
  NodeId/Variant types, and security policy stubs. No forking of shared code.
- The target OPC UA server for interop testing is the existing muc-opcua
  server built with the `standard` profile.
- SecurityPolicy Basic256Sha256 uses the same pluggable crypto backend as the
  server (implemented or stubbed) for asymmetric/symmetric encryption and
  signing.
- The client-side SecureChannel state machine mirrors the server's existing
  implementation, adapted for the client role (initiates channel, sends
  OpenSecureChannel request, receives response).
- Client API follows the same pattern as the server API: init with
  caller-provided storage, process-single-step poll model, no background
  threads.
- The host test environment has network access to localhost for server/client
  interop tests.
- Nano profile gating uses `MUC_OPCUA_CLIENT_PROFILE=nano` (new CMake option),
  following the same pattern as `MUC_OPCUA_PROFILE` for the server.
