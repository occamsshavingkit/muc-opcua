# Spec 060: Data Access Server Facet (B1)

First facet of **Project B** (Part 7 Server Facet rollout, per
`docs/superpowers/specs/2026-07-09-profile-modernization-and-part7-facets-roadmap.md`).
Feature flag: **`MUC_OPCUA_DATA_ACCESS`** (already exists; default ON for standard/full,
requires `MUC_OPCUA_BASE_NODES`). Scope decision (locked with user 2026-07-10):
**implement the facet completely** — serve the DA type-system nodes *and* wire percent
deadband end to end, not just the minimal wire-up.

Everything below is grounded against OPC-10000-8 (Part 8: Data Access) — type tables
pulled as raw spec markdown, type NodeIds via the reference `search_nodes`, and
instance-declaration (property) NodeIds against the official
`schemas/1.05/NodeIds.csv`. No constant is from memory.

## Motivating gap (current `MUC_OPCUA_DATA_ACCESS` state)

The flag today ships a percent-deadband *evaluator* and an EURange-span *reader* that
are **unreachable over the wire** plus **no DA type nodes**:

- `filter_reader.c:150-156` decodes `DeadbandType=Percent` but unconditionally sets
  `Bad_MonitoredItemFilterUnsupported` — and that file is **not even gated on
  `MUC_OPCUA_DATA_ACCESS`**. So the percent evaluator in
  `subscription_monitor.c:123-128` and the span reader `mu_resolve_eurange_span`
  (`base_nodes.c:1057-1080`) are dead code from the network path.
- `base_nodes.c` serves **no** DA type nodes (AnalogItemType/DataItemType/DiscreteItem*).
- `mu_eu_information_t` is defined (`types.h`) but has **no binary codec**.
- `docs/conformance/` treats percent deadband as deliberately *unsupported*
  (`services.md:93`, `profile-embedded.md:69`).

Two of those are outright bugs against the grounded spec (see below): the status code
is wrong, and the reject is unconditional.

## Grounded correction 1 — the no-EURange / bad-value status code

Current code returns `Bad_MonitoredItemFilterUnsupported` for percent deadband. **That
is wrong.** OPC-10000-8 §7.3.2 (verbatim):

> **Bad_DeadbandFilterInvalid** — The specified PercentDeadband is not between 0.0 and
> 100.0 **or a PercentDeadband is not supported, since an EURange is not configured.**

So on a DA build, a percent-deadband request must be:
- **Accepted** (`Good`) when the monitored Variable has a resolvable `EURange`
  Property and `0.0 ≤ deadbandValue ≤ 100.0`.
- **Rejected with `Bad_DeadbandFilterInvalid`** when `deadbandValue` is out of
  `[0.0, 100.0]`, **or** when the Variable has no `EURange` Property.

## Grounded correction 2 — percent deadband formula (OPC-10000-8 §7.2)

> DataChange if ( |last cached value − current value| >
> (deadbandValue/100.0) × ((high−low) of EURange) )

The existing evaluator (`subscription_monitor.c:124-127`) already computes exactly
`threshold = (deadband/100.0) * deadband_span` with `deadband_span = EURange.high −
EURange.low` — it is correct; it is only unreachable. This spec makes it reachable.

## Grounded correction 3 — AnalogItemType mandatory properties

Common misconception: EngineeringUnits is mandatory on AnalogItemType. **It is not.**
Per OPC-10000-8 §5.3.2.3, **AnalogItemType (2368) directly declares ONLY `EURange`
(2369) as Mandatory**; `EngineeringUnits` (2371) and `InstrumentRange` (2370) are
inherited from BaseAnalogType as **Optional**. `EngineeringUnits` is Mandatory only on
`AnalogUnitType` (17497). The served type model must reflect this exactly.

## Grounded DA type hierarchy (OPC-10000-8 §5.3; ns=0 NodeIds)

`BaseDataVariableType` → **DataItemType (2365)** → { **BaseAnalogType (15318)** →
{AnalogItemType, AnalogUnitType, …}, **DiscreteItemType (2372, abstract)** →
{TwoStateDiscreteType, MultiStateDiscreteType, MultiStateValueDiscreteType},
**ArrayItemType (12021)** → {YArrayItemType, …} }. All ValueRank = −2 (Any).

