# Spec 063: Enhanced DataChange Subscription Server Facet (B4)

Project-B facet B4 (roadmap `docs/superpowers/specs/2026-07-09-...-roadmap.md`). This is
a **capacity/minima** facet — no new feature flag; it formalizes the higher-capacity tier
of DataChange subscriptions as a named, asserted, documented facet. Scope stance (per
B1/B2/B3, locked with user): **implement completely** — assert the minima and document the
CU, don't just note it.

Grounded against OPC-10000-7 (Server Facets; the Enhanced DataChange Subscription 2017
facet) and the existing Standard DataChange facit baseline
(`tests/unit/test_subscriptions_capacity.c`, `docs/conformance/status.md`).

## Motivating gap

The server already ships the **Standard DataChange Subscription 2017 Server Facet**
(status.md:124, `test_subscriptions_capacity`) which asserts MonitoredItems ≥ 100,
Subscriptions ≥ 2, PublishRequests ≥ 5, queue depth ≥ 2. The **Enhanced** tier — which
industrial servers commonly claim — requires materially higher capacities (the roadmap's
"Monitor Items 500", grounded via the OPC profile DB, see [[opc-profile-reporting-api]]).
The current standard/full profiles already provision well above this (MonitoredItems
**1000**/**2000**, Subscriptions 50/100 — capacities.h), **but the Enhanced facet is
neither named, asserted, nor documented**, so:

- There is no build-time/test guarantee that a profile *claiming* the Enhanced facet meets
  its minima (a `-DMU_MAX_MONITORED_ITEMS=50` override would silently under-provision a
  server still advertising the facet).
- `docs/conformance/status.md` lists only the Standard facet; there is no Enhanced entry
  or conformance doc.

## Grounding OUTCOME (grounding task done — it overturned this spec's premise)

Grounded against the live OPC profile DB ([[opc-profile-reporting-api]]), facet
`EnhancedDataChangeSubscription2017` = **profile-DB id 1678**. Its four conformance units
are **all mandatory** (`isOptional=false`):

| CU | Minimum |
| --- | --- |
| `Monitor Items 500` | ≥ 500 MonitoredItems **per Subscription** |
| `Monitor MinQueueSize_05` | ≥ 5 queue entries **per MonitoredItem** |
| `Subscription Minimum 05` | ≥ 5 Subscriptions **per Session** |
| `Subscription Publish Min 10` | ≥ 10 Publish requests **per Session** |

Enhanced *includes* the Standard DataChange Subscription 2017 facet (id 1675) as its base.
There is **no** `MaxNotificationsPerPublish` or `MinSupportedSampleRate` CU in this facet —
the spec's earlier candidate list was wrong; the set above is exhaustive.

**The decisive fact — Enhanced is MANDATORY for the profile we advertise, not optional.**
`standard` and `full` builds advertise `http://opcfoundation.org/UA-Profile/Server/StandardUA2017`
(`base_nodes.c`, gated on `MUC_OPCUA_STANDARD_PROFILE`). The **Standard 2017 UA Server
Profile (id 1663)** lists the Enhanced DataChange 2017 facet directly with `isOptional=false`.
So a server advertising StandardUA2017 **must** meet the Enhanced minima.

Current provisioning vs. the grounded minima:

| Minimum | required | standard | full | met? |
| --- | --- | --- | --- | --- |
| MonitoredItems / Subscription | ≥ 500 | 1000 | 2000 | ✅ |
| Subscriptions / Session | ≥ 5 | 50 | 100 | ✅ |
| Publish / Session | ≥ 10 | 50 | 100 | ✅ |
| **Queue entries / MonitoredItem** | **≥ 5** | **2** | **2** | ❌ |

This **refutes** the original premise ("standard/full already exceed the minimum; assert
only, do not enlarge" — see revised Out-of-scope below). The monitored-item queue is a
fixed inline ring (`MU_INTERN_MONITORED_QUEUE_DEPTH`, 56 B/entry); we clamp any
client-requested `queueSize > 2` down to 2, so we do **not** satisfy `MinQueueSize_05` and
have been advertising StandardUA2017 while under-provisioning one of its mandatory facets.
The fix: raise the queue depth **2 → 5 for standard/full** (the StandardUA2017 claimants).
`embedded` advertises EmbeddedUA2017 → Standard DataChange 2017 (`MinQueueSize_02`), so it
correctly stays at depth 2. RAM cost of the fix (fixed-size model, no heap): +3 entries ×
56 B × MonitoredItems = **+164 KiB (standard, 1000 items) / +328 KiB (full, 2000 items)**;
`.text` is ~flat (a capacity constant, not code). nano/micro/embedded unchanged.

## Requirements

**FR-1 — Name the facet + which profiles claim it.** Introduce a marker for the Enhanced
DataChange Subscription facet (a documented predicate over the capacity macros, e.g.
"standard and full claim Enhanced; embedded claims Standard; nano/micro none"). No new
`-D` feature flag — the facet is a capacity tier, keyed off `MUC_OPCUA_SUBSCRIPTIONS_STANDARD`
+ the resolved `MU_INTERN_MAX_*` values.

**FR-2 — Raise the queue depth, then assert all four minima where the facet is claimed.**
First close the gap: split the queue-depth cascade so `standard`/`full` resolve
`MU_INTERN_MONITORED_QUEUE_DEPTH` to 5 (embedded stays 2). Then a compile-time
`_Static_assert` block, active when `MUC_OPCUA_ENHANCED_DATACHANGE` is claimed, that all
four grounded minima hold: `MU_INTERN_MAX_MONITORED_ITEMS` ≥ 500,
`MU_INTERN_MONITORED_QUEUE_DEPTH` ≥ 5, `MU_INTERN_MAX_SUBSCRIPTIONS` ≥ 5,
`MU_INTERN_MAX_PUBLISH_REQUESTS` ≥ 10 — so a capacity override (e.g.
`-DMU_MONITORED_QUEUE_DEPTH=2`) that drops below the claimed facet fails the build rather
than silently mis-advertising StandardUA2017. Mirrors spec 057's "advertised == enforced".
Verified behaviorally: a client requesting `queueSize = 5` on standard/full is honored, not
clamped.

**FR-3 — Conformance doc + status.** `docs/conformance/enhanced-datachange.md` (the facet,
its grounded minima, which profiles claim it, the assertion); add the Enhanced row to
`docs/conformance/status.md` alongside the Standard one; note it in the profile docs
(profile-embedded = Standard tier; standard/full = Enhanced).

**FR-4 — Size + RAM + matrix.** `.text` is ~flat (the change is a capacity constant, not
code); record the near-zero `.text` delta in the size ledger and the **RAM** delta
(+164 KiB standard / +328 KiB full from the deeper inline queue) so the cost is documented,
not silent; full stays under the 128 KiB `.text` stopper; all-profile matrix green;
nano/micro/embedded byte- and RAM-identical.

## Verification (test-first)

- **Minima assertion** — extend `test_subscriptions_capacity.c` (or a new
  `test_enhanced_datachange.c`) with the Enhanced minima checks, active only when the facet
  is claimed; a build with a below-minimum `-DMU_MAX_MONITORED_ITEMS` must fail the
  assertion (documented, not run in the default matrix).
- **Still-provisioned** — the existing capacity allocation tests continue to pass at the
  standard/full MonitoredItems counts.
- **Docs enforced** — `test_conformance_docs` / traceability green with the new doc + status
  row.
- Full matrix green; nano/micro/embedded unchanged.

## Out of scope

- Raising capacities *beyond* what StandardUA2017 mandates. (The original "assert only,
  never enlarge" stance is withdrawn: grounding proved the queue depth was below the
  mandatory `MinQueueSize_05`, so a single bounded raise — 2→5, standard/full — is in
  scope and required. MonitoredItems/Subscriptions/Publish already exceed their minima and
  are left untouched.)
- A dynamic / shared-pool queue that could support depth-5 without paying 5× storage on
  every item. Our no-heap model uses a fixed inline ring; a shared pool is a larger
  architectural change, deferred.
- New subscription features (deadband/aggregate are prior facets); this is capacity tiering.

## On approval

Confirm the grounded minima first (grounding task above), then speckit plan/tasks, execute
test-first, PR against `main` with a merge commit. B4 of Project B; remeasure `full`
`.text` and continue to B5 while under the stopper.
