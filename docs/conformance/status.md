# Conformance Status

**Target Profile:** Embedded 2017 UA Server Profile
(`http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2017`) for the `embedded`
build. `nano` and `micro` remain available as lower-tier profile builds.

**Status:** `profile-targeting`. This server has **not** been verified with the OPC
Foundation Compliance Test Tool (CTT); a compliance claim is withheld
until a CTT run passes (project policy, plan.md).

Conformance-unit status (Core 2017 Server Facet groups; see [services.md](services.md)):

| Conformance group / unit | Status | Evidence |
|---|---|---|
| Transport — UA-TCP UA-SC UA-Binary | Implemented | `test_server_handshake*`, `interop`/`dotnet-interop` CI |
| Security — SecurityPolicy None | Implemented | interop with asyncua + .NET reference client |
| Security — Basic256Sha256 (beyond Nano) | Implemented | `dotnet-interop` SignAndEncrypt (#162-#166) |
| Security — User Tokens | Implemented | ActivateSession supports Anonymous, UserName, and X509 |
| Discovery — FindServers / GetEndpoints | Implemented | `test_discovery_*` |
| Session — Base / General Behaviour / Minimum 1 | Implemented | `test_session`, `test_server_handshake*` |
| Attribute — Read | Implemented | `test_read_service`, Base Info reads |
| View — Browse (View Basic) | Implemented | `test_browse_*`, handshake tests |
| View — BrowseNext | Implemented | `test_view_services` |
| View — TranslateBrowsePaths | Implemented | `test_view_services` |
| View — RegisterNodes / UnregisterNodes | Implemented | `test_view_services` |
| Address Space Base — standard nodes | Implemented (static) | `base_nodes.c`; `test_view_services` |
| Base Info — Server / ServerCapabilities objects | Implemented (static) | `base_nodes.c` |
| Base Info — ServerStatus.CurrentTime/StartTime (live) | Implemented | `base_nodes.c` runtime value sources; `test_view_services` |

All in-scope Nano conformance units are implemented and covered by tests/CI.

## Micro profile progress (Embedded Data Change Subscription Server Facet + ≥2 sessions)

The Micro Embedded Device 2017 Server Profile builds on Nano (see
[profile-micro.md](profile-micro.md)). The subscription facet is implemented; the
≥2-session requirement is implemented.

| Conformance group / unit | Status | Evidence |
|---|---|---|
| Subscription — Create/Modify/SetPublishingMode/Delete | Implemented | `test_subscriptions` (§5.14.2/.3/.4/.8) |
| Subscription — Publish (async) + keep-alive | Implemented | `test_subscriptions` (§5.14.5, §5.14.1) |
| Subscription — Republish + acknowledgements | Implemented | `test_subscriptions` (§5.14.6, §5.14.5) |
| MonitoredItem — Create/Modify/SetMode/Delete (data change) | Implemented | `test_subscriptions` (§5.13.2/.3/.4/.6) |
| Data-change sampling (DataChangeFilter trigger) | Implemented | `test_subscriptions` (§5.12.1.6, §7.17.2) |
| ≥2 concurrent sessions | Implemented | up to `MU_MAX_SESSIONS` logical sessions multiplexed over up to `MU_MAX_CONNECTIONS` active SecureChannels; `test_session` |

## Embedded profile progress

The Embedded 2017 profile surface is implemented behind `make embedded` /
`MICRO_OPCUA_EMBEDDED_PROFILE=ON`. See [profile-embedded.md](profile-embedded.md) for the
conformance-unit map.

| Conformance group / unit | Status | Evidence |
|---|---|---|
| SecurityPolicy Basic256Sha256 | Implemented | `test_asym_chunk`, `test_sym_chunk`, `test_server_handshake_secure` |
| Security Default ApplicationInstance Certificate | Satisfied | `test_certificate`, existing certificate handling |
| Standard DataChange Subscription 2017 facet | Implemented | `test_subscriptions_capacity`, `test_subscriptions` |
| Base Info Type System | Implemented | `test_type_system`, `test_view_services` |
| Method Call: GetMonitoredItems / ResendData | Implemented | `test_method_call`, `test_method_call_errors` |
| Write Service | Implemented | Optional feature via `MICRO_OPCUA_SERVICE_WRITE` |
| Alarms & Conditions (Events) | Implemented | Event notifications via `MICRO_OPCUA_EVENTS` |
| Dynamic Nodes | Implemented | Runtime node addition via `MICRO_OPCUA_DYNAMIC_NODES` |

## Remaining
1. **CTT verification** — run the OPC Foundation Compliance Test Tool against the
   server and record results; only then may a profile-compliance claim be made.
   (External step.)
2. **External interoperability/CTT evidence** — the implementation is profile-targeting
   and locally tested; CTT evidence is still required before promoting the claim.

See `specs/002-nano-profile-completion/` and `specs/003-micro-profile-completion/` for
the task breakdowns.