| Type | NodeId | IsAbstract | DataType | Subtype of | Mandatory props (instance NodeId) | Optional props (instance NodeId) |
|---|---|---|---|---|---|---|
| DataItemType | 2365 | False | BaseDataType | BaseDataVariableType | — | Definition (2366), ValuePrecision (2367) |
| BaseAnalogType | 15318 | False | Number | DataItemType | — | InstrumentRange (17567), EURange (17568), EngineeringUnits (17569), (+ number-range variants) |
| **AnalogItemType** | 2368 | False | Number | BaseAnalogType | **EURange (2369)** | EngineeringUnits (2371), InstrumentRange (2370), Definition (3774), ValuePrecision (3775) |
| AnalogUnitType | 17497 | False | Number | BaseAnalogType | **EngineeringUnits (17502)** | EURange (17501), InstrumentRange (17500) |
| DiscreteItemType | 2372 | **True** | BaseDataType | DataItemType | — | (inherits Definition/ValuePrecision) |
| **TwoStateDiscreteType** | 2373 | False | **Boolean** | DiscreteItemType | TrueState (2375), FalseState (2374) | — |
| **MultiStateDiscreteType** | 2376 | False | **UInteger** | DiscreteItemType | EnumStrings (2377) | — |
| **MultiStateValueDiscreteType** | 11238 | False | **Number** | DiscreteItemType | EnumValues (11241), ValueAsText (11461) | — |
| ArrayItemType (+5 subtypes) | 12021 | (base) | — | DataItemType | axis/Title/AxisScaleType/EURange/EngineeringUnits | — |

Property DataTypes: EURange/InstrumentRange = `Range` (884); EngineeringUnits =
`EUInformation` (887); TrueState/FalseState/ValueAsText = `LocalizedText`; EnumStrings
= `LocalizedText[]`; EnumValues = `EnumValueType[]` (7594); Definition = `String`;
ValuePrecision = `Double`.

## Grounded structures

- **Range (DataType 884, §5.6.2):** `low` Double, `high` Double. (Codec already
  exists: `mu_binary_read_range`/`mu_binary_write_range`; verify field order matches.)
- **EUInformation (DataType 887, §5.6.4.3):** `namespaceUri` String, `unitId` Int32,
  `displayName` LocalizedText, `description` LocalizedText — in that order. **No codec
  exists yet.** `mu_eu_information_t` in `types.h` matches these four fields.

## Requirements

**FR-1 — Percent deadband accept path (the behavioral core).** Under
`MUC_OPCUA_DATA_ACCESS`, `read_datachange_filter_body` (`filter_reader.c`) must NOT
blanket-reject `DeadbandType=Percent`. Instead:
- gate the reject on `#ifndef MUC_OPCUA_DATA_ACCESS` (non-DA builds keep returning
  `Bad_MonitoredItemFilterUnsupported`, unchanged);
- on a DA build, validate `0.0 ≤ deadbandValue ≤ 100.0` → else
  `Bad_DeadbandFilterInvalid` (set on `body->filter_result`);
- at monitored-item creation (`subscription_helpers.c`), resolve the EURange span
  from the target Variable; if the span is unavailable (no EURange Property) →
  `Bad_DeadbandFilterInvalid` on that item's result. (Distinguish "no EURange" from a
  legitimately zero-width span — see FR-1a.)
- when accepted, the existing evaluator applies unchanged.

**FR-1a — EURange resolution must distinguish absent from zero.** `mu_resolve_eurange_span`
currently returns `0.0` both for "no EURange" and for "EURange with high==low". These
must be told apart (a real zero-width EURange is degenerate but *configured*). Change
the resolver to report presence separately (out-param / sentinel), so FR-1 can return
`Bad_DeadbandFilterInvalid` only for genuinely-absent EURange.

**FR-2 — Serve the DA type-system nodes.** `base_nodes.c` (under
`MUC_OPCUA_DATA_ACCESS`) must serve the VariableType nodes in the table above with
correct `HasSubtype` hierarchy from `BaseDataVariableType`, and each type's
Mandatory/Optional **HasProperty** instance-declaration nodes with the correct
`HasModellingRule` reference (Mandatory → 78, Optional → 80) and `HasTypeDefinition` →
`PropertyType` (68). NodeId-sorted (binary-search invariant; `test_base_address_space_is_sorted`).
Minimum set to serve: DataItemType, BaseAnalogType, AnalogItemType, AnalogUnitType,
DiscreteItemType, TwoStateDiscreteType, MultiStateDiscreteType,
MultiStateValueDiscreteType + their declared property instance-declarations.
ArrayItemType family: serve the type nodes (browse-completeness) but its axis
property model may be deferred (see Out of scope).

