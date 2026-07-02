# Traceability: StatusCodes

This document tracks OPC UA StatusCodes returned by the minimal server.

| StatusCode | Value | Origin/Condition | OPC UA Reference |
|------------|-------|------------------|------------------|
| `Bad_ServiceUnsupported` | 0x800B0000 | Unsupported services (Write, Subs, Method, History, etc.) | Part 4, 5.9.3, 5.9.4, 5.9.5, 5.11.3, 5.11.4, 5.12.2, 5.14.2, 5.14.5, 7.38.2 |
| `Bad_DecodingError` | 0x80070000 | Truncated payload, invalid chunk, invalid NodeId | Part 6, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.5, 6.7.3 |
| `Bad_EncodingLimitsExceeded` | 0x80080000 | String or ExtensionObject too large | Part 6, 5.2.2.4, 5.2.2.15 |
| `Bad_SecureChannelIdInvalid` | 0x80220000 | Service before channel | OPC-10000-4 §5.6.2.2, §7.38.2 |
| `Bad_SessionIdInvalid` | 0x80250000 | Service before session or invalid session | Part 4, 5.7.2.1, 5.9.2.2, 5.11.2.2, 7.38.2 |
| `Bad_SecurityPolicyRejected` | 0x80550000 | Unsupported SecurityPolicy | Part 4, 5.6.2.2, 7.38.2; Part 6, 6.7.4 |
| `Bad_IdentityTokenInvalid` | 0x80200000 | Unsupported identity token | Part 4, 5.7.3.2, 7.38.2, 7.40.1, 7.40.3, 7.41 |
| `Bad_TcpMessageTypeInvalid` | 0x807E0000 | Invalid TCP message | Part 6, 7.1.5 |
| `Bad_TcpSecureChannelUnknown` | 0x807F0000 | Unknown channel | Part 6, 7.1.5 |
| `Bad_TcpMessageTooLarge` | 0x80800000 | Message too large | Part 6, 7.1.5 |
| `Bad_TcpNotEnoughResources` | 0x80810000 | No resources for 2nd connection | Part 6, 7.1.5 |
| `Bad_TcpInternalError` | 0x80820000 | Internal error | Part 6, 7.1.5 |
| `Bad_TcpEndpointUrlInvalid` | 0x80830000 | Invalid Endpoint | Part 6, 7.1.5 |
| `Bad_SecurityChecksFailed` | 0x80130000 | Security failure | Part 6, 7.1.5 |
| `Bad_RequestTooLarge` | 0x80B80000 | Request too large | Part 6, 7.1.5 |
| `Bad_ResponseTooLarge` | 0x80B90000 | Response too large | Part 6, 7.1.5 |
| `Bad_NotFound` | 0x803E0000 | Requested item not found | OPC-10000-4 §7.38.2 |
| `Bad_NodeIdExists` | 0x805E0000 | AddNodes requested NodeId already exists (`test_node_management`) | OPC-10000-4 §5.8, §7.38.2 |
| `Bad_NodeClassInvalid` | 0x805F0000 | Invalid NodeClass | OPC-10000-4 §7.38.2 |
| `Bad_NoContinuationPoints` | 0x804B0000 | Continuation point capacity exhausted | OPC-10000-4 §7.38.2 |
| `Bad_MonitoredItemFilterInvalid` | 0x80430000 | MonitoredItem filter parameter invalid | OPC-10000-4 §5.13.2.4; OPC-10000-4 §7.38.2 |
| `Bad_MonitoredItemFilterUnsupported` | 0x80440000 | MonitoredItem filter unsupported | OPC-10000-4 §5.13.2.4; OPC-10000-4 §7.38.2 |
| `Bad_HistoryOperationUnsupported` | 0x80720000 | History operation unsupported | OPC-10000-4 §7.38.2 |
| `Bad_WriteNotSupported` | 0x80730000 | Write combination unsupported | OPC-10000-4 §7.38.2 |
| `Bad_TooManySubscriptions` | 0x80770000 | Subscription capacity exhausted | OPC-10000-4 §5.14.2.4; OPC-10000-4 §7.38.2 |
| `Bad_TooManyPublishRequests` | 0x80780000 | Publish request queue capacity exhausted | OPC-10000-4 §5.14.5.4; OPC-10000-4 §7.38.2 |
| `Bad_SequenceNumberUnknown` | 0x807A0000 | Republish sequence unknown | OPC-10000-4 §5.14.6.4; OPC-10000-4 §7.38.2 |
| `Bad_MessageNotAvailable` | 0x807B0000 | Republish message no longer retained | OPC-10000-4 §5.14.6.4; OPC-10000-4 §7.38.2 |
