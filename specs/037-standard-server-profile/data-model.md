# Data Model: Standard 2017 UA Server Profile

**Feature**: 037-standard-server-profile | **Phase**: 1 | **Date**: 2026-07-05

## Core Entities

### AnalogItem Properties

Per OPC-10000-3 §5.6.2 (AnalogItemType).

| Field | Type | Description |
|-------|------|-------------|
| EURange | Range | Engineering unit range (Low, High doubles). Mandatory for percent deadband. |
| EngineeringUnits | EUInformation | Engineering unit descriptor (namespaceUri, unitId, displayName, description). |
| InstrumentRange | Range | Physical instrument range (Low, High doubles). Optional. |

Relationship: HasProperty from AnalogItemType Variable nodes. Each AnalogItem node carries references to these Properties.

### Event Types

```
BaseEventType (OPC-10000-5 §6.4.2)
├── EventId        : ByteString  (generated)
├── EventType      : NodeId      (identifies event subtype)
├── SourceNode     : NodeId      (node that emitted the event)
├── SourceName     : String      (display name of source node)
├── Time           : UtcTime     (event occurrence time)
├── ReceiveTime    : UtcTime     (server receipt time)
├── LocalTime      : TimeZoneDataType (offset from UTC)
├── Message        : LocalizedText
├── Severity       : UInt16      (1..1000)
│
├── AuditEventType (OPC-10000-5 §6.5.2)
│   ├── ActionTimeStamp    : UtcTime
│   ├── Status             : Boolean  (true = operation succeeded)
│   ├── ServerId           : String
│   ├── ClientAuditEntryId : String   (correlation id from client)
│   └── ClientUserId       : String
│   │
│   ├── AuditChannelEventType (§6.5.3)
│   │   └── SecureChannelId : String
│   │   └─── AuditOpenSecureChannelEventType
│   │
│   ├── AuditSessionEventType (§6.5.4)
│   │   └── SessionId : NodeId
│   │   └─── AuditCreateSessionEventType
│   │   └─── AuditActivateSessionEventType
│   │
│   └── AuditUpdateEventType (§6.5.7)
│       └─── AuditWriteUpdateEventType (§6.5.8)
│           ├── NodeId    : NodeId
│           ├── OldValues : Variant[]
│           └── NewValues : Variant[]
```

Event types map 1:1 to OPC-10000-5 hierarchy. Only the leaf types with `───` prefix are instantiated.

### ComplexType Definitions

#### StructureDefinition (OPC-10000-3 §5.6.4)

```
DataTypeNode
├── DataTypeDefinition : StructureDefinition
│   ├── defaultEncodingId : NodeId
│   ├── baseDataType      : NodeId (always Structure)
│   ├── structureType     : StructureType (Structure_0 = 0, StructureWithOptionalFields_1 = 1)
│   └── Fields[]          : StructureField[]
│       ├── Name          : String
│       ├── Description   : LocalizedText (optional)
│       ├── DataType      : NodeId (built-in type or other Structure)
│       ├── ValueRank     : Int32 (-1=Scalar, 1=OneDimension, etc.)
│       ├── ArrayDimensions: UInt32[] (optional)
│       ├── MaxStringLength: UInt32 (optional, 0=unbounded)
│       └── IsOptional    : Boolean
```

#### EnumDefinition (OPC-10000-3 §5.6.5)

```
DataTypeNode
├── DataTypeDefinition : EnumDefinition
│   └── Fields[]       : EnumField[]
│       ├── Value       : Int64
│       ├── DisplayName : LocalizedText
│       ├── Description : LocalizedText (optional)
│       └── Name        : String
```

### Server Diagnostics (OPC-10000-5 §6.3)

#### ServerDiagnosticsSummaryDataType

| Field | Type | Behavior |
|-------|------|----------|
| ServerViewCount | UInt32 | Number of server views (static, 0) |
| CurrentSessionCount | UInt32 | Active sessions (incremented on create, decremented on close/timeout) |
| CumulatedSessionCount | UInt32 | Total sessions created (monotonic counter) |
| SecurityRejectedSessionCount | UInt32 | Sessions rejected due to security |
| RejectedSessionCount | UInt32 | Sessions rejected for other reasons |
| SessionTimeoutCount | UInt32 | Sessions that timed out |
| SessionAbortCount | UInt32 | Sessions aborted (RST, TCP close) |
| CurrentSubscriptionCount | UInt32 | Active subscriptions |
| CumulatedSubscriptionCount | UInt32 | Total subscriptions created |
| PublishingIntervalCount | UInt32 | Number of publishing intervals elapsed |
| SecurityRejectedRequestsCount | UInt32 | Requests rejected for security |
| RejectedRequestsCount | UInt32 | Requests rejected for other reasons |

