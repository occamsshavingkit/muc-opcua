<!--
SYNC IMPACT REPORT
==================
Version change: 1.1.2 → 1.1.3
Bump rationale: PATCH — added 4 missing CUs from OPC API to manifest and
roadmap; resolved Q7; recount from 100→104 genuinely unimplemented CUs.

Changes this revision:
  - Resolved Q7: 4 genuinely new CUs added to manifest and scope-in lists
    (3080 Security Default ApplicationInstance Certificate — mandatory;
    3201 Base Info Custom Type System — optional; 5592 Base Info
    Engineering Units — optional; 5814 Security – No Application
    Authentication — optional). 3721 (ECC Policy) is DUP. 2371/2837/2853
    are already claimed via Kconfig symbols.
  - Updated CU counts: nano 34→38, micro 9→13, embedded 56→57, standard 1
    — 104 total (was 100).
  - Added 3080 to spec 007 scope-in; 3201 and 5592 to spec 005 scope-in;
    5814 to spec 007 scope-in.
  - nano mandatory count: 5→6 (adding 3080).
  - Updated manifest with cu_optional flags from OPC API for all 110
    previously-existing CUs + 4 new entries.

Specs affected: 005, 007 (scope-in lists expanded)
Open questions added: none
Open questions resolved: Q7
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
- Complete OPC UA conformance unit (CU) coverage for the server side, starting
  from the mandatory Core 2022 Server Facet CUs at nano and progressing through
  micro, embedded, and standard tiers. Every unimplemented CU declared in
  `profiles/opcua-profile-manifest.yaml` is addressed by a roadmap spec.

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
- **C-05 — Per-profile server CU ordering:** Server CU specs are ordered by
  profile tier: nano → micro → embedded → standard. Each tier's CUs must be
  completed before the next tier begins, since higher profiles inherit all
  lower-profile CUs through the Kconfig cascade mechanism.
- **C-06 — Type system additivity:** Exposes Type System Server Facet CUs are
  split across two specs (nano and embedded). Nano adds the minimum type nodes
  required for Core 2022 Server Facet. Embedded adds the remaining specialized
  DataTypes, ReferenceTypes, and ObjectTypes. No CU is duplicated.
- **C-07 — Non-regression of existing profiles:** Unimplemented CU work must not
  degrade existing profile builds (nano, micro, embedded, standard, full).
  Each profile's CI gates (`profile-tests`, `interop`, `freestanding-core`)
  must pass at every commit. CUs default to off in Kconfig until verified.
- **C-08 — CU spec grounding:** Every server CU spec must cite the CU's OPC
  profile ID and its specification section (OPC-10000-7 §4.2 minimum), declare
  expected flash/RAM impact per Constitution Principle VII, and define a test
  strategy (host unit test, interop, or conformance test) before implementation.
- **C-09 — Type system tiered compilation:** Type system CUs (specs 005, 009)
  must be individually gated behind Kconfig symbols so constrained nano builds
  can exclude non-mandatory type nodes. The nano profile defconfig enables only
  the minimum subset required by Core 2022 Server Facet; higher profiles enable
  the full set through the Kconfig cascade.

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

---

## Server CU Implementation Roadmap

Source: `profiles/opcua-profile-manifest.yaml` filtered to `kind: conformance_unit`
with `implementation_state: unimplemented` and `kconfig_symbol: null`. Grouped by
lowest-profile-default (a CU that defaults on in nano and all higher profiles is
listed under nano).

CU counts (deduplicated, API-verified): nano 38, micro 13, embedded 57, standard 1 — 104 total.

> **Deduplication and API enrichment complete (2026-07-13):**
> - 10 CUs matched claimed entries with `MUC_OPCUA_CU_*` Kconfig symbols (Q4).
> - `cu_optional` flags added to all 110 previously-existing unimplemented CUs
>   from `profiles.opcfoundation.org/api/profile/includedconformanceunits/`.
> - 4 genuinely new CUs added from OPC API that were missing from the manifest
>   (Q7): 3080, 3201, 5592, 5814.

### 005 — Server: Nano Core Type System & Base Info CUs  [status: planned]

