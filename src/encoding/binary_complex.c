/* src/encoding/binary_complex.c
 *
 * ComplexType binary encoder/decoder.
 * OPC-10000-6 §5.2.2.12 (Structure), §5.2.2.9 (Enumeration), §5.4.1.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_COMPLEX_TYPES

#include "muc_opcua/address_space/complex_types.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/types.h"
#include <stddef.h>
#include <string.h>

/* OPC UA built-in DataType node IDs (namespace=0, numeric).
 * OPC-10000-6 §5.2.2.12 (Structure encoding body = field concatenation). */
#define OPC_DT_BOOLEAN 1
#define OPC_DT_SBYTE 2
#define OPC_DT_BYTE 3
#define OPC_DT_INT16 4
#define OPC_DT_UINT16 5
#define OPC_DT_INT32 6
#define OPC_DT_UINT32 7
#define OPC_DT_INT64 8
#define OPC_DT_UINT64 9
#define OPC_DT_FLOAT 10
#define OPC_DT_DOUBLE 11
#define OPC_DT_STRING 12
#define OPC_DT_DATETIME 13
#define OPC_DT_GUID 14
#define OPC_DT_BYTESTRING 15
#define OPC_DT_NODEID 17
#define OPC_DT_STATUSCODE 19

static opcua_uint32_t field_type_id_from_nodeid(const mu_nodeid_t *data_type) {
    if (data_type == NULL || data_type->identifier_type != MU_NODEID_NUMERIC || data_type->namespace_index != 0)
        return 0;
    return data_type->identifier.numeric;
}

