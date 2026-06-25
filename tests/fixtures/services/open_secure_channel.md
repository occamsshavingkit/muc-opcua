# OpenSecureChannel Fixtures

These fixtures validate OPC UA UASC OpenSecureChannel requests and responses with SecurityPolicy None.

## OPC References
- OPC-10000-4 5.6.2.2 (OpenSecureChannel)
- OPC-10000-6 6.7.4 (AsymmetricSecurityHeader)
- OPC-10000-6 6.7.7 (SequenceHeader)

## Generation Script

The following Python sidecar generates `opn_req.bin`:

```python
import struct

# OPN F chunk
# MessageHeader (12 bytes)
#   MessageType (3 bytes): b'OPN'
#   ChunkType (1 byte): b'F'
#   MessageSize (4 bytes): length
#   SecureChannelId (4 bytes): 0
# AsymmetricSecurityHeader
#   SecurityPolicyUri: "http://opcfoundation.org/UA/SecurityPolicy#None"
#   SenderCertificate: null
#   ReceiverCertificateThumbprint: null
# SequenceHeader (8 bytes)
#   SequenceNumber (4 bytes): 1
#   RequestId (4 bytes): 1
# MessageBody
#   NodeId: 446 (FourByte, 0x01 0x00 0xBE 0x01)
#   ...

sec_policy = b"http://opcfoundation.org/UA/SecurityPolicy#None"

asym_header = struct.pack(
    "<i{}sii".format(len(sec_policy)),
    len(sec_policy), sec_policy,
    -1, # SenderCertificate (null)
    -1  # ReceiverCertificateThumbprint (null)
)

seq_header = struct.pack("<II", 1, 1)

# Body
body = struct.pack(
    "<BBH", 
    0x01, 0x00, 446 # NodeId FourByte, ns=0, id=446
)

msg_size = 12 + len(asym_header) + len(seq_header) + len(body)
msg_chunk = struct.pack(
    "<3scII",
    b"OPN", b"F",
    msg_size,
    0,  # SecureChannelId
) + asym_header + seq_header + body

with open("opn_req.bin", "wb") as f:
    f.write(msg_chunk)
```
