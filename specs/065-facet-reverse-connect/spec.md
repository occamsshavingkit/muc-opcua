# Spec 065: Reverse Connect Facet (B6)

Project-B facet B6 (roadmap `docs/superpowers/specs/2026-07-09-...-roadmap.md`). Scope
stance (per B1–B5, locked with user): **follow the spec** — ground the requirement, fix the
mandatory correctness gap, don't gold-plate.

## Grounding OUTCOME

Reverse Connect is an **OPTIONAL transport capability** (OPC-10000-6 §7.1.3), not a
profile-mandated Server Facet — the UA-TCP transport facet (profile-DB id 219) has only
`Protocol UA TCP` / `UA Binary Encoding` / `UA Secure Conversation`, no reverse-connect CU,
and no profile we advertise mandates it. **But** the spec is unambiguous that *when* a server
initiates the connection, the ReverseHello is mandatory-first:

> OPC-10000-6 §7.1.3: "If the Server creates the TransportConnection the first Message shall
> be a **ReverseHello** sent to the Client. If the Client accepts the connection, it sends a
> Hello message back." The Client "may close the TransportConnection after a period of time
> if it does not receive a Hello or ReverseHello Message."

**The existing `MUC_OPCUA_REVERSE_CONNECT` is BROKEN, not merely inert** (unlike spec 064's
dead-but-harmless counters). It ships in `standard`/`full`: config field
`reverse_connect_url` + an `init.c` branch that calls `tcp_adapter.connect()` instead of
`listen()` — but it then **sends nothing** and falls into the receive loop waiting for a
HELLO. Per §7.1.3 that connection never completes: the client is waiting for the server's
ReverseHello and times out. So the server advertises a reverse-connect option that produces a
non-conformant, non-functional connection. This is a real correctness gap in a shipped
feature (mirror of spec 063's queue-depth gap), and offering the option **obligates** sending
the RHE.

Confirmed by gap analysis: no `RHE` message-type constant, no ReverseHello encoder, no RHE
emission, no RHE→HEL/ACK ordering exist anywhere; the tests only assert connect-vs-listen
dispatch (no wire bytes).

### Grounded ReverseHello message (OPC-10000-6 §7.1.2.6, Table 77; §7.1.2.2)

- Header (same fixed shape as HEL/ACK/ERR): message type **`RHE`** (3 bytes) + chunk type
  **`F`** + `MessageSize` (UInt32).
- Body: `ServerUri` (String, the server's `ApplicationUri`; encoded value < 4096 bytes),
  `EndpointUrl` (String, one of the server's endpoint URLs; < 4096 bytes).

## Requirements

**FR-1 — ReverseHello encoder.** Add `mu_tcp_create_reverse_hello(config, buf, &len)` in
`tcp_connection.c` (mirroring `mu_tcp_create_error_message`): write `'R','H','E','F'`, a
placeholder `MessageSize`, then `ServerUri` = `config->application_uri` and `EndpointUrl` =
`config->endpoint_url` as OPC UA Strings, then backpatch `MessageSize`. Enforce the < 4096
per-field limit → `Bad_...` on overflow. Gated on `MUC_OPCUA_REVERSE_CONNECT`.

**FR-2 — Emit the RHE first (the correctness fix).** In the `init.c` reverse-connect branch,
after `connect()` succeeds and the outbound connection handle is registered, send the
ReverseHello as the first message on that connection *before* entering the receive loop
(OPC-10000-6 §7.1.3). The subsequent HEL from the client is then handled by the existing
`process_message` HELLO path (RHE → HEL → ACK). Requires `application_uri`/`endpoint_url` to
be configured when `reverse_connect_url` is set (extend `mu_server_config_validate`).

**FR-3 — Conformance doc + status.** `docs/conformance/reverse-connect.md` (the §7.1.3 flow,
the RHE message layout, that it is an optional capability whose contract we now honor, the
integrator's `connect` adapter responsibility); `status.md` + `claim_test_map.md` rows.

**FR-4 — Size + matrix.** Measure `.text` (encoder + emission add a little to standard/full);
`full` stays under the 128 KiB stopper; size ledger; all-profile matrix + sanitizers green;
nano/micro/embedded byte-identical (facet off there).

## Verification (test-first)

- **RHE encoder** — assert exact wire bytes: `'R','H','E','F'`, declared `MessageSize` ==
  actual length, decoded `ServerUri`/`EndpointUrl` strings match config; over-length field
  rejected.
- **Emit-on-connect** — a mock `connect` adapter whose `write` captures the buffer; assert
  `mu_server_init` (with `reverse_connect_url`) sent an `RHE` message as the first bytes on
  the connection (extends the existing `test_reverse_connect.c`, which today only checks
  connect-vs-listen dispatch).
- **Config validation** — `reverse_connect_url` set but `application_uri`/`endpoint_url`
  missing → `mu_server_config_validate` non-GOOD.
- Full matrix green; nano/micro/embedded unchanged.

## Out of scope

- A server-side outbound `connect` adapter implementation for the host platform
  (`host_tcp_adapter.c` leaves `connect` NULL) — integrator responsibility; the library
  provides the vtable slot, calls it, and now sends the RHE. Tests use a mock adapter.
- Reconnect/backoff policy and the client-side of reverse connect (this is a Server facet).
- Receiving/decoding an inbound RHE (servers send, never receive, ReverseHello).

## On approval

Speckit plan/tasks, execute test-first, PR against `main` with a merge commit. B6 of Project
B; remeasure `full` `.text` and continue to B7 (client-redundancy) while under the stopper.
