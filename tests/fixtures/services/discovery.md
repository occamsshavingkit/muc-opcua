# Discovery Services Fixtures

These fixtures validate OPC UA UASC FindServers and GetEndpoints requests.

## OPC References
- OPC-10000-4 5.5.2.2 (FindServers)
- OPC-10000-4 5.5.4.2 (GetEndpoints)

## Generation Script

The following Python sidecar generates `findservers_req.bin` and `getendpoints_req.bin`:

```python
import struct

# FindServers Request Body
# NodeId FourByte ns=0 id=422
body_findservers = struct.pack("<BBH", 0x01, 0x00, 422)

msg_size_fs = 12 + 4 + 8 + len(body_findservers)
msg_chunk_fs = struct.pack(
    "<3scIIIII",
    b"MSG", b"F",
    msg_size_fs,
    1,  # SecureChannelId
    1,  # TokenId
    1,  # SequenceNumber
    1   # RequestId
) + body_findservers

with open("findservers_req.bin", "wb") as f:
    f.write(msg_chunk_fs)

# GetEndpoints Request Body
# NodeId FourByte ns=0 id=428
body_getendpoints = struct.pack("<BBH", 0x01, 0x00, 428)

msg_size_ge = 12 + 4 + 8 + len(body_getendpoints)
msg_chunk_ge = struct.pack(
    "<3scIIIII",
    b"MSG", b"F",
    msg_size_ge,
    1,  # SecureChannelId
    1,  # TokenId
    2,  # SequenceNumber
    2   # RequestId
) + body_getendpoints

with open("getendpoints_req.bin", "wb") as f:
    f.write(msg_chunk_ge)
```
