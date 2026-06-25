# Traceability: StatusCodes

This document tracks OPC UA StatusCodes returned by the minimal server.

| StatusCode | Value | Origin/Condition | OPC UA Reference |
|------------|-------|------------------|------------------|
| `Bad_ServiceUnsupported` | 0x800B0000 | Unsupported services (Write, Subs, Method, History, etc.) | Part 4, 5.9.3, 5.9.4, 5.9.5, 5.11.3, 5.11.4, 5.12.2, 5.14.2, 5.14.5, 7.38.2 |
| `Bad_DecodingError` | 0x80070000 | Truncated payload, invalid chunk, invalid NodeId | Part 6, 5.2.2.4, 5.2.2.9, 5.2.2.15, 5.2.5, 6.7.3 |
| `Bad_EncodingLimitsExceeded` | 0x80080000 | String or ExtensionObject too large | Part 6, 5.2.2.4, 5.2.2.15 |
| `Bad_SecureChannelIdInvalid` | 0x80210000 | Service before channel | Part 4, 5.6.2.2, 7.38.2 |
| `Bad_SessionIdInvalid` | 0x80250000 | Service before session or invalid session | Part 4, 5.7.2.1, 5.9.2.2, 5.11.2.2, 7.38.2 |
| `Bad_SessionClosed` | 0x80260000 | Session closed | Part 4, 5.7.4.2 |
| `Bad_SecurityPolicyRejected` | 0x80550000 | Unsupported SecurityPolicy | Part 4, 5.6.2.2, 7.38.2; Part 6, 6.7.4 |
| `Bad_IdentityTokenInvalid` | 0x80200000 | Unsupported identity token | Part 4, 5.7.3.2, 7.38.2, 7.40.1, 7.40.3, 7.41 |
| `Bad_TcpServerTooBusy` | 0x807D0000 | TCP Server busy | Part 6, 7.1.5 |
| `Bad_TcpMessageTypeInvalid` | 0x807E0000 | Invalid TCP message | Part 6, 7.1.5 |
| `Bad_TcpSecureChannelUnknown` | 0x807F0000 | Unknown channel | Part 6, 7.1.5 |
| `Bad_TcpMessageTooLarge` | 0x80800000 | Message too large | Part 6, 7.1.5 |
| `Bad_TcpNotEnoughResources` | 0x80810000 | No resources for 2nd connection | Part 6, 7.1.5 |
| `Bad_TcpInternalError` | 0x80820000 | Internal error | Part 6, 7.1.5 |
| `Bad_TcpEndpointUrlInvalid` | 0x80830000 | Invalid Endpoint | Part 6, 7.1.5 |
| `Bad_SecurityChecksFailed` | 0x80130000 | Security failure | Part 6, 7.1.5 |
| `Bad_RequestTooLarge` | 0x80B80000 | Request too large | Part 6, 7.1.5 |
| `Bad_ResponseTooLarge` | 0x80B90000 | Response too large | Part 6, 7.1.5 |
| `Bad_Timeout` | 0x800A0000 | Timeout | Part 6, 7.1.5 |
