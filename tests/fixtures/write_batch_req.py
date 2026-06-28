# Fixture Generator: Write Batch Request
# Normative References:
# - OPC-10000-4 §5.11.4.2 (Write Service Parameters)
# - OPC-10000-6 §7.36 (WriteRequest Binary serialization)

import struct

# 1. Type NodeId: 4-byte numeric (0x01), namespace 0, identifier 673
type_id = struct.pack("<BBH", 0x01, 0, 673)

# 2. RequestHeader
auth_token = struct.pack("<BB", 0, 0)
timestamp = struct.pack("<q", 0)
req_handle = struct.pack("<I", 1)
return_diag = struct.pack("<I", 0)
audit_id = struct.pack("<i", -1)
timeout = struct.pack("<I", 10000)
additional_hdr = struct.pack("<BBB", 0, 0, 0)

req_header = auth_token + timestamp + req_handle + return_diag + audit_id + timeout + additional_hdr

# 3. nodesToWrite array (size = 2)
nodes_to_write_size = struct.pack("<i", 2)

# WriteValue 0 (Int32 write to ns=1, identifier 1000)
node_id_0 = struct.pack("<BBH", 0x01, 1, 1000)
attr_id_0 = struct.pack("<i", 13)
index_range_0 = struct.pack("<i", -1)
dv_mask_0 = struct.pack("<B", 0x01)
variant_mask_0 = struct.pack("<B", 0x06) # Int32
variant_val_0 = struct.pack("<i", 12345)
write_value_0 = node_id_0 + attr_id_0 + index_range_0 + dv_mask_0 + variant_mask_0 + variant_val_0

# WriteValue 1 (Float write to ns=1, identifier 2000)
node_id_1 = struct.pack("<BBH", 0x01, 1, 2000)
attr_id_1 = struct.pack("<i", 13)
index_range_1 = struct.pack("<i", -1)
dv_mask_1 = struct.pack("<B", 0x01)
variant_mask_1 = struct.pack("<B", 0x0a) # Float
variant_val_1 = struct.pack("<f", 1.5)
write_value_1 = node_id_1 + attr_id_1 + index_range_1 + dv_mask_1 + variant_mask_1 + variant_val_1

body_write = type_id + req_header + nodes_to_write_size + write_value_0 + write_value_1

with open("tests/fixtures/write_batch_req.bin", "wb") as f:
    f.write(body_write)

print("Successfully generated tests/fixtures/write_batch_req.bin of size", len(body_write))
