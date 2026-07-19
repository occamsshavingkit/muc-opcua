# Conformance Status

**Targeted spec revision:** OPC-10000-7 **v1.05.02** (2022). The 2017 Application
Profile URIs are retained (v1.05.02 did not mint new ones), but the backing
conformance units are the v1.05.02 set. Per-profile limits/capacities are
documented in [documentation.md](documentation.md) (the mandatory documentation
CU for limits, Mantis 7003).

**Target Profile:** Embedded 2022 UA Server Profile
(`http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2017`) for the `embedded`
build. `nano` and `micro` remain available as lower-tier profile builds.

**Status:** `profile-targeting`. This server has **not** been verified with the OPC
Foundation Compliance Test Tool (CTT); a compliance claim is withheld
until a CTT run passes (project policy, plan.md).

## StatusCode names

The following StatusCode names are the canonical identifiers used by
`include/muc_opcua/status.h` for the audit-hardening StatusCode surface. Name
spelling is grounded in OPC-10000-4 section 7.38.2 Common StatusCodes; numeric
values are listed below.

| StatusCode name | Implementation constant | Audit-hardening use | OPC UA reference |
|---|---|---|---|
| `Good` | `MU_STATUS_GOOD` | Successful service or operation result | OPC-10000-4 section 7.38.2 |
| `Bad_DecodingError` | `MU_STATUS_BAD_DECODINGERROR` | Malformed or truncated binary input | OPC-10000-4 section 7.38.2 |
| `Bad_EncodingLimitsExceeded` | `MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED` | Encoded input exceeds supported limits, incl. a value array over `MaxArrayLength` (spec 057) | OPC-10000-4 section 7.38.2 |
| `Bad_ServiceUnsupported` | `MU_STATUS_BAD_SERVICEUNSUPPORTED` | Unsupported service request | OPC-10000-4 section 7.38.2 |
| `Bad_NotSupported` | `MU_STATUS_BAD_NOTSUPPORTED` | Unsupported operation, field encoding, or PubSub layout within a supported service/API | OPC-10000-4 section 7.38.2 |
| `Bad_InvalidArgument` | `MU_STATUS_BAD_INVALIDARGUMENT` | Missing or inconsistent caller-supplied arguments | OPC-10000-4 section 7.38.2 |
| `Bad_MonitoredItemFilterUnsupported` | `MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED` | Unsupported monitored item filter | OPC-10000-4 section 7.38.2 |
| `Bad_MonitoredItemFilterInvalid` | `MU_STATUS_BAD_MONITOREDITEMFILTERINVALID` | Invalid monitored item filter parameters | OPC-10000-4 section 7.38.2 |
| `Bad_FilterNotAllowed` | `MU_STATUS_BAD_FILTERNOTALLOWED` | Filter supplied where the service disallows one | OPC-10000-4 section 7.38.2 |
| `Bad_TimestampsToReturnInvalid` | `MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID` | Invalid TimestampsToReturn value | OPC-10000-4 section 7.38.2 |
| `Bad_BrowseDirectionInvalid` | `MU_STATUS_BAD_BROWSEDIRECTIONINVALID` | Invalid BrowseDirection value | OPC-10000-4 section 7.38.2 |
| `Bad_SessionNotActivated` | `MU_STATUS_BAD_SESSIONNOTACTIVATED` | Existing session used before activation | OPC-10000-4 section 7.38.2 |
| `Bad_SessionIdInvalid` | `MU_STATUS_BAD_SESSIONIDINVALID` | Missing, unknown, or invalid session id | OPC-10000-4 section 7.38.2 |
| `Bad_SecureChannelIdInvalid` | `MU_STATUS_BAD_SECURECHANNELIDINVALID` | Service request not bound to a valid SecureChannel | OPC-10000-4 section 7.38.2 |
| `Bad_SecurityChecksFailed` | `MU_STATUS_BAD_SECURITYCHECKSFAILED` | Security check failure, incl. OpenSecureChannel clock-skew rejection when `MUC_OPCUA_TIME_SYNC` is enabled (Security Time Synch - Configuration, spec 055) | OPC-10000-4 section 7.38.2 |
| `Bad_IdentityTokenRejected` | `MU_STATUS_BAD_IDENTITYTOKENREJECTED` | Rejected user identity token | OPC-10000-4 section 7.38.2 |
| `Bad_ResponseTooLarge` | `MU_STATUS_BAD_RESPONSETOOLARGE` | Response would exceed supported response limits | OPC-10000-4 section 7.38.2 |
| `Bad_TcpMessageTypeInvalid` | `MU_STATUS_BAD_TCPMESSAGETYPEINVALID` | Invalid OPC UA TCP message type | OPC-10000-4 section 7.38.2 |
| `Bad_TooManyOperations` | `MU_STATUS_BAD_TOOMANYOPERATIONS` | Request contains more operations than supported | OPC-10000-4 section 7.38.2 |
| `Bad_TypeMismatch` | `MU_STATUS_BAD_TYPEMISMATCH` | Value type does not match the target attribute | OPC-10000-4 section 7.38.2 |
| `Bad_ContinuationPointInvalid` | `MU_STATUS_BAD_CONTINUATIONPOINTINVALID` | Invalid continuation point supplied by the client | OPC-10000-4 section 7.38.2 |

