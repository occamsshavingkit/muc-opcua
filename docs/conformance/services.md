# Conformance: Services Support Matrix

Status of each OPC UA Service in micro-opcua. `Implemented` means handled with a
spec-correct response and covered by tests; `Unsupported` returns
`Bad_ServiceUnsupported`. Concurrent sessions (up to MU_MAX_SESSIONS, default 2)
are supported.

| Service | OPC 10000-4 § | Status | Notes |
|---------|---------------|--------|-------|
| FindServers | 5.5.2 | Implemented | Returns this server's ApplicationDescription |
| GetEndpoints | 5.5.4.2 | Implemented | None + Basic256Sha256 (Sign / SignAndEncrypt) endpoints; applies `profileUris` transport-profile filtering |
| OpenSecureChannel | 5.6.2 | Implemented | None and Basic256Sha256 (asymmetric handshake) |
| CloseSecureChannel | 5.6.3 | Implemented | |
| CreateSession | 5.7.2 | Implemented | ServerSignature on secured channels; ServerEndpoints |
| ActivateSession | 5.7.3 | Implemented | Supports Anonymous, UserName, and X509 identity tokens |
| CloseSession | 5.7.4 | Implemented | |
| Read | 5.11.2 | Implemented | Attribute Read incl. the Base Information nodes |
| Browse | 5.9.2 | Implemented | HierarchicalReferences + includeSubtypes |
| BrowseNext | 5.9.3 | Implemented | No continuation points issued -> `Bad_ContinuationPointInvalid` |
| TranslateBrowsePathsToNodeIds | 5.9.4 | Implemented | RelativePath walk over the address space; `Bad_NoMatch` |
| RegisterNodes | 5.9.5 | Implemented | Identity mapping (NodeIds copied back) |
| UnregisterNodes | 5.9.6 | Implemented | No-op, returns Good |
| Write | 5.11.4 | Implemented | Supported behind `MICRO_OPCUA_SERVICE_WRITE` |
| Call | 5.11.2 | Implemented | Behind `MICRO_OPCUA_EMBEDDED_PROFILE` (supports GetMonitoredItems/ResendData + custom methods) |
| CreateMonitoredItems | 5.13.2 | Implemented | Data-change monitoring; initial sample; `Bad_NodeIdUnknown` / `Bad_TooManyMonitoredItems` |
| ModifyMonitoredItems | 5.13.3 | Implemented | Revised sampling interval / clientHandle |
| SetMonitoringMode | 5.13.4 | Implemented | Disabled / Sampling / Reporting |
| DeleteMonitoredItems | 5.13.6 | Implemented | `Bad_MonitoredItemIdInvalid` |
| CreateSubscription | 5.14.2 | Implemented | Revised publishing interval / lifetime / keep-alive; `Bad_TooManySubscriptions` |
| ModifySubscription | 5.14.3 | Implemented | Revised timing parameters |
| SetPublishingMode | 5.14.4 | Implemented | Disabled → keep-alives only |
| Publish | 5.14.5 | Implemented | Parked + answered asynchronously by the publishing timer; keep-alive; ack processing |
| Republish | 5.14.6 | Implemented | Resends the retained NotificationMessage; `Bad_MessageNotAvailable` |
| TransferSubscriptions | 5.14.7 | Unsupported | Above the Embedded DataChange facet (Standard tier) |
| DeleteSubscriptions | 5.14.8 | Implemented | Deletes the subscription and its MonitoredItems |
| SetTriggering | 5.13.5 | Implemented | Behind MICRO_OPCUA_EMBEDDED_PROFILE (links monitored items within a subscription) |
| HistoryRead / HistoryUpdate | 5.11.3 / 5.11.5 | Implemented | Behind `MICRO_OPCUA_SERVICE_HISTORY`, persistence adapter based |
| AddNodes / DeleteNodes / AddReferences / DeleteReferences | 5.7 | Implemented | Behind `MICRO_OPCUA_SERVICE_NODEMANAGEMENT` and `MICRO_OPCUA_DYNAMIC_NODES` |
| QueryFirst / QueryNext | 5.9.x | Implemented | Behind `MICRO_OPCUA_SERVICE_QUERY` |

The View Service Set (Browse, BrowseNext, TranslateBrowsePaths, RegisterNodes,
UnregisterNodes) is the set of Core Server Facet "View" conformance units required
by Nano; all are now implemented (`tests/integration/test_view_services.c`).

The Subscription Service Set (§5.14) and MonitoredItem Service Set (§5.13) implement
the **Embedded Data Change Subscription Server Facet** required by the Micro profile —
data-change monitoring, an asynchronous Publish flow driven by `mu_server_poll`,
keep-alives, and Republish — all no-heap and behind the `MICRO_OPCUA_SUBSCRIPTIONS`
build option (`tests/integration/test_subscriptions.c`). The Standard DataChange 2017
facet is supported under `MICRO_OPCUA_EMBEDDED_PROFILE=ON`, which implements SetTriggering,
Call (with GetMonitoredItems/ResendData methods), and larger monitored-item/queue bounds.
The TransferSubscriptions service remains unsupported. Aggregate filters (Average/Min/Max)
are supported under `MICRO_OPCUA_SUBSCRIPTIONS_STANDARD` as scoped `AggregateFilter`
support per OPC-10000-4 §7.22.4 and OPC-10000-13 §5.4.3.5, §5.4.3.10, and §5.4.3.11
(Feature 018).
