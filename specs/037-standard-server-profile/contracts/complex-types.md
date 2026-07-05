# Contract: Complex Types

**Feature**: 037-standard-server-profile | **OPC Reference**: OPC-10000-3 §5.6.4, §5.6.5

## Registration API

```c
// Register a custom Structure DataType.
mu_status_t mu_register_structure_type(
    mu_address_space_t *addr,
    mu_node_id_t type_id,
    const mu_structure_definition_t *def
);

// Register a custom Enumeration DataType.
mu_status_t mu_register_enumeration_type(
    mu_address_space_t *addr,
    mu_node_id_t type_id,
    const mu_enum_definition_t *def
);

// Structure definition
typedef struct {
    mu_node_id_t    default_encoding_id;   // default binary encoding NodeId
    mu_node_id_t    base_data_type;        // usually ns=0;i=22 (Structure)
    mu_structure_type_t structure_type;    // Structure_0 or StructureWithOptionalFields_1
    mu_structure_field_t *fields;
    uint16_t        field_count;
} mu_structure_definition_t;

typedef struct {
    const char             *name;
    mu_localized_text_t     description;
    mu_node_id_t            data_type;
    int32_t                 value_rank;       // -1 = scalar
    uint32_t               *array_dimensions; // NULL if scalar
    uint32_t                max_string_length;
    bool                    is_optional;
} mu_structure_field_t;

// Enumeration definition
typedef struct {
    mu_enum_field_t *fields;
    uint16_t         field_count;
} mu_enum_definition_t;

typedef struct {
    int64_t              value;
    mu_localized_text_t  display_name;
    mu_localized_text_t  description;
    const char          *name;
} mu_enum_field_t;
```

## Address Space Integration

On registration:
1. Create a DataType node in the DataType hierarchy as a child of Structure
   (ns=0;i=22) or Enumeration (ns=0;i=29) via HasSubtype reference.
2. Set DataTypeDefinition attribute with the StructureDefinition or
   EnumDefinition value.
3. Create the default binary encoding node (a child Object with HasEncoding
   reference).

## Binary Encoding

Structure encoding follows OPC-10000-6 §5.2.2.12 (Structure encoding):
- Fields are encoded in declaration order
- Optional fields preceded by a UInt32 EncodingMask (bit 0 = field 0 present, etc.)
- StructureWithOptionalFields uses EncodingMask; Structure_0 does not

Enumeration encoding follows OPC-10000-6 §5.2.2.9 (Enumeration encoding):
- Encoded as Int32

## Decode/Encode Interface

```c
// Encode a structure value to binary.
mu_status_t mu_encode_structure(
    mu_encoder_t *enc,
    mu_node_id_t type_id,
    const void *value
);

// Decode a structure value from binary.
mu_status_t mu_decode_structure(
    mu_decoder_t *dec,
    mu_node_id_t type_id,
    void *value,
    size_t value_size
);
```

## Test Contract

1. Register a custom structure (e.g., Point3D with x, y, z Double fields).
2. Browse Types/DataTypes/BaseDataType/Structure/ → Point3D appears.
3. Read DataTypeDefinition → StructureDefinition with 3 Double fields returned.
4. Encode Point3D value → correct binary layout (24 bytes: 3 × Double).
5. Decode binary → correct Point3D values reconstructed.
6. Register an enumeration (e.g., StatusEnum with OK=0, WARN=1, ERR=2).
7. Encode/decode enumeration → correct Int32 value.
8. Create Variable of custom structure type → value can be read/written correctly.

## Gating

`MUC_OPCUA_COMPLEX_TYPES` CMake flag. When OFF, only built-in types are supported.