- **Description:** Implements the ~14 genuinely new Exposes Type System and
  Base Info CUs required by the Core 2022 Server Facet at the nano tier (8
  duplicates already claimed via `MUC_OPCUA_CU_*` symbols removed — see Q4).
  Adds the minimum set of OPC UA type nodes, reference types, and Base Info
  properties to the server's static address space.
- **Outcome:** A nano-profile server exposes all mandatory type-system nodes
  not yet claimed: local time, locations, currency, interfaces, selection
  list, option set, value-as-text, and related Base Info properties. Kconfig
  toggles `MUC_OPCUA_CU_*` symbols for each completed CU, gating their
  inclusion per C-09.
- **Scope (in):**
  - `opc_cu_2446` Address Space AddIn Reference
  - `opc_cu_2447` Address Space AddIn DefaultInstanceBrowsename
  - `opc_cu_2476` Base Info LocalTime
  - `opc_cu_2711` Base Info Selection List
  - `opc_cu_2809` Address Space Atomicity
  - `opc_cu_2820` Address Space Full Array Only
  - `opc_cu_2969` Base Info ValueAsText
  - `opc_cu_3127` Base Info OptionSet
  - `opc_cu_3198` Base Info Estimated Return Time
  - `opc_cu_3560` Address Space Interfaces
  - `opc_cu_3808` Documentation - Core Capacities
  - `opc_cu_4053` Base Info Locations Object
  - `opc_cu_4237` Address Space NonVolatile and Constant
  - `opc_cu_5240` Base Info Currency
  - `opc_cu_5592` Base Info Engineering Units [API]
  - `opc_cu_3201` Base Info Custom Type System [API]
- **Scope (out):**
  - Service behaviour CUs (spec 006)
  - Security, diagnostics, time sync CUs (spec 007)
  - Exposes Type System CUs that first appear at embedded (spec 009)
- **Depends on:** none (existing address space infrastructure in
  `src/address_space/base_nodes.c`)
- **Governed by:** C-05, C-06, C-07, C-08; ADR-0003
- **Spec dir:** _not yet created_
- **Notes:** Most of these CUs are "expose type node in address space" tasks —
  adding static Node entries to `base_nodes.c` with correct NodeIds,
  BrowseNames, and type definitions per OPC-10000-3 §4 and OPC-10000-5.
  Existing `base_nodes.c` infrastructure already supports this pattern.
  Estimated flash impact ~1-3 KB for static tables.

### 006 — Server: Nano Core Service Behaviour CUs  [status: planned]

- **Description:** Implements the ~10 genuinely new service behaviour CUs
  required at nano (5 duplicates already claimed removed — Session Base,
  Session General Service Behaviour, Attribute Read, View RegisterNodes, and
  View TranslateBrowsePath/Basic 2 are functionally implemented via existing
  symbols). Covers remaining Session, View, Write, and Discovery conformance
  units that enforce correct protocol-level behaviour.
- **Outcome:** A nano-profile server passes all remaining Core 2022 Server
  Facet service behaviour requirements. Session change-user, write with
  StatusCode/Timestamp and IndexRange, and extended discovery/view semantics
  are conformant.
- **Scope (in):**
  - `opc_cu_2317` View TranslateBrowsePath
  - `opc_cu_2328` Discovery Get Endpoints
  - `opc_cu_2352` Discovery Find Servers Self
  - `opc_cu_2389` Attribute Write Values
  - `opc_cu_2400` Session Change User
  - `opc_cu_2936` Attribute Write StatusCode & Timestamp
  - `opc_cu_3147` Attribute Write Index
  - `opc_cu_3530` View Basic 2
  - `opc_cu_3192` Base Info Diagnostics
  - `opc_cu_3983` Base Services Diagnostics
- **Scope (out):**
  - Security administration CUs (spec 007)
  - Subscription services (spec 008)
  - NodeManagement, Query, History (full tier only, already invoked by existing
    feature flags)
- **Depends on:** 005 (type system nodes must exist for Read/Write/Browse to
  return meaningful results)
- **Governed by:** C-05, C-07, C-08; ADR-0003; Constitution Principle IV
  (protocol correctness gates)
