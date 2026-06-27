# ActivateSession Non-Empty Certificates Fixture

Fixture: `nonempty_certs.bin`

This is the service request body passed to `mu_service_dispatch` for
`MU_ID_ACTIVATESESSIONREQUEST`. It starts at `RequestHeader` and does not include
the request-type NodeId or UASC message framing.

OPC UA references:

- OPC-10000-4 5.7.3.2, ActivateSession request parameters
- OPC-10000-4 7.37, SignedSoftwareCertificate
- OPC-10000-4 7.40.3, AnonymousIdentityToken
- OPC-10000-6 5.2.2.15, ExtensionObject binary encoding
- OPC-10000-6 5.2.5, array length encoding

All integer fields are little-endian.

| Offset | Bytes | Field |
| ------ | ----- | ----- |
| 0x00 | `01 00 39 30` | RequestHeader.authenticationToken NodeId, FourByte, ns=0, i=12345 |
| 0x04 | `00 00 00 00 00 00 00 00` | RequestHeader.timestamp = 0 |
| 0x0c | `00 00 00 00` | RequestHeader.requestHandle = 0 |
| 0x10 | `00 00 00 00` | RequestHeader.returnDiagnostics = 0 |
| 0x14 | `ff ff ff ff` | RequestHeader.auditEntryId = null String |
| 0x18 | `00 00 00 00` | RequestHeader.timeoutHint = 0 |
| 0x1c | `00 00 00` | RequestHeader.additionalHeader = null ExtensionObject, typeId i=0, no body |
| 0x1f | `ff ff ff ff` | clientSignature.algorithm = null String |
| 0x23 | `ff ff ff ff` | clientSignature.signature = null ByteString |
| 0x27 | `01 00 00 00` | clientSoftwareCertificates array length = 1 |
| 0x2b | `03 00 00 00 41 42 43` | clientSoftwareCertificates[0].certificateData = ByteString `ABC` |
| 0x32 | `02 00 00 00 58 59` | clientSoftwareCertificates[0].signature = ByteString `XY` |
| 0x38 | `01 00 00 00` | localeIds array length = 1 |
| 0x3c | `05 00 00 00 65 6e 2d 55 53` | localeIds[0] = String `en-US` |
| 0x45 | `01 00 41 01` | userIdentityToken.typeId = NodeId FourByte, ns=0, i=321 |
| 0x49 | `01` | userIdentityToken encoding = ByteString body |
| 0x4a | `0d 00 00 00` | userIdentityToken body length = 13 bytes |
| 0x4e | `09 00 00 00 61 6e 6f 6e 79 6d 6f 75 73` | AnonymousIdentityToken.policyId = String `anonymous` |
| 0x5b | `06 00 00 00 73 69 67 61 6c 67` | userTokenSignature.algorithm = String `sigalg` |
| 0x65 | `01 00 00 00 ab` | userTokenSignature.signature = ByteString `ab` |

Total length: 106 bytes.
