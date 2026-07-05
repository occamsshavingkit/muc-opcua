/* src/core/message_chunk.h */
#ifndef MUC_OPCUA_MESSAGE_CHUNK_H
#define MUC_OPCUA_MESSAGE_CHUNK_H

#include "muc_opcua/config.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/status.h"
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

#ifdef MUC_OPCUA_MULTI_CHUNK
typedef struct {
    opcua_byte_t buffer[MU_CHUNK_ASSEMBLY_BUFFER_SIZE];
    size_t length;
    opcua_uint32_t secure_channel_id;
    opcua_uint32_t token_id;
    bool is_active;
} mu_chunk_assembler_t;

void mu_chunk_assembler_init(mu_chunk_assembler_t *assembler);
void mu_chunk_assembler_reset(mu_chunk_assembler_t *assembler);
#endif /* MUC_OPCUA_MULTI_CHUNK */

opcua_statuscode_t mu_parse_message_header(const opcua_byte_t *buffer, size_t length, mu_message_header_t *header);
opcua_statuscode_t mu_write_message_header(opcua_byte_t *buffer, size_t length, const mu_message_header_t *header);

opcua_statuscode_t mu_parse_sequence_header(const opcua_byte_t *buffer, size_t length, size_t *offset,
                                            mu_sequence_header_t *header);
opcua_statuscode_t mu_write_sequence_header(opcua_byte_t *buffer, size_t length, size_t *offset,
                                            const mu_sequence_header_t *header);

#endif /* MUC_OPCUA_MESSAGE_CHUNK_H */
