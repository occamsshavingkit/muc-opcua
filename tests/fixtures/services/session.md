# Session Services Fixtures

These fixtures validate OPC UA UASC CreateSession, ActivateSession, and CloseSession requests.

## OPC References
- OPC-10000-4 5.7.2.2 (CreateSession)
- OPC-10000-4 5.7.3.2 (ActivateSession)
- OPC-10000-4 5.7.4.2 (CloseSession)

## Generation Script

The following Python sidecar generates `createsession_req.bin`, `activatesession_req.bin`, and `closesession_req.bin`:

```python
import struct

# CreateSession Request Body
body_create = struct.pack("<BBH", 0x01, 0x00, 461)

msg_size_create = 12 + 4 + 8 + len(body_create)
msg_chunk_create = struct.pack(
    "<3scIIIII",
    b"MSG", b"F",
    msg_size_create,
    1,  # SecureChannelId
    1,  # TokenId
    3,  # SequenceNumber
    3   # RequestId
) + body_create

with open("createsession_req.bin", "wb") as f:
    f.write(msg_chunk_create)

# ActivateSession Request Body
body_activate = struct.pack("<BBH", 0x01, 0x00, 467)

msg_size_activate = 12 + 4 + 8 + len(body_activate)
msg_chunk_activate = struct.pack(
    "<3scIIIII",
    b"MSG", b"F",
    msg_size_activate,
    1,  # SecureChannelId
    1,  # TokenId
    4,  # SequenceNumber
    4   # RequestId
) + body_activate

with open("activatesession_req.bin", "wb") as f:
    f.write(msg_chunk_activate)

# CloseSession Request Body
body_close = struct.pack("<BBH", 0x01, 0x00, 473)

msg_size_close = 12 + 4 + 8 + len(body_close)
msg_chunk_close = struct.pack(
    "<3scIIIII",
    b"MSG", b"F",
    msg_size_close,
    1,  # SecureChannelId
    1,  # TokenId
    5,  # SequenceNumber
    5   # RequestId
) + body_close

with open("closesession_req.bin", "wb") as f:
    f.write(msg_chunk_close)
```
