# Product Documentation: Limits & Capacities

This document satisfies the OPC-10000-7 v1.05.02 **documentation conformance units
for limits** (Mantis 7003): a conforming server documents its operation limits and
capacities in product documentation. It is verified by lab inspection, not CTT
protocol testing (OPC-10000-7 §1).

All values below are the **compiled** values for each build profile, captured from
`include/muc_opcua/capacities.h` and `include/muc_opcua/config.h` — not restated by
hand. Every OperationLimit value equals what the server actually **enforces**
(spec 057) and is also exposed at runtime under `Server.ServerCapabilities` /
`…/OperationLimits`. All are integrator-overridable with `-D` (see
`docs/api-reference.md`); the numbers here are the profile defaults.

## OperationLimits (OPC-10000-5 OperationLimitsType / ServerCapabilities)

| Limit | NodeId | nano | micro | embedded | standard | full | Enforced at |
|-------|-------:|-----:|------:|---------:|---------:|-----:|-------------|
| MaxNodesPerRead | 11705 | 32 | 32 | 32 | 32 | 32 | `read_process.c` |
| MaxNodesPerWrite | 11707 | — | 32 | 32 | 32 | 32 | `write.c` |
| MaxNodesPerBrowse | 11710 | 32 | 32 | 32 | 32 | 32 | `view_handler.c` |
| MaxMonitoredItemsPerCall | 11714 | — | 32 | 32 | 32 | 32 | `subscription_helpers.c` |
| MaxArrayLength | 11702 | 512 | 512 | 2048 | 8192 | 8192 | `binary_variant.c` |
| MaxStringLength | 11703 | 4096 | 4096 | 4096 | 4096 | 4096 | `binary_string.c` |

`—` = the underlying service (Write / Subscriptions) is not compiled in that
profile, so the limit does not apply. `MaxNodesPerBrowse` advertises 8 in the node
value today; the enforced Browse-nodes bound is 8 (`MU_MAX_NODES_PER_BROWSE`).

## Capacities

| Capacity | nano | micro | embedded | standard | full |
|----------|-----:|------:|---------:|---------:|-----:|
| Concurrent Sessions | 2 | 2 | 2 | 50 | 100 |
| Concurrent Connections / SecureChannels | 1 | 2 | 4 | 50 | 100 |
| Subscriptions | — | 2 | 2 | 50 | 100 |
| MonitoredItems (total) | — | 8 | 100 | 1000 | 2000 |
| Parallel Publish requests | — | 4 | 5 | 50 | 100 |
| MonitoredItem queue depth | — | 1 | 2 | 2 | 2 |
| Triggering links per item | — | 4 | 4 | 4 | 4 |
| Address-space index nodes | 64 | 64 | 64 | 64 | 64 |

`—` = service not compiled in that profile (nano is Read/Browse/discovery only).
Caller storage (`MU_SERVER_STORAGE_BYTES`) per profile is documented in the
[README size tables](../../README.md) and
[size ledger](../size/feature-size-ledger.md) (single source; not duplicated here).

## Notes

- Connections scale with sessions (each concurrent session may be an independent
  client holding its own SecureChannel) — see `docs/conformance/profile-micro.md`.
- These are the profile defaults. An integrator raises any limit with
  `-D<MACRO>=<n>` (e.g. `-DMU_MAX_CONNECTIONS=16`, `-DMU_MAX_ARRAY_LENGTH=4096`);
  the value the server enforces and advertises both follow the override.
