/* src/core/message_chunk.c */
#include "message_chunk.h"
#include "micro_opcua/encoding.h"

opcua_statuscode_t mu_parse_message_header(const opcua_byte_t *buffer, size_t length, mu_message_header_t *header) {
    if (!buffer || !header) return MU_STATUS_BAD_INTERNALERROR;
    if (length < 12) return MU_STATUS_BAD_TCPMESSAGETOOLARGE;

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, length);

    header->message_type[0] = buffer[0];
    header->message_type[1] = buffer[1];
    header->message_type[2] = buffer[2];
    header->chunk_type = buffer[3];
    
    reader.position = 4;
    opcua_statuscode_t status = mu_binary_read_uint32(&reader, &header->message_size);
    if (status != MU_STATUS_GOOD) return status;
    
    if (header->message_size > length) return MU_STATUS_BAD_TCPMESSAGETOOLARGE;
    
    status = mu_binary_read_uint32(&reader, &header->secure_channel_id);
    if (status != MU_STATUS_GOOD) return status;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_message_header(opcua_byte_t *buffer, size_t length, const mu_message_header_t *header) {
    if (!buffer || !header) return MU_STATUS_BAD_INTERNALERROR;
    if (length < 12) return MU_STATUS_BAD_INTERNALERROR;

    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, length);

    buffer[0] = header->message_type[0];
    buffer[1] = header->message_type[1];
    buffer[2] = header->message_type[2];
    buffer[3] = header->chunk_type;
    writer.position = 4;
    
    opcua_statuscode_t status = mu_binary_write_uint32(&writer, header->message_size);
    if (status != MU_STATUS_GOOD) return status;
    
    status = mu_binary_write_uint32(&writer, header->secure_channel_id);
    if (status != MU_STATUS_GOOD) return status;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_parse_sequence_header(const opcua_byte_t *buffer, size_t length, size_t *offset, mu_sequence_header_t *header) {
    if (!buffer || !offset || !header) return MU_STATUS_BAD_INTERNALERROR;
    
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, length);
    reader.position = *offset;

    opcua_statuscode_t status = mu_binary_read_uint32(&reader, &header->sequence_number);
    if (status != MU_STATUS_GOOD) return status;
    
    status = mu_binary_read_uint32(&reader, &header->request_id);
    if (status != MU_STATUS_GOOD) return status;

    *offset = reader.position;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_sequence_header(opcua_byte_t *buffer, size_t length, size_t *offset, const mu_sequence_header_t *header) {
    if (!buffer || !offset || !header) return MU_STATUS_BAD_INTERNALERROR;
    
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, length);
    writer.position = *offset;

    opcua_statuscode_t status = mu_binary_write_uint32(&writer, header->sequence_number);
    if (status != MU_STATUS_GOOD) return status;
    
    status = mu_binary_write_uint32(&writer, header->request_id);
    if (status != MU_STATUS_GOOD) return status;

    *offset = writer.position;
    return MU_STATUS_GOOD;
}