#### SessionDiagnosticsDataType

| Field | Type | Behavior |
|-------|------|----------|
| SessionId | NodeId | Session identifier |
| SessionName | String | Session name from CreateSession |
| ClientDescription | ApplicationDescription | Client application info |
| ServerUri | String | Server URI |
| EndpointUrl | String | Endpoint used |
| LocaleIds[] | LocaleId[] | Locale IDs |
| ActualSessionTimeout | Duration | Actual session timeout in ms |
| MaxResponseMessageSize | UInt32 | Max response message size |
| ClientConnectionTime | UtcTime | When session was created |
| ClientLastContactTime | UtcTime | Last service request time |
| CurrentSubscriptionsCount | UInt32 | Subscriptions in this session |
| CurrentMonitoredItemsCount | UInt32 | Monitored items in this session |
| CurrentPublishRequestsInQueue | UInt32 | Queued publish requests |
| TotalRequestCount | UInt32 | Cumulative requests on this session |
| UnauthorizedRequestCount | UInt32 | Unauthorized requests |
| ReadCount, HistoryReadCount, WriteCount, HistoryUpdateCount, CallCount, CreateMonitoredItemsCount, ModifyMonitoredItemsCount, SetMonitoringModeCount, SetTriggeringCount, DeleteMonitoredItemsCount, CreateSubscriptionCount, ModifySubscriptionCount, SetPublishingModeCount, PublishCount, RepublishCount, TransferSubscriptionsCount, DeleteSubscriptionsCount, AddNodesCount, AddReferencesCount, DeleteNodesCount, DeleteReferencesCount, BrowseCount, BrowseNextCount, TranslateBrowsePathsToNodeIdsCount, QueryFirstCount, QueryNextCount, RegisterNodesCount, UnregisterNodesCount | UInt32 | Per-service request counters |

#### Server Object Hierarchy (new nodes)

```
Server (ns=0;i=2253)
├── ServerDiagnostics (ns=0;i=2274) [Object]
│   ├── ServerDiagnosticsSummary (ns=0;i=2275) [Variable]
│   ├── SessionsDiagnosticsSummary (ns=0;i=2276) [Variable]
│   │   └── SessionDiagnosticsArray (ns=0;i=2277) [Variable: SessionDiagnosticsDataType[]]
│   └── EnabledFlag (ns=0;i=2294) [Variable: Boolean]
├── ServerRedundancy (ns=0;i=2296) [Object: ServerRedundancyType]
│   ├── RedundancySupport (ns=0;i=3709) [Variable: RedundancySupport enum]
│   │   Values: None(0), Cold(1), Warm(2), Hot(3), Transparent(4), HotAndMirrored(5)
│   └── ServiceLevel (ns=0;i=11311) [Variable: Byte]
├── Namespaces (ns=0;i=11715) [Object]
│   └── <namespace_index> [Variable: NamespaceMetadataType]
│       ├── NamespaceUri (ns=0;i=11698) [Property: String]
│       ├── NamespaceVersion (ns=0;i=11699) [Property: String]
│       ├── NamespacePublicationDate (ns=0;i=11700) [Property: DateTime]
│       └── IsNamespaceSubset (ns=0;i=11701) [Property: Boolean]
└── ServerCapabilities (existing, ns=0;i=2268)
    └── ServerProfileArray (existing, ns=0;i=2269) [Variable: String[]]
        NOW INCLUDES: "http://opcfoundation.org/UA-Profile/Server/StandardUA2017"
```

### Method Call Registration

**Methods are registered as key-value pairs**: `(MethodId, CallbackFunction)`.

| Field | Type | Description |
|-------|------|-------------|
| method_id | NodeId | Fully qualified Method NodeId |
| callback | `mu_method_callback_t` | `mu_status_t (*)(mu_method_call_t *call)` |
| input_args | mu_variant_t[] | Expected input argument types/schema |
| output_args | mu_variant_t[] | Expected output argument types/schema |

The callback receives input arguments, executes application logic, and populates output arguments.
