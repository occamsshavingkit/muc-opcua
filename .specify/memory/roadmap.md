<!--
SYNC IMPACT REPORT
==================
Version change: 1.0.1 → 1.0.2
Bump rationale: PATCH — Nano client implementation has started but is not complete

Changes this revision:
  - Marked spec 001 — Nano Client: Discovery + Read as in-progress
  - Preserved spec dir for spec 001 as specs/047-nano-client/

Specs affected: 001 — Nano Client: Discovery + Read
Open questions added/resolved: none

Notes: Nano client scaffolding and local verification gates exist, but real OPC UA
wire-service interop tasks remain open. Do not claim implemented or verified.
-->

# muc-opcua — Spec Roadmap

Living, non-binding map of the specs planned for muc-opcua. It is **not a
commitment to order or scope** — it captures the spec-specific discussion,
decisions, technology choices, outcomes, and constraints surfaced during the
constitution and grilling phases so they are not lost before the spec that needs
them is written. Specs are scoped and clarified when they are actually started.
Foundations: the project [constitution](constitution.md).

Status legend (lifecycle): **undecided** · **needs-info** · **planned** ·
**specced** · **in-progress** · **implemented** · **verified** · **deferred** ·
**abandoned**.

---

## Vision & End States

- A complete OPC UA client library (bundled in the same repo as the server)
  supporting the Standard UA Client Profile, built on the same C11/no-heap core.
- Same profile tier system as the server: nano → micro → embedded → full,
  gated on `MUC_OPCUA_CLIENT_PROFILE` variant, so integrators pay only for
  the client service surface they need.
- Shared transport, encoding, and security layer between client and server,
  with no server feature leaking into the client and vice versa.

## Constraints & Decisions

- **C-01 — Embedded-first C core:** The client uses the same freestanding C11,
  zero-heap protocol hot path as the server. Caller-provided buffers,
  compile-time capacity limits, no `malloc` on any I/O or dispatch path.
- **C-02 — Shared infrastructure:** Client builds on the existing OPC UA Binary
  encoder/decoder, SecureChannel framing, and security policy stubs. No
  duplication of the wire layer.
- **C-03 — Profile gating via CMake:** Each client tier maps to a set of
  `MUC_OPCUA_CLIENT_*` compile definitions managed identically to the server's
  `MUC_OPCUA_PROFILE` system. `MUC_OPCUA_CLIENT_PROFILE` selects the tier.
- **C-04 — Integrity isolation:** Client state (endpoint cache, session handle,
  subscription client state) is separate from server state. A build can include
  one or both. Platform adapter interfaces are extended for client-side I/O.

## Planned Specs

### 001 — Nano Client: Discovery + Read  [status: in-progress]

- **Description:** Minimal viable OPC UA client implementing the Nano Embedded
  UA Client Profile: discover endpoints, open a SecureChannel, create/activate
  a session, read variable values, and browse the server's address space.
- **Outcome:** A C client can connect to any standard OPC UA server, authenticate,
  read values, and navigate the node tree via Browse.
- **Scope (in):**
  - FindServers / GetEndpoints (Discovery Service Set)
  - OpenSecureChannel / CloseSecureChannel (Security)
  - CreateSession / ActivateSession / CloseSession
  - Read (Attribute Service Set)
  - Browse / BrowseNext (View Service Set)
  - OPC UA Binary encoding on opc.tcp
  - Pluggable SecurityPolicy (None + Basic256Sha256)
  - Host tests + interop against the existing muc-opcua server
- **Scope (out):**
  - Subscriptions, MonitoredItems
  - Write, Call, HistoryRead
  - Events, Alarms
  - PubSub, JSON, HTTPS
- **Depends on:** none (shared transport/encoding layer already exists)
- **Governed by:** C-01, C-02, C-03, C-04; ADR-0001, ADR-0002, ADR-0003
- **Spec dir:** `specs/047-nano-client/`
- **Notes:** First client spec. Nano profile is the smallest client conformance
  unit from OPC-10000-7; targeted flash budget ~8-12 KB when server is also
  linked (shared encoding/transport). Implementation order follows the service
  call chain: discover → connect → authenticate → read → browse. Current work has
  API/state-machine scaffolding and local fake-transport tests; full OPC UA TCP
  service encoding/decoding against the muc-opcua server remains open.

