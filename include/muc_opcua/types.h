/* include/muc_opcua/types.h */
#ifndef MUC_OPCUA_TYPES_H
#define MUC_OPCUA_TYPES_H

#include "muc_opcua/config.h"
#include "muc_opcua/opcua_types.h"

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
typedef enum { MU_NODEID_NUMERIC = 0, MU_NODEID_STRING = 1, MU_NODEID_GUID = 2, MU_NODEID_OPAQUE = 3 } mu_nodeid_type_t;

/* OPC UA NodeId (Numeric and String supported for Nano profile) */
typedef struct {
    opcua_uint16_t namespace_index;
    mu_nodeid_type_t identifier_type;
    union {
        opcua_uint32_t numeric;
        mu_string_t string;
        opcua_byte_t guid[16];
        struct {
            opcua_byte_t data[16];
            opcua_uint16_t length;
        } opaque;
    } identifier;
} mu_nodeid_t;

/* OPC UA ExpandedNodeId (OPC 10000-6 5.2.2.10) */
typedef struct {
    mu_nodeid_t node_id;
    mu_string_t namespace_uri;
    opcua_uint32_t server_index;
} mu_expanded_nodeid_t;

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
    MU_TYPE_NULL = 0,
    MU_TYPE_BOOLEAN = 1,
    MU_TYPE_SBYTE = 2,
    MU_TYPE_BYTE = 3,
    MU_TYPE_INT16 = 4,
    MU_TYPE_UINT16 = 5,
    MU_TYPE_INT32 = 6,
    MU_TYPE_UINT32 = 7,
    MU_TYPE_INT64 = 8,
    MU_TYPE_UINT64 = 9,
    MU_TYPE_FLOAT = 10,
    MU_TYPE_DOUBLE = 11,
    MU_TYPE_STRING = 12,
    MU_TYPE_DATETIME = 13,
    MU_TYPE_GUID = 14,
    MU_TYPE_BYTESTRING = 15,
    MU_TYPE_XMLELEMENT = 16,
    MU_TYPE_NODEID = 17,
    MU_TYPE_EXPANDEDNODEID = 18,
    MU_TYPE_STATUSCODE = 19,
    MU_TYPE_QUALIFIEDNAME = 20,
    MU_TYPE_LOCALIZEDTEXT = 21,
    MU_TYPE_EXTENSIONOBJECT = 22,
    MU_TYPE_DATAVALUE = 23,
    MU_TYPE_VARIANT = 24,
    MU_TYPE_DIAGNOSTICINFO = 25
} mu_builtin_type_t;

#if MUC_OPCUA_DATA_ACCESS
typedef struct {
    opcua_double_t low;
    opcua_double_t high;
} mu_range_t;

/* EUInformation (OPC-10000-8 §5.6.4.3, DataType 887). displayName/description are
   LocalizedText per the spec (not plain String). */
typedef struct {
    mu_string_t namespace_uri;
    opcua_int32_t unit_id;
    mu_localized_text_t display_name;
    mu_localized_text_t description;
} mu_eu_information_t;
#endif /* MUC_OPCUA_DATA_ACCESS */

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
        const void *array; /* element array when is_array (points to type[array_length]) */
    } value;
    /* 1-D array support. These follow the union so existing scalar initializers
       ({ type, { .scalar } }) keep working (is_array defaults to false). */
    opcua_boolean_t is_array;
    opcua_int32_t array_length; /* element count when is_array (>= 0; -1 = null array) */
    /* for scalar ExtensionObject Values: the DataType's DefaultBinary Encoding NodeId
       (ns0), so the encoder can dispatch to the right struct writer. 0 = not set. */
    opcua_uint32_t ext_encoding_id;
} mu_variant_t;

/* Variant DataType assignability check (OPC-10000-4 5.11.4.2 Table 53).
   Returns true if `actual` is the same as, or a subtype of, `expected`. The
   simple built-in types (Boolean, SByte, Byte, ..., NodeId) have no subtypes
   in the address space, so for those this reduces to exact equality today.
   The infrastructure (switch on `expected`) is in place so that custom
   DataTypes defined as subtypes of a base type can be accepted here without
   touching the call sites. */
opcua_boolean_t mu_variant_type_is_assignable(mu_builtin_type_t expected, mu_builtin_type_t actual);

