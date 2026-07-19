# Conformance: Enhanced DataChange Subscription 2022 Server Facet (spec 063)

This server implements the OPC UA **Enhanced DataChange Subscription 2022 Server Facet**
(OPC profile-DB facet `EnhancedDataChangeSubscription2017`, **id 1678**). It is a
**capacity tier** of the DataChange subscription engine, not a distinct code path — it
raises the mandatory minima of the base **Standard DataChange Subscription 2022** facet
(id 1675, which Enhanced *includes*).

## Why the server claims it (it is mandatory, not optional)

The `standard` and `full` Kconfig profiles advertise
`http://opcfoundation.org/UA-Profile/Server/StandardUA2017` (see
`src/address_space/base_nodes.c`, `s_server_profile_array`, gated by the generated
`MUC_OPCUA_STANDARD_PROFILE` marker). The **Standard 2022 UA Server Profile (id 1663)** lists the
Enhanced DataChange 2022 facet with `isOptional = false` — so any server advertising
StandardUA2017 **must** meet the Enhanced minima. `embedded` advertises `EmbeddedUA2017`,
which mandates only the plain **Standard DataChange 2022** facet (a lower queue-depth
floor), so embedded is a Standard-tier — not Enhanced — DataChange server.

| Profile | Advertised profile | DataChange facet claimed | Queue depth |
|---|---|---|---|
| nano / micro | Nano / (Micro via Nano) | none / base subscriptions | 1 |
| embedded | EmbeddedUA2017 | Standard DataChange 2022 (`MinQueueSize_02`) | 2 |
| standard | StandardUA2017 | **Enhanced DataChange 2022** (`MinQueueSize_05`) | **5** |
| full | StandardUA2017 | **Enhanced DataChange 2022** (`MinQueueSize_05`) | **5** |

## Grounded minima (all four CUs are mandatory)

Grounded from the live OPC profile DB (facet id 1678,
[profiles.opcfoundation.org](https://profiles.opcfoundation.org)); `isOptional = false`
for every unit. There is **no** `MaxNotificationsPerPublish` or `MinSupportedSampleRate`
CU in this facet — the set below is exhaustive.

| Conformance unit | Minimum | Resolved (standard / full) | Enforced by |
|---|---|---|---|
| `Monitor Items 500` | ≥ 500 MonitoredItems / Subscription | 1000 / 2000 | `MU_INTERN_MAX_MONITORED_ITEMS` |
| `Monitor MinQueueSize_05` | ≥ 5 queue entries / MonitoredItem | 5 / 5 | `MU_INTERN_MONITORED_QUEUE_DEPTH` |
| `Subscription Minimum 05` | ≥ 5 Subscriptions / Session | 50 / 100 | `MU_INTERN_MAX_SUBSCRIPTIONS` |
| `Subscription Publish Min 10` | ≥ 10 Publish requests / Session | 50 / 100 | `MU_INTERN_MAX_PUBLISH_REQUESTS` |

## Advertised == enforced

The facet marker is `MUC_OPCUA_ENHANCED_DATACHANGE` (derived in
`include/muc_opcua/features.h` when the generated `MUC_OPCUA_STANDARD_PROFILE` marker is set —
i.e. when the build advertises StandardUA2017). When it is set, `src/services/subscription.h` carries
four `_Static_assert`s that fail the build if any resolved capacity drops below the claimed
minima. So a capacity override such as `-DMU_MONITORED_QUEUE_DEPTH=2` on a standard build is
a **compile error**, not a silently mis-advertised profile. Mirrors the "advertised ==
enforced" rule of spec 057.

## Cost of the queue-depth floor

The monitored-item queue is a **fixed inline ring** of `MU_INTERN_MONITORED_QUEUE_DEPTH`
entries per item (56 B/entry; no heap). Raising the floor from 2 (Standard) to 5 (Enhanced)
adds 3 entries × 56 B **per MonitoredItem**:

- standard (1000 items): **+164 KiB** static RAM
- full (2000 items): **+328 KiB** static RAM

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
- Compile-time: the `subscription.h` `_Static_assert`s (negative-tested: a
  `-DMU_MONITORED_QUEUE_DEPTH=2` standard build fails to compile).