## StatusCode numeric values

The following hexadecimal values are the exact `include/muc_opcua/status.h`
constants used by the audit-hardening StatusCode surface. Symbolic names are
grounded in OPC-10000-4 section 7.38.2 Common StatusCodes.

| StatusCode name | Numeric value | Implementation constant | OPC UA reference |
|---|---:|---|---|
| `Good` | `0x00000000` | `MU_STATUS_GOOD` | OPC-10000-4 section 7.38.2 |
| `Bad_DecodingError` | `0x80070000` | `MU_STATUS_BAD_DECODINGERROR` | OPC-10000-4 section 7.38.2 |
| `Bad_EncodingLimitsExceeded` | `0x80080000` | `MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED` | OPC-10000-4 section 7.38.2 |
| `Bad_ServiceUnsupported` | `0x800B0000` | `MU_STATUS_BAD_SERVICEUNSUPPORTED` | OPC-10000-4 section 7.38.2 |
| `Bad_NotSupported` | `0x803D0000` | `MU_STATUS_BAD_NOTSUPPORTED` | OPC-10000-4 section 7.38.2 |
| `Bad_InvalidArgument` | `0x80AB0000` | `MU_STATUS_BAD_INVALIDARGUMENT` | OPC-10000-4 section 7.38.2 |
| `Bad_MonitoredItemFilterUnsupported` | `0x80440000` | `MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED` | OPC-10000-4 section 7.38.2 |
| `Bad_MonitoredItemFilterInvalid` | `0x80430000` | `MU_STATUS_BAD_MONITOREDITEMFILTERINVALID` | OPC-10000-4 section 7.38.2 |
| `Bad_FilterNotAllowed` | `0x80450000` | `MU_STATUS_BAD_FILTERNOTALLOWED` | OPC-10000-4 section 7.38.2 |
| `Bad_TimestampsToReturnInvalid` | `0x802B0000` | `MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID` | OPC-10000-4 section 7.38.2 |
| `Bad_BrowseDirectionInvalid` | `0x804D0000` | `MU_STATUS_BAD_BROWSEDIRECTIONINVALID` | OPC-10000-4 section 7.38.2 |
| `Bad_SessionNotActivated` | `0x80270000` | `MU_STATUS_BAD_SESSIONNOTACTIVATED` | OPC-10000-4 section 7.38.2 |
| `Bad_SessionIdInvalid` | `0x80250000` | `MU_STATUS_BAD_SESSIONIDINVALID` | OPC-10000-4 section 7.38.2 |
| `Bad_SecureChannelIdInvalid` | `0x80220000` | `MU_STATUS_BAD_SECURECHANNELIDINVALID` | OPC-10000-4 section 7.38.2 |
| `Bad_SecurityChecksFailed` | `0x80130000` | `MU_STATUS_BAD_SECURITYCHECKSFAILED` | OPC-10000-4 section 7.38.2 |
| `Bad_IdentityTokenRejected` | `0x80210000` | `MU_STATUS_BAD_IDENTITYTOKENREJECTED` | OPC-10000-4 section 7.38.2 |
| `Bad_ResponseTooLarge` | `0x80B90000` | `MU_STATUS_BAD_RESPONSETOOLARGE` | OPC-10000-4 section 7.38.2 |
| `Bad_TcpMessageTypeInvalid` | `0x807E0000` | `MU_STATUS_BAD_TCPMESSAGETYPEINVALID` | OPC-10000-4 section 7.38.2 |
| `Bad_TooManyOperations` | `0x80100000` | `MU_STATUS_BAD_TOOMANYOPERATIONS` | OPC-10000-4 section 7.38.2 |
| `Bad_TypeMismatch` | `0x80740000` | `MU_STATUS_BAD_TYPEMISMATCH` | OPC-10000-4 section 7.38.2 |
| `Bad_ContinuationPointInvalid` | `0x804A0000` | `MU_STATUS_BAD_CONTINUATIONPOINTINVALID` | OPC-10000-4 section 7.38.2 |

