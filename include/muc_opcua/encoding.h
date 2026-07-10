/* include/muc_opcua/encoding.h */
/*
 * Spec grounding:
 *   OPC-10000-6 §5 OPC UA Binary Encoding
 *   OPC-10000-3 §5 Address Space types
 */
#ifndef MUC_OPCUA_ENCODING_H
#define MUC_OPCUA_ENCODING_H

#include "muc_opcua/config.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"
#include <stddef.h>

typedef struct {
    const opcua_byte_t *buffer;
    size_t length;
    size_t position;
    /* Sticky first failure for chained binary decoder calls. */
    opcua_statuscode_t status;
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
    /* Sticky first failure for chained binary encoder calls. */
    opcua_statuscode_t status;
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

opcua_statuscode_t mu_binary_read_string(mu_binary_reader_t *reader, mu_string_t *value);
opcua_statuscode_t mu_binary_write_string(mu_binary_writer_t *writer, const mu_string_t *value);
opcua_statuscode_t mu_binary_read_bytestring(mu_binary_reader_t *reader, mu_bytestring_t *value);
opcua_statuscode_t mu_binary_write_bytestring(mu_binary_writer_t *writer, const mu_bytestring_t *value);

opcua_statuscode_t mu_binary_read_nodeid(mu_binary_reader_t *reader, mu_nodeid_t *value);
opcua_statuscode_t mu_binary_write_nodeid(mu_binary_writer_t *writer, const mu_nodeid_t *value);

opcua_statuscode_t mu_binary_read_expanded_nodeid(mu_binary_reader_t *reader, mu_expanded_nodeid_t *value);
opcua_statuscode_t mu_binary_write_expanded_nodeid(mu_binary_writer_t *writer, const mu_expanded_nodeid_t *value);

opcua_statuscode_t mu_binary_read_qualified_name(mu_binary_reader_t *reader, mu_qualified_name_t *value);
opcua_statuscode_t mu_binary_write_qualified_name(mu_binary_writer_t *writer, const mu_qualified_name_t *value);

opcua_statuscode_t mu_binary_read_extension_object_header(mu_binary_reader_t *reader, mu_nodeid_t *type_id,
                                                          size_t *length);
opcua_statuscode_t mu_binary_write_extension_object_header(mu_binary_writer_t *writer, const mu_nodeid_t *type_id,
                                                           size_t length);
opcua_statuscode_t mu_binary_skip_extension_object(mu_binary_reader_t *reader);

opcua_statuscode_t mu_binary_read_variant(mu_binary_reader_t *reader, mu_variant_t *value);
opcua_statuscode_t mu_binary_write_variant(mu_binary_writer_t *writer, const mu_variant_t *value);

opcua_statuscode_t mu_binary_read_datavalue(mu_binary_reader_t *reader, mu_datavalue_t *value);
opcua_statuscode_t mu_binary_write_datavalue(mu_binary_writer_t *writer, const mu_datavalue_t *value);

opcua_statuscode_t mu_binary_read_username_identity_token(mu_binary_reader_t *reader,
                                                          mu_username_identity_token_t *value);
opcua_statuscode_t mu_binary_read_certificate_identity_token(mu_binary_reader_t *reader,
                                                             mu_certificate_identity_token_t *value);

#if MUC_OPCUA_DATA_ACCESS
opcua_statuscode_t mu_binary_read_range(mu_binary_reader_t *reader, mu_range_t *value);
opcua_statuscode_t mu_binary_write_range(mu_binary_writer_t *writer, const mu_range_t *value);
/* LocalizedText (OPC-10000-6 §5.2.2.14): EncodingMask byte (bit0 Locale, bit1
   Text present), then each present field as a String. A field with length < 0
   (null) is absent (its mask bit is clear). Currently used by EUInformation and
   the Data Access discrete types; gated with DATA_ACCESS until another feature
   needs it. */
opcua_statuscode_t mu_binary_read_localized_text(mu_binary_reader_t *reader, mu_localized_text_t *value);
opcua_statuscode_t mu_binary_write_localized_text(mu_binary_writer_t *writer, const mu_localized_text_t *value);
/* EUInformation (OPC-10000-8 §5.6.4.3): namespaceUri String, unitId Int32,
   displayName LocalizedText, description LocalizedText. */
opcua_statuscode_t mu_binary_read_eu_information(mu_binary_reader_t *reader, mu_eu_information_t *value);
opcua_statuscode_t mu_binary_write_eu_information(mu_binary_writer_t *writer, const mu_eu_information_t *value);
#endif

#if MUC_OPCUA_COMPLEX_TYPES
#include "muc_opcua/address_space/complex_types.h"

opcua_statuscode_t mu_binary_encode_struct(mu_binary_writer_t *writer, const mu_structure_definition_t *def,
                                           const void *field_values);
opcua_statuscode_t mu_binary_decode_struct(mu_binary_reader_t *reader, const mu_structure_definition_t *def,
                                           void *field_values);
opcua_statuscode_t mu_binary_encode_enum(mu_binary_writer_t *writer, opcua_int32_t value);
opcua_statuscode_t mu_binary_decode_enum(mu_binary_reader_t *reader, opcua_int32_t *value);
#endif

#endif /* MUC_OPCUA_ENCODING_H */
