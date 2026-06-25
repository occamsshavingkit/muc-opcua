# BrowseNext Services Fixtures

These fixtures validate that OPC UA UASC BrowseNext requests are rejected with `Bad_ServiceUnsupported`.

## OPC References
- OPC-10000-4 5.9.3.2 (BrowseNext)
- OPC-10000-4 5.9.3.4 (BrowseNext Response)
- OPC-10000-4 7.38.2 (ServiceUnsupported)

## Generation Script

The following Python sidecar generates `browse_next_req.bin`:

```python
import struct

# BrowseNext Request Body
body_browsenext = struct.pack("<BBH", 0x01, 0x00, 533)

msg_size_browsenext = 12 + 4 + 8 + len(body_browsenext)
msg_chunk_browsenext = struct.pack(
    "<3scIIIII",
    b"MSG", b"F",
    msg_size_browsenext,
    1,  # SecureChannelId
    1,  # TokenId
    7,  # SequenceNumber
    7   # RequestId
) + body_browsenext

with open("browse_next_req.bin", "wb") as f:
    f.write(msg_chunk_browsenext)
```
