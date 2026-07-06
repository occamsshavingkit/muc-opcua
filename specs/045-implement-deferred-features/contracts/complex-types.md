# API Contract: Complex Type Binary Encode/Decode

**Feature**: 045-implement-deferred-features (D1)
**OPC Ref**: OPC-10000-6 §5.2.2.12 (Structure), §5.2.2.9 (Enumeration), §5.4.1

## Public API

### Structure Encoding

```c
opcua_statuscode_t mu_binary_encode_struct(
    mu_binary_writer_t *writer,
    const mu_structure_definition_t *def,
    const void *field_values);
```

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `writer` | in/out | Binary writer with caller-provided output buffer |
| `def` | in | Structure type definition (from `mu_register_structure_type`) |
| `field_values` | in | Pointer to caller-allocated storage with field values in definition order |

**Returns**: `MU_STATUS_GOOD` on success, `MU_STATUS_BAD_WRITE` on buffer overflow,
`MU_STATUS_BAD_INVALIDARGUMENT` on NULL writer/def.

### Structure Decoding

```c
opcua_statuscode_t mu_binary_decode_struct(
    mu_binary_reader_t *reader,
    const mu_structure_definition_t *def,
    void *field_values);
```

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `reader` | in/out | Binary reader with caller-provided input buffer |
| `def` | in | Structure type definition |
| `field_values` | out | Caller-allocated output storage, filled with decoded values |

**Returns**: `MU_STATUS_GOOD` on success, `MU_STATUS_BAD_DECODINGERROR` on malformed data,
`MU_STATUS_BAD_UNEXPECTEDEOF` on truncation, `MU_STATUS_BAD_INVALIDARGUMENT` on NULL args.

### Enumeration Encoding

```c
opcua_statuscode_t mu_binary_encode_enum(mu_binary_writer_t *writer, opcua_int32_t value);
```

Encodes an enumeration as an Int32 (OPC-10000-6 §5.2.2.9).

### Enumeration Decoding

```c
opcua_statuscode_t mu_binary_decode_enum(mu_binary_reader_t *reader, opcua_int32_t *value);
```

Decodes an Int32 enumeration value.

### Type Lookup (NEW)

```c
const mu_structure_definition_t *mu_find_structure_definition(
    const struct mu_server *server,
    const mu_nodeid_t *type_id);

const mu_enum_definition_t *mu_find_enum_definition(
    const struct mu_server *server,
    const mu_nodeid_t *type_id);
```

Return the registered definition matching `type_id`, or NULL if not found.

## Encoding Rules

Per OPC-10000-6 §5.4.1, structure body encoding is the sequential concatenation
of each field's binary encoding in definition order:

1. For each field in `def->fields`:
   a. If `is_optional`: check EncodingMask bit at field position; skip if bit not set
   b. If `value_rank == -1` (scalar): encode as scalar of `data_type`
   c. If `value_rank >= 0` (array): encode `Int32` length, then each element
   d. If `data_type` is a structure: recurse with nested definition
   e. If `data_type` is an enumeration: encode as `Int32`

2. Decoder validates:
   a. Sufficient bytes remain before each field read → `MU_STATUS_BAD_DECODINGERROR`
   b. EncodingMask bit is set but field value ptr is NULL → `MU_STATUS_BAD_DECODINGERROR`
   c. Array length fits in remaining buffer → `MU_STATUS_BAD_DECODINGERROR`
