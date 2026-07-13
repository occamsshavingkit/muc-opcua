/* src/encoding/binary_reader.c */
#include "binary_le.h"
#include "muc_opcua/encoding.h"
#include <string.h>

void mu_binary_reader_init(mu_binary_reader_t *reader, const opcua_byte_t *buffer, size_t length) {
    if (reader) {
        reader->buffer = buffer;
        reader->length = length;
        reader->position = 0;
        reader->status = MU_STATUS_GOOD;
    }
}

#if defined(MUC_OPCUA_CU_USER_AUTH) || defined(MUC_OPCUA_FACET_CORE_2022_SERVER)
static opcua_statuscode_t reader_status(const mu_binary_reader_t *reader) {
    if (!reader) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    /* Once tripped, later primitive reads are no-ops. */
    if (reader->status != MU_STATUS_GOOD) {
        return reader->status;
    }
    return MU_STATUS_GOOD;
}
#endif

static opcua_statuscode_t reader_fail(mu_binary_reader_t *reader, opcua_statuscode_t status) {
    if (reader) {
        reader->status = status;
    }
    return status;
}

static opcua_statuscode_t ensure_bytes(mu_binary_reader_t *reader, size_t count) {
    /* OPC-10000-6 section 5.2: validate remaining bytes before advancing. */
    if (reader && reader->status == MU_STATUS_GOOD && reader->buffer && reader->position <= reader->length &&
        count <= reader->length - reader->position) {
        return MU_STATUS_GOOD;
    }
    if (!reader) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (reader->status != MU_STATUS_GOOD) {
        return reader->status;
    }
    if (!reader->buffer) {
        return reader_fail(reader, MU_STATUS_BAD_INTERNALERROR);
    }
    return reader_fail(reader, MU_STATUS_BAD_DECODINGERROR);
}

opcua_statuscode_t mu_binary_read_boolean(mu_binary_reader_t *reader, opcua_boolean_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = reader->buffer[reader->position++] ? true : false;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_sbyte(mu_binary_reader_t *reader, opcua_sbyte_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = (opcua_sbyte_t)reader->buffer[reader->position++];
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_byte(mu_binary_reader_t *reader, opcua_byte_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = reader->buffer[reader->position++];
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_int16(mu_binary_reader_t *reader, opcua_int16_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 2);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = (opcua_int16_t)mu_binary_le_get_u16(&reader->buffer[reader->position]);
    reader->position += 2;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_uint16(mu_binary_reader_t *reader, opcua_uint16_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 2);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = mu_binary_le_get_u16(&reader->buffer[reader->position]);
    reader->position += 2;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_int32(mu_binary_reader_t *reader, opcua_int32_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 4);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = (opcua_int32_t)mu_binary_le_get_u32(&reader->buffer[reader->position]);
    reader->position += 4;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_array_length(mu_binary_reader_t *reader, opcua_int32_t *length) {
    opcua_statuscode_t status = ensure_bytes(reader, 4);
    opcua_int32_t decoded_length;
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    decoded_length = (opcua_int32_t)mu_binary_le_get_u32(&reader->buffer[reader->position]);
    *length = decoded_length;
    reader->position += 4;
    /* OPC-10000-6 section 5.2.5 encodes array length as Int32; only -1 is null.
       Element-size checks happen at callers; reject counts that would overflow
       even one-byte element position arithmetic. */
    if (decoded_length < -1 || (decoded_length > 0 && (size_t)decoded_length > SIZE_MAX - reader->position)) {
        return reader_fail(reader, MU_STATUS_BAD_DECODINGERROR);
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_uint32(mu_binary_reader_t *reader, opcua_uint32_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 4);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = mu_binary_le_get_u32(&reader->buffer[reader->position]);
    reader->position += 4;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_int64(mu_binary_reader_t *reader, opcua_int64_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 8);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = (opcua_int64_t)mu_binary_le_get_u64(&reader->buffer[reader->position]);
    reader->position += 8;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_uint64(mu_binary_reader_t *reader, opcua_uint64_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 8);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    *value = mu_binary_le_get_u64(&reader->buffer[reader->position]);
    reader->position += 8;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_float(mu_binary_reader_t *reader, opcua_float_t *value) {
    opcua_uint32_t tmp;
    opcua_statuscode_t status = mu_binary_read_uint32(reader, &tmp);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    (void)memcpy(value, &tmp, sizeof(opcua_float_t));
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_double(mu_binary_reader_t *reader, opcua_double_t *value) {
    opcua_uint64_t tmp;
    opcua_statuscode_t status = mu_binary_read_uint64(reader, &tmp);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    (void)memcpy(value, &tmp, sizeof(opcua_double_t));
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_statuscode(mu_binary_reader_t *reader, opcua_statuscode_t *value) {
    return mu_binary_read_uint32(reader, value);
}

#ifdef MUC_OPCUA_CU_USER_AUTH
opcua_statuscode_t mu_binary_read_username_identity_token(mu_binary_reader_t *reader,
                                                          mu_username_identity_token_t *value) {
    opcua_statuscode_t status = reader_status(reader);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_string(reader, &value->policy_id);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_string(reader, &value->username);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_bytestring(reader, &value->password);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_string(reader, &value->encryption_algorithm);
    return status;
}
#endif

#ifdef MUC_OPCUA_FACET_CORE_2022_SERVER
opcua_statuscode_t mu_binary_read_certificate_identity_token(mu_binary_reader_t *reader,
                                                             mu_certificate_identity_token_t *value) {
    opcua_statuscode_t status = reader_status(reader);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_string(reader, &value->policy_id);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_bytestring(reader, &value->certificate_data);
    return status;
}

#if MUC_OPCUA_CU_DATA_ACCESS
opcua_statuscode_t mu_binary_read_range(mu_binary_reader_t *reader, mu_range_t *value) {
    if (!reader || !value)
        return MU_STATUS_BAD_INTERNALERROR;
    opcua_statuscode_t s = mu_binary_read_double(reader, &value->low);
    if (s != MU_STATUS_GOOD)
        return s;
    return mu_binary_read_double(reader, &value->high);
}

opcua_statuscode_t mu_binary_read_localized_text(mu_binary_reader_t *reader, mu_localized_text_t *value) {
    if (!reader || !value)
        return MU_STATUS_BAD_INTERNALERROR;
    value->locale.length = -1;
    value->locale.data = NULL;
    value->text.length = -1;
    value->text.data = NULL;
    opcua_byte_t mask = 0;
    opcua_statuscode_t s = mu_binary_read_byte(reader, &mask);
    if (s != MU_STATUS_GOOD)
        return s;
    if (mask & 0x01u) {
        s = mu_binary_read_string(reader, &value->locale);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    if (mask & 0x02u) {
        s = mu_binary_read_string(reader, &value->text);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_eu_information(mu_binary_reader_t *reader, mu_eu_information_t *value) {
    if (!reader || !value)
        return MU_STATUS_BAD_INTERNALERROR;
    opcua_statuscode_t s = mu_binary_read_string(reader, &value->namespace_uri);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_read_int32(reader, &value->unit_id);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_read_localized_text(reader, &value->display_name);
    if (s != MU_STATUS_GOOD)
        return s;
    return mu_binary_read_localized_text(reader, &value->description);
}
#endif
#endif