static opcua_statuscode_t encode_scalar_field(mu_binary_writer_t *writer, opcua_uint32_t type_id,
                                              const void *field_ptr) {
    switch (type_id) {
    case OPC_DT_BOOLEAN: {
        opcua_boolean_t v = *(const opcua_boolean_t *)field_ptr;
        return mu_binary_write_boolean(writer, v);
    }
    case OPC_DT_SBYTE: {
        opcua_sbyte_t v = *(const opcua_sbyte_t *)field_ptr;
        return mu_binary_write_sbyte(writer, v);
    }
    case OPC_DT_BYTE:
        return mu_binary_write_byte(writer, *(const opcua_byte_t *)field_ptr);
    case OPC_DT_INT16:
        return mu_binary_write_int16(writer, *(const opcua_int16_t *)field_ptr);
    case OPC_DT_UINT16:
        return mu_binary_write_uint16(writer, *(const opcua_uint16_t *)field_ptr);
    case OPC_DT_INT32:
        return mu_binary_write_int32(writer, *(const opcua_int32_t *)field_ptr);
    case OPC_DT_UINT32:
        return mu_binary_write_uint32(writer, *(const opcua_uint32_t *)field_ptr);
    case OPC_DT_INT64:
        return mu_binary_write_int64(writer, *(const opcua_int64_t *)field_ptr);
    case OPC_DT_UINT64:
        return mu_binary_write_uint64(writer, *(const opcua_uint64_t *)field_ptr);
    case OPC_DT_FLOAT:
        return mu_binary_write_float(writer, *(const opcua_float_t *)field_ptr);
    case OPC_DT_DOUBLE:
        return mu_binary_write_double(writer, *(const opcua_double_t *)field_ptr);
    case OPC_DT_STRING:
        return mu_binary_write_string(writer, (const mu_string_t *)field_ptr);
    case OPC_DT_DATETIME:
        return mu_binary_write_int64(writer, *(const opcua_int64_t *)field_ptr);
    case OPC_DT_GUID:
        return mu_binary_write_bytestring(writer, (const mu_bytestring_t *)field_ptr);
    case OPC_DT_BYTESTRING:
        return mu_binary_write_bytestring(writer, (const mu_bytestring_t *)field_ptr);
    case OPC_DT_NODEID:
        return mu_binary_write_nodeid(writer, (const mu_nodeid_t *)field_ptr);
    case OPC_DT_STATUSCODE:
        return mu_binary_write_statuscode(writer, *(const opcua_statuscode_t *)field_ptr);
    default:
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
}

static opcua_statuscode_t decode_scalar_field(mu_binary_reader_t *reader, opcua_uint32_t type_id, void *field_ptr) {
    switch (type_id) {
    case OPC_DT_BOOLEAN:
        return mu_binary_read_boolean(reader, (opcua_boolean_t *)field_ptr);
    case OPC_DT_SBYTE:
        return mu_binary_read_sbyte(reader, (opcua_sbyte_t *)field_ptr);
    case OPC_DT_BYTE:
        return mu_binary_read_byte(reader, (opcua_byte_t *)field_ptr);
    case OPC_DT_INT16:
        return mu_binary_read_int16(reader, (opcua_int16_t *)field_ptr);
    case OPC_DT_UINT16:
        return mu_binary_read_uint16(reader, (opcua_uint16_t *)field_ptr);
    case OPC_DT_INT32:
        return mu_binary_read_int32(reader, (opcua_int32_t *)field_ptr);
    case OPC_DT_UINT32:
        return mu_binary_read_uint32(reader, (opcua_uint32_t *)field_ptr);
    case OPC_DT_INT64:
        return mu_binary_read_int64(reader, (opcua_int64_t *)field_ptr);
    case OPC_DT_UINT64:
        return mu_binary_read_uint64(reader, (opcua_uint64_t *)field_ptr);
    case OPC_DT_FLOAT:
        return mu_binary_read_float(reader, (opcua_float_t *)field_ptr);
    case OPC_DT_DOUBLE:
        return mu_binary_read_double(reader, (opcua_double_t *)field_ptr);
    case OPC_DT_STRING:
        return mu_binary_read_string(reader, (mu_string_t *)field_ptr);
    case OPC_DT_DATETIME:
        return mu_binary_read_int64(reader, (opcua_int64_t *)field_ptr);
    case OPC_DT_GUID:
        return mu_binary_read_bytestring(reader, (mu_bytestring_t *)field_ptr);
    case OPC_DT_BYTESTRING:
        return mu_binary_read_bytestring(reader, (mu_bytestring_t *)field_ptr);
    case OPC_DT_NODEID:
        return mu_binary_read_nodeid(reader, (mu_nodeid_t *)field_ptr);
    case OPC_DT_STATUSCODE:
        return mu_binary_read_statuscode(reader, (opcua_statuscode_t *)field_ptr);
    default:
        if (reader->status == MU_STATUS_GOOD)
            reader->status = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }
}

static size_t scalar_type_size(opcua_uint32_t type_id) {
    switch (type_id) {
    case OPC_DT_BOOLEAN:
        return sizeof(opcua_boolean_t);
    case OPC_DT_SBYTE:
        return sizeof(opcua_sbyte_t);
    case OPC_DT_BYTE:
        return sizeof(opcua_byte_t);
    case OPC_DT_INT16:
        return sizeof(opcua_int16_t);
    case OPC_DT_UINT16:
        return sizeof(opcua_uint16_t);
    case OPC_DT_INT32:
        return sizeof(opcua_int32_t);
    case OPC_DT_UINT32:
        return sizeof(opcua_uint32_t);
    case OPC_DT_INT64:
        return sizeof(opcua_int64_t);
    case OPC_DT_UINT64:
        return sizeof(opcua_uint64_t);
    case OPC_DT_FLOAT:
        return sizeof(opcua_float_t);
    case OPC_DT_DOUBLE:
        return sizeof(opcua_double_t);
    case OPC_DT_STRING:
        return sizeof(mu_string_t);
    case OPC_DT_DATETIME:
        return sizeof(opcua_int64_t);
    case OPC_DT_GUID:
        return sizeof(mu_bytestring_t);
    case OPC_DT_BYTESTRING:
        return sizeof(mu_bytestring_t);
    case OPC_DT_NODEID:
        return sizeof(mu_nodeid_t);
    case OPC_DT_STATUSCODE:
        return sizeof(opcua_statuscode_t);
    default:
        return 0;
    }
}

static size_t structure_value_size(const mu_structure_definition_t *def);

static size_t field_value_size(const mu_structure_field_t *field) {
    opcua_uint32_t type_id = field_type_id_from_nodeid(&field->data_type);
    if (type_id != 0)
        return scalar_type_size(type_id);
    if (field->nested_structure != NULL)
        return structure_value_size(field->nested_structure);
    return 0;
}

static size_t structure_value_size(const mu_structure_definition_t *def) {
    size_t size = 0;
    if (def == NULL)
        return 0;
    for (opcua_uint16_t fi = 0; fi < def->field_count; fi++) {
        const mu_structure_field_t *field = &def->fields[fi];
        if (field->is_optional && fi % 32 == 0)
            size += sizeof(opcua_uint32_t);
        size += field_value_size(field);
    }
    return size;
}

opcua_statuscode_t mu_binary_encode_struct(mu_binary_writer_t *writer, const mu_structure_definition_t *def,
                                           const void *field_values) {
    if (writer == NULL || def == NULL || (field_values == NULL && def->field_count != 0))
        return MU_STATUS_BAD_INVALIDARGUMENT;
    if (writer->status != MU_STATUS_GOOD)
        return writer->status;

    const opcua_byte_t *base = (const opcua_byte_t *)field_values;
    opcua_uint32_t offset = 0;
    opcua_uint32_t mask_value = 0;
    opcua_uint16_t fi;

    for (fi = 0; fi < def->field_count; fi++) {
        const mu_structure_field_t *f = &def->fields[fi];
        opcua_uint32_t type_id = field_type_id_from_nodeid(&f->data_type);

        if (f->is_optional) {
            if (fi % 32 == 0) {
                memcpy(&mask_value, base + offset, sizeof(opcua_uint32_t));
                offset += sizeof(opcua_uint32_t);
                opcua_statuscode_t sc = mu_binary_write_uint32(writer, mask_value);
                if (sc != MU_STATUS_GOOD)
                    return sc;
            }
            if (!(mask_value & ((opcua_uint32_t)1u << (fi % 32)))) {
                size_t skipped_size = field_value_size(f);
                if (skipped_size == 0)
                    return MU_STATUS_BAD_INVALIDARGUMENT;
                offset += (opcua_uint32_t)skipped_size;
                continue;
            }
        }

        size_t fsize = field_value_size(f);
        const void *field_ptr = (const void *)(base + offset);

        if (f->value_rank >= 0 && type_id != 0) {
            opcua_int32_t arr_len = 0;
            memcpy(&arr_len, base + offset, sizeof(opcua_int32_t));
            offset += sizeof(opcua_int32_t);
            field_ptr = (const void *)(base + offset);
            mu_binary_write_int32(writer, arr_len);
            if (arr_len < 0)
                continue;
            for (opcua_int32_t ai = 0; ai < arr_len && writer->status == MU_STATUS_GOOD; ai++) {
                opcua_statuscode_t sc =
                    encode_scalar_field(writer, type_id, (const opcua_byte_t *)field_ptr + (size_t)ai * fsize);
                if (sc != MU_STATUS_GOOD)
                    return sc;
            }
            offset += (opcua_uint32_t)arr_len * (opcua_uint32_t)fsize;
        } else if (f->value_rank >= 0 && f->nested_structure != NULL) {
            opcua_int32_t arr_len = 0;
            memcpy(&arr_len, base + offset, sizeof(opcua_int32_t));
            offset += sizeof(opcua_int32_t);
            field_ptr = (const void *)(base + offset);
            mu_binary_write_int32(writer, arr_len);
            if (arr_len < 0)
                continue;
            for (opcua_int32_t ai = 0; ai < arr_len && writer->status == MU_STATUS_GOOD; ai++) {
                opcua_statuscode_t sc = mu_binary_encode_struct(
                    writer, f->nested_structure, (const opcua_byte_t *)field_ptr + (size_t)ai * fsize);
                if (sc != MU_STATUS_GOOD)
                    return sc;
            }
            offset += (opcua_uint32_t)arr_len * (opcua_uint32_t)fsize;
        } else if (type_id != 0) {
            opcua_statuscode_t sc = encode_scalar_field(writer, type_id, field_ptr);
            if (sc != MU_STATUS_GOOD)
                return sc;
            offset += (opcua_uint32_t)fsize;
        } else if (f->nested_structure != NULL && f->value_rank < 0) {
            opcua_statuscode_t sc = mu_binary_encode_struct(writer, f->nested_structure, field_ptr);
            if (sc != MU_STATUS_GOOD)
                return sc;
            offset += (opcua_uint32_t)fsize;
        } else {
            return MU_STATUS_BAD_INVALIDARGUMENT;
        }
    }
    return writer->status;
}

opcua_statuscode_t mu_binary_decode_struct(mu_binary_reader_t *reader, const mu_structure_definition_t *def,
                                           void *field_values) {
    if (reader == NULL || def == NULL || (field_values == NULL && def->field_count != 0))
        return MU_STATUS_BAD_INVALIDARGUMENT;
    if (reader->status != MU_STATUS_GOOD)
        return reader->status;

    opcua_byte_t *base = (opcua_byte_t *)field_values;
    opcua_uint32_t offset = 0;
    opcua_uint32_t mask_value = 0;
    opcua_uint16_t fi;

    for (fi = 0; fi < def->field_count; fi++) {
        const mu_structure_field_t *f = &def->fields[fi];
        opcua_uint32_t type_id = field_type_id_from_nodeid(&f->data_type);
        size_t fsize = field_value_size(f);

        if (f->is_optional) {
            if (fi % 32 == 0) {
                opcua_statuscode_t sc = mu_binary_read_uint32(reader, &mask_value);
                if (sc != MU_STATUS_GOOD)
                    return MU_STATUS_BAD_DECODINGERROR;
                memcpy(base + offset, &mask_value, sizeof(opcua_uint32_t));
                offset += sizeof(opcua_uint32_t);
            }
            if (!(mask_value & ((opcua_uint32_t)1u << (fi % 32)))) {
                if (fsize == 0)
                    return MU_STATUS_BAD_DECODINGERROR;
                memset((opcua_byte_t *)field_values + offset, 0, fsize);
                offset += (opcua_uint32_t)fsize;
                continue;
            }
        }

        void *field_ptr = (void *)(base + offset);

        if (f->value_rank >= 0 && type_id != 0) {
            opcua_int32_t arr_len = 0;
            opcua_statuscode_t sc = mu_binary_read_int32(reader, &arr_len);
            if (sc != MU_STATUS_GOOD || arr_len < 0) {
                if (reader->status == MU_STATUS_GOOD)
                    reader->status = MU_STATUS_BAD_DECODINGERROR;
                return MU_STATUS_BAD_DECODINGERROR;
            }
            memcpy((opcua_byte_t *)field_values + offset, &arr_len, sizeof(opcua_int32_t));
            offset += sizeof(opcua_int32_t);
            field_ptr = (void *)(base + offset);
            for (opcua_int32_t ai = 0; ai < arr_len && reader->status == MU_STATUS_GOOD; ai++) {
                opcua_statuscode_t sc2 =
                    decode_scalar_field(reader, type_id, (opcua_byte_t *)field_ptr + (size_t)ai * fsize);
                if (sc2 != MU_STATUS_GOOD)
                    return MU_STATUS_BAD_DECODINGERROR;
            }
            offset += (opcua_uint32_t)arr_len * (opcua_uint32_t)fsize;
        } else if (type_id != 0) {
            opcua_statuscode_t sc = decode_scalar_field(reader, type_id, field_ptr);
            if (sc != MU_STATUS_GOOD)
                return MU_STATUS_BAD_DECODINGERROR;
            offset += (opcua_uint32_t)fsize;
        } else if (f->nested_structure != NULL && f->value_rank < 0) {
            opcua_statuscode_t sc = mu_binary_decode_struct(reader, f->nested_structure, field_ptr);
            if (sc != MU_STATUS_GOOD)
                return MU_STATUS_BAD_DECODINGERROR;
            offset += (opcua_uint32_t)fsize;
        } else {
            if (reader->status == MU_STATUS_GOOD)
                reader->status = MU_STATUS_BAD_DECODINGERROR;
            return MU_STATUS_BAD_DECODINGERROR;
        }
    }
    return reader->status;
}

opcua_statuscode_t mu_binary_encode_enum(mu_binary_writer_t *writer, opcua_int32_t value) {
    return mu_binary_write_int32(writer, value);
}

opcua_statuscode_t mu_binary_decode_enum(mu_binary_reader_t *reader, opcua_int32_t *value) {
    return mu_binary_read_int32(reader, value);
}

#endif /* MUC_OPCUA_COMPLEX_TYPES */
