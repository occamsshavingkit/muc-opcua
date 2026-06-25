/* src/core/service_message.c */
#include "service_message.h"
#include "micro_opcua/encoding.h"

opcua_statuscode_t mu_parse_service_prefix(const opcua_byte_t *buffer, size_t length, size_t *offset, mu_nodeid_t *node_id) {
    if (!buffer || !offset || !node_id) return MU_STATUS_BAD_INTERNALERROR;
    
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, length);
    reader.position = *offset;

    opcua_statuscode_t status = mu_binary_read_nodeid(&reader, node_id);
    if (status != MU_STATUS_GOOD) return status;

    *offset = reader.position;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_service_prefix(opcua_byte_t *buffer, size_t length, size_t *offset, const mu_nodeid_t *node_id) {
    if (!buffer || !offset || !node_id) return MU_STATUS_BAD_INTERNALERROR;
    
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, length);
    writer.position = *offset;

    opcua_statuscode_t status = mu_binary_write_nodeid(&writer, node_id);
    if (status != MU_STATUS_GOOD) return status;

    *offset = writer.position;
    return MU_STATUS_GOOD;
}