**FR-3 — EUInformation binary codec.** Add `mu_binary_read_eu_information` /
`mu_binary_write_eu_information` (encoding.h) matching the grounded 4-field layout, so
integrators can serve/read an `EngineeringUnits` Property value. Symmetric with the
existing Range codec.

**FR-4 — Integrator-facing AnalogItem helper.** Because muc-opcua's model is
"integrator supplies instance nodes," provide a helper to construct a conformant
AnalogItem instance (a Variable typed `AnalogItemType` with its Mandatory `EURange`
Property wired) so integrators can expose DA values without hand-assembling the
reference/property graph. Scope the helper to AnalogItem (the dominant case); document
the pattern for the discrete/multistate types.

**FR-5 — Conformance doc.** `docs/conformance/data-access.md` claiming the Data Access
Server Facet: the served type model, the percent-deadband behavior with the grounded
status codes, per-type CU names (OPC-10000-8 names each: "Data Access AnalogItemType",
"Data Access DataItemType", …), and the verification pointers. Update `services.md` and
`profile-embedded.md`/`profile-micro.md` where they currently say percent deadband is
unsupported (now conditional on the DA build).

**FR-6 — Size + gating.** Measure `.text` delta (`scripts/measure_size.sh`); confirm
the DA type nodes land only in DA-ON builds and standard/full stays under the 128 KiB
Project-B stopper. Record in `docs/size/feature-size-ledger.md`.

## Verification (test-first; internal conformance evidence)

- **Wire e2e (the crux):** over a secured or plain channel, CreateMonitoredItems with
  a percent-deadband DataChangeFilter on an AnalogItem with EURange → `Good`, and the
  item reports only on changes exceeding `(pct/100)·span`; on a Variable WITHOUT
  EURange → per-item `Bad_DeadbandFilterInvalid`; with `deadbandValue` 150.0 →
  `Bad_DeadbandFilterInvalid`. (Replaces the current
  `test_subscriptions.c` assertion that percent → `Bad_MonitoredItemFilterUnsupported`,
  which must now hold only on a non-DA build.)
- **Type-node browse:** Browse AnalogItemType (2368) → `HasProperty` EURange (2369)
  with `HasModellingRule` Mandatory (78); TwoStateDiscreteType (2373) → TrueState/
  FalseState Mandatory; MultiStateValueDiscreteType (11238) → EnumValues + ValueAsText
  Mandatory. Assert the served ModellingRule matches the grounded table (non-circular:
  the expected NodeIds/rules are the grounded constants, not read back from our own
  table).
- **EUInformation codec round-trip** KAT with the 4 fields.
- **Sorted-nodes invariant** stays green (`test_base_address_space_is_sorted`).
- **Non-DA build:** percent deadband still `Bad_MonitoredItemFilterUnsupported`; no DA
  type nodes present (symbol/browse check); `.text` unchanged for nano/micro/embedded.

## Out of scope (this spec)

- The **ArrayItemType axis metadata model** (AxisInformation, Title, AxisScaleType,
  X/Y/Z axis definitions) — serve the type nodes for browse-completeness but defer the
  full array/matrix DA value semantics to a later spec if a facet CU demands it.
- **Write-side DA validation** (e.g. rejecting a Write outside InstrumentRange) — DA is
  primarily a read/subscribe metadata facet; no Part-8 mandate to clamp writes here.
- **AnalogUnitRangeType / AnalogNumberItemType** and other deep BaseAnalog subtypes
  beyond the table — add only if the facet CU list (once groundable) requires them.
- The aggregated **Data Access Server Facet CU membership list** could not be cheaply
  ground from the profile DB (opaque GUID ids; SPA-only search). Per-type CU names ARE
  grounded from Part 8. Confirm the exact facet CU set during implementation if the
  profile API becomes queryable; the per-type coverage above is the substantive target.

## On approval

Formalize plan.md + tasks.md (speckit), execute test-first, measure size, PR against
`main` with a **merge commit** (project convention). Merge order: this is B1; B2–B7
follow only while the size budget holds.
