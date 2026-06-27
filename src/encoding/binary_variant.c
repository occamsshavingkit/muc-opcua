/* src/encoding/binary_variant.c */
#include "micro_opcua/encoding.h"

opcua_statuscode_t mu_binary_read_variant(mu_binary_reader_t *reader, mu_variant_t *variant) {
    opcua_byte_t encoding_mask;
    opcua_statuscode_t status = mu_binary_read_byte(reader, &encoding_mask);
    if (status != MU_STATUS_GOOD) return status;
    
    opcua_byte_t type = encoding_mask & 0x3F;
    if ((encoding_mask & 0x80) || (encoding_mask & 0x40)) {
        /* Arrays / dimensions not supported */
        return MU_STATUS_BAD_DECODINGERROR;
    }
    
    variant->type = (mu_builtin_type_t)type;
    
    switch (type) {
        case MU_TYPE_NULL:
            break;
        case MU_TYPE_BOOLEAN:
            return mu_binary_read_boolean(reader, &variant->value.b);
        case MU_TYPE_SBYTE:
            return mu_binary_read_sbyte(reader, &variant->value.sb);
        case MU_TYPE_BYTE:
            return mu_binary_read_byte(reader, &variant->value.by);
        case MU_TYPE_INT16:
            return mu_binary_read_int16(reader, &variant->value.i16);
        case MU_TYPE_UINT16:
            return mu_binary_read_uint16(reader, &variant->value.ui16);
        case MU_TYPE_INT32:
            return mu_binary_read_int32(reader, &variant->value.i32);
        case MU_TYPE_UINT32:
            return mu_binary_read_uint32(reader, &variant->value.ui32);
        case MU_TYPE_INT64:
            return mu_binary_read_int64(reader, &variant->value.i64);
        case MU_TYPE_UINT64:
            return mu_binary_read_uint64(reader, &variant->value.ui64);
        case MU_TYPE_FLOAT:
            return mu_binary_read_float(reader, &variant->value.f);
        case MU_TYPE_DOUBLE:
            return mu_binary_read_double(reader, &variant->value.d);
        case MU_TYPE_STRING:
            return mu_binary_read_string(reader, &variant->value.str);
        case MU_TYPE_BYTESTRING:
            return mu_binary_read_bytestring(reader, &variant->value.bytestr);
        case MU_TYPE_DATETIME:
            return mu_binary_read_int64(reader, &variant->value.dt);
        case MU_TYPE_STATUSCODE:
            return mu_binary_read_statuscode(reader, &variant->value.status);
        case MU_TYPE_NODEID:
            return mu_binary_read_nodeid(reader, &variant->value.nodeid);
        case MU_TYPE_QUALIFIEDNAME:
            status = mu_binary_read_uint16(reader, &variant->value.qualified_name.namespace_index);
            if (status != MU_STATUS_GOOD) return status;
            return mu_binary_read_string(reader, &variant->value.qualified_name.name);
        case MU_TYPE_LOCALIZEDTEXT: {
            opcua_byte_t lt_mask;
            status = mu_binary_read_byte(reader, &lt_mask);
            if (status != MU_STATUS_GOOD) return status;
            variant->value.localized_text.locale = (mu_string_t){ -1, NULL };
            variant->value.localized_text.text = (mu_string_t){ -1, NULL };
            if (lt_mask & 0x01) {
                status = mu_binary_read_string(reader, &variant->value.localized_text.locale);
                if (status != MU_STATUS_GOOD) return status;
            }
            if (lt_mask & 0x02) {
                status = mu_binary_read_string(reader, &variant->value.localized_text.text);
                if (status != MU_STATUS_GOOD) return status;
            }
            return MU_STATUS_GOOD;
        }
        default:
            return MU_STATUS_BAD_DECODINGERROR;
    }
    return MU_STATUS_GOOD;
}

/* Write one built-in value from a pointer to it (used for both scalars and array
   elements). For scalars the pointer is &variant->value (all union members alias
   offset 0); for arrays it is the element address. */
