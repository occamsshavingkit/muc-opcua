# Fixture Generator: Write Int32 Request
# Normative References:
# - OPC-10000-4 §5.11.4 (Write Service)
# - OPC-10000-4 §5.11.4.2 (Write Service Parameters)
# - OPC-10000-6 §7.36 (WriteRequest Binary serialization)
# - OPC-10000-6 §5.2.2 (Numeric NodeId binary encoding)

import struct

# 1. Type NodeId: 4-byte numeric (0x01), namespace 0, identifier 673 (0x02a1)
# Citing OPC-10000-6 §5.2.2.4 (Four-byte NodeId encoding)
type_id = struct.pack("<BBH", 0x01, 0, 673)

# 2. RequestHeader (OPC-10000-4 §7.32)
# - authenticationToken: Two-byte numeric (0x00), value 0
auth_token = struct.pack("<BB", 0, 0)
timestamp = struct.pack("<q", 0)
req_handle = struct.pack("<I", 1)
return_diag = struct.pack("<I", 0)
audit_id = struct.pack("<i", -1)
timeout = struct.pack("<I", 10000)
# - additionalHeader: Null ExtensionObject (Two-byte NodeId 0, encoding mask 0)
additional_hdr = struct.pack("<BBB", 0, 0, 0)

req_header = auth_token + timestamp + req_handle + return_diag + audit_id + timeout + additional_hdr

# 3. WriteRequest Parameters (OPC-10000-4 §5.11.4.2)
# - nodesToWriteSize: 1 (Array of length 1)
nodes_to_write_size = struct.pack("<i", 1)

# WriteValue 0 (OPC-10000-4 Table 53)
# - nodeId: 4-byte numeric (0x01), namespace 1, identifier 5001 (0x1389)
node_id = struct.pack("<BBH", 0x01, 1, 5001)
# - attributeId: 13 (Value)
attr_id = struct.pack("<i", 13)
# - indexRange: null String (-1 length)
index_range = struct.pack("<i", -1)
# - value: DataValue
#   - encoding mask: 0x01 (has value variant, no status, no timestamps)
#   - variant: type 0x06 (Int32), value 42
dv_mask = struct.pack("<B", 0x01)
variant_mask = struct.pack("<B", 0x06)
variant_val = struct.pack("<i", 42)
data_value = dv_mask + variant_mask + variant_val

write_value = node_id + attr_id + index_range + data_value

body_write = type_id + req_header + nodes_to_write_size + write_value

with open("tests/fixtures/write_int32_req.bin", "wb") as f:
    f.write(body_write)

print("Successfully generated tests/fixtures/write_int32_req.bin of size", len(body_write))
