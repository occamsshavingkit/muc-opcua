<!-- markdownlint-disable MD013 -->

# Conformance Completion

Generated from `profiles/opcua-profile-manifest.yaml`, the OPC transitive
CU closure in `profiles/opcua-profile-normalized-snapshot.json`, and the
full Server catalog in `profiles/opcua-server-conformance.json`.
Do not edit by hand. See spec 073.

## Full Server Surface (all 90 OPC Server profiles/facets)

Denominator is the full OPC Server conformance surface from the REST API
catalog (`profiles/opcua-server-conformance.json`); status is codebase
capability from the build manifest. Coarse feature-level CUs that are not
yet reconciled to individual OPC CU ids count as not-implemented — see the
reconciliation note below.

- Distinct Server CUs: **525** (required 179, optional 346)
- Reconciled as implemented: **required 52/179**, **optional 24/346**

### Per Server profile / facet

| OPC id | Profile / Facet | Required | Optional |
| --- | --- | --- | --- |
| 661 | Client Redundancy Server Facet | 0/1 | 0/0 |
| 744 | Address Space Notifier Server Facet | 0/2 | 0/0 |
| 768 | Documentation Server Facet | 0/2 | 0/4 |
| 887 | A & C AlarmMetrics Server Facet | 0/1 | 0/0 |
| 1026 | Global Service Authorization Request Server Facet | 0/1 | 0/0 |
| 1027 | Global Service KeyCredential Pull Facet | 0/1 | 0/0 |
| 1029 | GDS AliasName Server Facet | 0/5 | 0/5 |
| 1219 | Exposes Type System Server Facet | 1/8 | 1/38 |
| 1322 | Core 2022 Server Facet | 16/17 | 23/27 |
| 1324 | Standard DataChange Subscription 2022 Server Facet | 16/16 | 0/1 |
| 1328 | Auditing 2022 Server Facet | 17/18 | 1/12 |
| 1329 | Node Management 2022 Server Facet | 5/15 | 1/39 |
| 1330 | Nano Embedded Device 2022 Server Profile | 21/22 | 23/29 |
| 1332 | Embedded 2022 UA Server Profile | 41/51 | 23/68 |
| 1333 | Standard 2022 UA Server Profile | 41/55 | 23/68 |
| 1343 | Global Discovery Server 2022 Profile | 37/41 | 23/28 |
| 1344 | Global Discovery and Certificate Mgmt 2022 Server | 45/52 | 24/42 |
| 1346 | A & E Wrapper 2022 Facet | 10/18 | 0/0 |
| 1348 | File Access Server Facet | 0/1 | 0/2 |
| 1351 | User Role Base 2022 Server Facet | 0/2 | 0/1 |
| 1500 | A & C Exclusive Alarming 2022 Server Facet | 15/23 | 0/77 |
| 1501 | A & C Non-Exclusive Alarming 2022 Server Facet | 15/23 | 0/80 |
| 1502 | A & C Alarm 2022 Server Facet | 15/22 | 0/62 |
| 1503 | A & C Alarm Auditing Server Facet | 0/1 | 0/7 |
| 1504 | A & C Dialog 2022 Server Facet | 15/19 | 0/13 |
| 1505 | Data Access Server Facet | 0/1 | 0/21 |
| 1524 | Dictionary Reference Server Facet | 0/1 | 0/2 |
| 1525 | Temporary File Access Server Facet | 0/1 | 0/4 |
| 1551 | A & C Base Condition 2022 Server Facet | 15/18 | 0/8 |
| 1562 | A & C Address Space Instance 2022 Server Facet | 0/1 | 0/0 |
| 1563 | A & C Enable 2022 Server Facet | 15/20 | 0/11 |
| 1564 | A & C Previous Instances 2022 Server Facet | 15/19 | 0/8 |
| 1565 | A & C Acknowledgeable Alarm 2022 Server Facet | 15/19 | 0/15 |
| 1566 | A & C CertificateExpiration 2022 Server Facet | 15/22 | 0/10 |
| 1568 | A & C Refresh2 2022 Server Facet | 15/19 | 0/8 |
| 1571 | Historical Raw Data 2022 Server Facet | 0/4 | 0/1 |
| 1572 | Historical Annotation 2022 Server Facet | 0/6 | 0/0 |
| 1573 | Historical Data Update 2022 Server Facet | 0/3 | 0/1 |
| 1574 | Historical Data Insert 2022 Server Facet | 0/3 | 0/1 |
| 1575 | Historical Data Replace 2022 Server Facet | 0/3 | 0/1 |
| 1576 | Historical Data Delete 2022 Server Facet | 0/3 | 0/0 |
| 1577 | Base Historical Event 2022 Server Facet | 0/3 | 0/0 |
| 1578 | Historical Event Update 2022 Server Facet | 0/3 | 0/0 |
| 1579 | Historical Event Insert 2022 Server Facet | 0/3 | 0/0 |
| 1580 | Historical Event Replace 2022 Server Facet | 0/3 | 0/0 |
| 1581 | Historical Event Delete 2022 Server Facet | 0/3 | 0/0 |
| 1582 | Aggregate Subscription 2022 Server Facet | 16/18 | 0/39 |
| 1627 | Enhanced DataChange Subscription 2017 Server Facet | 16/20 | 0/1 |
| 1629 | Authorization Service Server Facet | 0/1 | 0/0 |
| 1630 | Sessionless Server Facet | 0/2 | 0/0 |
| 1631 | Global Certificate Management Server Facet | 0/1 | 0/0 |
| 1632 | Reverse Connect Server Facet | 0/1 | 0/0 |
| 1633 | Request State Change Server Facet | 0/1 | 0/0 |
| 1636 | AliasName Server Facet | 0/3 | 0/4 |
| 1637 | AliasName Aggregating Server Facet | 0/4 | 0/5 |
| 1638 | State Machine 2022 Server Facet | 15/16 | 0/14 |
| 1639 | Method 2022 Server Facet | 0/4 | 0/2 |
| 1691 | User Token – Anonymous Server Facet | 0/1 | 0/0 |
| 1695 | User Token – User Name Password Server Facet | 1/2 | 0/1 |
| 1696 | User Token – X509 Certificate Server Facet | 0/2 | 0/0 |
| 1697 | User Token – JWT Server Facet | 0/3 | 0/4 |
| 1698 | User Token – Issued Token Server Facet | 0/2 | 0/0 |
| 1699 | User Token – Issued Token Windows Server Facet | 0/3 | 0/0 |
| 1707 | Historical Data AtTime 2022 Server Facet | 0/4 | 0/0 |
| 1708 | Historical Aggregate 2022 Server Facet | 0/5 | 0/39 |
| 1709 | Historical Access Modified Data 2022 Server Facet | 0/4 | 0/0 |
| 1710 | Historical Access Structured Data 2022 Server Facet | 0/4 | 0/6 |
| 1715 | Base Server Behaviour Facet | 0/4 | 0/0 |
| 1725 | ComplexType 2017 Server Facet | 0/3 | 0/3 |
| 1733 | Model Change Event Server Facet | 0/2 | 0/1 |
| 1875 | Scheduler Base Server Facet | 0/5 | 0/3 |
| 1876 | Scheduler Configuration Server Facet | 0/6 | 0/4 |
| 1996 | Attribute WriteMask Server 2023 Facet | 1/7 | 0/1 |
| 1997 | Attribute WriteMask Server Facet | 1/6 | 0/1 |
| 2069 | Subnet Discovery Server Facet | 0/1 | 0/0 |
| 2080 | User Role Management 2022 Server Facet | 0/6 | 0/8 |
| 2085 | Standard Event Subscription 2022 Server Facet | 15/15 | 0/7 |
| 2098 | Durable Subscription 2022 Server Facet | 0/3 | 0/0 |
| 2113 | KeyCredential Service Server Facet | 0/2 | 0/3 |
| 2242 | LogObject Facet | 0/1 | 0/13 |
| 2249 | Redundancy Transparent Server Facet | 0/1 | 0/0 |
| 2250 | Embedded DataChange Subscription 2022 Server Facet | 9/9 | 0/1 |
| 2252 | Redundancy Visible Server Facet | 0/1 | 0/0 |
| 2255 | Micro Embedded Device 2022 Server Profile | 31/32 | 23/30 |
| 2266 | Nano Embedded Device 2025 Server Profile | 21/22 | 23/29 |
| 2267 | Micro Embedded Device 2025 Server Profile | 31/32 | 23/30 |
| 2268 | Embedded 2025 UA Server Profile | 41/51 | 23/68 |
| 2269 | Standard 2025 UA Server Profile | 41/55 | 23/68 |
| 2322 | AliasName Configuration Facet | 0/4 | 0/5 |
| 2323 | AliasName Server PubSub Publisher Facet | 0/4 | 0/4 |

