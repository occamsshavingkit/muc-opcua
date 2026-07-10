# Conformance: Data Access Server Facet (spec 060)

This server implements the OPC UA **Data Access Server Facet** (OPC-10000-8, Part 8:
Data Access) — the first facet of the Project-B Part-7 rollout. Gated behind
**`MUC_OPCUA_DATA_ACCESS`** (default **ON for standard/full**, requires
`MUC_OPCUA_BASE_NODES`). The Data Access facet is a standalone optional facet — not
mandated by the Standard 2025 UA Server Profile — that a server claims when it exposes
process values (AnalogItems, discrete items) with engineering metadata and supports
percent-deadband filtering.

Everything below is grounded against OPC-10000-8 (type tables from the spec markdown,
NodeIds against the official `schemas/1.05/NodeIds.csv`, the percent-deadband rule and
status codes from §7.2/§7.3.2) — no value is from memory.

## Served type model (OPC-10000-8 §5.3)

Under `MUC_OPCUA_DATA_ACCESS`, the base address space serves the DA VariableType nodes
with their `HasSubtype` hierarchy from `BaseDataVariableType` and each type's
Mandatory/Optional `HasProperty` instance-declarations (`HasModellingRule` →
Mandatory 78 / Optional 80, `HasTypeDefinition` → PropertyType 68):

| Type | NodeId | Mandatory properties | Optional properties |
|---|---|---|---|
| DataItemType | 2365 | — | Definition (2366), ValuePrecision (2367) |
| BaseAnalogType | 15318 | — | InstrumentRange (17567), EURange (17568), EngineeringUnits (17569) |
| **AnalogItemType** | 2368 | **EURange (2369)** | EngineeringUnits (2371), InstrumentRange (2370) |
| AnalogUnitType | 17497 | **EngineeringUnits (17502)** | EURange (17501), InstrumentRange (17500) |
| DiscreteItemType | 2372 (abstract) | — | — |
| TwoStateDiscreteType | 2373 | TrueState (2375), FalseState (2374) | — |
| MultiStateDiscreteType | 2376 | EnumStrings (2377) | — |
| MultiStateValueDiscreteType | 11238 | EnumValues (11241), ValueAsText (11461) | — |

Note (a common misconception, corrected here): **AnalogItemType mandates only
`EURange`** — `EngineeringUnits` and `InstrumentRange` are inherited from BaseAnalogType
as *Optional*. `EngineeringUnits` is Mandatory only on `AnalogUnitType`.

Property value DataTypes: EURange/InstrumentRange = `Range` (884); EngineeringUnits =
`EUInformation` (887); TrueState/FalseState/ValueAsText = `LocalizedText`; EnumStrings =
`LocalizedText[]`; EnumValues = `EnumValueType[]`.

**Instance nodes are integrator-supplied.** Consistent with muc-opcua's model, the
library serves the *type system*; the application supplies the actual AnalogItem /
discrete-item instance Variables (typed via `HasTypeDefinition`) and their EURange /
EngineeringUnits Property values. A helper is provided to construct a conformant
AnalogItem instance (Variable typed `AnalogItemType` with its Mandatory EURange
Property wired) — see `docs/integration-guide.md`.

## Data structure codecs

- **Range** (884, §5.6.2): `low` Double, `high` Double —
  `mu_binary_{read,write}_range`.
- **EUInformation** (887, §5.6.4.3): `namespaceUri` String, `unitId` Int32,
  `displayName` LocalizedText, `description` LocalizedText —
  `mu_binary_{read,write}_eu_information` (with a reusable
  `mu_binary_{read,write}_localized_text`, §5.2.2.14).

## Percent deadband (OPC-10000-8 §7.2 / §7.3.2)

A `DataChangeFilter` with `DeadbandType = Percent` (2) reports a value change when:

> |last cached value − current value| > (deadbandValue / 100.0) × ((high − low) of EURange)

The server's handling of a percent-deadband CreateMonitoredItems request:

| Condition | Result |
|---|---|
| `deadbandValue` ∈ [0.0, 100.0] and the monitored Variable has a readable `EURange` Property | **Good** — the item reports per the formula above |
| `deadbandValue` < 0.0 or > 100.0 | **Bad_DeadbandFilterInvalid** (§7.2) |
| the monitored Variable has **no** `EURange` Property | **Bad_DeadbandFilterInvalid** (§7.3.2: "a PercentDeadband is not supported, since an EURange is not configured") |
| `MUC_OPCUA_DATA_ACCESS` not built | **Bad_MonitoredItemFilterUnsupported** — percent deadband belongs to this facet, not the base data-change subset |

A zero-width EURange (`high == low`) is a *configured* span of 0.0 and is accepted
(distinct from an absent EURange).

## Out of scope (per spec 060)

- The **ArrayItemType** family axis metadata model (AxisInformation, Title,
  AxisScaleType) — the type nodes may be browsable but the array/matrix value
  semantics are deferred.
- **Write-side** range validation (clamping/rejecting Writes outside InstrumentRange) —
  DA is primarily a read/subscribe metadata facet.

## Verification

- **Codec KATs** — `tests/unit/test_eu_information.c` (EUInformation + LocalizedText
  round-trips).
- **Type-node structure** — `tests/unit/test_da_type_nodes.c` browses the served DA
  types and asserts the Mandatory/Optional property model against the grounded NodeIds
  (non-circular).
- **Percent deadband end-to-end** — CreateMonitoredItems over the wire: accepted on an
  AnalogItem with EURange (reports per formula); `Bad_DeadbandFilterInvalid` on a
  Variable without EURange and on an out-of-range percentage; still
  `Bad_MonitoredItemFilterUnsupported` on a non-DA build
  (`tests/integration/test_subscriptions.c`).

Still profile-targeting pending external CTT — the internal tests are the conformance
evidence (see `docs/conformance/services.md`).
