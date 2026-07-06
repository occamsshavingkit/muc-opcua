/* src/core/server/process_message.c */
#include "../uasc.h"
#include "common.h"
#include <string.h>

/* Process the TCP HEL message before the secure channel is opened. */
static void process_message_hello(mu_server_t *server, opcua_byte_t *msg, size_t msg_len) {
    size_t ack_length = server->config.send_buffer_size;
    opcua_statuscode_t status =
        mu_tcp_process_hello(&server_tcp_conn, msg, msg_len, &server->config, server->config.send_buffer, &ack_length);
    if (status == MU_STATUS_GOOD) {
        send_buffer_chunk(server, ack_length);
    } else {
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
        server_client_handle = NULL;
    }
}

#ifdef MUC_OPCUA_MULTI_CHUNK
/* Buffer body bytes from a multi-chunk MSG continuation chunk. */
static void process_message_multi_chunk_continuation(mu_server_t *server, const opcua_byte_t *msg, size_t msg_len) {
    size_t body_offset = MU_UASC_SYMMETRIC_HEADER_SIZE;
    if (msg_len <= body_offset) {
        send_tcp_error_chunk(server, MU_STATUS_BAD_TCPINTERNALERROR);
        return;
    }
    size_t body_len = msg_len - body_offset;
    mu_chunk_assembler_t *ca = &server_chunk_assembly;
    if (!ca->is_active) {
        ca->is_active = true;
        ca->length = 0;
    }
    if (ca->length + body_len > sizeof(ca->buffer)) {
        mu_chunk_assembler_reset(ca);
        send_tcp_error_chunk(server, MU_STATUS_BAD_TCPINTERNALERROR);
        return;
    }
    (void)memcpy(ca->buffer + ca->length, msg + body_offset, body_len);
    ca->length += body_len;
}

/* Dispatch a fully-assembled multi-chunk MSG body and send the response,
   then reset the chunk assembler. */
static void process_message_multi_chunk_final_dispatch(mu_server_t *server, const mu_nodeid_t *request_type,
                                                       opcua_uint32_t response_request_id, const opcua_byte_t *req_body,
                                                       size_t req_body_len) {
    size_t body_offset = MU_UASC_SYMMETRIC_HEADER_SIZE;
    if (body_offset >= server->config.send_buffer_size) {
        mu_chunk_assembler_reset(&server_chunk_assembly);
        return;
    }
    opcua_byte_t *resp_body = server->config.send_buffer + body_offset;
    size_t payload_len = server->config.send_buffer_size - body_offset;

#if MUC_OPCUA_SUBSCRIPTIONS
    server->current_request_id = response_request_id;
#endif
    opcua_statuscode_t status;
    if (!is_ns0_numeric_nodeid(request_type)) {
        status = MU_STATUS_BAD_DECODINGERROR;
    } else {
        status = mu_service_dispatch(server, request_type->identifier.numeric, req_body, req_body_len, resp_body,
                                     &payload_len);
    }

    if (status != MU_STATUS_GOOD) {
        payload_len = server->config.send_buffer_size - body_offset;
        if (mu_write_service_fault(resp_body, &payload_len, 0, status
#ifdef MUC_OPCUA_TIME_SYNC
                                   ,
                                   server
#endif
                                   ) == MU_STATUS_GOOD) {
            status = MU_STATUS_GOOD;
        } else {
            payload_len = 0;
        }
    }

    if (status == MU_STATUS_GOOD && payload_len > 0) {
        opcua_uint32_t out_seq = ++server_secure_channel.out_sequence_number;
        size_t total = 0;
        opcua_statuscode_t ws = mu_uasc_finalize_symmetric(
            server->config.send_buffer, server->config.send_buffer_size, server_secure_channel.channel_id,
            server_secure_channel.token_id, out_seq, response_request_id, payload_len, &total);
        if (ws == MU_STATUS_GOOD) {
            send_buffer_chunk(server, total);
        }
    }

    mu_chunk_assembler_reset(&server_chunk_assembly);
}
#endif /* MUC_OPCUA_MULTI_CHUNK */

/* Route an OPN or MSG chunk to the appropriate handler based on the
   configured security policy. */
static void process_message_opn_or_msg(mu_server_t *server, opcua_byte_t *msg, size_t msg_len, bool is_opn) {
#ifdef MUC_OPCUA_SECURITY
    if (server->config.crypto_adapter != NULL) {
        handle_data_chunk_secure(server, msg, msg_len, is_opn);
        return;
    }
#endif
    handle_data_chunk_plaintext(server, msg, msg_len, is_opn);
}