> Reconciliation status: of the full Server surface, only CUs linked to a
> build-manifest entry (directly or via `satisfied_by`) count as implemented.
> Our feature-level implementations (PubSub, Alarms, History, Methods,
> Aggregates, Redundancy, …) are tracked as coarse CUs that do not yet map
> 1:1 to the granular OPC CU ids; reconciling them is tracked, ongoing work.

## Base Server Profiles (detailed)

## Profile Nano Embedded Device 2025 Server Profile

- Required CUs implemented: **18/18**
- Optional CUs implemented: **24/31**
- Not applicable (grounded): 2

Required CUs:

- [x] `opc_cu_2317` View TranslateBrowsePath
- [x] `opc_cu_2328` Discovery Get Endpoints
- [x] `opc_cu_2352` Discovery Find Servers Self
- [x] `opc_cu_2600` SecurityPolicy Support
- [x] `opc_cu_2809` Address Space Atomicity
- [x] `opc_cu_2820` Address Space Full Array Only
- [x] `opc_cu_3072` Attribute Read
- [x] `opc_cu_3073` View RegisterNodes
- [x] `opc_cu_3175` Session Base
- [x] `opc_cu_3184` Base Info Core Structure 2
- [x] `opc_cu_3530` View Basic 2
- [x] `opc_cu_3554` Address Space Base
- [x] `opc_cu_3912` Base Info Server Capabilities 2
- [x] `opc_cu_3985` Session General Service Behaviour
- [x] `opc_cu_5793` Time Sync - Support
- [x] `opc_cu_protocol_ua_tcp` Protocol UA TCP
- [x] `opc_cu_ua_binary_encoding` UA Binary Encoding
- [x] `opc_cu_ua_secure_conversation` UA Secure Conversation