- **Spec dir:** _not yet created_
- **Notes:** Several of these CUs may already be functionally implemented (e.g.
  Read, Browse, Session, Discovery) but are marked unimplemented in the manifest
  because they lack dedicated Kconfig symbols or CU-level tests. The spec's
  planning phase must audit each CU against the existing implementation and
  closed claimed CUs, producing a list of: already satisfied, needs symbols,
  needs tests, and genuinely missing behaviour.
  Estimated flash impact ~0.5-2 KB (mostly StatusCode paths and validation).

### 007 — Server: Nano Security & Time Sync CUs  [status: planned]

- **Description:** Implements the ~10 remaining nano CUs covering security
  administration, role-based authorization, and all seven time synchronization
  conformance units (diagnostics CUs moved to spec 006).
- **Outcome:** A nano-profile server supports configurable security policies,
  role-based access control (RBAC), and at least one time synchronization
  mechanism (NTP, PTP, IEEE 802.1AS, UA-based, or OS-based).
- **Scope (in):**
  - `opc_cu_2407` Security Administration
  - `opc_cu_2600` SecurityPolicy Support
  - `opc_cu_2808` Security Role Server Authorization
  - `opc_cu_2478` Time Sync - OS based support
  - `opc_cu_2479` Time Sync - IEEE 1588 (PTP)
  - `opc_cu_2480` Time Sync - IEEE 802.1AS
  - `opc_cu_2786` Time Sync - NTP
  - `opc_cu_3802` Time Sync - Configure Clock Skew
  - `opc_cu_5505` Time Sync - UA based support
  - `opc_cu_5793` Time Sync - Support (umbrella: at least one mechanism)
  - `opc_cu_3080` Security Default ApplicationInstance Certificate [API]
  - `opc_cu_5814` Security – No Application Authentication [API]
- **Scope (out):**
  - User token security policies beyond administration (spec 010)
  - ECC SecurityPolicies (already claimed in full tier via
    `MUC_OPCUA_CU_SECURITY_ECC`)
- **Depends on:** 006 (Session and service infrastructure needed for diagnostics
  counters; the time adapter interface is already defined in
  `include/muc_opcua/platform.h`)
- **Governed by:** C-05, C-07, C-08; Constitution Principle V (security and
  conformance honesty)
- **Spec dir:** _not yet created_
- **Notes:** Time sync CUs form a family — OPC requires at least one mechanism
  be supported (`opc_cu_5793`). NTP is the simplest path for host builds.
  Diagnostics CUs wire live counters into the existing `ServerDiagnostics`
  address space nodes. SecurityRole Authorization adds an RBAC lookup on
  session activation. Estimated flash impact ~2-5 KB.

### 008 — Server: Micro Subscription Core CUs  [status: planned]

- **Description:** Implements the 9 genuinely new subscription and monitoring
  CUs that differentiate micro from nano (1 duplicate already claimed removed —
  Subscription Basic is `MUC_OPCUA_CU_SUBSCRIPTION_BASIC`). Covers the
  Embedded DataChange Subscription 2022 Server Facet core: monitored item
  lifecycle, data change notifications, publish request management, and
  server capability advertisement.
- **Outcome:** A micro-profile server supports full subscription semantics:
  create/modify/delete subscriptions and monitored items, process publish
  requests with overflow handling, deliver data change notifications with
  value change triggers, semantic change bits, and fixed sampling interval
  diagnostics.
- **Scope (in):**
  - `opc_cu_2963` Monitor Basic
  - `opc_cu_3196` Base Info Fixed SamplingInterval
  - `opc_cu_3911` Base Info Server Capabilities Subscriptions
  - `opc_cu_3913` Subscription Publish Basic
  - `opc_cu_3922` Base Info SemanticChange Bit
  - `opc_cu_4055` Base Info Server Capabilities MaxMonitoredItemsQueueSize
  - `opc_cu_5207` Monitor Items 2
  - `opc_cu_5208` Monitor Value Change V2
  - `opc_cu_3143` Subscription PublishRequest Queue Overflow
- **Scope (out):**
  - Deadband filter, queueing, triggering (spec 010 — embedded extensions)
  - Event subscriptions (already claimed in full tier)
  - Client-side subscription handling
