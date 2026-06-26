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

opcua_statuscode_t mu_binary_write_variant(mu_binary_writer_t *writer, const mu_variant_t *variant) {
    if (!variant) return MU_STATUS_BAD_ENCODINGERROR;
    
    opcua_byte_t encoding_mask = (opcua_byte_t)variant->type;
    opcua_statuscode_t status = mu_binary_write_byte(writer, encoding_mask);
    if (status != MU_STATUS_GOOD) return status;
    
    switch (variant->type) {
        case MU_TYPE_NULL:
            break;
        case MU_TYPE_BOOLEAN:
            return mu_binary_write_boolean(writer, variant->value.b);
        case MU_TYPE_SBYTE:
            return mu_binary_write_sbyte(writer, variant->value.sb);
        case MU_TYPE_BYTE:
            return mu_binary_write_byte(writer, variant->value.by);
        case MU_TYPE_INT16:
            return mu_binary_write_int16(writer, variant->value.i16);
        case MU_TYPE_UINT16:
            return mu_binary_write_uint16(writer, variant->value.ui16);
        case MU_TYPE_INT32:
            return mu_binary_write_int32(writer, variant->value.i32);
        case MU_TYPE_UINT32:
            return mu_binary_write_uint32(writer, variant->value.ui32);
        case MU_TYPE_INT64:
            return mu_binary_write_int64(writer, variant->value.i64);
        case MU_TYPE_UINT64:
            return mu_binary_write_uint64(writer, variant->value.ui64);
        case MU_TYPE_FLOAT:
            return mu_binary_write_float(writer, variant->value.f);
        case MU_TYPE_DOUBLE:
            return mu_binary_write_double(writer, variant->value.d);
        case MU_TYPE_STRING:
            return mu_binary_write_string(writer, &variant->value.str);
        case MU_TYPE_BYTESTRING:
            return mu_binary_write_bytestring(writer, &variant->value.bytestr);
        case MU_TYPE_DATETIME:
            return mu_binary_write_int64(writer, variant->value.dt);
        case MU_TYPE_STATUSCODE:
            return mu_binary_write_statuscode(writer, variant->value.status);
        case MU_TYPE_NODEID:
            return mu_binary_write_nodeid(writer, &variant->value.nodeid);
        case MU_TYPE_QUALIFIEDNAME:
            status = mu_binary_write_uint16(writer, variant->value.qualified_name.namespace_index);
            if (status != MU_STATUS_GOOD) return status;
            return mu_binary_write_string(writer, &variant->value.qualified_name.name);
        case MU_TYPE_LOCALIZEDTEXT: {
            const mu_localized_text_t *lt = &variant->value.localized_text;
            opcua_byte_t lt_mask = 0;
            if (lt->locale.length >= 0) lt_mask |= 0x01;
            if (lt->text.length >= 0) lt_mask |= 0x02;
            status = mu_binary_write_byte(writer, lt_mask);
            if (status != MU_STATUS_GOOD) return status;
            if (lt_mask & 0x01) {
                status = mu_binary_write_string(writer, &lt->locale);
                if (status != MU_STATUS_GOOD) return status;
            }
            if (lt_mask & 0x02) {
                status = mu_binary_write_string(writer, &lt->text);
                if (status != MU_STATUS_GOOD) return status;
            }
            return MU_STATUS_GOOD;
        }
        default:
            return MU_STATUS_BAD_ENCODINGERROR;
    }
    return MU_STATUS_GOOD;
}
