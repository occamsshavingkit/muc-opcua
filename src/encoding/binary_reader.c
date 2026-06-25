/* src/encoding/binary_reader.c */
#include "micro_opcua/encoding.h"
#include <string.h>

void mu_binary_reader_init(mu_binary_reader_t *reader, const opcua_byte_t *buffer, size_t length) {
    if (reader) {
        reader->buffer = buffer;
        reader->length = length;
        reader->position = 0;
    }
}

static opcua_statuscode_t ensure_bytes(mu_binary_reader_t *reader, size_t count) {
    if (!reader || !reader->buffer) return MU_STATUS_BAD_INVALIDARGUMENT;
    if (reader->position + count > reader->length) return MU_STATUS_BAD_DECODINGERROR;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_boolean(mu_binary_reader_t *reader, opcua_boolean_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 1);
    if (status != MU_STATUS_GOOD) return status;
    *value = reader->buffer[reader->position++] ? true : false;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_sbyte(mu_binary_reader_t *reader, opcua_sbyte_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 1);
    if (status != MU_STATUS_GOOD) return status;
    *value = (opcua_sbyte_t)reader->buffer[reader->position++];
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_byte(mu_binary_reader_t *reader, opcua_byte_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 1);
    if (status != MU_STATUS_GOOD) return status;
    *value = reader->buffer[reader->position++];
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_int16(mu_binary_reader_t *reader, opcua_int16_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 2);
    if (status != MU_STATUS_GOOD) return status;
    *value = (opcua_int16_t)((opcua_uint16_t)reader->buffer[reader->position] |
                            ((opcua_uint16_t)reader->buffer[reader->position + 1] << 8));
    reader->position += 2;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_uint16(mu_binary_reader_t *reader, opcua_uint16_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 2);
    if (status != MU_STATUS_GOOD) return status;
    *value = (opcua_uint16_t)((opcua_uint16_t)reader->buffer[reader->position] |
                             ((opcua_uint16_t)reader->buffer[reader->position + 1] << 8));
    reader->position += 2;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_int32(mu_binary_reader_t *reader, opcua_int32_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 4);
    if (status != MU_STATUS_GOOD) return status;
    *value = (opcua_int32_t)((opcua_uint32_t)reader->buffer[reader->position] |
                            ((opcua_uint32_t)reader->buffer[reader->position + 1] << 8) |
                            ((opcua_uint32_t)reader->buffer[reader->position + 2] << 16) |
                            ((opcua_uint32_t)reader->buffer[reader->position + 3] << 24));
    reader->position += 4;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_uint32(mu_binary_reader_t *reader, opcua_uint32_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 4);
    if (status != MU_STATUS_GOOD) return status;
    *value = (opcua_uint32_t)reader->buffer[reader->position] |
            ((opcua_uint32_t)reader->buffer[reader->position + 1] << 8) |
            ((opcua_uint32_t)reader->buffer[reader->position + 2] << 16) |
            ((opcua_uint32_t)reader->buffer[reader->position + 3] << 24);
    reader->position += 4;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_int64(mu_binary_reader_t *reader, opcua_int64_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 8);
    if (status != MU_STATUS_GOOD) return status;
    *value = (opcua_int64_t)((opcua_uint64_t)reader->buffer[reader->position] |
                            ((opcua_uint64_t)reader->buffer[reader->position + 1] << 8) |
                            ((opcua_uint64_t)reader->buffer[reader->position + 2] << 16) |
                            ((opcua_uint64_t)reader->buffer[reader->position + 3] << 24) |
                            ((opcua_uint64_t)reader->buffer[reader->position + 4] << 32) |
                            ((opcua_uint64_t)reader->buffer[reader->position + 5] << 40) |
                            ((opcua_uint64_t)reader->buffer[reader->position + 6] << 48) |
                            ((opcua_uint64_t)reader->buffer[reader->position + 7] << 56));
    reader->position += 8;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_uint64(mu_binary_reader_t *reader, opcua_uint64_t *value) {
    opcua_statuscode_t status = ensure_bytes(reader, 8);
    if (status != MU_STATUS_GOOD) return status;
    *value = (opcua_uint64_t)reader->buffer[reader->position] |
            ((opcua_uint64_t)reader->buffer[reader->position + 1] << 8) |
            ((opcua_uint64_t)reader->buffer[reader->position + 2] << 16) |
            ((opcua_uint64_t)reader->buffer[reader->position + 3] << 24) |
            ((opcua_uint64_t)reader->buffer[reader->position + 4] << 32) |
            ((opcua_uint64_t)reader->buffer[reader->position + 5] << 40) |
            ((opcua_uint64_t)reader->buffer[reader->position + 6] << 48) |
            ((opcua_uint64_t)reader->buffer[reader->position + 7] << 56);
    reader->position += 8;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_float(mu_binary_reader_t *reader, opcua_float_t *value) {
    opcua_uint32_t tmp;
    opcua_statuscode_t status = mu_binary_read_uint32(reader, &tmp);
    if (status != MU_STATUS_GOOD) return status;
    memcpy(value, &tmp, sizeof(opcua_float_t));
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_double(mu_binary_reader_t *reader, opcua_double_t *value) {
    opcua_uint64_t tmp;
    opcua_statuscode_t status = mu_binary_read_uint64(reader, &tmp);
    if (status != MU_STATUS_GOOD) return status;
    memcpy(value, &tmp, sizeof(opcua_double_t));
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_read_statuscode(mu_binary_reader_t *reader, opcua_statuscode_t *value) {
    return mu_binary_read_uint32(reader, value);
}