- **Depends on:** 007 (nano tier complete; subscription services require
  session and time-sync infrastructure)
- **Governed by:** C-05, C-07, C-08; Constitution Principle IV
- **Spec dir:** _not yet created_
- **Notes:** Many of these CUs may already be functionally satisfied by the
  existing subscription implementation (`src/services/subscription*.c`,
  `src/core/service_dispatch/monitored_items.c`). The spec's planning phase
  must audit each CU against the existing codebase. The main gap is likely
  the `ServerCapabilities` advertisement (exposing internal limits as
  address space nodes) and the `SemanticChange` bit. Estimated flash impact
  ~0.5-3 KB (mostly additions to `base_nodes.c` for capabilities nodes).

### 009 — Server: Embedded Full Type System CUs  [status: planned]

- **Description:** Implements the ~46 remaining Exposes Type System Server
  Facet CUs that first appear at the embedded tier. Adds the full set of
  specialized OPC UA DataTypes, ReferenceTypes, ObjectTypes, and VariableTypes
  to the server's address space.
- **Outcome:** An embedded-profile server exposes the complete OPC UA type
  system: all Base Info data types (RationalNumber, EUInformation, Range,
  OptionSet, etc.), all reference type hierarchies (HasOrderedComponent,
  IsExecutableOn, Controls, Utilizes, etc.), and auxiliary nodes
  (ServerType, TypeInformation, UaBinaryFile).
- **Scope (in):**
  - All ~46 Exposes Type System CUs from the embedded list:
    `opc_cu_2423` through `opc_cu_5868`, plus `opc_cu_2231` (GDS Push Model)
  - Subgroup: Base Info data types (~15 CUs — Rational Number, NormalizedString,
    DecimalString, Date DataTypes, BitFieldMask, KeyValuePair, EUInformation,
    OrderedList, AudioType, SpatialData, ContentFilter, OptionSet DataType,
    Range DataType, Decimal, Handle DataType, Portable IDs, TrimmedString)
  - Subgroup: Reference types (~14 CUs — HasOrderedComponent, IsExecutableOn,
    IsExecutingOn, Controls, Utilizes, Requires, IsPhysicallyConnectedTo,
    RepresentsSameEntityAs, RepresentsSameHardwareAs,
    RepresentsSameFunctionalityAs, IsHostedBy, HasPhysicalComponent,
    HasContainedComponent, HasAttachedComponent)
  - Subgroup: Type information & auxiliary (~10 CUs — Core Types Folders,
    Base Types, ServerType, Deprecated Information, Image DataTypes,
    UaBinary File, StatusResult DataType, UriString, Method Argument DataType,
    SemanticVersionString, ReferenceDescription)
  - Subgroup: Global Certificate Management (`opc_cu_2231`)
- **Scope (out):**
  - Security/subscription CUs (spec 010)
  - Standard X509 CU (spec 011)