Conformance-unit status (Core 2022 Server Facet groups; see [services.md](services.md)):

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

The Micro Embedded Device 2022 Server Profile builds on Nano (see
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

The Embedded 2022 profile surface is selected with `MUC_OPCUA_PROFILE=embedded`
or the `configs/embedded.defconfig` seed. See [profile-embedded.md](profile-embedded.md)
for the conformance-unit map.

| Conformance group / unit | Status | Evidence |
|---|---|---|
| SecurityPolicy Basic256Sha256 | Implemented | `test_asym_chunk`, `test_sym_chunk`, `test_server_handshake_secure` |
| Security Default ApplicationInstance Certificate | Satisfied | `test_certificate`, existing certificate handling |
| Security Time Sync (CU 151) | Implemented | `test_time_sync` |
| ECC Security Policies (curve25519/nist256) | Implemented (full only) | `test_ecc_crypto`, `test_ecc_handshake_e2e` |
| Authorization Service / JWT (CU 1629/1697) | Implemented (full only) | `test_jwt`, `test_jwt_activate_session` |
| Standard DataChange Subscription 2022 facet | Implemented | `test_subscriptions_capacity`, `test_subscriptions` |
| Enhanced DataChange Subscription 2022 facet (standard/full; mandated by StandardUA2017) | Implemented | `test_subscriptions_capacity` (`test_enhanced_*`); see `enhanced-datachange.md` |
| Base Server Behaviour: Session General Service Behaviour (auth token · requestHandle · timeoutHint) | Implemented | `test_service_state_errors`, `test_write_response`, `test_base_server_behaviour`; see `base-server-behaviour.md` |
| ServerDiagnostics object (optional; `ServerDiagnosticsSummary` i=2275, standard/full) | Implemented | `test_diagnostics`; see `base-server-behaviour.md` |
| Reverse Connect (optional; server-initiated, ReverseHello first per §7.1.3, standard/full) | Implemented | `test_reverse_connect`; see `reverse-connect.md` |
| Client Redundancy (optional; TransferSubscriptions cross-session + RedundancySupport, full) | Implemented | `test_transfer_subscriptions`; see `redundancy.md` |
| Base Info Type System | Implemented | `test_type_system`, `test_view_services` |
| Data Access Server Facet | Implemented (standard/full) | `test_da_type_nodes`, `test_percent_deadband` |
| Method Call: GetMonitoredItems / ResendData | Implemented | `test_method_call`, `test_method_call_errors` |
| Method Server Facet (arbitrary user methods) | Implemented (full) | `test_method_call_arbitrary` |
| Write Service | Implemented | Optional feature via `MUC_OPCUA_SERVICE_WRITE` |
| Alarms & Conditions (Events) | Implemented | Event notifications via `MUC_OPCUA_EVENTS` |
| Event Filter Where-Clause | Implemented (full) | `src/services/event_filter.c` |
| Dynamic Nodes | Implemented | Runtime node addition via `MUC_OPCUA_DYNAMIC_NODES` |
| NodeManagement Services | Implemented | Dynamic NodeManagement via `MUC_OPCUA_SERVICE_NODEMANAGEMENT` |
| Query Services | Implemented | Search address space via `MUC_OPCUA_SERVICE_QUERY` |
| Historical Access (HA) | Implemented | HistoryRead/HistoryUpdate via `MUC_OPCUA_SERVICE_HISTORY` |
| Aggregate Subscriptions | Implemented | Average/Min/Max calculation via `MUC_OPCUA_SUBSCRIPTIONS_STANDARD` (Feature 018) |

## Remaining
1. **CTT verification** — run the OPC Foundation Compliance Test Tool against the
   server and record results; only then may a profile-compliance claim be made.
   (External step.)
2. **External interoperability/CTT evidence** — the implementation is profile-targeting
   and locally tested; CTT evidence is still required before promoting the claim.

See `specs/002-nano-profile-completion/` and `specs/003-micro-profile-completion/` for
the task breakdowns.
