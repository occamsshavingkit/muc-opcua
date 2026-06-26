/* src/core/uasc.c */
#include "uasc.h"
#include "micro_opcua/encoding.h"

opcua_statuscode_t mu_uasc_finalize_symmetric(
    opcua_byte_t *buffer, size_t buffer_size,
    opcua_uint32_t secure_channel_id, opcua_uint32_t token_id,
    opcua_uint32_t sequence_number, opcua_uint32_t request_id,
    size_t body_length, size_t *out_total_length)
{
    if (!buffer || !out_total_length) return MU_STATUS_BAD_INTERNALERROR;

    size_t total = MU_UASC_SYMMETRIC_HEADER_SIZE + body_length;
    if (total > buffer_size) return MU_STATUS_BAD_RESPONSETOOLARGE;

    /* MessageHeader: MessageType "MSG" + IsFinal 'F' */
    buffer[0] = 'M';
    buffer[1] = 'S';
    buffer[2] = 'G';
    buffer[3] = 'F';

    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, buffer_size);
    w.position = 4;

    opcua_statuscode_t status = mu_binary_write_uint32(&w, (opcua_uint32_t)total); /* MessageSize */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&w, secure_channel_id);                        /* SecureChannelId */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&w, token_id);                                 /* SymmetricSecurityHeader.TokenId */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&w, sequence_number);                          /* SequenceHeader.SequenceNumber */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&w, request_id);                               /* SequenceHeader.RequestId */
    if (status != MU_STATUS_GOOD) return status;

    /* MessageBody is assumed already present at offset MU_UASC_SYMMETRIC_HEADER_SIZE. */
    *out_total_length = total;
    return MU_STATUS_GOOD;
}