/* DataValue */
typedef struct {
    mu_variant_t value;
    opcua_statuscode_t status;
    opcua_datetime_t source_timestamp;
    opcua_uint16_t source_picoseconds;
    opcua_datetime_t server_timestamp;
    opcua_uint16_t server_picoseconds;
    opcua_boolean_t has_value;
    opcua_boolean_t has_status;
    opcua_boolean_t has_source_timestamp;
    opcua_boolean_t has_source_picoseconds;
    opcua_boolean_t has_server_timestamp;
    opcua_boolean_t has_server_picoseconds;
} mu_datavalue_t;

#ifdef MUC_OPCUA_SERVICE_WRITE
/* WriteValue (OPC 10000-4 §5.11.4.2) */
typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t attribute_id;
    mu_string_t index_range;
    mu_datavalue_t value;
} mu_write_value_t;
#endif

/* Size report structure */
typedef struct {
    size_t config_bytes;
    size_t connection_bytes;
    size_t channel_bytes;
    size_t session_bytes;
    size_t total_allocated;
} mu_size_report_t;

/* UserNameIdentityToken and AnonymousIdentityToken representations */
typedef struct {
    mu_string_t policy_id;
    mu_string_t username;
    mu_bytestring_t password;
    mu_string_t encryption_algorithm;
} mu_username_identity_token_t;

typedef struct {
    mu_string_t policy_id;
} mu_anonymous_identity_token_t;

typedef struct {
    mu_string_t policy_id;
    mu_bytestring_t certificate_data;
} mu_certificate_identity_token_t;

#ifdef MUC_OPCUA_EVENTS
#if MUC_OPCUA_CU_AUDITING
/* Reference into the server's shared audit-payload pool (spec 074): the queued
 * event carries only this (a few bytes), not the full audit fields. `valid` is
 * false for non-audit events; the resolver checks the pool slot's sequence
 * against `sequence` and resolves audit fields to Null on a ring-wrap miss. */
typedef struct {
    opcua_boolean_t valid;
    opcua_byte_t index;
    opcua_uint32_t sequence;
} mu_audit_ref_t;
#endif

typedef struct {
    mu_nodeid_t event_type;
    mu_bytestring_t event_id;
    opcua_datetime_t time;
    mu_string_t message;
    opcua_uint16_t severity;
#if MUC_OPCUA_CU_AUDITING
    mu_audit_ref_t audit_ref;
#endif
} mu_event_notification_t;

typedef struct {
    mu_bytestring_t event_id;
    mu_nodeid_t event_type;
    mu_nodeid_t source_node;
    mu_string_t source_name;
    opcua_datetime_t time;
    opcua_datetime_t receive_time;
    struct {
        opcua_int16_t offset;
        opcua_boolean_t daylight_saving_in_offset;
    } local_time;
    mu_string_t message;
    opcua_uint16_t severity;
} mu_event_fields_t;

/* The EventFilter WhereClause (ContentFilter) model and evaluator live in the
   internal header src/services/event_filter.h (mu_where_clause_t /
   mu_where_clause_eval); mu_event_fields_t above is the value carrier they read. */
#endif

#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY
/* AccessLevelEx bitmask flags (OPC-10000-7 CU 2809:Address Space Atomicity).
   Set these bits in a Variable's AccessLevelEx Attribute when Read or Write
   operations on that Variable cannot be guaranteed atomic. When the bit is '1',
   atomicity is not assured. */
#define MU_ACCESS_LEVEL_EX_NONATOMIC_READ (1u << 8)
#define MU_ACCESS_LEVEL_EX_NONATOMIC_WRITE (1u << 9)
#endif /* MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY */

#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_FULL_ARRAY_ONLY
/* AccessLevelEx bitmask flag (OPC-10000-7 CU 2820:Address Space Full Array
   Only). Set this bit in a Variable's AccessLevelEx Attribute to indicate that
   write operations for an array can only be performed on the full array (i.e.
   IndexRange partial writes are not permitted). */
#define MU_ACCESS_LEVEL_EX_WRITE_FULL_ARRAY_ONLY (1u << 10)
#endif /* MUC_OPCUA_CU_ADDRESS_SPACE_FULL_ARRAY_ONLY */

#ifdef MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT
/* AccessLevelEx bitmask flags (OPC-10000-7 CU 4237:Address Space NonVolatile
   and Constant). Set these bits in a Variable's AccessLevelEx Attribute to
   indicate persisted values and configuration-version-relevant constants. */
#define MU_ACCESS_LEVEL_EX_NON_VOLATILE (1u << 12)
#define MU_ACCESS_LEVEL_EX_CONSTANT (1u << 13)
#endif /* MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT */

#endif /* MUC_OPCUA_TYPES_H */