static opcua_statuscode_t write_scalar_value(mu_binary_writer_t *w, mu_builtin_type_t type, const void *p) {
    switch (type) {
        case MU_TYPE_BOOLEAN:    return mu_binary_write_boolean(w, *(const opcua_boolean_t *)p);
        case MU_TYPE_SBYTE:      return mu_binary_write_sbyte(w, *(const opcua_sbyte_t *)p);
        case MU_TYPE_BYTE:       return mu_binary_write_byte(w, *(const opcua_byte_t *)p);
        case MU_TYPE_INT16:      return mu_binary_write_int16(w, *(const opcua_int16_t *)p);
        case MU_TYPE_UINT16:     return mu_binary_write_uint16(w, *(const opcua_uint16_t *)p);
        case MU_TYPE_INT32:      return mu_binary_write_int32(w, *(const opcua_int32_t *)p);
        case MU_TYPE_UINT32:     return mu_binary_write_uint32(w, *(const opcua_uint32_t *)p);
        case MU_TYPE_INT64:      return mu_binary_write_int64(w, *(const opcua_int64_t *)p);
        case MU_TYPE_UINT64:     return mu_binary_write_uint64(w, *(const opcua_uint64_t *)p);
        case MU_TYPE_FLOAT:      return mu_binary_write_float(w, *(const opcua_float_t *)p);
        case MU_TYPE_DOUBLE:     return mu_binary_write_double(w, *(const opcua_double_t *)p);
        case MU_TYPE_STRING:     return mu_binary_write_string(w, (const mu_string_t *)p);
        case MU_TYPE_BYTESTRING: return mu_binary_write_bytestring(w, (const mu_bytestring_t *)p);
        case MU_TYPE_DATETIME:   return mu_binary_write_int64(w, *(const opcua_datetime_t *)p);
        case MU_TYPE_STATUSCODE: return mu_binary_write_statuscode(w, *(const opcua_statuscode_t *)p);
        case MU_TYPE_NODEID:     return mu_binary_write_nodeid(w, (const mu_nodeid_t *)p);
        case MU_TYPE_QUALIFIEDNAME: {
            const mu_qualified_name_t *q = (const mu_qualified_name_t *)p;
            opcua_statuscode_t s = mu_binary_write_uint16(w, q->namespace_index);
            if (s != MU_STATUS_GOOD) return s;
            return mu_binary_write_string(w, &q->name);
        }
        case MU_TYPE_LOCALIZEDTEXT: {
            const mu_localized_text_t *lt = (const mu_localized_text_t *)p;
            opcua_byte_t lt_mask = 0;
            if (lt->locale.length >= 0) lt_mask |= 0x01;
            if (lt->text.length >= 0) lt_mask |= 0x02;
            opcua_statuscode_t s = mu_binary_write_byte(w, lt_mask);
            if (s != MU_STATUS_GOOD) return s;
            if (lt_mask & 0x01) { s = mu_binary_write_string(w, &lt->locale); if (s != MU_STATUS_GOOD) return s; }
            if (lt_mask & 0x02) { s = mu_binary_write_string(w, &lt->text);   if (s != MU_STATUS_GOOD) return s; }
            return MU_STATUS_GOOD;
        }
        default:
            return MU_STATUS_BAD_ENCODINGERROR;
    }
}

/* In-memory element stride for a built-in type, for indexing array values. */
static size_t element_size(mu_builtin_type_t type) {
    static const size_t sizes[MU_TYPE_LOCALIZEDTEXT + 1] = {
        [MU_TYPE_BOOLEAN]       = sizeof(opcua_boolean_t),
        [MU_TYPE_SBYTE]         = sizeof(opcua_sbyte_t),
        [MU_TYPE_BYTE]          = sizeof(opcua_byte_t),
        [MU_TYPE_INT16]         = sizeof(opcua_int16_t),
        [MU_TYPE_UINT16]        = sizeof(opcua_uint16_t),
        [MU_TYPE_INT32]         = sizeof(opcua_int32_t),
        [MU_TYPE_UINT32]        = sizeof(opcua_uint32_t),
        [MU_TYPE_INT64]         = sizeof(opcua_int64_t),
        [MU_TYPE_UINT64]        = sizeof(opcua_uint64_t),
        [MU_TYPE_FLOAT]         = sizeof(opcua_float_t),
        [MU_TYPE_DOUBLE]        = sizeof(opcua_double_t),
        [MU_TYPE_STRING]        = sizeof(mu_string_t),
        [MU_TYPE_DATETIME]      = sizeof(opcua_datetime_t),
        [MU_TYPE_BYTESTRING]    = sizeof(mu_bytestring_t),
        [MU_TYPE_NODEID]        = sizeof(mu_nodeid_t),
        [MU_TYPE_STATUSCODE]    = sizeof(opcua_statuscode_t),
        [MU_TYPE_QUALIFIEDNAME] = sizeof(mu_qualified_name_t),
        [MU_TYPE_LOCALIZEDTEXT] = sizeof(mu_localized_text_t),
    };
    int type_id = (int)type;
    if (type_id < 0 || (size_t)type_id >= sizeof(sizes) / sizeof(sizes[0])) {
        return 0;
    }
    return sizes[type_id];
}

opcua_statuscode_t mu_binary_write_variant(mu_binary_writer_t *writer, const mu_variant_t *variant) {
    if (!variant) return MU_STATUS_BAD_ENCODINGERROR;

    opcua_byte_t encoding_mask = (opcua_byte_t)variant->type;
    if (variant->is_array) encoding_mask |= 0x80; /* OPC 10000-6 5.2.2.16 array bit */
    opcua_statuscode_t status = mu_binary_write_byte(writer, encoding_mask);
    if (status != MU_STATUS_GOOD) return status;

    if (variant->type == MU_TYPE_NULL) {
        return MU_STATUS_GOOD;
    }

    if (variant->is_array) {
        status = mu_binary_write_int32(writer, variant->array_length);
        if (status != MU_STATUS_GOOD) return status;
        if (variant->array_length < 0 || variant->value.array == NULL) {
            return MU_STATUS_GOOD; /* null array */
        }
        size_t stride = element_size(variant->type);
        if (stride == 0) return MU_STATUS_BAD_ENCODINGERROR;
        const opcua_byte_t *base = (const opcua_byte_t *)variant->value.array;
        for (opcua_int32_t i = 0; i < variant->array_length; ++i) {
            status = write_scalar_value(writer, variant->type, base + (size_t)i * stride);
            if (status != MU_STATUS_GOOD) return status;
        }
        return MU_STATUS_GOOD;
    }

    return write_scalar_value(writer, variant->type, &variant->value);
}