Optional CUs:

- [x] `opc_cu_2389` Attribute Write Values
- [x] `opc_cu_2400` Session Change User
- [x] `opc_cu_2446` Address Space AddIn Reference
- [x] `opc_cu_2447` Address Space AddIn DefaultInstanceBrowsename
- [x] `opc_cu_2476` Base Info LocalTime
- [x] `opc_cu_2478` Time Sync – OS based support
- [x] `opc_cu_2711` Base Info Selection List
- [x] `opc_cu_2786` Time Sync – NTP
- [x] `opc_cu_2936` Attribute Write StatusCode & Timestamp
- [x] `opc_cu_2969` Base Info ValueAsText
- [x] `opc_cu_3127` Base Info OptionSet
- [x] `opc_cu_3147` Attribute Write Index
- [x] `opc_cu_3186` Base Info Core Views Folder
- [x] `opc_cu_3192` Base Info Diagnostics
- [x] `opc_cu_3198` Base Info Estimated Return Time
- [x] `opc_cu_3545` Base Info Namespace Metadata
- [x] `opc_cu_3560` Address Space Interfaces
- [x] `opc_cu_3721` Security ECC Policy
- [x] `opc_cu_3802` Time Sync - Configure Clock Skew
- [x] `opc_cu_3983` Base Services Diagnostics
- [x] `opc_cu_4053` Base Info Locations Object
- [x] `opc_cu_4237` Address Space NonVolatile and Constant
- [x] `opc_cu_5240` Base Info Currency
- [x] `opc_cu_5592` Base Info Engineering Units
- [ ] `opc_cu_2407` Security Administration
- [ ] `opc_cu_2479` Time Sync – IEEE 1588 (PTP)
- [ ] `opc_cu_2480` Time Sync – IEEE 802.1AS
- [ ] `opc_cu_2808` Security Role Server Authorization
- [ ] `opc_cu_3201` Base Info Custom Type System
- [ ] `opc_cu_5505` Time Sync – UA based support
- [ ] `opc_cu_5814` Security – No Application Authentication

Not applicable (grounded):

- `opc_cu_3080` Security Default ApplicationInstance Certificate — Nano is SecurityPolicy None + Anonymous (spec 072); no ApplicationInstance certificate is required.
- `opc_cu_3808` Documentation - Core Capacities — Documentation CU; satisfied by documentation presence, not code.

## Profile Micro Embedded Device 2025 Server Profile

- Required CUs implemented: **30/30**
- Optional CUs implemented: **24/32**

Required CUs:

- [x] `opc_cu_2317` View TranslateBrowsePath
- [x] `opc_cu_2328` Discovery Get Endpoints
- [x] `opc_cu_2352` Discovery Find Servers Self
- [x] `opc_cu_2600` SecurityPolicy Support
- [x] `opc_cu_2809` Address Space Atomicity
- [x] `opc_cu_2820` Address Space Full Array Only
- [x] `opc_cu_2963` Monitor Basic
- [x] `opc_cu_3072` Attribute Read
- [x] `opc_cu_3073` View RegisterNodes
- [x] `opc_cu_3080` Security Default ApplicationInstance Certificate
- [x] `opc_cu_3143` Subscription PublishRequest Queue Overflow
- [x] `opc_cu_3175` Session Base
- [x] `opc_cu_3184` Base Info Core Structure 2
- [x] `opc_cu_3530` View Basic 2
- [x] `opc_cu_3554` Address Space Base
- [x] `opc_cu_3727` Subscription Basic
- [x] `opc_cu_3808` Documentation - Core Capacities
- [x] `opc_cu_3911` Base Info Server Capabilities Subscriptions
- [x] `opc_cu_3912` Base Info Server Capabilities 2
- [x] `opc_cu_3913` Subscription Publish Basic
- [x] `opc_cu_3922` Base Info SemanticChange Bit
- [x] `opc_cu_3923` Session Multiple
- [x] `opc_cu_3985` Session General Service Behaviour
- [x] `opc_cu_4055` Base Info Server Capabilities MaxMonitoredItemsQueueSize
- [x] `opc_cu_5207` Monitor Items 2
- [x] `opc_cu_5208` Monitor Value Change V2
- [x] `opc_cu_5793` Time Sync - Support
- [x] `opc_cu_protocol_ua_tcp` Protocol UA TCP
- [x] `opc_cu_ua_binary_encoding` UA Binary Encoding
- [x] `opc_cu_ua_secure_conversation` UA Secure Conversation

Optional CUs:

- [x] `opc_cu_2389` Attribute Write Values
- [x] `opc_cu_2400` Session Change User
- [x] `opc_cu_2446` Address Space AddIn Reference
- [x] `opc_cu_2447` Address Space AddIn DefaultInstanceBrowsename
- [x] `opc_cu_2476` Base Info LocalTime
- [x] `opc_cu_2478` Time Sync – OS based support
- [x] `opc_cu_2711` Base Info Selection List
- [x] `opc_cu_2786` Time Sync – NTP
- [x] `opc_cu_2936` Attribute Write StatusCode & Timestamp
- [x] `opc_cu_2969` Base Info ValueAsText
- [x] `opc_cu_3127` Base Info OptionSet
- [x] `opc_cu_3147` Attribute Write Index
- [x] `opc_cu_3186` Base Info Core Views Folder
- [x] `opc_cu_3192` Base Info Diagnostics
- [x] `opc_cu_3198` Base Info Estimated Return Time
- [x] `opc_cu_3545` Base Info Namespace Metadata
- [x] `opc_cu_3560` Address Space Interfaces
- [x] `opc_cu_3721` Security ECC Policy
- [x] `opc_cu_3802` Time Sync - Configure Clock Skew
- [x] `opc_cu_3983` Base Services Diagnostics
- [x] `opc_cu_4053` Base Info Locations Object
- [x] `opc_cu_4237` Address Space NonVolatile and Constant
- [x] `opc_cu_5240` Base Info Currency
- [x] `opc_cu_5592` Base Info Engineering Units
- [ ] `opc_cu_2407` Security Administration
- [ ] `opc_cu_2479` Time Sync – IEEE 1588 (PTP)
- [ ] `opc_cu_2480` Time Sync – IEEE 802.1AS
- [ ] `opc_cu_2808` Security Role Server Authorization
- [ ] `opc_cu_3196` Base Info Fixed SamplingInterval
- [ ] `opc_cu_3201` Base Info Custom Type System
- [ ] `opc_cu_5505` Time Sync – UA based support
- [ ] `opc_cu_5814` Security – No Application Authentication

## Profile Embedded 2025 UA Server Profile

- Required CUs implemented: **39/48**
- Optional CUs implemented: **25/71**

Required CUs:

- [x] `opc_cu_2317` View TranslateBrowsePath
- [x] `opc_cu_2328` Discovery Get Endpoints
- [x] `opc_cu_2352` Discovery Find Servers Self
- [x] `opc_cu_2600` SecurityPolicy Support
- [x] `opc_cu_2809` Address Space Atomicity
- [x] `opc_cu_2820` Address Space Full Array Only
- [x] `opc_cu_2863` Security Policy Required
- [x] `opc_cu_2928` Monitored Items Deadband Filter
- [x] `opc_cu_2940` Base Info GetMonitoredItems Method
- [x] `opc_cu_2963` Monitor Basic
- [x] `opc_cu_3072` Attribute Read
- [x] `opc_cu_3073` View RegisterNodes
- [x] `opc_cu_3080` Security Default ApplicationInstance Certificate
- [x] `opc_cu_3143` Subscription PublishRequest Queue Overflow
- [x] `opc_cu_3146` Monitor Triggering
- [x] `opc_cu_3175` Session Base
- [x] `opc_cu_3184` Base Info Core Structure 2
- [x] `opc_cu_3530` View Basic 2
- [x] `opc_cu_3532` Monitor Queueing
- [x] `opc_cu_3534` Subscription Multiple
- [x] `opc_cu_3535` Subscription Retransmission Queue
- [x] `opc_cu_3536` Security User Name Password 2
- [x] `opc_cu_3544` Base Info ResendData Method
- [x] `opc_cu_3554` Address Space Base
- [x] `opc_cu_3727` Subscription Basic
- [x] `opc_cu_3808` Documentation - Core Capacities
- [x] `opc_cu_3911` Base Info Server Capabilities Subscriptions
- [x] `opc_cu_3912` Base Info Server Capabilities 2
- [x] `opc_cu_3913` Subscription Publish Basic
- [x] `opc_cu_3922` Base Info SemanticChange Bit
- [x] `opc_cu_3923` Session Multiple
- [x] `opc_cu_3985` Session General Service Behaviour
- [x] `opc_cu_4055` Base Info Server Capabilities MaxMonitoredItemsQueueSize
- [x] `opc_cu_5207` Monitor Items 2
- [x] `opc_cu_5208` Monitor Value Change V2
- [x] `opc_cu_5793` Time Sync - Support
- [x] `opc_cu_protocol_ua_tcp` Protocol UA TCP
- [x] `opc_cu_ua_binary_encoding` UA Binary Encoding
- [x] `opc_cu_ua_secure_conversation` UA Secure Conversation
- [ ] `opc_cu_2231` Push Model for Global Certificate and TrustList Management
- [ ] `opc_cu_2483` Base Info Date DataTypes
- [ ] `opc_cu_2823` Security Invalid user token
- [ ] `opc_cu_3185` Base Info Core Types Folders
- [ ] `opc_cu_3188` Base Info Base Types
- [ ] `opc_cu_3189` Base Info ServerType
- [ ] `opc_cu_3641` Base Info Method Argument DataType
- [ ] `opc_cu_4426` Base Info Decimal DataType
- [ ] `opc_cu_5801` Base Info Type Information

Optional CUs:

