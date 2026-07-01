# Conformance: Services Support Matrix

Status of each OPC UA Service in micro-opcua. This matrix is a
profile-targeting claim only: OPC-10000-7 section 4.2 governs
ConformanceUnits, and OPC-10000-7 section 4.3 governs Profiles. The rows below
record implemented subsets that are used to target selected ConformanceUnits;
they are not profile-compliant and not CTT-verified claims. `Implemented
subset` means the documented behavior is handled with a cited service response
and covered by project tests. `Optional implemented subset` means the behavior
is available only when the named build option is enabled. `Unsupported` returns
`Bad_ServiceUnsupported`. Concurrent sessions (up to MU_MAX_SESSIONS, default
2) are an implemented subset of session handling, not a profile claim.

| Service | OPC 10000-4 § | Status | Notes |
|---------|---------------|--------|-------|
| FindServers | 5.5.2 | Implemented subset | Returns this server's ApplicationDescription after applying documented filters |
| GetEndpoints | 5.5.4.2 | Implemented subset | None + Basic256Sha256 (Sign / SignAndEncrypt) endpoints; applies `profileUris` transport-profile filtering |
| OpenSecureChannel | 5.6.2 | Implemented subset | None and Basic256Sha256 asymmetric handshake behavior documented for this project |
| CloseSecureChannel | 5.6.3 | Implemented subset | Closes the active SecureChannel state |
| CreateSession | 5.7.2 | Implemented subset | ServerSignature on secured channels; returns ServerEndpoints |
| ActivateSession | 5.7.3 | Implemented subset | Anonymous, UserName, and X509 identity token paths documented for this project |
| CloseSession | 5.7.4 | Implemented subset | Closes the active Session state |
| Read | 5.11.2 | Implemented subset | Attribute Read incl. the Base Information nodes |
| Browse | 5.9.2 | Implemented subset | HierarchicalReferences + includeSubtypes |
| BrowseNext | 5.9.3 | Implemented subset | No continuation points issued -> `Bad_ContinuationPointInvalid` |
| TranslateBrowsePathsToNodeIds | 5.9.4 | Implemented subset | RelativePath walk over the address space; `Bad_NoMatch` |
| RegisterNodes | 5.9.5 | Implemented subset | Identity mapping (NodeIds copied back) |
| UnregisterNodes | 5.9.6 | Implemented subset | No-op, returns Good |
| Write | 5.11.4 | Optional implemented subset | Behind `MICRO_OPCUA_SERVICE_WRITE` |
| Call | 5.12.2 | Optional implemented subset | Behind `MICRO_OPCUA_EMBEDDED_PROFILE` (GetMonitoredItems/ResendData + custom methods only) |
| CreateMonitoredItems | 5.13.2 | Implemented subset | Data-change monitoring; initial sample; `Bad_NodeIdUnknown` / `Bad_TooManyMonitoredItems`; queue bounds |
| ModifyMonitoredItems | 5.13.3 | Implemented subset | Revised sampling interval / clientHandle; invalid MonitoredItemId -> `Bad_MonitoredItemIdInvalid` |
| SetMonitoringMode | 5.13.4 | Implemented subset | Disabled / Sampling / Reporting |
| DeleteMonitoredItems | 5.13.6 | Implemented subset | `Bad_MonitoredItemIdInvalid` |
| CreateSubscription | 5.14.2 | Implemented subset | Revised publishing interval / lifetime / keep-alive; `Bad_TooManySubscriptions` |
| ModifySubscription | 5.14.3 | Implemented subset | Revised timing parameters; invalid SubscriptionId -> `Bad_SubscriptionIdInvalid` |
| SetPublishingMode | 5.14.4 | Implemented subset | Disabled -> keep-alives only |
| Publish | 5.14.5 | Implemented subset | Parked + answered asynchronously by the publishing timer; keep-alive; ack processing; `Bad_TooManyPublishRequests` |
| Republish | 5.14.6 | Implemented subset | Resends the retained NotificationMessage; invalid sequence -> `Bad_MessageNotAvailable` |
| TransferSubscriptions | 5.14.7 | Unsupported | Dispatch returns `Bad_ServiceUnsupported`; `TransferSubscriptionsRequest_Encoding_DefaultBinary` ns=0;i=841 tested |
| DeleteSubscriptions | 5.14.8 | Implemented subset | Deletes the subscription and its MonitoredItems; invalid SubscriptionId -> `Bad_SubscriptionIdInvalid` |
| SetTriggering | 5.13.5 | Optional implemented subset | Behind `MICRO_OPCUA_EMBEDDED_PROFILE` (links monitored items within a subscription; link capacity -> `Bad_TooManyOperations`) |
| HistoryRead / HistoryUpdate | 5.11.3 / 5.11.5 | Optional implemented subset | Behind `MICRO_OPCUA_SERVICE_HISTORY`, persistence adapter based |
| AddNodes / DeleteNodes / AddReferences / DeleteReferences | 5.8 | Optional implemented subset | Behind `MICRO_OPCUA_SERVICE_NODEMANAGEMENT` and `MICRO_OPCUA_DYNAMIC_NODES` |
| QueryFirst / QueryNext | B.2.3 / B.2.4 | Optional implemented subset | Behind `MICRO_OPCUA_SERVICE_QUERY`; OPC-10000-4 Appendix B §B.2.3/§B.2.4 |

