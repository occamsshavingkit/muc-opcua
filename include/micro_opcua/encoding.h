/* include/micro_opcua/encoding.h */
#ifndef MICRO_OPCUA_ENCODING_H
#define MICRO_OPCUA_ENCODING_H

#include "micro_opcua/config.h"
#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include <stddef.h>

typedef struct {
    const opcua_byte_t *buffer;
    size_t length;
    size_t position;
} mu_binary_reader_t;

void mu_binary_reader_init(mu_binary_reader_t *reader, const opcua_byte_t *buffer, size_t length);
opcua_statuscode_t mu_binary_read_boolean(mu_binary_reader_t *reader, opcua_boolean_t *value);
opcua_statuscode_t mu_binary_read_sbyte(mu_binary_reader_t *reader, opcua_sbyte_t *value);
opcua_statuscode_t mu_binary_read_byte(mu_binary_reader_t *reader, opcua_byte_t *value);
opcua_statuscode_t mu_binary_read_int16(mu_binary_reader_t *reader, opcua_int16_t *value);
opcua_statuscode_t mu_binary_read_uint16(mu_binary_reader_t *reader, opcua_uint16_t *value);
opcua_statuscode_t mu_binary_read_int32(mu_binary_reader_t *reader, opcua_int32_t *value);
opcua_statuscode_t mu_binary_read_uint32(mu_binary_reader_t *reader, opcua_uint32_t *value);
opcua_statuscode_t mu_binary_read_int64(mu_binary_reader_t *reader, opcua_int64_t *value);
opcua_statuscode_t mu_binary_read_uint64(mu_binary_reader_t *reader, opcua_uint64_t *value);
opcua_statuscode_t mu_binary_read_float(mu_binary_reader_t *reader, opcua_float_t *value);
opcua_statuscode_t mu_binary_read_double(mu_binary_reader_t *reader, opcua_double_t *value);
opcua_statuscode_t mu_binary_read_statuscode(mu_binary_reader_t *reader, opcua_statuscode_t *value);

typedef struct {
    opcua_byte_t *buffer;
    size_t length;
    size_t position;
} mu_binary_writer_t;

void mu_binary_writer_init(mu_binary_writer_t *writer, opcua_byte_t *buffer, size_t length);
opcua_statuscode_t mu_binary_write_boolean(mu_binary_writer_t *writer, opcua_boolean_t value);
opcua_statuscode_t mu_binary_write_sbyte(mu_binary_writer_t *writer, opcua_sbyte_t value);
opcua_statuscode_t mu_binary_write_byte(mu_binary_writer_t *writer, opcua_byte_t value);
opcua_statuscode_t mu_binary_write_int16(mu_binary_writer_t *writer, opcua_int16_t value);
opcua_statuscode_t mu_binary_write_uint16(mu_binary_writer_t *writer, opcua_uint16_t value);
opcua_statuscode_t mu_binary_write_int32(mu_binary_writer_t *writer, opcua_int32_t value);
opcua_statuscode_t mu_binary_write_uint32(mu_binary_writer_t *writer, opcua_uint32_t value);
opcua_statuscode_t mu_binary_write_int64(mu_binary_writer_t *writer, opcua_int64_t value);
opcua_statuscode_t mu_binary_write_uint64(mu_binary_writer_t *writer, opcua_uint64_t value);
opcua_statuscode_t mu_binary_write_float(mu_binary_writer_t *writer, opcua_float_t value);
opcua_statuscode_t mu_binary_write_double(mu_binary_writer_t *writer, opcua_double_t value);
opcua_statuscode_t mu_binary_write_statuscode(mu_binary_writer_t *writer, opcua_statuscode_t value);

#endif /* MICRO_OPCUA_ENCODING_H */
