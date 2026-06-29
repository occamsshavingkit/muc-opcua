# Discovery Services Fixtures

These fixtures validate OPC UA UASC FindServers and GetEndpoints requests.

## OPC References
- OPC-10000-4 5.5.2.2 (FindServers)
- OPC-10000-4 5.5.4.2 (GetEndpoints)

## Generation Script

The following Python sidecar generates `findservers_req.bin` and `getendpoints_req.bin`:

```python
import struct

def nodeid_twobyte(identifier):
    return struct.pack("<BB", 0x00, identifier)

def nodeid_fourbyte(identifier):
    return struct.pack("<BBH", 0x01, 0x00, identifier)

def ua_string(value):
    if value is None:
        return struct.pack("<i", -1)
    data = value.encode("utf-8")
    return struct.pack("<i", len(data)) + data

def request_header(request_handle):
    return b"".join([
        nodeid_twobyte(0),          # AuthenticationToken
        struct.pack("<q", 0),       # Timestamp
        struct.pack("<I", request_handle),
        struct.pack("<I", 0),       # ReturnDiagnostics
        ua_string(None),            # AuditEntryId
        struct.pack("<I", 0),       # TimeoutHint
        nodeid_twobyte(0) + b"\x00" # AdditionalHeader ExtensionObject
    ])

# FindServers Request Body
# NodeId FourByte ns=0 id=422
body_findservers = nodeid_fourbyte(422) + request_header(1)

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
body_getendpoints = b"".join([
    nodeid_fourbyte(428),
    request_header(2),
    ua_string(None),      # endpointUrl
    struct.pack("<i", 0), # localeIds[]
    struct.pack("<i", 0)  # profileUris[]
])

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

## GetEndpoints Profile URI Filtering

OPC-10000-4 §5.5.4.2 defines `profileUris[]` as the list of Transport Profiles
that returned endpoints shall support. `tests/unit/test_discovery_endpoint.c`
covers two in-memory request bodies:

- `profileUris[] = ["http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary"]`
  returns the local OPC UA TCP Binary endpoint.
- `profileUris[] = ["http://opcfoundation.org/UA-Profile/Transport/https-uabinary"]`
  returns a Good service result with an empty endpoint array.
