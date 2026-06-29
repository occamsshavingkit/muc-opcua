# Contract: Protocol Repair Behavior

## StatusCode Contract

**References**: OPC-10000-4 §7.38.2; OPC Foundation `StatusCode.csv`.

The public StatusCode constants changed by this feature must equal the canonical wire values. Required examples include:

| Macro | Standard Value |
|-------|----------------|
| `MU_STATUS_BAD_NOTFOUND` | `0x803E0000` |
| `MU_STATUS_BAD_NODECLASSINVALID` | `0x805F0000` |
| `MU_STATUS_BAD_NODEIDEXISTS` | `0x805E0000` |
| `MU_STATUS_BAD_NODEIDREJECTED` | `0x805D0000` |
| `MU_STATUS_BAD_NOCONTINUATIONPOINTS` | `0x804B0000` |
| `MU_STATUS_BAD_MONITOREDITEMFILTERINVALID` | `0x80430000` |
| `MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED` | `0x80440000` |
| `MU_STATUS_BAD_WRITENOTSUPPORTED` | `0x80730000` |
| `MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED` | `0x80720000` |
| `MU_STATUS_BAD_TOOMANYSUBSCRIPTIONS` | `0x80770000` |
| `MU_STATUS_BAD_TOOMANYPUBLISHREQUESTS` | `0x80780000` |
| `MU_STATUS_BAD_SEQUENCENUMBERUNKNOWN` | `0x807A0000` |
| `MU_STATUS_BAD_MESSAGENOTAVAILABLE` | `0x807B0000` |

## AggregateFilter Contract

**References**: OPC-10000-4 §5.13.2.4, §5.13.3.4, §7.22.1, §7.22.4; OPC-10000-6 §5.2.2.9 and §5.2.2.15; OPC-10000-13 §4.2.2.4, §4.2.2.9, §4.2.2.10, §5.4.2.3, §5.4.3.5, §5.4.3.10, §5.4.3.11; OPC Foundation `NodeIds.csv`.

The server accepts `AggregateFilter_Encoding_DefaultBinary` as namespace 0 numeric `730`. It accepts only these aggregate function NodeIds for this scoped feature:

| Aggregate Function | NodeId |
|--------------------|--------|
| Average | `ns=0;i=2342` |
| Minimum | `ns=0;i=2346` |
| Maximum | `ns=0;i=2347` |

Stale identifiers `11565`, `11569`, and `11570` are OperationLimits variables and must not be accepted as standard aggregate functions.

## GetEndpoints Contract

**Reference**: OPC-10000-4 §5.5.4.2.

The server parses the `profileUris` array in the `GetEndpoints` request. If the array is empty or omitted, the server returns all locally configured endpoints. If the array is non-empty, each returned endpoint must support at least one requested transport profile URI. If none match, the service succeeds with an empty endpoint array.

## Size and Stack Contract

**References**: Project constitution §VII Size Discipline; plan audit baselines.

Post-change validation must record archive text sizes for all profiles and the stack-budget gate result. The gate may ignore a missing `.su` entry only when the compiler omitted an internal helper symbol; emitted frames remain subject to the configured threshold.
