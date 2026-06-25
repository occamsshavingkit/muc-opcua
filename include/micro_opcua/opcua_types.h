/* include/micro_opcua/opcua_types.h */
#ifndef OPCUA_TYPES_H
#define OPCUA_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Basic OPC UA Built-in Types (OPC 10000-6, 5.2.1) */

typedef bool opcua_boolean_t;
typedef int8_t opcua_sbyte_t;
typedef uint8_t opcua_byte_t;
typedef int16_t opcua_int16_t;
typedef uint16_t opcua_uint16_t;
typedef int32_t opcua_int32_t;
typedef uint32_t opcua_uint32_t;
typedef int64_t opcua_int64_t;
typedef uint64_t opcua_uint64_t;
typedef float opcua_float_t;
typedef double opcua_double_t;

/* DateTime is a 64-bit signed int (100-ns intervals since Jan 1, 1601 UTC) */
typedef int64_t opcua_datetime_t;

/* StatusCode is a 32-bit unsigned int */
typedef uint32_t opcua_statuscode_t;

#endif /* OPCUA_TYPES_H */
