/* src/encoding/binary_string.c */
#include "muc_opcua/encoding.h"
#include <string.h>

static opcua_statuscode_t string_reader_fail(mu_binary_reader_t *reader, opcua_statuscode_t status) {
    if (reader) {
        reader->status = status;
    }
    return status;
}

static opcua_statuscode_t ensure_string_payload(mu_binary_reader_t *reader, opcua_int32_t length) {
    if (length <= 0) {
        return MU_STATUS_GOOD;
    }

    /* OPC-10000-6 section 5.2: declared String/ByteString bytes must fit in the remaining reader span. */
    if (!reader || reader->position > reader->length) {
        return string_reader_fail(reader, MU_STATUS_BAD_DECODINGERROR);
    }
    if ((size_t)length > reader->length - reader->position) {
        return string_reader_fail(reader, MU_STATUS_BAD_DECODINGERROR);
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_string(mu_binary_reader_t *reader, mu_string_t *value) {
    opcua_statuscode_t status = mu_binary_read_int32(reader, &value->length);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (value->length == -1) {
        value->data = NULL;
        return MU_STATUS_GOOD;
    }

    if (value->length < 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    if (value->length > MU_MAX_ENCODED_STRING_LENGTH) {
        return MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED;
    }

    status = ensure_string_payload(reader, value->length);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    value->data = reader->buffer + reader->position;
    reader->position += (size_t)value->length;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_string(mu_binary_writer_t *writer, const mu_string_t *value) {
    if (!value || value->length == -1) {
        return mu_binary_write_int32(writer, -1);
    }

    if (value->length < 0) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }

    if (value->length > MU_MAX_ENCODED_STRING_LENGTH) {
        return MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED;
    }

    opcua_statuscode_t status = mu_binary_write_int32(writer, value->length);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (value->length > 0) {
        if (writer->position + (size_t)value->length > writer->length) {
            return MU_STATUS_BAD_ENCODINGERROR;
        }
        memcpy(writer->buffer + writer->position, value->data, (size_t)value->length);
        writer->position += (size_t)value->length;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_bytestring(mu_binary_reader_t *reader, mu_bytestring_t *value) {
    return mu_binary_read_string(reader, (mu_string_t *)value);
}

opcua_statuscode_t mu_binary_write_bytestring(mu_binary_writer_t *writer, const mu_bytestring_t *value) {
    return mu_binary_write_string(writer, (const mu_string_t *)value);
}
