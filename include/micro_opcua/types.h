/* include/micro_opcua/types.h */
#ifndef MICRO_OPCUA_TYPES_H
#define MICRO_OPCUA_TYPES_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/config.h"

/* OPC UA ByteString and String (bounded by caller configuration or static max) */
typedef struct {
    opcua_int32_t length;
    const opcua_byte_t *data;
} mu_string_t;

typedef struct {
    opcua_int32_t length;
    const opcua_byte_t *data;
} mu_bytestring_t;

/* NodeId Type Enumeration */
typedef enum {
    MU_NODEID_NUMERIC = 0,
    MU_NODEID_STRING  = 1,
    MU_NODEID_GUID    = 2,
    MU_NODEID_OPAQUE  = 3
} mu_nodeid_type_t;

/* OPC UA NodeId (Numeric and String supported for Nano profile) */
typedef struct {
    opcua_uint16_t namespace_index;
    mu_nodeid_type_t identifier_type;
    union {
        opcua_uint32_t numeric;
        mu_string_t string;
    } identifier;
} mu_nodeid_t;

/* OPC UA QualifiedName (OPC 10000-6 5.2.2.13) */
typedef struct {
    opcua_uint16_t namespace_index;
    mu_string_t name;
} mu_qualified_name_t;

/* OPC UA LocalizedText (OPC 10000-6 5.2.2.14) */
typedef struct {
    mu_string_t locale;
    mu_string_t text;
} mu_localized_text_t;

/* Variant Type Enumeration (Built-in Types) */
typedef enum {
    MU_TYPE_NULL             = 0,
    MU_TYPE_BOOLEAN          = 1,
    MU_TYPE_SBYTE            = 2,
    MU_TYPE_BYTE             = 3,
    MU_TYPE_INT16            = 4,
    MU_TYPE_UINT16           = 5,
    MU_TYPE_INT32            = 6,
    MU_TYPE_UINT32           = 7,
    MU_TYPE_INT64            = 8,
    MU_TYPE_UINT64           = 9,
    MU_TYPE_FLOAT            = 10,
    MU_TYPE_DOUBLE           = 11,
    MU_TYPE_STRING           = 12,
    MU_TYPE_DATETIME         = 13,
    MU_TYPE_GUID             = 14,
    MU_TYPE_BYTESTRING       = 15,
    MU_TYPE_XMLELEMENT       = 16,
    MU_TYPE_NODEID           = 17,
    MU_TYPE_EXPANDEDNODEID   = 18,
    MU_TYPE_STATUSCODE       = 19,
    MU_TYPE_QUALIFIEDNAME    = 20,
    MU_TYPE_LOCALIZEDTEXT    = 21,
    MU_TYPE_EXTENSIONOBJECT  = 22,
    MU_TYPE_DATAVALUE        = 23,
    MU_TYPE_VARIANT          = 24,
    MU_TYPE_DIAGNOSTICINFO   = 25
} mu_builtin_type_t;

/* Variant */
typedef struct {
    mu_builtin_type_t type;
    union {
        opcua_boolean_t b;
        opcua_sbyte_t sb;
        opcua_byte_t by;
        opcua_int16_t i16;
        opcua_uint16_t ui16;
        opcua_int32_t i32;
        opcua_uint32_t ui32;
        opcua_int64_t i64;
        opcua_uint64_t ui64;
        opcua_float_t f;
        opcua_double_t d;
        mu_string_t str;
        mu_bytestring_t bytestr;
        opcua_datetime_t dt;
        opcua_statuscode_t status;
        mu_nodeid_t nodeid;
        mu_qualified_name_t qualified_name;
        mu_localized_text_t localized_text;
        const void *array;          /* element array when is_array (points to type[array_length]) */
    } value;
    /* 1-D array support. These follow the union so existing scalar initializers
       ({ type, { .scalar } }) keep working (is_array defaults to false). */
    opcua_boolean_t is_array;
    opcua_int32_t array_length;     /* element count when is_array (>= 0; -1 = null array) */
} mu_variant_t;

/* DataValue */
typedef struct {
    mu_variant_t value;
    opcua_statuscode_t status;
    opcua_datetime_t source_timestamp;
    opcua_datetime_t server_timestamp;
    opcua_boolean_t has_value;
    opcua_boolean_t has_status;
    opcua_boolean_t has_source_timestamp;
    opcua_boolean_t has_server_timestamp;
} mu_datavalue_t;

/* Size report structure */
typedef struct {
    size_t config_bytes;
    size_t connection_bytes;
    size_t channel_bytes;
    size_t session_bytes;
    size_t total_allocated;
} mu_size_report_t;

#endif /* MICRO_OPCUA_TYPES_H */
