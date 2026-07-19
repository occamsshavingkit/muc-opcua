# Conformance: Nano Embedded Device 2022 Server Profile

This server targets the **Nano Embedded Device 2022 Server Profile**
(`http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017`, OPC 10000-7).
Per the OPC Foundation profile definition, Nano is *functionally equivalent to the
Core 2022 Server Facet* and requires the OPC UA TCP binary transport.

> **Strict profile (spec 067).** The `nano` build now equals exactly the mandatory Core 2022
> facet set: Read/Browse/Discovery/**RegisterNodes**, the **Base Info Server object**
> (`BASE_NODES`), and **UserName/Password** user tokens (`USER_AUTH`) — the last two were
> previously integrator-supplied and are now built in for standalone conformance. Everything
> else (subscriptions, security, write, …) remains a `-D` opt-in.

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
  UnregisterNodes (OPC-10000-4 §5.9.5 / §5.9.6) are compiled in for the standalone
  Nano conformance-targeting build (`MUC_OPCUA_SERVICE_REGISTER_NODES`).
- **Address Space / Base Info:** The Nano build compiles the standard Base
  Information node set (`MUC_OPCUA_BASE_NODES`) so a standalone example exposes
  the Server object hierarchy, ServerStatus (+ State = Running), ServerCapabilities
  (ServerProfileArray, LocaleIdArray, OperationLimits), NamespaceArray, and
  ServerArray. The integrator's address space still takes precedence; the library
  base nodes resolve as a fallback.

- **ServerStatus timestamps (when the base node set is built):**
  `ServerStatus.CurrentTime` is served live from the time adapter and `StartTime`
  from the value captured at init, via runtime-bound value sources in the
  (caller-owned) server struct.

## Security Time Synchronization (spec 055)
- `Security Time Synch - Configuration` (Mandatory in the v1.05.02 Core 2022
  Server Facet) is **targeted**: when `MUC_OPCUA_TIME_SYNC` is enabled the server
  stamps `ServerTimestamp` in response headers and, at OpenSecureChannel,
  validates the client's `RequestHeader.timestamp` against server time within
  `MU_TIME_SYNC_MAX_CLOCK_SKEW_MS` (default 5 min), rejecting drift with
  `Bad_SecurityChecksFailed`. A clockless client (timestamp 0) is exempt.

## Remaining Nano evidence (see [status.md](status.md))
- External conformance evidence, including CTT verification, has not been
  produced; status remains `profile-targeting` until OPC-10000-7 §4.2/§4.3
  profile and ConformanceUnit evidence is reviewed and verified.