The View Service Set entries (Browse, BrowseNext, TranslateBrowsePaths,
RegisterNodes, UnregisterNodes) are documented as profile-targeting service
subsets for OPC-10000-7 section 4.2 ConformanceUnit analysis and section 4.3
Profile analysis. Current project evidence is `tests/integration/test_view_services.c`;
this does not assert Nano profile completion.

PubSub is outside the Part 4 Service Set matrix, so it is not listed as a Part
4 service. The documented optional `MICRO_OPCUA_PUBSUB` status is scoped
interoperability evidence only: a UADP/UDP publisher and matching
caller-storage subscriber decoder for one UInt32 PublisherId, one PayloadHeader
entry, one Data Key Frame DataSetMessage, and scalar Variant fields. The
Publisher and Subscriber scope is tied to OPC-10000-14 sections 5.4.2.1 and
5.4.2.2 plus the UDP transport in OPC-10000-14 section 7.3.2.1; the supported
UADP layout is limited to OPC-10000-14 sections 7.2.4.4.2, 7.2.4.5.2, and
7.2.4.5.5, with Variant fields grounded in OPC-10000-6 section 5.2.2.16. Under
OPC-10000-7 sections 4.2 and 4.3, this is no profile-compliance claim, no CTT
claim, and no full PubSub Subscriber profile claim.

The Subscription Service Set (OPC 10000-4 §5.14) and MonitoredItem Service Set
(OPC 10000-4 §5.13) are implemented as a profile-targeting data-change subset:
data-change monitoring, an asynchronous Publish flow driven by `mu_server_poll`,
keep-alives, and Republish, all no-heap and behind the `MICRO_OPCUA_SUBSCRIPTIONS`
build option (`tests/integration/test_subscriptions.c`). Under OPC-10000-7
section 4.2 and section 4.3 this is only evidence for selected ConformanceUnit
and Profile targeting, not a Micro profile or facet completion claim. The
`MICRO_OPCUA_EMBEDDED_PROFILE=ON` build adds an optional implemented subset for
SetTriggering, Call (with GetMonitoredItems/ResendData methods), and larger
monitored-item/queue bounds. The TransferSubscriptions service remains unsupported.

Subscription and MonitoredItem negative-path evidence is scoped to the tested
profile-targeting behavior. `tests/unit/test_subscriptions_errors.c` covers
OPC-10000-4 sections 5.13.3 and 5.13.6 invalid MonitoredItemId requests for
ModifyMonitoredItems and DeleteMonitoredItems returning
`Bad_MonitoredItemIdInvalid` without mutating subscription storage, and
OPC-10000-4 sections 5.14.3 and 5.14.8 invalid SubscriptionId requests for
ModifySubscription and DeleteSubscriptions returning `Bad_SubscriptionIdInvalid`
without mutating subscription storage.

| Negative path | OPC 10000-4 refs | Expected StatusCode | Evidence |
|---------------|------------------|---------------------|----------|
| ModifyMonitoredItems invalid MonitoredItemId | §5.13.3, §7.38.2 | Per-item `Bad_MonitoredItemIdInvalid`; existing MonitoredItem state unchanged | `tests/unit/test_subscriptions_errors.c`; related ModifyMonitoredItems flow in `tests/integration/test_subscriptions.c` |
| DeleteMonitoredItems invalid MonitoredItemId | §5.13.6, §7.38.2 | Per-item `Bad_MonitoredItemIdInvalid`; existing MonitoredItem state unchanged | `tests/unit/test_subscriptions_errors.c`; invalid delete coverage in `tests/integration/test_subscriptions.c` |
| ModifySubscription invalid SubscriptionId | §5.14.3, §7.38.2 | Service result `Bad_SubscriptionIdInvalid`; existing Subscription state unchanged | `tests/unit/test_subscriptions_errors.c`; related ModifySubscription/session flow in `tests/integration/test_subscriptions.c` |
| DeleteSubscriptions invalid SubscriptionId | §5.14.8, §7.38.2 | Per-id `Bad_SubscriptionIdInvalid`; existing Subscription state unchanged | `tests/unit/test_subscriptions_errors.c`; invalid delete/session ownership coverage in `tests/integration/test_subscriptions.c` |
| DataChangeFilter percent deadband unsupported | §5.13.2, §5.13.3, §7.22.2, §7.38.2 | Per-item `Bad_MonitoredItemFilterUnsupported`; percent deadband is outside the scoped data-change subset | `tests/unit/test_subscriptions_errors.c` |
| DataChangeFilter malformed filter body | §5.13.2, §5.13.3, §7.22.2, §7.38.2 | Malformed DataChangeFilter ExtensionObject decode returns `Bad_DecodingError`; monitored-item state is not updated | `tests/unit/test_subscriptions_errors.c` |
| AggregateFilter unsupported aggregate | §5.13.2, §5.13.3, §7.22.4, §7.38.2 | `Bad_MonitoredItemFilterUnsupported` for aggregate functions outside the scoped Average/Minimum/Maximum subset | `tests/unit/test_aggregate.c` |
| AggregateFilter malformed filter body | §5.13.2, §5.13.3, §7.22.4, §7.38.2 | Malformed AggregateFilter ExtensionObject decode returns `Bad_DecodingError`; full Part 13 aggregate coverage is not claimed | `tests/unit/test_aggregate.c` |
| Publish acknowledgement invalid sequence | §5.14.5.4, §7.38.2 | Per-ack result `Bad_SequenceNumberUnknown`; PublishResponse remains valid for other acknowledgements | `tests/integration/test_subscriptions.c` |
| Republish invalid sequence | §5.14.6.3, §7.38.2 | Service result `Bad_MessageNotAvailable`; unavailable retained NotificationMessage is not fabricated | `tests/integration/test_subscriptions.c` |

