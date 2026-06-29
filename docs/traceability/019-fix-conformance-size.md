# Traceability: OPC UA Conformance and Size Repairs

## Normative Sources

| Topic | Reference |
|-------|-----------|
| Profiles and conformance claims | OPC-10000-7 §4.3 |
| GetEndpoints `profileUris` filtering | OPC-10000-4 §5.5.4.2 |
| MonitoredItem Create/Modify filter StatusCodes | OPC-10000-4 §5.13.2.4; OPC-10000-4 §5.13.3.4 |
| AggregateFilter | OPC-10000-4 §7.22.4 |
| Common StatusCodes | OPC-10000-4 §7.38.2; OPC Foundation `StatusCode.csv` |
| NodeId binary encoding | OPC-10000-6 §5.2.2.9 |
| ExtensionObject binary encoding | OPC-10000-6 §5.2.2.15 |
| Average aggregate | OPC-10000-13 §4.2.2.4; OPC-10000-13 §5.4.3.5 |
| Minimum aggregate | OPC-10000-13 §4.2.2.9; OPC-10000-13 §5.4.3.10 |
| Maximum aggregate | OPC-10000-13 §4.2.2.10; OPC-10000-13 §5.4.3.11 |

## Official Table Sources

- OPC Foundation UA NodeSet `UA-1.05.07-2026-04-15` `NodeIds.csv`: `https://raw.githubusercontent.com/OPCFoundation/UA-Nodeset/UA-1.05.07-2026-04-15/Schema/NodeIds.csv`
- OPC Foundation UA NodeSet `UA-1.05.07-2026-04-15` `StatusCode.csv`: `https://raw.githubusercontent.com/OPCFoundation/UA-Nodeset/UA-1.05.07-2026-04-15/Schema/StatusCode.csv`

## Audit Baselines

| Profile | ARM archive text bytes |
|---------|------------------------|
| nano | 16,653 |
| micro | 23,962 |
| embedded | 47,126 |
| full | 47,400 |

| RAM Proxy | Host bytes |
|-----------|------------|
| `struct mu_server` nano | 944 |
| `struct mu_server` micro | 3,048 |
| `struct mu_server` embedded | 69,344 |
| `struct mu_server` full | 77,832 |
| `mu_monitored_item_t` embedded/full | 392 |
| `mu_connection_t` embedded/full | 3,376 |

## Post-Change Measurements

| Profile | ARM archive text bytes | Delta from baseline |
|---------|------------------------|---------------------|
| nano | 16,919 | +266 |
| micro | 24,228 | +266 |
| embedded | 47,400 | +274 |
| full-featured | 47,678 | +278 |

| RAM Proxy | Host bytes | Delta from baseline |
|-----------|------------|---------------------|
| `struct mu_server` nano | 944 | 0 |
| `struct mu_server` micro | 3,048 | 0 |
| `struct mu_server` embedded | 89,120 | +19,776 |
| `struct mu_server` full | 97,608 | +19,776 |
| `mu_monitored_item_t` embedded/full | 392 | 0 |
| `mu_connection_t` embedded/full | 8,560 | +5,184 |

The ROM delta is the measured cost of corrected protocol behavior. The RAM
delta is the measured cost of enforcing OPC-10000-6 §7.1.2.3 and §7.1.2.4
receive-buffer minimums for every configured multi-connection slot. A smaller
per-slot buffer was rejected because it could undercut negotiated OPC UA TCP
chunk limits.

## Requirement Traceability

| Requirement | Behaviour/Change | OPC UA section(s) | StatusCode / NodeId | Impl file(s) | Test(s) | Status |
|-------------|------------------|-------------------|---------------------|--------------|---------|--------|
| FR-001 | Public StatusCode constants use official values. | OPC-10000-4 §7.38.2 | `StatusCode.csv` values | `include/micro_opcua/status.h`; `src/core/status.c` | `tests/unit/test_status.c` | Done |
| FR-002 | AggregateFilter binary encoding NodeId is standard. | OPC-10000-4 §7.22.4; OPC-10000-6 §5.2.2.15 | `AggregateFilter_Encoding_DefaultBinary` `ns=0;i=730` | `include/micro_opcua/opcua_ids.h`; `src/core/service_dispatch.c` | `tests/unit/test_aggregate.c` | Done |
| FR-003 | Average, Minimum, and Maximum aggregate functions use standard NodeIds. | OPC-10000-13 §4.2.2.4; OPC-10000-13 §4.2.2.9; OPC-10000-13 §4.2.2.10 | `ns=0;i=2342`; `ns=0;i=2346`; `ns=0;i=2347` | `include/micro_opcua/opcua_ids.h`; `src/services/subscription.c` | `tests/unit/test_aggregate.c` | Done |
| FR-004 | Scoped AggregateFilter validation rejects non-standard functions. | OPC-10000-4 §5.13.2.4; OPC-10000-4 §7.22.4 | `Bad_MonitoredItemFilterUnsupported` | `src/core/service_dispatch.c` | `tests/unit/test_aggregate.c` | Done |
| FR-006 | GetEndpoints filters returned endpoints by requested transport profile URI. | OPC-10000-4 §5.5.4.2 | Good service result with matching or empty endpoint array | `src/core/service_dispatch.c`; `src/services/discovery.c` | `tests/unit/test_discovery_endpoint.c` | Done |
| FR-007 | Stack-budget gate handles optimized-away internal frames without hiding emitted-frame violations. | OPC-10000-7 §4.3 | Profile-targeting claim honesty | `scripts/check_stack_usage.sh` | `tests/fixtures/stack_usage/missing-root/server.su`; `test_stack_budget` | Done |
| FR-008 | RAM/ROM controls are measured and documented. | OPC-10000-7 §4.3 | Profile-targeting claim honesty | `include/micro_opcua/config.h`; `src/core/server_internal.h`; `src/services/session.c` | `tests/unit/test_build_config.c`; `tests/unit/test_session.c` | Done |
| FR-010 | Documentation cites current sections and avoids unsupported conformance claims. | OPC-10000-4 §7.22.4; OPC-10000-7 §4.3 | Profile-targeting, not CTT-verified | `docs/conformance/*`; `docs/traceability/*`; `specs/018-aggregate-subscriptions/*` | `tests/unit/test_traceability_docs.c`; `tests/unit/test_conformance_docs.c` | Done |
