/* src/encoding/binary_writer.c */
#include "micro_opcua/encoding.h"
#include <string.h>

void mu_binary_writer_init(mu_binary_writer_t *writer, opcua_byte_t *buffer, size_t length) {
    if (writer) {
        writer->buffer = buffer;
        writer->length = length;
        writer->position = 0;
    }
}

static opcua_statuscode_t ensure_space(mu_binary_writer_t *writer, size_t count) {
    if (!writer || !writer->buffer) return MU_STATUS_BAD_INTERNALERROR;
    if (writer->position + count > writer->length) return MU_STATUS_BAD_ENCODINGERROR;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_boolean(mu_binary_writer_t *writer, opcua_boolean_t value) {
    opcua_statuscode_t status = ensure_space(writer, 1);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = value ? 1 : 0;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_sbyte(mu_binary_writer_t *writer, opcua_sbyte_t value) {
    opcua_statuscode_t status = ensure_space(writer, 1);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = (opcua_byte_t)value;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_byte(mu_binary_writer_t *writer, opcua_byte_t value) {
    opcua_statuscode_t status = ensure_space(writer, 1);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = value;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_int16(mu_binary_writer_t *writer, opcua_int16_t value) {
    opcua_statuscode_t status = ensure_space(writer, 2);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = (opcua_byte_t)(value & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 8) & 0xFF);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_uint16(mu_binary_writer_t *writer, opcua_uint16_t value) {
    opcua_statuscode_t status = ensure_space(writer, 2);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = (opcua_byte_t)(value & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 8) & 0xFF);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_int32(mu_binary_writer_t *writer, opcua_int32_t value) {
    opcua_statuscode_t status = ensure_space(writer, 4);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = (opcua_byte_t)(value & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 24) & 0xFF);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_uint32(mu_binary_writer_t *writer, opcua_uint32_t value) {
    opcua_statuscode_t status = ensure_space(writer, 4);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = (opcua_byte_t)(value & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 24) & 0xFF);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_int64(mu_binary_writer_t *writer, opcua_int64_t value) {
    opcua_statuscode_t status = ensure_space(writer, 8);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = (opcua_byte_t)(value & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 24) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 32) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 40) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 48) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 56) & 0xFF);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_uint64(mu_binary_writer_t *writer, opcua_uint64_t value) {
    opcua_statuscode_t status = ensure_space(writer, 8);
    if (status != MU_STATUS_GOOD) return status;
    writer->buffer[writer->position++] = (opcua_byte_t)(value & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 8) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 16) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 24) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 32) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 40) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 48) & 0xFF);
    writer->buffer[writer->position++] = (opcua_byte_t)((value >> 56) & 0xFF);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_float(mu_binary_writer_t *writer, opcua_float_t value) {
    opcua_uint32_t tmp;
    memcpy(&tmp, &value, sizeof(opcua_float_t));
    return mu_binary_write_uint32(writer, tmp);
}

opcua_statuscode_t mu_binary_write_double(mu_binary_writer_t *writer, opcua_double_t value) {
    opcua_uint64_t tmp;
    memcpy(&tmp, &value, sizeof(opcua_double_t));
    return mu_binary_write_uint64(writer, tmp);
}

opcua_statuscode_t mu_binary_write_statuscode(mu_binary_writer_t *writer, opcua_statuscode_t value) {
    return mu_binary_write_uint32(writer, value);
}
