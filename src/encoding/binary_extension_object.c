/* src/encoding/binary_extension_object.c */
#include "muc_opcua/encoding.h"

typedef opcua_statuscode_t (*mu_extension_object_body_decoder_t)(mu_binary_reader_t *body_reader, void *context);

opcua_statuscode_t mu_binary_read_extension_object_body(mu_binary_reader_t *reader, size_t length,
                                                        mu_extension_object_body_decoder_t decoder, void *context);

opcua_statuscode_t mu_binary_read_extension_object_header(mu_binary_reader_t *reader, mu_nodeid_t *type_id,
                                                          size_t *length) {
    opcua_statuscode_t status = mu_binary_read_nodeid(reader, type_id);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    opcua_byte_t encoding_mask;
    status = mu_binary_read_byte(reader, &encoding_mask);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (encoding_mask == 0x00) {
        *length = 0;
    } else if (encoding_mask == 0x01 || encoding_mask == 0x02) {
        opcua_int32_t len;
        status = mu_binary_read_int32(reader, &len);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        if (len < 0) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        *length = (size_t)len;
    } else {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_extension_object_header(mu_binary_writer_t *writer, const mu_nodeid_t *type_id,
                                                           size_t length) {
    opcua_statuscode_t status = mu_binary_write_nodeid(writer, type_id);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (length == 0) {
        status = mu_binary_write_byte(writer, 0x00);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    } else {
        /* Always write ByteString mask */
        status = mu_binary_write_byte(writer, 0x01);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        status = mu_binary_write_int32(writer, (opcua_int32_t)length);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_extension_object_body(mu_binary_reader_t *reader, size_t length,
                                                        mu_extension_object_body_decoder_t decoder, void *context) {
    if (!reader || !decoder) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (reader->status != MU_STATUS_GOOD) {
        return reader->status;
    }
    if (!reader->buffer) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* OPC-10000-6 section 5.2.2.15: ByteString/XML bodies are bounded by their encoded byte count. */
    if (reader->position > reader->length || length > (reader->length - reader->position)) {
        reader->status = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }

    mu_binary_reader_t body_reader;
    mu_binary_reader_init(&body_reader, reader->buffer + reader->position, length);

    opcua_statuscode_t status = decoder(&body_reader, context);
    if (status != MU_STATUS_GOOD) {
        reader->status = status;
        return status;
    }
    if (body_reader.status != MU_STATUS_GOOD) {
        reader->status = body_reader.status;
        return body_reader.status;
    }
    if (body_reader.position != length) {
        reader->status = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }

    reader->position += length;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_skip_extension_object(mu_binary_reader_t *reader) {
    mu_nodeid_t type_id;
    size_t length;
    opcua_statuscode_t status = mu_binary_read_extension_object_header(reader, &type_id, &length);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    /* OPC-10000-6 section 5.2.2.15: skip stays inside the declared ByteString/XML body. */
    if (reader->position > reader->length || length > (reader->length - reader->position)) {
        reader->status = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }

    reader->position += length;
    return MU_STATUS_GOOD;
}