/* Process one complete message (HELLO during connect, otherwise an OPN/MSG/CLO
   chunk) and send any response. Sets client_handle to NULL if the connection is
   closed (HELLO failure or CloseSecureChannel). */
void process_message(mu_server_t *server, opcua_byte_t *msg, size_t msg_len) {
    opcua_statuscode_t status;

    if (server_tcp_conn.state == MU_TCP_STATE_CLOSED) {
        process_message_hello(server, msg, msg_len);
        return;
    }

    mu_message_header_t header = {{0, 0, 0}, 0, 0, 0};
    status = mu_parse_message_header(msg, msg_len, &header);
    if (status != MU_STATUS_GOOD) {
        if (header.chunk_type == 'C') {
#ifndef MUC_OPCUA_MULTI_CHUNK
            send_tcp_error_chunk(server, MU_STATUS_BAD_TCPMESSAGETYPEINVALID);
#else
            send_tcp_error_chunk(server, status);
#endif
        }
#ifdef MUC_OPCUA_MULTI_CHUNK
        mu_chunk_assembler_reset(&server_chunk_assembly);
#endif
        return;
    }

    bool is_opn = header.message_type[0] == 'O' && header.message_type[1] == 'P' && header.message_type[2] == 'N';
    bool is_msg = header.message_type[0] == 'M' && header.message_type[1] == 'S' && header.message_type[2] == 'G';
    bool is_clo = header.message_type[0] == 'C' && header.message_type[1] == 'L' && header.message_type[2] == 'O';

#ifdef MUC_OPCUA_MULTI_CHUNK
    if (is_msg && header.chunk_type == 'C') {
        process_message_multi_chunk_continuation(server, msg, msg_len);
        return;
    }

    if (is_msg && header.chunk_type == 'F' && server_chunk_assembly.is_active) {
        mu_chunk_assembler_t *ca = &server_chunk_assembly;
        size_t body_offset = MU_UASC_SYMMETRIC_HEADER_SIZE;
        if (msg_len <= body_offset) {
            mu_chunk_assembler_reset(ca);
            return;
        }
        size_t body_len = msg_len - body_offset;
        if (ca->length + body_len > sizeof(ca->buffer)) {
            mu_chunk_assembler_reset(ca);
            return;
        }
        (void)memcpy(ca->buffer + ca->length, msg + body_offset, body_len);
        ca->length += body_len;

        mu_binary_reader_t reader;
        mu_binary_reader_init(&reader, msg, msg_len);
        reader.position = 8;

        opcua_uint32_t channel_id = 0;
        opcua_uint32_t token_id = 0;
        mu_sequence_header_t seq;
        mu_nodeid_t request_type;

        if (mu_binary_read_uint32(&reader, &channel_id) != MU_STATUS_GOOD) {
            mu_chunk_assembler_reset(ca);
            return;
        }
        if (mu_binary_read_uint32(&reader, &token_id) != MU_STATUS_GOOD) {
            mu_chunk_assembler_reset(ca);
            return;
        }
        if (mu_binary_read_uint32(&reader, &seq.sequence_number) != MU_STATUS_GOOD) {
            mu_chunk_assembler_reset(ca);
            return;
        }
        if (mu_binary_read_uint32(&reader, &seq.request_id) != MU_STATUS_GOOD) {
            mu_chunk_assembler_reset(ca);
            return;
        }

        if (!accept_inbound_chunk(server, false, channel_id, token_id, seq.sequence_number)) {
            mu_chunk_assembler_reset(ca);
            abort_connection(server);
            return;
        }

        mu_binary_reader_t body_reader;
        mu_binary_reader_init(&body_reader, ca->buffer, ca->length);
        if (mu_binary_read_nodeid(&body_reader, &request_type) != MU_STATUS_GOOD) {
            mu_chunk_assembler_reset(ca);
            return;
        }

        const opcua_byte_t *req_body = ca->buffer + body_reader.position;
        size_t req_body_len = ca->length - body_reader.position;

        process_message_multi_chunk_final_dispatch(server, &request_type, seq.request_id, req_body, req_body_len);
        return;
    }
#endif /* MUC_OPCUA_MULTI_CHUNK */

    if (is_clo) {
        mu_secure_channel_close(&server_secure_channel);
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
        server_client_handle = NULL;
        return;
    }
    if (!is_opn && !is_msg) {
        return;
    }

    process_message_opn_or_msg(server, msg, msg_len, is_opn);
}
