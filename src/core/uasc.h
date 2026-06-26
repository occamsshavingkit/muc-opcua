/* src/core/uasc.h - UASC (secure channel) message chunk framing for responses. */
#ifndef MICRO_OPCUA_UASC_H
#define MICRO_OPCUA_UASC_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include <stddef.h>

/* Offset where the MessageBody begins in a symmetric (MSG) response chunk:
   MessageHeader(8) + SecureChannelId(4) + SymmetricSecurityHeader/TokenId(4)
   + SequenceHeader(8) = 24. (OPC 10000-6 6.7.2/6.7.3/6.7.7) */
#define MU_UASC_SYMMETRIC_HEADER_SIZE 24

/* Given a MessageBody already written at offset MU_UASC_SYMMETRIC_HEADER_SIZE,
   fill in the MSG chunk header (type, size, channel, token, sequence header).
   Returns the total chunk length in out_total_length. */
opcua_statuscode_t mu_uasc_finalize_symmetric(
    opcua_byte_t *buffer, size_t buffer_size,
    opcua_uint32_t secure_channel_id, opcua_uint32_t token_id,
    opcua_uint32_t sequence_number, opcua_uint32_t request_id,
    size_t body_length, size_t *out_total_length);

#endif /* MICRO_OPCUA_UASC_H */
