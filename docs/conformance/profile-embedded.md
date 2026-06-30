# Conformance: Embedded 2017 UA Server Profile

This server targets the **Embedded 2017 UA Server Profile**
(`http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2017`, OPC-10000-7 §6.6.69)
when built with `make embedded` or `-DMICRO_OPCUA_EMBEDDED_PROFILE=ON`.

Status is **profile-targeting**. OPC-10000-7 §4.2 governs conformance-unit and
conformance-group claims, and OPC-10000-7 §4.3 governs profile claims. This
document is a traceability map for the selected embedded target; the repo is not
profile-compliant and not CTT-verified without external CTT evidence.

## Transport and Encoding

The embedded build targets OPC UA TCP over `opc.tcp` with UA Secure Conversation
and OPC UA Binary. The transport claim is scoped to OPC-10000-6 §7.2 OPC UA TCP,
with the OPC UA TCP message header from OPC-10000-6 §7.1.2.2, Hello negotiation
from OPC-10000-6 §7.1.2.3, and Acknowledge negotiation from OPC-10000-6 §7.1.2.4.
Service payloads and scalar/container values are encoded with OPC UA Binary per
OPC-10000-6 §5.2. XML, JSON, HTTPS, WebSocket, and alternate transport profile
claims are out of scope for this profile-targeting build.

## Build Surface

`make embedded` enables:

- SecurityPolicy Basic256Sha256 and SecurityPolicy None.
- Data-change subscriptions with Standard DataChange Subscription 2017 facet additions.
- Raised Standard capacity minima:
  `MU_MAX_MONITORED_ITEMS=100`, `MU_MAX_SUBSCRIPTIONS=2`,
  `MU_MAX_PUBLISH_REQUESTS=5`, `MU_MONITORED_QUEUE_DEPTH=2`.
- Base Info Type System exposure.
- Minimal Call service limited to `Server.GetMonitoredItems` and `Server.ResendData`.

## Conformance Units

Statuses below describe targeted behavior with repository evidence, not completed
OPC-10000-7 §4.2 conformance-unit or §4.3 profile verification.

| Group / unit | OPC-10000 citation | Status | Evidence |
|---|---|---|---|
| Profile claim basis | OPC-10000-7 §4.2, §4.3, §6.6.69 | Profile-targeting only | `MICRO_OPCUA_EMBEDDED_PROFILE`, `make embedded`, `docs/traceability/005-embedded-profile-completion.md`; no external CTT evidence |
| Transport / encoding | OPC-10000-6 §5.2, §7.1.2.2, §7.1.2.3, §7.1.2.4, §7.2 | Targeted surface | `src/core/tcp_connection.c`, `src/core/message_chunk.c`, `src/encoding/*`, `tests/unit/test_tcp_connection.c`, `tests/unit/test_message_chunk_errors.c` |
| Micro 2017 base | OPC-10000-7 Micro profile definition | Targeted behavior | `profile-micro.md`, `test_subscriptions`, `test_session`, `test_single_client_limit` |
| SecurityPolicy None | OPC-10000-7 Core/Nano security baseline | Targeted behavior | `profile-nano.md`, handshake/interoperability tests |
| SecurityPolicy Basic256Sha256 | OPC-10000-7 Embedded security policy support | Targeted behavior | `src/security/*`, `test_asym_chunk`, `test_sym_chunk`, `test_server_handshake_secure` |
| Security Default ApplicationInstance Certificate | OPC-10000-7 §6.6.69 security CU | Targeted behavior | `src/security/certificate.c`, `test_certificate`, secure handshake tests |
| Standard DataChange Subscription 2017 facet | OPC-10000-7 §6.6.17 | Targeted behavior | `tests/unit/test_subscriptions_capacity.c`, `tests/integration/test_subscriptions.c` |
| Monitored Items Deadband Filter | OPC-10000-4 §7.22.2 | Targeted behavior | absolute-deadband coverage in subscription tests |
| Monitored Items Aggregate Filter | OPC-10000-4 §7.22.4; OPC-10000-13 §5.4.3.5; OPC-10000-13 §5.4.3.10; OPC-10000-13 §5.4.3.11 | Profile-targeting, scoped | Average/Min/Max filters in `src/services/subscription.c`, `tests/unit/test_aggregate.c` |
| Monitor MinQueueSize_02 | OPC-10000-4 §5.13.2, §7.20.1 | Targeted behavior | queue/discard/overflow coverage in subscription tests |
| Monitor Triggering | OPC-10000-4 §5.13.5, §5.13.1.6 | Targeted behavior | SetTriggering coverage in subscription tests |
| Subscription Minimum 02 | OPC-10000-4 §5.14.2 | Targeted behavior | `tests/unit/test_subscriptions_capacity.c` |
| Subscription Publish Min 05 | OPC-10000-4 §5.14.5 | Targeted behavior | `tests/unit/test_subscriptions_capacity.c` |
| Method Call | OPC-10000-4 §5.12.2.2 | Targeted, scoped | `src/core/service_dispatch.c`, `tests/unit/test_method_call*.c` |
| GetMonitoredItems | OPC-10000-5 §8.3.2, §9.1 | Targeted behavior | `src/services/subscription.c`, `tests/unit/test_method_call.c` |
| ResendData | OPC-10000-5 §8.3.2, §9.2 | Targeted behavior | `src/services/subscription.c`, `tests/unit/test_method_call.c` |
| Base Info Type System | OPC-10000-5 standard NodeSet; OPC-10000-3 §7.7 | Targeted behavior | `src/address_space/base_nodes.c`, `tests/unit/test_type_system.c` |
| ServerProfileArray | OPC-10000-5 Server object; OPC-10000-7 §4.3 | Targeted metadata | Embedded target URI in `Server.ServerCapabilities.ServerProfileArray`; not external CTT evidence |
| Events and alarms | OPC-10000-9; OPC-10000-4 §5.13.1 | Targeted behavior | Event notifications via `mu_server_trigger_event`, `tests/unit/test_event_notifications.c` |
| Historical Access (HA) | OPC-10000-11 | Targeted behavior | HistoryRead/HistoryUpdate services, `tests/unit/test_history.c` |
| Query Services | OPC-10000-4 §5.9 | Targeted behavior | QueryFirst/QueryNext services, `tests/unit/test_query_service.c` |

## Explicitly Out of Scope

| Feature | Reason | Expected behavior |
|---|---|---|
| TransferSubscriptions | Client Redundancy Facet, not part of the selected Embedded profile slice | Service unsupported |
| Percent deadband | Data Access Server Facet, not required by the Standard DataChange Subscription 2017 facet | Filter unsupported |
| Arbitrary user methods | Only the two Base Info methods are required for this profile work | `Bad_MethodInvalid` for unknown methods |

## Validation Snapshot

- Default host suite: `ctest --test-dir build/test --output-on-failure` -> 61/61 passed.
- Embedded-capacity host suite: `ctest --test-dir build/us3-call --output-on-failure` -> 61/61 passed.
- Embedded Makefile build: `make embedded` -> built `build/embedded/examples/minimal_server`.
- ASan focused Call suite: `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/us3-call-asan --output-on-failure -R 'test_method_call'` -> 2/2 passed.
- Fuzz target: `fuzz_call` builds with Clang 18.1.3 and passes fixed-input smoke.

CTT remains the external evidence gate. Do not claim profile-compliant status
without an external CTT evidence report. Do not claim CTT-verified status without
an external CTT evidence report.