- **Depends on:** 008 (micro tier complete; type nodes are additive on top
  of nano's core type system)
- **Governed by:** C-05, C-06, C-08; ADR-0003
- **Spec dir:** _not yet created_
- **Notes:** Largest single spec by CU count (~46). Implementation is
  mechanically repetitive: each CU typically requires adding one or more
  static Node entries to `base_nodes.c` with correct NodeIds from
  OPC-10000-3/OPC-10000-5. A code-generation approach for the type-table
  entries should be evaluated during planning to reduce manual error.
  Spec will need subtask decomposition by CU subgroup.
  Estimated flash impact ~5-15 KB (static tables with cross-references).

### 010 — Server: Embedded Security & Subscription Extension CUs  [status: planned]

- **Description:** Implements the ~11 remaining embedded-tier CUs covering
  user token security policies, extended subscription behaviour, and
  monitoring extensions: deadband filtering, queueing, triggering, and
  method-based subscription queries (GetMonitoredItems, ResendData).
- **Outcome:** An embedded-profile server supports encrypted User Name/Password
  authentication, invalid-token attack protection, unencrypted token fallback,
  multiple subscriptions per session, retransmission queues, multi-entry
  monitored item queues, triggering links, and deadband data change filters.
- **Scope (in):**
  - `opc_cu_2823` Security Invalid user token
  - `opc_cu_3534` Subscription Multiple
  - `opc_cu_3535` Subscription Retransmission Queue
  - `opc_cu_3536` Security User Name Password 2
  - `opc_cu_3645` Security User Token Unencrypted
  - `opc_cu_2928` Monitored Items Deadband Filter
  - `opc_cu_2940` Base Info GetMonitoredItems Method
  - `opc_cu_3146` Monitor Triggering
  - `opc_cu_3532` Monitor Queueing
  - `opc_cu_3544` Base Info ResendData Method
- **Scope (out):**
  - ECC SecurityPolicies (already claimed)
  - X509 certificate user token (spec 011)
  - JWT/Issued Token (deferred — requires OAuth2 infrastructure)
- **Depends on:** 009 (type system nodes may be referenced by subscription
  diagnostics nodes)
- **Governed by:** C-05, C-07, C-08; Constitution Principle V
- **Spec dir:** _not yet created_
- **Notes:** Several of these CUs may already be partially satisfied: user
  token encryption works, subscription retransmission exists
  (`src/services/subscription_publish/retransmit.c`), and basic deadband
  filtering is present (`src/services/subscription_publish/deadband.c`).
  The spec's planning phase must audit each CU against the codebase and
  identify the gap between "functional" and "CU-claimable".
  Estimated flash impact ~1-4 KB (mostly additional validation paths and
  the ResendData/GetMonitoredItems method handlers).

### 011 — Server: Standard Security User X509 CU  [status: planned]

- **Description:** Implements the single CU that first appears at standard
  (not inherited from embedded): `opc_cu_3125` — Security User X509, which
  requires the server to support public/private key pair authentication for
  user identity with full certificate validation.
- **Outcome:** A standard-profile server accepts X509 certificate-based user
  identity tokens during ActivateSession, validating the client certificate
  against the trust list.
- **Scope (in):**
  - `opc_cu_3125` Security User X509
  - User token policy advertisement in GetEndpoints
  - Certificate chain validation for user identity
  - Administrator-configurable enable/disable
- **Scope (out):**
  - JWT/OAuth2 user tokens (deferred)
  - Issued Token / Kerberos (deprecated in OPC 1.05.02)
- **Depends on:** 010 (embedded security infrastructure, trust list management)
- **Governed by:** C-05, C-08; Constitution Principle V
- **Spec dir:** _not yet created_
- **Notes:** The server already supports application-instance X509 certificate
  handling for SecureChannel establishment. User X509 extends the same pattern
  to ActivateSession's user identity token validation. Reuses existing
  `src/security/trustlist.c` infrastructure.
  Estimated flash impact ~0.5-1 KB.

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

- **Q4 — Manifest CU deduplication** [resolved 2026-07-13]: 10 of 110
  unimplemented CUs are duplicates of claimed entries with `MUC_OPCUA_CU_*`
  Kconfig symbols, identified by display-name cross-reference against the
  `implementation_state: claimed` items in `profiles/opcua-profile-manifest.yaml`
  and the table in `docs/build-and-gating.md`. Removed from scope:
  - `opc_cu_3184` Base Info Core Structure 2 ← `MUC_OPCUA_CU_CORE_STRUCTURE_2`
  - `opc_cu_3186` Base Info Core Views Folder ← `MUC_OPCUA_CU_CORE_VIEWS_FOLDER`
  - `opc_cu_3545` Base Info Namespace Metadata ← `MUC_OPCUA_CU_NAMESPACE_METADATA`
  - `opc_cu_3554` Address Space Base ← `MUC_OPCUA_CU_ADDRESS_SPACE_BASE`
  - `opc_cu_3912` Base Info Server Capabilities 2 ← `MUC_OPCUA_CU_SERVER_CAPABILITIES_2`
  - `opc_cu_3072` Attribute Read ← `MUC_OPCUA_CU_ATTRIBUTE_READ`
  - `opc_cu_3073` View RegisterNodes ← `MUC_OPCUA_CU_VIEW_REGISTERNODES`
  - `opc_cu_3175` Session Base ← `MUC_OPCUA_CU_SESSION_BASE`
  - `opc_cu_3985` Session General Service Behaviour ← `MUC_OPCUA_CU_SESSION_GENERAL_SERVICE`
  - `opc_cu_3727` Subscription Basic ← `MUC_OPCUA_CU_SUBSCRIPTION_BASIC`
  
  Real unimplemented count: **100 CUs** (nano 34, micro 9, embedded 56, standard 1).

- **Q5 — Size envelope per tier** [resolved 2026-07-13, data from
  `profiles.opcfoundation.org/api/profile/includedconformanceunits/2266/`]:
  
  Current nano baseline `elf_text` = 27,080 bytes (`scripts/measure_size.sh nano`).
  
  **Authoritative OPC data** (API query 2026-07-13 for nano profile ID 2266):
  - 20 mandatory CUs (isOptional=false), 31 optional (isOptional=true) = 51 total
  - Of the 20 mandatory, 10 are already claimed (DUPs with `MUC_OPCUA_CU_*`),
    4 more are covered by existing claimed symbol groups
  - **5 genuinely new mandatory CUs**: 2809, 2820, 3808, 2600, 5793
  - Plus 1 CU (3080, Security Default ApplicationInstance Certificate) present in
    the API but missing from our manifest entirely
  - 26 optional CUs that must be implemented but default OFF in nano defconfig
  
  **Nano default defconfig (mandatory only):** 5-6 CUs × ~200-1000 bytes =
  ~1-6 KB added → **28-33 KB .text**. Fits 32-64 KB flash devices.
  
  **Nano with all optionals enabled:** +8-25 KB additional → **36-58 KB .text**.
  
  **Full completion (all 100 CUs across all profiles):** 21-56 KB cumulative →
  **48-83 KB .text** (full profile builds already have higher capacities).

- **Q6 — Nano constrained-device deferral strategy** [resolved 2026-07-13]:
  The OPC API data shows only 5 genuinely new mandatory CUs for nano. The 26
  optional CUs default OFF — the nano defconfig does not enable them. Users on
  constrained MCUs ship with the mandatory-only set (~28-33 KB). Users with more
  flash enable optionals through menuconfig or Kconfig overrides. No formal
  "deferral tier" needed — the Kconfig cascade already handles this.

- **Q7 — CUs missing from manifest** [resolved 2026-07-13]: 8 nano CUs from the
  OPC API were absent from `profiles/opcua-profile-manifest.yaml`. Of these:
  - 3 are already claimed via `MUC_OPCUA_CU_*` symbols: 2371 (Protocol UA TCP),
    2837 (UA Binary Encoding), 2853 (UA Secure Conversation).
  - 1 is DUP: 3721 (Security ECC Policy) = `MUC_OPCUA_CU_SECURITY_ECC`.
  - 4 added to manifest and roadmap: 3080 (mandatory, spec 007), 3201 (optional,
    spec 005), 5592 (optional, spec 005), 5814 (optional, spec 007).

## Cross-Cutting Notes

- The client reuses the server's binary encoder/decoder, StatusCode system,
  NodeId/Variant types, and security policy stubs. No forking of shared code.
- The client state machine (disconnected → connecting → connected → session →
  subscribed) mirrors the server's connection model but is independent.
- Test strategy: interop tests against the existing muc-opcua server, plus
  the OPC UA .NET reference client as a secondary validation target.
- **Server CU source of truth:** `profiles/opcua-profile-manifest.yaml` is the
  canonical registry of all OPC conformance units, their `implementation_state`,
  `kconfig_symbol`, and `profile_defaults`. The Kconfig tree is generated from
  this manifest. Unimplemented CUs appear as `comment` directives in Kconfig
  (visible in menuconfig, not toggleable) until a spec promotes them.
- **Size ledger:** Every server CU spec must produce a cumulative size ledger
  entry in `docs/size/feature-size-ledger.md` and verify against the
  `scripts/measure_size.sh` baseline before marking `verified`.
- **Server CI gates:** The `profile-tests` matrix (nano, micro, embedded,
  standard, full) in CI must remain green through all CU work. CUs default
  off (no `default y`) until the spec that implements them explicitly gates
  them on the profile cascade symbol.

---

**Version**: 1.1.3 | **Ratified**: 2026-07-07 | **Last Amended**: 2026-07-13