Capacity/resource paths are tested for
CreateSubscription per OPC-10000-4 section 5.14.2 (`Bad_TooManySubscriptions`),
CreateMonitoredItems per OPC-10000-4 section 5.13.2
(`Bad_TooManyMonitoredItems`), Publish queued request capacity per OPC-10000-4
section 5.14.5 (`Bad_TooManyPublishRequests`), and SetTriggering link capacity
per OPC-10000-4 section 5.13.5 (local `Bad_TooManyOperations`). Monitoring
parameters queue bounds and overflow behavior are tied to OPC-10000-4 section
5.13.2 and the resulting queue/notification behavior, not a full Subscription
Service Set claim. `tests/integration/test_subscriptions.c` covers the successful
service flow and session ownership paths used by integration-level evidence.
Publish invalid acknowledgement handling follows
OPC-10000-4 section 5.14.5.4 by returning per-ack
`Bad_SequenceNumberUnknown` results in PublishResponse. Republish invalid
sequence handling follows OPC-10000-4 section 5.14.6.3 by returning
service-level `Bad_MessageNotAvailable`. TransferSubscriptions remains outside
the implemented subset and dispatches `Bad_ServiceUnsupported` for
`TransferSubscriptionsRequest_Encoding_DefaultBinary` (ns=0;i=841).
Aggregate filters remain scoped `AggregateFilter` support under
`MICRO_OPCUA_SUBSCRIPTIONS_STANDARD`, not full OPC-10000-13 aggregate coverage.
Average is supported only within the implementation's verified subset and
build/config constraints: OPC-10000-4 section 7.22.4 describes the
`AggregateFilter` selection, OPC-10000-13 section 4.2.2.4 identifies the Average
AggregateFunction object, and OPC-10000-13 section 5.4.3.5 covers the Average
calculation exercised by `tests/unit/test_aggregate.c` (Feature 018). Minimum is
supported only within the implementation's verified subset and build/config
constraints: OPC-10000-4 section 7.22.4 describes the `AggregateFilter`
selection, OPC-10000-13 section 4.2.2.9 identifies the Minimum
AggregateFunction object, and OPC-10000-13 section 5.4.3.10 covers the Minimum
calculation exercised by `tests/unit/test_aggregate.c` (Feature 018). Maximum
is supported only within the implementation's verified subset and build/config
constraints: OPC-10000-4 section 7.22.4 describes the `AggregateFilter`
selection, OPC-10000-13 section 4.2.2.10 identifies the Maximum
AggregateFunction object, and OPC-10000-13 section 5.4.3.11 covers the Maximum
calculation exercised by `tests/unit/test_aggregate.c` (Feature 018).
Unsupported `AggregateFilter` configurations remain outside this scoped subset:
OPC-10000-4 section 7.22.4 defines the filter selection, OPC-10000-13 sections
4.2.2.4, 4.2.2.9, and 4.2.2.10 identify the only Average, Minimum, and Maximum
AggregateFunction objects recognized by this build, and OPC-10000-13 sections
5.4.3.5, 5.4.3.10, and 5.4.3.11 are the corresponding calculations exercised
by project aggregate tests. Other AggregateFunction NodeIds and full Part 13
aggregate status/configuration semantics are not claimed as implemented
coverage; `TimeAverage` and other AggregateFunction NodeIds outside the
Average/Minimum/Maximum trio are rejected with
`Bad_MonitoredItemFilterUnsupported`. Malformed `AggregateFilter`
ExtensionObjects are rejected during OPC-10000-6 section 5.2.2.15
ExtensionObject decode with `Bad_DecodingError`, while invalid or disallowed
filter uses remain per-item filter failures rather than success.
