# Traceability: Embedded Profile Completion (feature 005)

Maps the Embedded 2017 UA Server Profile conformance units to OPC-10000 sections and to the
implementation/test files that satisfy them. Authoritative membership: `specs/005-embedded-profile-completion/research.md`.

**Target profile**: Embedded 2017 UA Server Profile —
`http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2017` (OPC-10000-7 §6.6.69) = Micro 2017
Server Profile + SecurityPolicy Basic256Sha256 + Standard DataChange Subscription 2017 Server
Facet (§6.6.17) + `Base Info Type System` CU. Status: `profile-targeting` (CTT pending).

## Conformance-unit → OPC section → file map

| Conformance unit (facet) | OPC-10000 citation | Impl file(s) | Test(s) | Status |
|---|---|---|---|---|
| `Monitored Items Deadband Filter` (Std DataChange) — absolute deadband | OPC-10000-4 §7.22.2 (DeadbandType=Absolute) | `src/services/subscription.c` (T013 eval), `src/core/service_dispatch.c` (T017 decode) | `test_subscriptions.c::test_monitored_item_absolute_deadband` | DONE |
| `Monitor MinQueueSize_02` (Std DataChange) — queue ≥2 + discardOldest + overflow | OPC-10000-4 §5.13.2, §7.20.1 (overflow) | `src/services/subscription.c` (T014), `src/core/service_dispatch.c` | `test_subscriptions.c::test_monitored_item_queue_overflow` | DONE |
| `Monitor Triggering` (Std DataChange) — SetTriggering | OPC-10000-4 §5.13.5, §5.13.1.6 | `src/services/subscription.c` (T015), `src/core/service_dispatch.c` (T018) | `test_subscriptions.c::test_set_triggering` | DONE |
| `Monitor Items 10` / `Monitor Items 100` (Std DataChange) | OPC-10000-4 §5.13.2 | `src/services/subscription.{c,h}`, `Makefile`/CMake `-D` | `tests/integration/test_subscriptions.c` (capacity) | pending |
| `Subscription Minimum 02` / `Subscription Publish Min 05` (Std DataChange) | OPC-10000-4 §5.14.2, §5.14.5 | `src/services/subscription.{c,h}`, build `-D` | `tests/integration/test_subscriptions.c` (capacity) | pending |
| `Method Call` (Std DataChange 2017) — Call service | OPC-10000-4 §5.11 | `src/core/service_dispatch.c` | `tests/integration/test_method_call.c` | pending |
| `Base Info GetMonitoredItems Method` (Std DataChange 2017) | OPC-10000-5 (Server methods); behavior via §5.11 | `src/address_space/base_nodes.c`, `src/services/subscription.c` | `tests/integration/test_method_call.c` | pending |
| `Base Info ResendData Method` (Std DataChange 2017) | OPC-10000-5 (Server methods); behavior via §5.11 | `src/address_space/base_nodes.c`, `src/services/subscription.c` | `tests/integration/test_method_call.c` | pending |
| `Base Info Type System` (Embedded) — full ns-0 type exposure | OPC-10000-5 (standard NodeSet); OPC-10000-3 (model) | `src/address_space/base_nodes.c` | `tests/integration/test_type_system.c` | pending |
| HasTypeDefinition instance→type | OPC-10000-3 §7.7 | `src/address_space/base_nodes.c` | `tests/integration/test_type_system.c` | pending |
| ServerProfileArray advertises profile | OPC-10000-5 (Server object) | `src/address_space/base_nodes.c` | `tests/integration/test_type_system.c` | pending |
| `Security Default ApplicationInstance Certificate` (Embedded) | OPC-10000-7 §6.6.69 (already shipped) | `src/security/certificate.c` (existing) | existing cert tests | satisfied (verify in docs) |

## Unsupported / out-of-scope (cited rejection)

| Feature | Why out | Expected StatusCode |
|---|---|---|
| TransferSubscriptions (§5.14.7) | Client Redundancy Facet, not required | `Bad_ServiceUnsupported` (if called) |
| Percent / aggregate deadband (§7.22.2 / OPC-10000-8) | Data Access Server Facet | `Bad_MonitoredItemFilterUnsupported` |
| Arbitrary methods via Call | only GetMonitoredItems/ResendData implemented | `Bad_MethodInvalid` / `Bad_NotImplemented` |

(Fill the per-file/test mappings and flip Status to `done` as each task completes.)
