# Research: Nano Client — Discovery + Read

## Decisions

### D-01: Client state machine
- **Decision**: The client uses its own state machine
  (disconnected → resolving → connecting → secure_channel → session →
  active → closing → disconnected), separate from the server's connection
  state. The client initiates all requests; the server responds.
- **Rationale**: Client role is fundamentally different from server —
  initiates outbound connections, drives the request/response cycle,
  handles timeouts and retries.
- **Alternatives considered**: Sharing the server's connection state
  machine was rejected because server state is inbound-oriented
  (accept, read, dispatch, write response).

### D-02: Client TCP adapter
- **Decision**: The client uses a new TCP adapter interface
  (`mu_client_transport_t`) with the same plug-in model as the server's
  platform adapters. Host implementation uses POSIX sockets.
- **Rationale**: Consistent with the existing platform adapter pattern
  (ADR-0002). Allows embedded targets to provide their own TCP
  implementation without modifying the client core.
- **Alternatives considered**: Embedding POSIX sockets directly in the
  client core would break the embedded-first principle.

### D-03: Shared encoder/decoder
- **Decision**: The client reuses the existing binary encoder/decoder
  from `src/encoding/` without modification. Client request serialization
  and response parsing use the same `mu_binary_writer_t` /
  `mu_binary_reader_t` pattern as the server.
- **Rationale**: The OPC UA Binary encoding is symmetric (same wire format
  for client and server messages). No code duplication needed.
- **Alternatives considered**: Writing separate client-side encoders would
  duplicate ~3 KB of code and create maintenance burden.

### D-04: Service dispatch pattern
- **Decision**: Client services are implemented as direct call paths
  (construct request → encode → write to transport → read response →
  decode → return), not as a generic service dispatch loop like the server.
- **Rationale**: The client sends one request at a time and waits for the
  matching response. A full dispatch loop (used on the server for
  demultiplexing requests from multiple clients) is unnecessary overhead.
- **Alternatives considered**: Using the server's service dispatch was
  considered but rejected — the server dispatches incoming requests to
  handlers, while the client constructs and sends outgoing requests.

### D-05: SecurityPolicy support
- **Decision**: Nano client supports SecurityPolicy Basic256Sha256 and
  SecurityPolicy None, reusing the server's existing crypto stubs.
- **Rationale**: Nano servers typically use Basic256Sha256. The crypto
  implementation already exists in the server; the client just needs the
  asymmetric encrypt/sign and symmetric encrypt/sign functions for the
  SecureChannel handshake.
- **Alternatives considered**: Limiting to SecurityPolicy None only would
  prevent interop with secured servers. Supporting all security policies
  would add too much code for the Nano profile.

### D-06: Profile gating
- **Decision**: Client features gate on `MUC_OPCUA_CLIENT_PROFILE` CMake
  variable, following the server's `MUC_OPCUA_PROFILE` pattern.
  Nano client = `MUC_OPCUA_CLIENT_PROFILE=nano`.
- **Rationale**: Consistency with the server's proven profile tier system
  (ADR-0003). Allows integrators to select client capabilities independently
  of server capabilities.
- **Alternatives considered**: Using a single combined profile was rejected
  — an integrator may want a full server with a nano client, or a nano
  server with a full client.
