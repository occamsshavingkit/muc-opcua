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

muc-opcua provides profile-targeted CMake configurations for Nano-, Micro-,
and Embedded-oriented feature sets; these remain profile-targeting labels scoped
to OPC-10000-7 §4.2/§4.3 evidence.

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
- **View:** Browse, BrowseNext, TranslateBrowsePathsToNodeIds. RegisterNodes /
  UnregisterNodes (OPC-10000-4 §5.9.5 / §5.9.6) are optional for Nano and are
  compiled only in the full profile (`MUC_OPCUA_SERVICE_REGISTER_NODES`, OFF here);
  a Nano build returns `Bad_ServiceUnsupported` for them (`test_profile_surface`).
- **Address Space / Base Info:** Base Information is optional for the Nano profile
  (OPC-10000-7 Core Server Facet), and the Nano build does **not** compile the
  standard node set (`MUC_OPCUA_BASE_NODES` is OFF) — a Nano server exposes the
  integrator-supplied address space only (`test_profile_surface`). The library's
  standard Base Information node set (`src/address_space/base_nodes.c` — the Server
  object hierarchy, ServerStatus (+ State = Running), ServerCapabilities
  (ServerProfileArray, LocaleIdArray, OperationLimits), NamespaceArray, ServerArray)
  is available and is enabled in the Embedded and full profiles, where it resolves
  as a fallback after the integrator's address space.

- **ServerStatus timestamps (Embedded / full, when the base node set is built):**
  `ServerStatus.CurrentTime` is served live from the time adapter and `StartTime`
  from the value captured at init, via runtime-bound value sources in the
  (caller-owned) server struct.

## Remaining Nano evidence (see [status.md](status.md))
- External conformance evidence, including CTT verification, has not been
  produced; status remains `profile-targeting` until OPC-10000-7 §4.2/§4.3
  profile and ConformanceUnit evidence is reviewed and verified.
