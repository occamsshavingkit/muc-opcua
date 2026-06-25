# Traceability: StatusCodes

This document tracks OPC UA StatusCodes returned by the minimal server.

| StatusCode | Value | Origin/Condition | OPC UA Reference |
|------------|-------|------------------|------------------|
| `Bad_ServiceUnsupported` | 0x800B0000 | Unsupported services | Part 4, 7.38.2 |
| `Bad_TcpServerTooBusy` | 0x807D0000 | TCP Server busy | Part 6, 7.1.5 |
| `Bad_TcpMessageTypeInvalid` | 0x807E0000 | Invalid TCP message | Part 6, 7.1.5 |
| `Bad_TcpSecureChannelUnknown` | 0x807F0000 | Unknown channel | Part 6, 7.1.5 |
| `Bad_TcpMessageTooLarge` | 0x80800000 | Message too large | Part 6, 7.1.5 |
| `Bad_TcpNotEnoughResources` | 0x80810000 | No resources | Part 6, 7.1.5 |
| `Bad_TcpInternalError` | 0x80820000 | Internal error | Part 6, 7.1.5 |
| `Bad_TcpEndpointUrlInvalid` | 0x80830000 | Invalid Endpoint | Part 6, 7.1.5 |
| `Bad_SecurityChecksFailed` | 0x80130000 | Security failure | Part 6, 7.1.5 |
| `Bad_RequestTooLarge` | 0x80B80000 | Request too large | Part 6, 7.1.5 |
| `Bad_ResponseTooLarge` | 0x80B90000 | Response too large | Part 6, 7.1.5 |
| `Bad_Timeout` | 0x800A0000 | Timeout | Part 6, 7.1.5 |
