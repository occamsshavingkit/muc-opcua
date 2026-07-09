# Spec 057: Base Info Server Capabilities (OperationLimits)

**Status**: Draft — scope corrected after investigation | **Branch**: `057-base-info-capabilities`
**Roadmap**: Project A / A2 | **Implements**: spec 051 §2/§7
**OPC UA**: OPC-10000-7 v1.05.02 `Base Info Server Capabilities` (Optional→Mandatory,
Mantis 7003); OPC-10000-5 ServerCapabilities(2268)/OperationLimits(11704)

## Investigation findings (scope correction)

The roadmap estimated ~0.5d assuming the limits were advertised and just needed
verifying. In fact the address space exposes only **2 of the mandatory set**:

| Node | NodeId | Present? | Value today | Enforced? |
|------|--------|----------|-------------|-----------|
| `MaxNodesPerRead` | 11705 | ✅ | 32 | yes (`read_process.c:119`) |
| `MaxNodesPerBrowse` | 11710 | ✅ | — | yes (`view_handler.c`) |
| `MaxNodesPerWrite` | 11707 | ❌ | — | Write path enforces? (verify) |
| `MaxMonitoredItemsPerCall` | 11714 | ❌ | — | `subscription_helpers.c:188` |
| `MaxArrayLength` (ServerCapabilities scalar) | 11549 | ❌ | — | encoding limit |
| `MaxStringLength` (ServerCapabilities scalar) | 11550 | ❌ | — | `MU_MAX_ENCODED_STRING_LENGTH` |

Note spec 051's "MaxNodesPerSubscription" is **not** a standard OperationLimit
name; the subscription-related per-call limit is `MaxMonitoredItemsPerCall`.

`base_nodes.c` is hand-maintained (no generator) and gated by `MUC_OPCUA_BASE_NODES`.
It contains **two node tables** (full type-system vs. reduced, ~lines 640-720 and
808-860); every new node must be added to both, each with: a string constant, a
`mu_value_source_t` (static `uint32`), the node entry (NodeId, VARIABLE class,
`s_property_type_ref`, type-definition 68), and a `HasProperty` reference from the
parent (OperationLimits(11704) for the per-call limits; ServerCapabilities(2268)
for MaxArrayLength/MaxStringLength).

## Open decisions (must resolve before authoring)

1. **Exact mandatory set** — which nodes does `Base Info Server Capabilities`
   require for the Nano/Micro/Embedded targets? Candidate: add `MaxNodesPerWrite`,
   `MaxMonitoredItemsPerCall`, `MaxArrayLength`, `MaxStringLength` (→ 6 total with
   the existing two).
2. **Advertised values** — each must equal what the server actually enforces:
   - `MaxNodesPerWrite` → the Write dispatch's per-request cap (verify/define)
   - `MaxMonitoredItemsPerCall` → `subscription_helpers.c` cap
   - `MaxArrayLength` → the encoder's array-length ceiling
   - `MaxStringLength` → `MU_MAX_ENCODED_STRING_LENGTH` (4096)
   Advertising a value the server does not enforce is a false conformance claim.

## Requirements (pending decision confirmation)

- **FR-001**: Add the confirmed OperationLimits/ServerCapabilities nodes to both
  `base_nodes.c` tables with values equal to the enforced limits.
- **FR-002**: `HasProperty` references wire each new node under its parent.
- **FR-003**: Tests Read each new node and assert the advertised value equals the
  compiled/enforced limit.

## Out of Scope

- OperationLimits not required by the target profiles (e.g. `MaxNodesPerMethodCall`
  when Method server is off) — gate per feature.
- CTT verification.
