# Conformance: Reverse Connect Facet (spec 065)

Reverse Connect (server-initiated connectivity, OPC-10000-6 §7.1.3) lets a server behind a
firewall with no open inbound ports **initiate** the TCP connection to a client. It is an
**optional transport capability** — no Server Profile this server advertises mandates it —
enabled by `MUC_OPCUA_REVERSE_CONNECT` (standard/full).

## The contract we now honor (previously broken)

Per **OPC-10000-6 §7.1.3**: *"If the Server creates the TransportConnection the first Message
shall be a ReverseHello sent to the Client. If the Client accepts the connection, it sends a
Hello message back."* The client may close the connection if it receives no Hello/ReverseHello
in time.

Before spec 065, `MUC_OPCUA_REVERSE_CONNECT` dispatched `tcp_adapter.connect()` instead of
`listen()` but then **sent nothing** and waited to receive a Hello — so a real reverse-connect
deployment never completed (the client waits for the server's ReverseHello and times out).
This facet closes that gap: after `connect()` succeeds, the server now sends the ReverseHello
as the first message, and the existing HELLO→ACK path then runs normally
(RHE → HEL → ACK → OPN …).

## ReverseHello message (OPC-10000-6 §7.1.2.6, Table 77)

Fixed transport header + two String fields (same header shape as HEL/ACK/ERR):

| Field | Type | Value / constraint |
|---|---|---|
| MessageType | 3 bytes | `RHE` |
| ChunkType | 1 byte | `F` |
| MessageSize | UInt32 | total message length |
| ServerUri | String | `config.application_uri` (the server ApplicationUri); encoded value < 4096 bytes |
| EndpointUrl | String | `config.endpoint_url`; encoded value < 4096 bytes |

Built by `mu_tcp_create_reverse_hello` (`src/core/tcp_connection.c`) and emitted in
`mu_server_init` (`src/core/server/init.c`).

## Integrator responsibility

The library provides the `connect` vtable slot on `mu_tcp_adapter_t`, calls it, and sends the
ReverseHello. The **outbound `connect` adapter implementation is the integrator's**: the
reference host adapter (`host_tcp_adapter.c`) leaves `connect` NULL (accept-only). Config
validation enforces that when `reverse_connect_url` is set, the `connect` adapter op **and**
`application_uri`/`endpoint_url` (the ReverseHello's ServerUri/EndpointUrl) are provided.

## Evidence

- `test_reverse_connect`:
  - `test_reverse_hello_encoder_produces_valid_rhe` — the RHE wire bytes: `RHEF`, declared
    MessageSize == actual, decoded ServerUri/EndpointUrl.
  - `test_reverse_connect_emits_reverse_hello_first` — `mu_server_init` with
    `reverse_connect_url` sends an `RHE` as the first bytes on the connection.
  - `test_reverse_connect_url_calls_connect_not_listen` / `_requires_connect_callback` /
    `_requires_application_uri` — dispatch + config validation.

## Out of scope

- A host-platform outbound `connect` adapter (integrator supplies it).
- Reconnect/backoff policy; the client side of reverse connect (this is a Server facet).
- Receiving an inbound ReverseHello (servers send, never receive, ReverseHello).