- [x] `opc_cu_2389` Attribute Write Values
- [x] `opc_cu_2400` Session Change User
- [x] `opc_cu_2446` Address Space AddIn Reference
- [x] `opc_cu_2447` Address Space AddIn DefaultInstanceBrowsename
- [x] `opc_cu_2476` Base Info LocalTime
- [x] `opc_cu_2478` Time Sync – OS based support
- [x] `opc_cu_2536` Base Info ContentFilter
- [x] `opc_cu_2711` Base Info Selection List
- [x] `opc_cu_2786` Time Sync – NTP
- [x] `opc_cu_2936` Attribute Write StatusCode & Timestamp
- [x] `opc_cu_2969` Base Info ValueAsText
- [x] `opc_cu_3127` Base Info OptionSet
- [x] `opc_cu_3147` Attribute Write Index
- [x] `opc_cu_3186` Base Info Core Views Folder
- [x] `opc_cu_3192` Base Info Diagnostics
- [x] `opc_cu_3198` Base Info Estimated Return Time
- [x] `opc_cu_3545` Base Info Namespace Metadata
- [x] `opc_cu_3560` Address Space Interfaces
- [x] `opc_cu_3721` Security ECC Policy
- [x] `opc_cu_3802` Time Sync - Configure Clock Skew
- [x] `opc_cu_3983` Base Services Diagnostics
- [x] `opc_cu_4053` Base Info Locations Object
- [x] `opc_cu_4237` Address Space NonVolatile and Constant
- [x] `opc_cu_5240` Base Info Currency
- [x] `opc_cu_5592` Base Info Engineering Units
- [ ] `opc_cu_2407` Security Administration
- [ ] `opc_cu_2423` Base Info Rational Number
- [ ] `opc_cu_2479` Time Sync – IEEE 1588 (PTP)
- [ ] `opc_cu_2480` Time Sync – IEEE 802.1AS
- [ ] `opc_cu_2481` Base Info NormalizedString DataType
- [ ] `opc_cu_2482` Base Info DecimalString DataType
- [ ] `opc_cu_2484` Base Info BitFieldMaskDataType
- [ ] `opc_cu_2485` Base Info KeyValuePair
- [ ] `opc_cu_2490` Base Info Subvariables of Structures
- [ ] `opc_cu_2491` Base Info AssociatedWith
- [ ] `opc_cu_2500` Base Info EUInformation
- [ ] `opc_cu_2512` Base Info OrderedList
- [ ] `opc_cu_2513` Base Info Audio Type
- [ ] `opc_cu_2514` Base Info Spatial Data
- [ ] `opc_cu_2516` Base Info HasOrderedComponent
- [ ] `opc_cu_2517` Base Info Deprecated Information
- [ ] `opc_cu_2518` Base Info Image DataTypes
- [ ] `opc_cu_2808` Security Role Server Authorization
- [ ] `opc_cu_3196` Base Info Fixed SamplingInterval
- [ ] `opc_cu_3201` Base Info Custom Type System
- [ ] `opc_cu_3207` Base Info OptionSet DataType
- [ ] `opc_cu_3214` Base Info Range DataType
- [ ] `opc_cu_3547` Base Info UaBinary File
- [ ] `opc_cu_3550` Base Info StatusResult DataType
- [ ] `opc_cu_3551` Base Info UriString
- [ ] `opc_cu_3644` Base Info SemanticVersionString
- [ ] `opc_cu_3645` Security User Token Unencrypted
- [ ] `opc_cu_3747` Base Info IsExecutableOn
- [ ] `opc_cu_3748` Base Info IsExecutingOn
- [ ] `opc_cu_3749` Base Info Controls
- [ ] `opc_cu_3750` Base Info Utilizes
- [ ] `opc_cu_3751` Base Info Requires
- [ ] `opc_cu_3752` Base Info IsPhysicallyConnectedTo
- [ ] `opc_cu_3753` Base Info RepresentsSameEntityAs
- [ ] `opc_cu_3754` Base Info RepresentsSameHardwareAs
- [ ] `opc_cu_3755` Base Info RepresentsSameFunctionalityAs
- [ ] `opc_cu_3756` Base Info IsHostedBy
- [ ] `opc_cu_3757` Base Info HasPhysicalComponent
- [ ] `opc_cu_3758` Base Info HasContainedComponent
- [ ] `opc_cu_3759` Base Info HasAttachedComponent
- [ ] `opc_cu_3996` Base Info ReferenceDescription
- [ ] `opc_cu_4052` Base Info TrimmedString
- [ ] `opc_cu_4054` Base Info Handle DataType
- [ ] `opc_cu_5505` Time Sync – UA based support
- [ ] `opc_cu_5814` Security – No Application Authentication
- [ ] `opc_cu_5868` Base Info Portable IDs

## Profile Standard 2025 UA Server Profile

- Required CUs implemented: **39/52**
- Optional CUs implemented: **25/71**

Required CUs:

