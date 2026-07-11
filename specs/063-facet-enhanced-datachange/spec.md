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

## Grounding task (do first, before requirements are final)

The exact Enhanced DataChange Subscription 2017 minima must be confirmed from OPC-10000-7 /
the OPC profile DB ([[opc-profile-reporting-api]]) rather than memory — this is the one
un-pinned constant. The roadmap figure (MonitoredItems ≥ 500) is the working value; verify
and pin the full minima set (MonitoredItems, Subscriptions, MaxNotificationsPerPublish,
queue depth, MinSupportedSampleRate) before finalizing FR-2. Per project convention
([[ground-spec-constants-in-opc-ua-reference]]), no minimum ships un-grounded.

## Requirements

**FR-1 — Name the facet + which profiles claim it.** Introduce a marker for the Enhanced
DataChange Subscription facet (a documented predicate over the capacity macros, e.g.
"standard and full claim Enhanced; embedded claims Standard; nano/micro none"). No new
`-D` feature flag — the facet is a capacity tier, keyed off `MUC_OPCUA_SUBSCRIPTIONS_STANDARD`
+ the resolved `MU_INTERN_MAX_*` values.

**FR-2 — Assert the Enhanced minima where the facet is claimed.** A compile-time
`_Static_assert` (or the capacity test) that, for profiles claiming Enhanced, the resolved
`MU_INTERN_MAX_MONITORED_ITEMS` ≥ (grounded minimum, ~500), and the other grounded minima
hold — so a capacity override that drops below the claimed facet fails the build/test
rather than silently mis-advertising. Mirrors spec 057's "advertised == enforced" rule.

**FR-3 — Conformance doc + status.** `docs/conformance/enhanced-datachange.md` (the facet,
its grounded minima, which profiles claim it, the assertion); add the Enhanced row to
`docs/conformance/status.md` alongside the Standard one; note it in the profile docs
(profile-embedded = Standard tier; standard/full = Enhanced).

**FR-4 — Size + matrix.** No functional code change expected (capacities already suffice),
so `.text` should be ~flat; record the (near-zero) delta in the size ledger; full stays
under the 128 KiB stopper; all-profile matrix green.

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

- Raising any profile's actual capacities (standard/full already exceed the Enhanced
  minimum; this facet asserts, it does not enlarge).
- New subscription features (deadband/aggregate are prior facets); this is capacity tiering.

## On approval

Confirm the grounded minima first (grounding task above), then speckit plan/tasks, execute
test-first, PR against `main` with a merge commit. B4 of Project B; remeasure `full`
`.text` and continue to B5 while under the stopper.
