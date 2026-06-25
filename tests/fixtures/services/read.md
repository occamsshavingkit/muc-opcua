# Read Services Fixtures

These fixtures validate OPC UA UASC Read requests for reading Boolean, Int32, UInt32, Float, and bounded String values.

## OPC References
- OPC-10000-4 5.11.2.2 (Read)
- OPC-10000-4 5.11.2.3 (Read Response)

## Generation Script

The following Python sidecar generates `read_req.bin`:

```python
import struct

# Read Request Body
body_read = struct.pack("<BBH", 0x01, 0x00, 631)

msg_size_read = 12 + 4 + 8 + len(body_read)
msg_chunk_read = struct.pack(
    "<3scIIIII",
    b"MSG", b"F",
    msg_size_read,
    1,  # SecureChannelId
    1,  # TokenId
    8,  # SequenceNumber
    8   # RequestId
) + body_read

with open("read_req.bin", "wb") as f:
    f.write(msg_chunk_read)
```