- [x] `opc_cu_2317` View TranslateBrowsePath
- [x] `opc_cu_2328` Discovery Get Endpoints
- [x] `opc_cu_2352` Discovery Find Servers Self
- [x] `opc_cu_2600` SecurityPolicy Support
- [x] `opc_cu_2809` Address Space Atomicity
- [x] `opc_cu_2820` Address Space Full Array Only
- [x] `opc_cu_2863` Security Policy Required
- [x] `opc_cu_2928` Monitored Items Deadband Filter
- [x] `opc_cu_2940` Base Info GetMonitoredItems Method
- [x] `opc_cu_2963` Monitor Basic
- [x] `opc_cu_3072` Attribute Read
- [x] `opc_cu_3073` View RegisterNodes
- [x] `opc_cu_3080` Security Default ApplicationInstance Certificate
- [x] `opc_cu_3143` Subscription PublishRequest Queue Overflow
- [x] `opc_cu_3146` Monitor Triggering
- [x] `opc_cu_3175` Session Base
- [x] `opc_cu_3184` Base Info Core Structure 2
- [x] `opc_cu_3530` View Basic 2
- [x] `opc_cu_3532` Monitor Queueing
- [x] `opc_cu_3534` Subscription Multiple
- [x] `opc_cu_3535` Subscription Retransmission Queue
- [x] `opc_cu_3536` Security User Name Password 2
- [x] `opc_cu_3544` Base Info ResendData Method
- [x] `opc_cu_3554` Address Space Base
- [x] `opc_cu_3727` Subscription Basic
- [x] `opc_cu_3808` Documentation - Core Capacities
- [x] `opc_cu_3911` Base Info Server Capabilities Subscriptions
- [x] `opc_cu_3912` Base Info Server Capabilities 2
- [x] `opc_cu_3913` Subscription Publish Basic
- [x] `opc_cu_3922` Base Info SemanticChange Bit
- [x] `opc_cu_3923` Session Multiple
- [x] `opc_cu_3985` Session General Service Behaviour
- [x] `opc_cu_4055` Base Info Server Capabilities MaxMonitoredItemsQueueSize
- [x] `opc_cu_5207` Monitor Items 2
- [x] `opc_cu_5208` Monitor Value Change V2
- [x] `opc_cu_5793` Time Sync - Support
- [x] `opc_cu_protocol_ua_tcp` Protocol UA TCP
- [x] `opc_cu_ua_binary_encoding` UA Binary Encoding
- [x] `opc_cu_ua_secure_conversation` UA Secure Conversation
- [ ] `opc_cu_2190` Session Cancel
- [ ] `opc_cu_2231` Push Model for Global Certificate and TrustList Management
- [ ] `opc_cu_2271` Discovery Register
- [ ] `opc_cu_2483` Base Info Date DataTypes
- [ ] `opc_cu_2823` Security Invalid user token
- [ ] `opc_cu_3125` Security User X509
- [ ] `opc_cu_3170` Discovery Register2
- [ ] `opc_cu_3185` Base Info Core Types Folders
- [ ] `opc_cu_3188` Base Info Base Types
- [ ] `opc_cu_3189` Base Info ServerType
- [ ] `opc_cu_3641` Base Info Method Argument DataType
- [ ] `opc_cu_4426` Base Info Decimal DataType
- [ ] `opc_cu_5801` Base Info Type Information

Optional CUs:

