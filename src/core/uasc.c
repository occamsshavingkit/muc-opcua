/* src/core/uasc.c */

/*
 * Spec grounding: OPC-10000-6 §7.2 OPC UA TCP, §6.7.2 MessageChunk, §6.7.6 SecurityHeader
 * Implements the OPC UA Secure Conversation (UASC) message framing, including
 * symmetric/asymmetric header serialization and MessageChunk layout.
 */

#include "uasc.h"
#include "../encoding/binary_le.h"
#include <string.h>

opcua_statuscode_t mu_uasc_finalize_symmetric(opcua_byte_t *buffer, size_t buffer_size,
                                              opcua_uint32_t secure_channel_id, opcua_uint32_t token_id,
                                              opcua_uint32_t sequence_number, opcua_uint32_t request_id,
                                              size_t body_length, size_t *out_total_length) {
    if (!buffer || !out_total_length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    size_t total = MU_UASC_SYMMETRIC_HEADER_SIZE + body_length;
    if (total > buffer_size) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }

    /* MessageHeader: MessageType "MSG" + IsFinal 'F' */
    buffer[0] = 'M';
    buffer[1] = 'S';
    buffer[2] = 'G';
    buffer[3] = 'F';

    mu_binary_le_put_u32(&buffer[4u], (opcua_uint32_t)total); /* MessageSize */
    mu_binary_le_put_u32(&buffer[8u], secure_channel_id);     /* SecureChannelId */
    mu_binary_le_put_u32(&buffer[12u], token_id);             /* SymmetricSecurityHeader.TokenId */
    mu_binary_le_put_u32(&buffer[16u], sequence_number);      /* SequenceHeader.SequenceNumber */
    mu_binary_le_put_u32(&buffer[20u], request_id);           /* SequenceHeader.RequestId */

    /* MessageBody is assumed already present at offset MU_UASC_SYMMETRIC_HEADER_SIZE. */
    *out_total_length = total;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_uasc_finalize_asymmetric_none(opcua_byte_t *buffer, size_t buffer_size,
                                                    opcua_uint32_t secure_channel_id, opcua_uint32_t sequence_number,
                                                    opcua_uint32_t request_id, size_t body_length,
                                                    size_t *out_total_length) {
    static const char policy_uri[] = "http://opcfoundation.org/UA/SecurityPolicy#None";

    if (!buffer || !out_total_length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    size_t total = MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE + body_length;
    if (total > buffer_size) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }

    buffer[0] = 'O';
    buffer[1] = 'P';
    buffer[2] = 'N';
    buffer[3] = 'F';

    mu_binary_le_put_u32(&buffer[4u], (opcua_uint32_t)total);                     /* MessageSize */
    mu_binary_le_put_u32(&buffer[8u], secure_channel_id);                         /* SecureChannelId */
    mu_binary_le_put_u32(&buffer[12u], (opcua_uint32_t)(sizeof(policy_uri) - 1)); /* SecurityPolicyUri length */
    (void)memcpy(&buffer[16u], policy_uri, sizeof(policy_uri) - 1u);              /* SecurityPolicyUri bytes */
    mu_binary_le_put_u32(&buffer[63u], 0xFFFFFFFFu);                              /* SenderCertificate (null) */
    mu_binary_le_put_u32(&buffer[67u], 0xFFFFFFFFu);                              /* ReceiverThumbprint (null) */
    mu_binary_le_put_u32(&buffer[71u], sequence_number);                          /* SequenceHeader.SequenceNumber */
    mu_binary_le_put_u32(&buffer[75u], request_id);                               /* SequenceHeader.RequestId */

    /* MessageBody is assumed already present at offset MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE. */
    *out_total_length = total;
    return MU_STATUS_GOOD;
}
