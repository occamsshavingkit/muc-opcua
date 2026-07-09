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

## Decision (locked): enforce everything, advertise real caps

Chosen approach: every advertised limit maps to real server enforcement (no "0 =
unlimited" placeholders). Enforcement landscape (verified):

| Limit | NodeId | Enforced today? | Enforcing constant / site | Advertise |
|-------|--------|-----------------|---------------------------|-----------|
| MaxNodesPerRead | 11705 | ✅ | `MU_DISPATCH_MAX_READ_NODES`=32 (`read_process.c:119`) | 32 (already) |
| MaxNodesPerBrowse | 11710 | ✅ | `MU_DISPATCH_MAX_BROWSE_NODES`=8 (`view_handler.c`) | 8 (already) |
| MaxNodesPerWrite | 11707 | ✅ | `MU_DISPATCH_MAX_READ_NODES`=32 (`write.c:25`, shared) | **add node, 32** |
| MaxMonitoredItemsPerCall | 11714 | ✅ | `MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS`=32 | **add node, 32** |
| MaxStringLength (SC scalar) | 11550 | ✅ | `MU_MAX_ENCODED_STRING_LENGTH`=4096 (`binary_string.c:43`) | **add node, 4096** |
| MaxArrayLength (SC scalar) | 11549 | ❌ **gap** | none — `binary_variant.c:139` reads length unchecked | **enforce + add node** |

So the only missing *enforcement* is **MaxArrayLength**; the rest just need
*advertising*.

## Requirements

- **FR-001 (single source)**: Add operation-limit macros to `config.h`
  (`MU_MAX_NODES_PER_READ/_WRITE/_BROWSE`, `MU_MAX_MONITORED_ITEMS_PER_CALL`,
  `MU_MAX_ARRAY_LENGTH`). Each enforcing site gains a `_Static_assert` tying its
  local dispatch constant to the config macro, so advertised == enforced can never
  drift. `base_nodes.c` advertises the config macros (not literals).
- **FR-002 (array enforcement — the gap)**: `binary_variant` array decode rejects
  `array_length > MU_MAX_ARRAY_LENGTH` with `Bad_EncodingLimitsExceeded`.
- **FR-003 (advertise)**: Add `MaxNodesPerWrite`, `MaxMonitoredItemsPerCall`,
  `MaxArrayLength`, `MaxStringLength` nodes to **both** `base_nodes.c` node tables
  with `HasProperty` references from OperationLimits(11704) / ServerCapabilities(2268).
- **FR-004 (tests)**: (a) array over `MU_MAX_ARRAY_LENGTH` → `Bad_EncodingLimitsExceeded`;
  (b) Read each new node → value equals the enforcement constant;
  (c) per-request over-limit on Write → `Bad_TooManyOperations`.

## Open value decision

`MU_MAX_ARRAY_LENGTH` default — the DoS ceiling on decoded array element counts.
Must be large enough for legitimate arrays (Write values, PubSub datasets) yet
bounded. Candidate default **`8192`** (matches the chunk/message byte ceiling as a
coarse element cap), `-D`-overridable. Confirm before enforcing (too low breaks
legitimate large-array clients on the full profile).

## Out of Scope

- OperationLimits not required by the target profiles (`MaxNodesPerMethodCall`,
  `MaxNodesPerRegisterNodes`, `MaxNodesPerTranslateBrowsePathsToNodeIds`) — already
  enforced; advertising them is a follow-up.
- CTT verification.