### 002 — Micro Client: Subscriptions  [status: planned]

- **Description:** Adds data-change subscriptions and monitored items to the
  nano client, targeting the Micro Embedded UA Client Profile. The client can
  subscribe to variables and receive data change notifications.
- **Outcome:** A C client can create subscriptions, add monitored items with
  data change filters, and process publish responses containing notification
  messages.
- **Scope (in):**
  - CreateSubscription / ModifySubscription / DeleteSubscription
  - CreateMonitoredItems / ModifyMonitoredItems / DeleteMonitoredItems
  - SetMonitoringMode
  - Publish / Republish
  - DataChange notification processing
  - Queue management for sampled values
- **Scope (out):**
  - Events, Alarms, Conditions
  - Method calls
  - History
- **Depends on:** 001 (Nano Client)
- **Governed by:** C-01, C-03
- **Spec dir:** _not yet created_
- **Notes:** Subscription client is more complex than the server side because
  the client drives the Publish request loop. Must handle keep-alive, timeout,
  and subscription transfer.

### 003 — Embedded Client: Methods + Events  [status: planned]

- **Description:** Extends the client to support Method calls, Event
  subscriptions, and Write, targeting the Embedded UA Client Profile.
- **Outcome:** A C client can call remote methods, subscribe to event
  notifcations, and write values to the server.
- **Scope (in):**
  - Call (Method Service Set)
  - Write (Attribute Service Set)
  - EventFilter + Event notification processing
  - HistoryRead (basic)
  - SetTriggering
- **Scope (out):**
  - Complex type dynamic registration (deferred — reuses server's infrastructure
    when ready)
  - Auditing client-side events
- **Depends on:** 002 (Micro Client)
- **Governed by:** C-01, C-03
- **Spec dir:** _not yet created_
- **Notes:** Event subscription on the client side requires parsing the event
  structure received in notification messages — reuses the server's event type
  definitions.

### 004 — Full Client: Remaining Services  [status: needs-info]

- **Description:** Completes the Standard UA Client Profile with remaining
  services: NodeManagement, RegisterNodes, TranslateBrowsePathsToNodeIds,
  Query, Method call with complex types, and advanced HistoryRead/HistoryUpdate.
- **Outcome:** A C client capable of exercising every service set defined for
  the Standard UA Client Profile in OPC-10000-4.
- **Scope (in):** _to be defined — depends on gap analysis after specs 001-003_
- **Scope (out):** _to be defined_
- **Depends on:** 003 (Embedded Client)
- **Spec dir:** _not yet created_
- **Notes:** Needs research to identify exactly which OPC-10000-4 services are
  required for Standard UA Client conformance and which are optional. Status
  will move to `planned` when the gap analysis is done.

## Open Questions

- **Q1 — Standard UA Client Profile scope:** What is the exact set of services
  and conformance units required by OPC-10000-7 for the Standard UA Client
  Profile? Needs research before spec 004.

- **Q2 — Client-side SecurityPolicy negotiation:** Does the client need to
  support all security policies that the server does, or only the ones it
  initiates? Tied to C-01.

- **Q3 — Shared transport model:** Should the client own its TCP socket
  (platform adapter) or share the server's poll loop? Impacts C-04 and the
  adapter interface design.

## Cross-Cutting Notes

- The client reuses the server's binary encoder/decoder, StatusCode system,
  NodeId/Variant types, and security policy stubs. No forking of shared code.
- The client state machine (disconnected → connecting → connected → session →
  subscribed) mirrors the server's connection model but is independent.
- Test strategy: interop tests against the existing muc-opcua server, plus
  the OPC UA .NET reference client as a secondary validation target.

---

**Version**: 1.0.2 | **Ratified**: 2026-07-07 | **Last Amended**: 2026-07-07
