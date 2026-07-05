/* src/core/message_chunk.c */
#include "message_chunk.h"
#include "muc_opcua/encoding.h"
#include "uasc.h"
#include <string.h>

void mu_chunk_assembler_init(mu_chunk_assembler_t *assembler) {
    if (!assembler) return;
    (void)memset(assembler, 0, sizeof(*assembler));
}

void mu_chunk_assembler_reset(mu_chunk_assembler_t *assembler) {
    if (!assembler) return;
    assembler->is_active = false;
    assembler->length = 0;
}

opcua_statuscode_t mu_parse_message_header(const opcua_byte_t *buffer, size_t length, mu_message_header_t *header) {
    if (!buffer || !header) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (length < 8) {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }

    header->message_type[0] = buffer[0];
    header->message_type[1] = buffer[1];
    header->message_type[2] = buffer[2];
    header->chunk_type = buffer[3];

    /* OPC-10000-6 6.7.2 and 7.1.2.2 define the accepted 3-byte MessageType values. */
    opcua_uint32_t msg_type = ((opcua_uint32_t)header->message_type[0]) |
                              ((opcua_uint32_t)header->message_type[1] << 8u) |
                              ((opcua_uint32_t)header->message_type[2] << 16u);
    if (msg_type != 0x4C4548u && msg_type != 0x4B4341u && msg_type != 0x525245u && msg_type != 0x4E504Fu &&
        msg_type != 0x4F4C43u && msg_type != 0x47534Du) {
        return MU_STATUS_BAD_TCPMESSAGETYPEINVALID;
    }

    if (header->chunk_type != 'C' && header->chunk_type != 'F' && header->chunk_type != 'A') {
        return MU_STATUS_BAD_TCPMESSAGETYPEINVALID;
    }

    header->message_size = (opcua_uint32_t)buffer[4] | ((opcua_uint32_t)buffer[5] << 8u) |
                           ((opcua_uint32_t)buffer[6] << 16u) | ((opcua_uint32_t)buffer[7] << 24u);

    if (header->message_size < 8) {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }
    if (header->message_size > length) {
        return MU_STATUS_BAD_TCPMESSAGETOOLARGE;
    }

    if (header->chunk_type == 'C' && header->message_type[0] != 'M') {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }

    if (msg_type == 0x4C4548u || msg_type == 0x4B4341u || msg_type == 0x525245u) {
        header->secure_channel_id = 0;
        return MU_STATUS_GOOD;
    }

    if (length < 12) {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }
    if (header->message_size < 12) {
        return MU_STATUS_BAD_TCPINTERNALERROR;
    }

    header->secure_channel_id = (opcua_uint32_t)buffer[8] | ((opcua_uint32_t)buffer[9] << 8u) |
                                ((opcua_uint32_t)buffer[10] << 16u) | ((opcua_uint32_t)buffer[11] << 24u);

    if (header->chunk_type == 'A') {
        /* OPC-10000-6 section 6.7.2: abort chunks carry Error/Reason, not service MessageBody bytes. */
        if (header->message_type[0] == 'O') {
            return MU_STATUS_BAD_TCPINTERNALERROR;
        }
        if (header->message_size < (MU_UASC_SYMMETRIC_HEADER_SIZE + 4u)) {
            return MU_STATUS_BAD_TCPINTERNALERROR;
        }

        mu_binary_reader_t reader;
        mu_binary_reader_init(&reader, buffer, length);
        reader.position = MU_UASC_SYMMETRIC_HEADER_SIZE;
        opcua_statuscode_t abort_status = MU_STATUS_GOOD;
        opcua_statuscode_t status = mu_binary_read_statuscode(&reader, &abort_status);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        return abort_status == MU_STATUS_GOOD ? MU_STATUS_BAD_TCPINTERNALERROR : abort_status;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_message_header(opcua_byte_t *buffer, size_t length, const mu_message_header_t *header) {
    if (!buffer || !header) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (length < 12) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    buffer[0] = header->message_type[0];
    buffer[1] = header->message_type[1];
    buffer[2] = header->message_type[2];
    buffer[3] = header->chunk_type;
    buffer[4] = (opcua_byte_t)(header->message_size & 0xFFu);
    buffer[5] = (opcua_byte_t)((header->message_size >> 8u) & 0xFFu);
    buffer[6] = (opcua_byte_t)((header->message_size >> 16u) & 0xFFu);
    buffer[7] = (opcua_byte_t)((header->message_size >> 24u) & 0xFFu);
    buffer[8] = (opcua_byte_t)(header->secure_channel_id & 0xFFu);
    buffer[9] = (opcua_byte_t)((header->secure_channel_id >> 8u) & 0xFFu);
    buffer[10] = (opcua_byte_t)((header->secure_channel_id >> 16u) & 0xFFu);
    buffer[11] = (opcua_byte_t)((header->secure_channel_id >> 24u) & 0xFFu);

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_parse_sequence_header(const opcua_byte_t *buffer, size_t length, size_t *offset,
                                            mu_sequence_header_t *header) {
    if (!buffer || !offset || !header) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    size_t position = *offset;
    if (position > length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (4u > length - position) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    header->sequence_number = (opcua_uint32_t)buffer[position] | ((opcua_uint32_t)buffer[position + 1u] << 8u) |
                              ((opcua_uint32_t)buffer[position + 2u] << 16u) |
                              ((opcua_uint32_t)buffer[position + 3u] << 24u);
    position += 4u;

    if (4u > length - position) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    header->request_id = (opcua_uint32_t)buffer[position] | ((opcua_uint32_t)buffer[position + 1u] << 8u) |
                         ((opcua_uint32_t)buffer[position + 2u] << 16u) |
                         ((opcua_uint32_t)buffer[position + 3u] << 24u);
    position += 4u;

    *offset = position;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_sequence_header(opcua_byte_t *buffer, size_t length, size_t *offset,
                                            const mu_sequence_header_t *header) {
    if (!buffer || !offset || !header) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    size_t position = *offset;
    if (position > length) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }
    if (4u > length - position) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }

    buffer[position] = (opcua_byte_t)(header->sequence_number & 0xFFu);
    buffer[position + 1u] = (opcua_byte_t)((header->sequence_number >> 8u) & 0xFFu);
    buffer[position + 2u] = (opcua_byte_t)((header->sequence_number >> 16u) & 0xFFu);
    buffer[position + 3u] = (opcua_byte_t)((header->sequence_number >> 24u) & 0xFFu);
    position += 4u;

    if (4u > length - position) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }

    buffer[position] = (opcua_byte_t)(header->request_id & 0xFFu);
    buffer[position + 1u] = (opcua_byte_t)((header->request_id >> 8u) & 0xFFu);
    buffer[position + 2u] = (opcua_byte_t)((header->request_id >> 16u) & 0xFFu);
    buffer[position + 3u] = (opcua_byte_t)((header->request_id >> 24u) & 0xFFu);
    position += 4u;

    *offset = position;
    return MU_STATUS_GOOD;
}
