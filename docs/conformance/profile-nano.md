# Conformance: Nano Embedded Device 2017 Server Profile

This server targets the **Nano Embedded Device 2017 Server Profile**
(`http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017`, OPC 10000-7).
Per the OPC Foundation profile definition, Nano is *functionally equivalent to the
Core 2017 Server Facet* and requires the OPC UA TCP binary transport.

## The profile ladder (for orientation)
- **Nano** = Core Server Facet + UA-TCP UA-SC UA-Binary + SecurityPolicy None + Anonymous.
- **Micro** = Nano + subscriptions (Embedded Data Change Subscription facet) + ≥2 sessions.
- **Embedded** = Micro + security policies + the Standard DataChange Subscription facet
  + full type-system exposure.

micro-opcua targets **Nano** and has already implemented Basic256Sha256 (an
Embedded-tier feature) ahead of schedule; subscriptions (Micro) are out of scope.

## Profile constraints honoured
- OPC UA TCP transport only (`opc.tcp`, UA-SC, UA-Binary).
- SecurityPolicy None + Anonymous identity are the Nano baseline (Basic256Sha256 is
  additionally supported but is above Nano).
- Single client / SecureChannel / Session.
- No Subscriptions, MonitoredItems, Methods, Write, History, or NodeManagement.
- Diagnostic Objects/Variables are optional for Nano and are not exposed.
- Exposing the full type system is optional for Nano (no custom types are used).

## Implemented surface
- **Transport / SecureChannel / Session:** HELLO/ACK, OpenSecureChannel,
  CloseSecureChannel, Create/Activate/CloseSession.
- **Discovery:** GetEndpoints, FindServers.
- **Attribute:** Read.
- **View:** Browse, BrowseNext, TranslateBrowsePathsToNodeIds, RegisterNodes,
  UnregisterNodes (the full View Service Set).
- **Address Space / Base Info:** a library-default standard node set
  (`src/address_space/base_nodes.c`) — the Server object hierarchy, ServerStatus
  (+ State = Running), ServerCapabilities (ServerProfileArray, LocaleIdArray,
  OperationLimits), NamespaceArray, ServerArray — resolved as a fallback after the
  integrator's address space.

- **ServerStatus timestamps:** `ServerStatus.CurrentTime` is served live from the
  time adapter and `StartTime` from the value captured at init, via runtime-bound
  value sources in the (caller-owned) server struct.

## Remaining to full Nano (see [status.md](status.md))
- CTT verification — not yet run; `profile-targeting` until it passes. The
  implementation surface is otherwise complete for the profile.
