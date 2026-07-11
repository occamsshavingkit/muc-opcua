/* src/encoding/binary_writer.c */
#include "binary_le.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/services/method.h"
#include <string.h>

void mu_binary_writer_init(mu_binary_writer_t *writer, opcua_byte_t *buffer, size_t length) {
    if (writer) {
        writer->buffer = buffer;
        writer->length = length;
        writer->position = 0;
        writer->status = MU_STATUS_GOOD;
    }
}

static opcua_statuscode_t writer_status(const mu_binary_writer_t *writer) {
    if (!writer) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    /* Once tripped, later primitive writes are no-ops. */
    if (writer->status != MU_STATUS_GOOD) {
        return writer->status;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t writer_fail(mu_binary_writer_t *writer, opcua_statuscode_t status) {
    if (writer) {
        writer->status = status;
    }
    return status;
}

static opcua_statuscode_t ensure_space(mu_binary_writer_t *writer, size_t count) {
    /* OPC-10000-6 Section 5.2: encoded values must fit the output buffer. */
    if (writer && writer->status == MU_STATUS_GOOD && writer->buffer && writer->position <= writer->length &&
        count <= writer->length - writer->position) {
        return MU_STATUS_GOOD;
    }
    opcua_statuscode_t status = writer_status(writer);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    if (!writer->buffer) {
        return writer_fail(writer, MU_STATUS_BAD_INTERNALERROR);
    }
    return writer_fail(writer, MU_STATUS_BAD_ENCODINGERROR);
}

opcua_statuscode_t mu_binary_write_boolean(mu_binary_writer_t *writer, opcua_boolean_t value) {
    opcua_statuscode_t status = ensure_space(writer, 1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    writer->buffer[writer->position++] = value ? 1 : 0;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_sbyte(mu_binary_writer_t *writer, opcua_sbyte_t value) {
    opcua_statuscode_t status = ensure_space(writer, 1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    writer->buffer[writer->position++] = (opcua_byte_t)value;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_byte(mu_binary_writer_t *writer, opcua_byte_t value) {
    opcua_statuscode_t status = ensure_space(writer, 1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    writer->buffer[writer->position++] = value;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_int16(mu_binary_writer_t *writer, opcua_int16_t value) {
    return mu_binary_write_uint16(writer, (opcua_uint16_t)value);
}

opcua_statuscode_t mu_binary_write_uint16(mu_binary_writer_t *writer, opcua_uint16_t value) {
    opcua_statuscode_t status = ensure_space(writer, 2);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    mu_binary_le_put_u16(&writer->buffer[writer->position], value);
    writer->position += 2;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_int32(mu_binary_writer_t *writer, opcua_int32_t value) {
    return mu_binary_write_uint32(writer, (opcua_uint32_t)value);
}

opcua_statuscode_t mu_binary_write_uint32(mu_binary_writer_t *writer, opcua_uint32_t value) {
    opcua_statuscode_t status = ensure_space(writer, 4);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    mu_binary_le_put_u32(&writer->buffer[writer->position], value);
    writer->position += 4;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_int64(mu_binary_writer_t *writer, opcua_int64_t value) {
    return mu_binary_write_uint64(writer, (opcua_uint64_t)value);
}

opcua_statuscode_t mu_binary_write_uint64(mu_binary_writer_t *writer, opcua_uint64_t value) {
    opcua_statuscode_t status = ensure_space(writer, 8);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    mu_binary_le_put_u64(&writer->buffer[writer->position], value);
    writer->position += 8;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_float(mu_binary_writer_t *writer, opcua_float_t value) {
    opcua_uint32_t tmp;
    (void)memcpy(&tmp, &value, sizeof(opcua_float_t));
    return mu_binary_write_uint32(writer, tmp);
}

opcua_statuscode_t mu_binary_write_double(mu_binary_writer_t *writer, opcua_double_t value) {
    opcua_uint64_t tmp;
    (void)memcpy(&tmp, &value, sizeof(opcua_double_t));
    return mu_binary_write_uint64(writer, tmp);
}

opcua_statuscode_t mu_binary_write_statuscode(mu_binary_writer_t *writer, opcua_statuscode_t value) {
    return mu_binary_write_uint32(writer, value);
}

#if MUC_OPCUA_DATA_ACCESS
opcua_statuscode_t mu_binary_write_range(mu_binary_writer_t *writer, const mu_range_t *value) {
    if (!writer || !value)
        return MU_STATUS_BAD_INTERNALERROR;
    opcua_statuscode_t s = mu_binary_write_double(writer, value->low);
    if (s != MU_STATUS_GOOD)
        return s;
    return mu_binary_write_double(writer, value->high);
}

opcua_statuscode_t mu_binary_write_localized_text(mu_binary_writer_t *writer, const mu_localized_text_t *value) {
    if (!writer || !value)
        return MU_STATUS_BAD_INTERNALERROR;
    opcua_byte_t mask = 0;
    if (value->locale.length >= 0)
        mask |= 0x01u;
    if (value->text.length >= 0)
        mask |= 0x02u;
    opcua_statuscode_t s = mu_binary_write_byte(writer, mask);
    if (s != MU_STATUS_GOOD)
        return s;
    if (mask & 0x01u) {
        s = mu_binary_write_string(writer, &value->locale);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    if (mask & 0x02u) {
        s = mu_binary_write_string(writer, &value->text);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_eu_information(mu_binary_writer_t *writer, const mu_eu_information_t *value) {
    if (!writer || !value)
        return MU_STATUS_BAD_INTERNALERROR;
    opcua_statuscode_t s = mu_binary_write_string(writer, &value->namespace_uri);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_int32(writer, value->unit_id);
    if (s != MU_STATUS_GOOD)
        return s;
    s = mu_binary_write_localized_text(writer, &value->display_name);
    if (s != MU_STATUS_GOOD)
        return s;
    return mu_binary_write_localized_text(writer, &value->description);
}
#endif

#if MUC_OPCUA_METHOD_SERVER
opcua_statuscode_t mu_binary_write_argument(mu_binary_writer_t *writer, const mu_argument_t *arg) {
    if (!writer || !arg) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    /* Encode the Argument body (OPC-10000-5 §12.2.12.1) into scratch, then wrap
       it in an ExtensionObject with the exact body length. Argument_Encoding_
       DefaultBinary = ns0 i=298 (296 DataType + 2, the standard triple). */
    opcua_byte_t body[256];
    mu_binary_writer_t bw;
    mu_binary_writer_init(&bw, body, sizeof(body));
    (void)mu_binary_write_string(&bw, &arg->name);
    (void)mu_binary_write_nodeid(&bw, &arg->data_type);
    (void)mu_binary_write_int32(&bw, arg->value_rank);
    (void)mu_binary_write_int32(&bw, -1); /* arrayDimensions: null (not modelled) */
    (void)mu_binary_write_localized_text(&bw, &arg->description);
    if (bw.status != MU_STATUS_GOOD) {
        return bw.status;
    }

    mu_nodeid_t type_id = {0, MU_NODEID_NUMERIC, {298u}};
    opcua_statuscode_t s = mu_binary_write_extension_object_header(writer, &type_id, bw.position);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (writer->position + bw.position > writer->length) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    (void)memcpy(writer->buffer + writer->position, body, bw.position);
    writer->position += bw.position;
    return MU_STATUS_GOOD;
}
#endif
