# Contract: Documentation Evidence and Stale-Number Guards

## Scope

Documentation and tests must keep support claims current for user and implementor docs, with exact OPC UA citations where behavior affects the standard.

## Required Checks

| Check | Expected Result |
|---|---|
| Unsupported `profile-compliant` or `CTT-verified` wording | Fails unless explicit external evidence is present |
| Known stale README phrases | Fails for stale "remaining Micro item" and implemented-feature "Not implemented yet" lists |
| StatusCode names and values | Fails when docs cite a value different from `include/micro_opcua/status.h` |
| Aggregate NodeIds | Fails when stale `11565`, `11569`, or `11570` appear as Average/Minimum/Maximum aggregate IDs |
| Query section reference | Fails for the current vague Query section placeholder; requires OPC-10000-4 Appendix B §B.2.3/§B.2.4 |
| NodeManagement section reference | Requires OPC-10000-4 §5.8 |
| Numeric size/benchmark prose | Must be snapshot-labeled or point to a reproducible command/evidence file |

## OPC UA References

- OPC-10000-7 §4.2 and §4.3 for ConformanceUnit/Profile claim discipline.
- OPC-10000-4 §7.38.2 for StatusCode names and values.
- OPC-10000-4 §5.8 for NodeManagement.
- OPC-10000-4 Appendix B §B.2.3 and §B.2.4 for Query.
- OPC-10000-4 §7.22.4 and OPC-10000-13 §4.2.2.4/§4.2.2.9/§4.2.2.10 for aggregate function identifiers.