- [x] `opc_cu_2389` Attribute Write Values
- [x] `opc_cu_2400` Session Change User
- [x] `opc_cu_2446` Address Space AddIn Reference
- [x] `opc_cu_2447` Address Space AddIn DefaultInstanceBrowsename
- [x] `opc_cu_2476` Base Info LocalTime
- [x] `opc_cu_2478` Time Sync – OS based support
- [x] `opc_cu_2536` Base Info ContentFilter
- [x] `opc_cu_2711` Base Info Selection List
- [x] `opc_cu_2786` Time Sync – NTP
- [x] `opc_cu_2936` Attribute Write StatusCode & Timestamp
- [x] `opc_cu_2969` Base Info ValueAsText
- [x] `opc_cu_3127` Base Info OptionSet
- [x] `opc_cu_3147` Attribute Write Index
- [x] `opc_cu_3186` Base Info Core Views Folder
- [x] `opc_cu_3192` Base Info Diagnostics
- [x] `opc_cu_3198` Base Info Estimated Return Time
- [x] `opc_cu_3545` Base Info Namespace Metadata
- [x] `opc_cu_3560` Address Space Interfaces
- [x] `opc_cu_3721` Security ECC Policy
- [x] `opc_cu_3802` Time Sync - Configure Clock Skew
- [x] `opc_cu_3983` Base Services Diagnostics
- [x] `opc_cu_4053` Base Info Locations Object
- [x] `opc_cu_4237` Address Space NonVolatile and Constant
- [x] `opc_cu_5240` Base Info Currency
- [x] `opc_cu_5592` Base Info Engineering Units
- [ ] `opc_cu_2407` Security Administration
- [ ] `opc_cu_2423` Base Info Rational Number
- [ ] `opc_cu_2479` Time Sync – IEEE 1588 (PTP)
- [ ] `opc_cu_2480` Time Sync – IEEE 802.1AS
- [ ] `opc_cu_2481` Base Info NormalizedString DataType
- [ ] `opc_cu_2482` Base Info DecimalString DataType
- [ ] `opc_cu_2484` Base Info BitFieldMaskDataType
- [ ] `opc_cu_2485` Base Info KeyValuePair
- [ ] `opc_cu_2490` Base Info Subvariables of Structures
- [ ] `opc_cu_2491` Base Info AssociatedWith
- [ ] `opc_cu_2500` Base Info EUInformation
- [ ] `opc_cu_2512` Base Info OrderedList
- [ ] `opc_cu_2513` Base Info Audio Type
- [ ] `opc_cu_2514` Base Info Spatial Data
- [ ] `opc_cu_2516` Base Info HasOrderedComponent
- [ ] `opc_cu_2517` Base Info Deprecated Information
- [ ] `opc_cu_2518` Base Info Image DataTypes
- [ ] `opc_cu_2808` Security Role Server Authorization
- [ ] `opc_cu_3196` Base Info Fixed SamplingInterval
- [ ] `opc_cu_3201` Base Info Custom Type System
- [ ] `opc_cu_3207` Base Info OptionSet DataType
- [ ] `opc_cu_3214` Base Info Range DataType
- [ ] `opc_cu_3547` Base Info UaBinary File
- [ ] `opc_cu_3550` Base Info StatusResult DataType
- [ ] `opc_cu_3551` Base Info UriString
- [ ] `opc_cu_3644` Base Info SemanticVersionString
- [ ] `opc_cu_3645` Security User Token Unencrypted
- [ ] `opc_cu_3747` Base Info IsExecutableOn
- [ ] `opc_cu_3748` Base Info IsExecutingOn
- [ ] `opc_cu_3749` Base Info Controls
- [ ] `opc_cu_3750` Base Info Utilizes
- [ ] `opc_cu_3751` Base Info Requires
- [ ] `opc_cu_3752` Base Info IsPhysicallyConnectedTo
- [ ] `opc_cu_3753` Base Info RepresentsSameEntityAs
- [ ] `opc_cu_3754` Base Info RepresentsSameHardwareAs
- [ ] `opc_cu_3755` Base Info RepresentsSameFunctionalityAs
- [ ] `opc_cu_3756` Base Info IsHostedBy
- [ ] `opc_cu_3757` Base Info HasPhysicalComponent
- [ ] `opc_cu_3758` Base Info HasContainedComponent
- [ ] `opc_cu_3759` Base Info HasAttachedComponent
- [ ] `opc_cu_3996` Base Info ReferenceDescription
- [ ] `opc_cu_4052` Base Info TrimmedString
- [ ] `opc_cu_4054` Base Info Handle DataType
- [ ] `opc_cu_5505` Time Sync – UA based support
- [ ] `opc_cu_5814` Security – No Application Authentication
- [ ] `opc_cu_5868` Base Info Portable IDs

## Facets

Facet CU membership uses direct `included_conformance_units`
(sub-facet CUs are counted where the snapshot expands them).

- **GDS AliasName Server Facet** (`opc_facet_1029`): required 0/0, optional 0/0 — 10 closure CU(s) not in manifest
- **Exposes Type System Server Facet** (`opc_facet_1219`): required 0/7, optional 2/39
- **Core 2022 Server Facet** (`opc_facet_1322`): required 16/16, optional 23/28
- **Standard DataChange Subscription 2022 Server Facet** (`opc_facet_1324`): required 16/16, optional 0/1
- **Global Certificate Management Server Facet** (`opc_facet_1631`): required 0/1, optional 0/0
- **AliasName Server Facet** (`opc_facet_1636`): required 0/0, optional 0/0 — 7 closure CU(s) not in manifest
- **AliasName Aggregating Server Facet** (`opc_facet_1637`): required 0/0, optional 0/0 — 9 closure CU(s) not in manifest
- **User Token – User Name Password Server Facet** (`opc_facet_1695`): required 1/2, optional 0/1
- **User Token – X509 Certificate Server Facet** (`opc_facet_1696`): required 0/2, optional 0/0
- **LogObject Facet** (`opc_facet_2242`): required 0/0, optional 0/0 — 14 closure CU(s) not in manifest
- **Embedded DataChange Subscription 2022 Server Facet** (`opc_facet_2250`): required 9/9, optional 0/1
- **AliasName Configuration Facet** (`opc_facet_2322`): required 0/0, optional 0/0 — 9 closure CU(s) not in manifest
- **AliasName Server PubSub Publisher Facet** (`opc_facet_2323`): required 0/0, optional 0/0 — 8 closure CU(s) not in manifest
