# Spec 051: Update Profile Targets to OPC-10000-7 v1.05.02

**Status**: Draft | **Branch**: `051-update-profiles-to-v1.05.02`
**OPC UA Source**: OPC-10000-7 v1.05.02 (2022-11-01, current)
**Current Baseline**: v1.04 (2017-11-01)

## Motivation

The project's conformance documents target OPC-10000-7 v1.04 profiles (Nano/Micro/
Embedded/Standard 2017). The specification is now at v1.05.02, which introduced
several changes that affect all Server Facets and Application Profiles. Since the
v1.05.02 revision did **not** create new 2022-versioned Application Profile URIs
(no "NanoEmbeddedDevice2022" exists), the 2017 URIs remain current — but the
conformance units that back them have changed.

This spec identifies every delta between v1.04 and v1.05.02 that affects a
profile-targeting implementation and creates a migration plan.

## Specification Deltas (v1.04 → v1.05.02)

### 1. Security Time Synchronization — NEW Mandatory CU

**Affects**: Core Server Facet, Core 2017 Server Facet, all Application Profiles

The `Security Time Synch - Configuration` conformance unit changed from absent
(v1.04) to **Mandatory** in the Core 2017 Server Facet. Two optional CUs also
added: `Security Time Synch - NTP / OS Based support` and `Security Time Synch
- UA based support`.

**Action**: Implement per spec 050 (Security Time Sync).

### 2. Base Info Server Capabilities — Changed from Optional → Mandatory

**Affects**: Core 2017 Server Facet (Table 25), Core Server Facet (Table 24)

`Base Info Server Capabilities` CU was Optional=True in v1.04 → Optional=False
(Mandatory) in v1.05.02. This CU requires the server to publish operation limits
in `Server.ServerCapabilities`:

| Property | v1.04 | v1.05.02 |
|----------|-------|----------|
| `MaxArrayLength` | Optional (recommended) | Mandatory |
| `MaxStringLength` | Optional (recommended) | Mandatory |
| `MaxNodePerRead` | Optional (recommended) | Mandatory |
| `MaxNodesPerWrite` | Optional (recommended) | Mandatory |
| `MaxNodesPerSubscription` | Optional (recommended) | Mandatory |
| `MaxNodesPerBrowse` | Optional (recommended) | Mandatory |

Similar changes affect: Node Management Capabilities, Method Capabilities,
Security Role Capabilities, Events Capabilities, History Capabilities (all
now Mandatory in their respective Facets).

**Action**: Verify that `base_nodes.c` exposes all six operation limits in
`Server.ServerCapabilities`. If any are missing, add them.

### 3. Documentation CU — NEW Mandatory CU

**Affects**: All Server Facets, all Application Profiles

A new `Documentation` conformance unit (and associated `Documentation Server
Facet`) requires servers to document their operation limits and capacities in
product documentation. This is a lab-test CU (not CTT-testable).

**Action**: Create `docs/conformance/documentation.md` with capacity limits
documented per profile.

### 4. ECC Security Policies — NEW

**Affects**: Nano Embedded Device 2017 Server Profile (Table 87)

Two new SecurityPolicies added, both required for Nano:
- `SecurityPolicy - ECC-curve25519` (mandatory for Nano, replaces
  Basic128Rsa15 which was removed)
- `SecurityPolicy - ECC-nist256` (alternative)

Per the spec Agreement of Use (Mantis 7041): "ECC-curve25519 is the required
policy for low-end UA applications where security was not required."

**Action**: Implement ECC P-256 and Curve25519 key exchange + signing in the
security adapter layer.

### 5. Exposes Type System Server Facet — NEW Facet

**Affects**: Application-level Facet for Embedded (non-2017) profile

A new Facet `Exposes Type System Server Facet` aggregates the `Base Info Type
System` CU with additional type-exposure requirements. Created per Mantis 6990.

**Action**: No immediate change — the Embedded 2017 profile already requires
`Base Info Type System` as Mandatory. This is an organizational change only at
the profile level.

### 6. User Role Management — NEW Facets

**Affects**: Standard+ profiles

`User Role Base Server Facet` and `User Role Management Server Facet` added
per Mantis 7390. These CUs cover server-side user role CRUD and well-known
role groups (Security Role Well Known Group 2 and 3).

**Action**: Deferred — Standard profile prerequisite.

### 7. OperationLimits Requirement (Mantis 7003) — Mandatory Capability CUs

All Capability CUs in all Facets changed to Mandatory. Detailed changes:

| Facet | v1.04 CU | v1.05.02 Change |
|-------|----------|-----------------|
| Core 2017 | Base Info Server Capabilities | Optional → Mandatory |
| Node Mgmt 2022 | Base Info Node Mgmt Capabilities | Optional → Mandatory |
| Method 2022 | Method Capabilities | Optional → Mandatory |
| User Role Base 2022 | Base Info Security Role Capabilities | Optional → Mandatory |
| Subscription 2022 | Base Info Server Capabilities Subscriptions | Optional → Mandatory |
| Event Subscr 2022 | Base Info Events Capabilities | Optional → Mandatory |
| Hist Data 2022 | Base Info History Read Data Capabilities | Optional → Mandatory |
| Hist Events 2022 | Base Info History Read Events Capabilities | Optional → Mandatory |

**Action**: All OperationLimits Properties must be published by the server.
Verification pass on `base_nodes.c`.

## Migration Plan

### Phase 1 — Nano profile (Core 2017 Server Facet)

| Step | Change | Effort | Depends On |
|------|--------|--------|------------|
| 1.1 | Security Time Sync (spec 050) | ~1d | — |
| 1.2 | Verify/complete Base Info Server Capabilities (6 properties in base_nodes.c) | ~0.5d | — |
| 1.3 | Documentation Server Facet — create capacity limits document | ~0.5d | — |
| 1.4 | ECC Security Policy — curve25519 key exchange + signing | ~5d | crypto adapter refactor |

### Phase 2 — Embedded profile additions

| Step | Change | Effort |
|------|--------|--------|
| 2.1 | Verify all 6 OperationLimits in base_nodes.c for Embedded build | ~0.5d |
| 2.2 | `Base Info Server Capabilities` now Mandatory for Embedded profile verification | ~0.25d |

### Phase 3 — Standard profile (when implemented)

| Step | Change | Effort |
|------|--------|--------|
| 3.1 | Node Management Capabilities, Method Capabilities, Subscription Capabilities | ~0.5d |
| 3.2 | History Capabilities (Read Data, Read Events, Update) | ~0.5d |
| 3.3 | Events Capabilities | ~0.5d |
| 3.4 | Security Role Capabilities | ~0.5d |

### Phase 4 — Documentation

| File | Change |
|------|--------|
| `docs/conformance/documentation.md` | NEW — capacity limits per profile |
| `docs/conformance/profile-nano.md` | Reference v1.05.02 spec version; note Security Time Sync status |
| `docs/conformance/profile-micro.md` | Same |
| `docs/conformance/profile-embedded.md` | Same; note SecurityPolicy Required vs specific policy change |
| `docs/traceability/conformance-claims.md` | Update spec version reference |
| `Makefile` / `CMakeLists.txt` | Consider `MUC_OPCUA_PROFILE_SPEC_VERSION` identifier |

## Out of Scope

- PubSub profiles (separate spec track)
- Client profile updates (spec 049)
- CTT verification
- Full Standard UA Server 2017 profile implementation (spec 037)
