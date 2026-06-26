# Feature Spec: Nano Embedded Device 2017 Server Profile Completion

**Feature ID**: 002-nano-profile-completion
**Status**: profile-targeting (Nano completion)
**Target Profile**: Nano Embedded Device 2017 Server Profile
(`http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017`), which is
functionally equivalent to the **Core 2017 Server Facet** over UA-TCP UA-SC
UA-Binary with SecurityPolicy None and Anonymous identity.

## Goal

Close the gap between the current server (connect → discover → session → Browse →
Read, interop-validated) and a *complete* Nano profile surface, so the server can
be CTT-verified. The crypto/security work (Basic256Sha256) is already done and is
above Nano; this feature does **not** add subscriptions (that is the Micro profile).

## Division of work (per request)

- **Claude owns**: this spec, the plan, the **architecture/design**, every
  **test** (unit + integration), and all **validation** (build, sanitizers,
  interop, conformance traceability). Tests are written first (RED) per the
  constitution.
- **Codex owns**: **implementation only** — writing the production C to make the
  Claude-authored failing tests pass. Codex must not add or modify tests, CMake
  test wiring, specs, or docs.

## In scope

### US1 — Complete the View Service Set (P1)
The Core Server Facet's "View" conformance units require the full View Service Set,
not just Browse. Today BrowseNext / TranslateBrowsePaths / RegisterNodes /
UnregisterNodes return `Bad_ServiceUnsupported`.

- **BrowseNext** (OPC 10000-4 §5.9.3). This server never issues a ContinuationPoint
  (Browse returns all references inline; an overflow yields `Bad_NoContinuationPoints`
  per the existing design, OPC 10000-4 §7.9). BrowseNext must still respond: with
  `releaseContinuationPoints = true` → empty results, Good; otherwise each supplied
  continuation point is unknown → operation-level `Bad_ContinuationPointInvalid`.
- **TranslateBrowsePathsToNodeIds** (OPC 10000-4 §5.9.4). Resolve each BrowsePath
  (a starting node + RelativePath of BrowseNames) against the static address space
  to target NodeIds; unresolved elements yield `Bad_NoMatch`.
- **RegisterNodes** (OPC 10000-4 §5.9.5). "Does not validate the NodeIds … Servers
  will simply copy unknown NodeIds": return `registeredNodeIds` = `nodesToRegister`
  (identity mapping).
- **UnregisterNodes** (OPC 10000-4 §5.9.6). "Does not validate … simply unregister":
  return Good (ResponseHeader only).

### US2 — Standard Base Information address space (P1)
The Core Server Facet requires the **Server** and **ServerCapabilities** objects and
all mandatory Node attributes (OPC 10000-5 §6.3, §7; standard NodeIds per OPC 10000-6
Annex A). Today only the example supplies NamespaceArray/ServerArray + two
OperationLimits nodes. The **library** must expose a default standard node set so a
conformant server exists without the integrator hand-building it:

- Base hierarchy needed for Browse: Root(i=84), Objects(i=85), Types(i=86),
  Views(i=87), Server(i=2253) and the references between them.
- `Server` (i=2253) → `ServerStatus` (i=2256) with `State`(i=2259), `CurrentTime`,
  `StartTime`, `BuildInfo`(i=2260); `ServiceLevel`(i=2267);
  `ServerCapabilities`(i=2268) with `ServerProfileArray`, `LocaleIdArray`,
  `MaxBrowseContinuationPoints`(=0), and `OperationLimits` (`MaxNodesPerRead`(i=11705),
  `MaxNodesPerBrowse`(i=11710)); `NamespaceArray`(i=2255); `ServerArray`(i=2254).
- Dynamic attributes (`ServerStatus.CurrentTime`, `.State`, `.StartTime`) are served
  through value-source callbacks bound to the server's time adapter / running state
  (architecture: Claude defines the value-source contract; Codex fills the static
  table and the callback bodies the contract specifies).

### US3 — Conformance traceability (P1)
Fill `docs/conformance/{status,profile-nano,services}.md` with the Nano
conformance-unit → service/feature → test mapping, and update
`docs/traceability/files-to-sections.md` for new files. (Claude-authored.)

## Out of scope (not Nano)
Subscriptions / MonitoredItems / Publish (Micro), Write / Method / NodeManagement,
multiple sessions, full type-system exposure, Diagnostics (optional for Nano),
username/password identity, and any SecurityPolicy work (already complete, above Nano).

## Acceptance
- The four View services return spec-correct responses (not `Bad_ServiceUnsupported`),
  exercised by Claude integration tests through `mu_server_poll`.
- Browsing from `Objects`/`Server` and reading `ServerStatus`/`ServerCapabilities`
  attributes succeed against the library's default node set.
- asyncua and the .NET reference client still pass; conformance docs reflect the
  CU-by-CU status; `profile-compliant` is still **not** claimed until a CTT run.
