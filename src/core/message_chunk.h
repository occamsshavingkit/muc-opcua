/* src/core/message_chunk.h */
#ifndef MICRO_OPCUA_MESSAGE_CHUNK_H
#define MICRO_OPCUA_MESSAGE_CHUNK_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include <stddef.h>

typedef struct {
    opcua_byte_t message_type[3];
    opcua_byte_t chunk_type;
    opcua_uint32_t message_size;
    opcua_uint32_t secure_channel_id;
} mu_message_header_t;

typedef struct {
    opcua_uint32_t sequence_number;
    opcua_uint32_t request_id;
} mu_sequence_header_t;

opcua_statuscode_t mu_parse_message_header(const opcua_byte_t *buffer, size_t length, mu_message_header_t *header);
opcua_statuscode_t mu_write_message_header(opcua_byte_t *buffer, size_t length, const mu_message_header_t *header);

opcua_statuscode_t mu_parse_sequence_header(const opcua_byte_t *buffer, size_t length, size_t *offset, mu_sequence_header_t *header);
opcua_statuscode_t mu_write_sequence_header(opcua_byte_t *buffer, size_t length, size_t *offset, const mu_sequence_header_t *header);

#endif /* MICRO_OPCUA_MESSAGE_CHUNK_H */
