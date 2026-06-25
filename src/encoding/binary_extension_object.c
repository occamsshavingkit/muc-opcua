/* src/encoding/binary_extension_object.c */
#include "micro_opcua/encoding.h"

opcua_statuscode_t mu_binary_read_extension_object_header(mu_binary_reader_t *reader, mu_nodeid_t *type_id, size_t *length) {
    opcua_statuscode_t status = mu_binary_read_nodeid(reader, type_id);
    if (status != MU_STATUS_GOOD) return status;
    
    opcua_byte_t encoding_mask;
    status = mu_binary_read_byte(reader, &encoding_mask);
    if (status != MU_STATUS_GOOD) return status;
    
    if (encoding_mask == 0x00) {
        *length = 0;
    } else if (encoding_mask == 0x01 || encoding_mask == 0x02) {
        opcua_int32_t len;
        status = mu_binary_read_int32(reader, &len);
        if (status != MU_STATUS_GOOD) return status;
        if (len < 0) return MU_STATUS_BAD_DECODINGERROR;
        *length = (size_t)len;
    } else {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_write_extension_object_header(mu_binary_writer_t *writer, const mu_nodeid_t *type_id, size_t length) {
    opcua_statuscode_t status = mu_binary_write_nodeid(writer, type_id);
    if (status != MU_STATUS_GOOD) return status;
    
    if (length == 0) {
        status = mu_binary_write_byte(writer, 0x00);
        if (status != MU_STATUS_GOOD) return status;
    } else {
        /* Always write ByteString mask */
        status = mu_binary_write_byte(writer, 0x01);
        if (status != MU_STATUS_GOOD) return status;
        status = mu_binary_write_int32(writer, (opcua_int32_t)length);
        if (status != MU_STATUS_GOOD) return status;
    }
    
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_binary_skip_extension_object(mu_binary_reader_t *reader) {
    mu_nodeid_t type_id;
    size_t length;
    opcua_statuscode_t status = mu_binary_read_extension_object_header(reader, &type_id, &length);
    if (status != MU_STATUS_GOOD) return status;
    
    if (length > 0) {
        if (reader->position + length > reader->length) return MU_STATUS_BAD_DECODINGERROR;
        reader->position += length;
    }
    return MU_STATUS_GOOD;
}
