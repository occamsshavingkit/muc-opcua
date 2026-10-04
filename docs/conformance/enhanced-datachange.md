# Conformance: Enhanced DataChange Subscription 2022 Server Facet (spec 063)

This server implements the OPC UA **Enhanced DataChange Subscription 2022 Server Facet**
(OPC profile-DB facet `EnhancedDataChangeSubscription2017`, **id 1678**). This is the
documented source identity for the facet; the committed OPC profile composition graph at
`profiles/opcua-profile-graph.json` models the same facet under **id 1627**. It is a
**capacity tier** of the DataChange subscription engine, not a distinct code path — it
raises the mandatory minima of the base **Standard DataChange Subscription 2022** facet
(id 1675, which Enhanced *includes*).

## Canonical CU ownership (project policy bridges the graph snapshot)

The `standard` and `full` Kconfig profiles enable the four canonical capacity CUs
that comprise this facet and meet their minima. This capability is owned by those
CUs, not by a named-profile selector or advertised-profile marker. The committed
graph at `profiles/opcua-profile-graph.json` does **not** confer this:
**root 2269** (Standard 2025 UA Server Profile) has mandatory children 2268 (Embedded
2025) and 1696 (X509), but **no mandatory path** to **facet 1627** (Enhanced DataChange
Subscription 2017 Server Facet). Facet 1627 itself contains four mandatory CUs (5242,
5250, 5249, 5248) and one child facet (Standard DataChange Subscription 2022, id 1324,
`isOptional = false`).

The project enables these four CUs through two mechanisms in
`profiles/opcua-profile-manifest.yaml`, resolved by
`scripts/profile_manifest/graph_deps.py` (`resolve_into`):

- **`standard`**: Each of the four Enhanced CUs carries
  `project_profile_defaults: {"standard": true}` — an explicit project-policy layer
  applied additively **after** graph derivation. `resolve_into` only enables
  validated profile keys listed in `project_profile_defaults`; it never removes
  or weakens graph-derived defaults. This is what enables the CUs for standard
  builds despite the disconnected graph snapshot.

- **`full`**: `full` is derived from `implementation_state`: every CU whose state
  is `claimed` is automatically `true` for `full`. All four Enhanced CUs are
  `claimed` in the manifest, so `full` builds inherit them.

Each CU also lists `semantic_depends_on: [MUC_OPCUA_CU_SUBSCRIPTION_STANDARD]`,
requiring the Standard DataChange subscription core before Enhanced capacity can
activate.

The generated `MUC_OPCUA_MARKER_STANDARD_PROFILE` remains disabled until every
mandatory CU in the Standard profile closure is implemented and selectable. The
current `standard` and `full` builds therefore advertise `EmbeddedUA2017` through
the Base Information Type System fallback in `src/address_space/base_nodes.c` while
claiming the Enhanced DataChange facet independently through its four CUs.

| Profile | Advertised profile | DataChange facet claimed | Queue depth |
|---|---|---|---|
| nano / micro | Nano / (Micro via Nano) | none / base subscriptions | 1 |
| embedded | EmbeddedUA2017 | Standard DataChange 2022 (`MinQueueSize_02`) | 2 |
| standard | EmbeddedUA2017 | **Enhanced DataChange 2022** (`MinQueueSize_05`) | **5** |
| full | EmbeddedUA2017 | **Enhanced DataChange 2022** (`MinQueueSize_05`) | **5** |

## Grounded minima (all four CUs are mandatory)

Grounded from the live OPC profile DB (facet id 1678; graph node 1627 in the committed snapshot,
[profiles.opcfoundation.org](https://profiles.opcfoundation.org)); `isOptional = false`
for every unit. There is **no** `MaxNotificationsPerPublish` or `MinSupportedSampleRate`
CU in this facet — the set below is exhaustive.

| Conformance unit | Minimum | Resolved (standard / full) | Enforced by |
|---|---|---|---|
| `Monitor Items 500` | ≥ 500 MonitoredItems / Subscription | 1000 / 2000 | `MU_INTERN_MAX_MONITORED_ITEMS` |
| `Monitor MinQueueSize_05` | ≥ 5 queue entries / MonitoredItem | 5 / 5 | `MU_INTERN_MONITORED_QUEUE_DEPTH` |
| `Subscription Minimum 05` | ≥ 5 Subscriptions / Session | 50 / 100 | `MU_INTERN_MAX_SUBSCRIPTIONS` |
| `Subscription Publish Min 10` | ≥ 10 Publish requests / Session | 50 / 100 | `MU_INTERN_MAX_PUBLISH_REQUESTS` |

## CU closure == enforced

`include/muc_opcua/features.h` defines `MUC_OPCUA_ENHANCED_DATACHANGE` only when
all four mandatory CU symbols are enabled. Each canonical CU translation unit under
`src/cu/core_2022_server/subscription/` carries its own `_Static_assert` against the
resolved capacity. A capacity override below an enabled CU's minimum is therefore a
compile error rather than a silently false capability claim.

## Cost of the queue-depth floor

The monitored-item queue is a **fixed inline ring** of
`MU_INTERN_MONITORED_QUEUE_DEPTH` entries per item (56 B per entry on the measured
32-bit Arm ABI; no heap). Raising the floor from 2 (Standard) to 5 (Enhanced) adds
three entries per MonitoredItem to the caller-owned server object:

- standard (1000 items): **+168,000 B (~164.1 KiB)**
- full (2000 items): **+336,000 B (~328.1 KiB)**

`.text` is unaffected (the change is a capacity constant, not code). A future shared-pool
queue could support depth-5-on-request without paying 5× on every item, but the no-heap
fixed-size model trades that RAM for determinism; the trade is deliberate and documented
here rather than hidden.

## Evidence

- `test_subscriptions_capacity` — `test_enhanced_capacity_macros_meet_profile_minimums`
  asserts all four grounded minima; `test_enhanced_monitored_item_queue_holds_five_before_overflow`
  is the behavioral proof that a client-requested `queueSize = 5` retains 5 distinct
  samples before overflow (not clamped to the Standard depth of 2). Both are gated on
  `MUC_OPCUA_ENHANCED_DATACHANGE`.
- `test_subscriptions` — the underlying DataChange sampling / notification / Republish
  behavior shared with the Standard tier.
- Compile-time: one `_Static_assert` in each canonical capacity-CU translation
  unit rejects a resolved value below that CU's minimum.
