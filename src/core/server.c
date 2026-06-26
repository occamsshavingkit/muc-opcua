/* src/core/server.c */
#include "micro_opcua/server.h"
#include "micro_opcua/encoding.h"
#include "server_internal.h"
#include "message_chunk.h"
#include "service_message.h"
#include "uasc.h"
#include "../services/secure_channel.h"
#ifdef MICRO_OPCUA_SECURITY
#include "../security/asym_chunk.h"
#include "../security/sym_chunk.h"
#endif
#include <string.h>

opcua_statuscode_t mu_server_config_validate(const mu_server_config_t *config)
{
    if (config == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate Endpoints */
    if (config->endpoint_url == NULL || strncmp(config->endpoint_url, "opc.tcp://", 10) != 0) {
        return MU_STATUS_BAD_TCPENDPOINTURLINVALID;
    }

    /* Validate caller-owned buffers */
    if (config->receive_buffer == NULL || config->receive_buffer_size < MU_MIN_CHUNK_SIZE) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (config->send_buffer == NULL || config->send_buffer_size < MU_MIN_CHUNK_SIZE) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate limits */
    if (config->max_sessions == 0 || config->max_secure_channels == 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (config->max_chunk_count == 0 || config->max_message_size < MU_MIN_CHUNK_SIZE) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate Platform Adapters */
    if (config->tcp_adapter.listen == NULL || config->tcp_adapter.accept == NULL ||
        config->tcp_adapter.read == NULL || config->tcp_adapter.write == NULL ||
        config->tcp_adapter.close_connection == NULL || config->tcp_adapter.shutdown == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (config->time_adapter.get_time == NULL || config->time_adapter.get_tick_ms == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (config->entropy_adapter.generate_random == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate address space if provided */
    if (config->address_space != NULL) {
        opcua_statuscode_t status = mu_address_space_validate(config->address_space);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_server_init(void *storage, size_t storage_size, const mu_server_config_t *config, mu_server_t **out_server)
{
    mu_server_t *server;
    opcua_statuscode_t status;

    if (storage == NULL || out_server == NULL || config == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (storage_size < sizeof(struct mu_server)) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    status = mu_server_config_validate(config);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    /* Initialize without heap */
    server = (mu_server_t *)storage;
    memset(server, 0, sizeof(struct mu_server));
    server->config = *config;
    server->is_running = true;
    server->client_handle = NULL;
    
    mu_tcp_connection_init(&server->tcp_conn);
    mu_secure_channel_init(&server->secure_channel);
    mu_session_init(&server->session);

    /* Initialize platform TCP adapter */
    status = server->config.tcp_adapter.listen(server->config.tcp_adapter.context, server->config.endpoint_url);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    *out_server = server;
    return MU_STATUS_GOOD;
}

/* Largest decrypted request / response body handled on the secured path. Sized to
   hold a GetEndpoints/CreateSession response, which carries the server certificate
   in each advertised endpoint. */
#define MU_SECURE_BODY_MAX 8192

static void send_buffer_chunk(mu_server_t *server, size_t total) {
    size_t bytes_written;
    server->config.tcp_adapter.write(server->config.tcp_adapter.context, server->client_handle,
                                     server->config.send_buffer, total, &bytes_written);
}

/* Plaintext (SecurityPolicy None) OPN/MSG handling — the only path when no crypto
   adapter is configured. Dispatch writes the response body directly into the send
   buffer past the fixed UASC header, then a finalize step frames the chunk. */
static void handle_data_chunk_plaintext(mu_server_t *server, const opcua_byte_t *msg, size_t msg_len, bool is_opn)
{
    opcua_statuscode_t status;
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, msg, msg_len);
    reader.position = 12; /* MessageHeader(8) + SecureChannelId(4) */

    if (is_opn) {
        mu_string_t policy_uri;
        mu_bytestring_t sender_cert, receiver_thumbprint;
        status = mu_binary_read_string(&reader, &policy_uri);              if (status != MU_STATUS_GOOD) return;
        status = mu_binary_read_bytestring(&reader, &sender_cert);         if (status != MU_STATUS_GOOD) return;
        status = mu_binary_read_bytestring(&reader, &receiver_thumbprint); if (status != MU_STATUS_GOOD) return;
    } else {
        opcua_uint32_t token_id;
        status = mu_binary_read_uint32(&reader, &token_id);               if (status != MU_STATUS_GOOD) return;
    }

    mu_sequence_header_t seq;
    status = mu_binary_read_uint32(&reader, &seq.sequence_number);        if (status != MU_STATUS_GOOD) return;
    status = mu_binary_read_uint32(&reader, &seq.request_id);             if (status != MU_STATUS_GOOD) return;

    mu_nodeid_t request_type;
    status = mu_binary_read_nodeid(&reader, &request_type);              if (status != MU_STATUS_GOOD) return;

    const opcua_byte_t *req_body = msg + reader.position;
    size_t req_body_len = msg_len - reader.position;

    size_t body_offset = is_opn ? MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE : MU_UASC_SYMMETRIC_HEADER_SIZE;
    if (body_offset >= server->config.send_buffer_size) return;

    opcua_byte_t *resp_body = server->config.send_buffer + body_offset;
    size_t payload_len = server->config.send_buffer_size - body_offset;

    status = mu_service_dispatch(server, request_type.identifier.numeric, req_body, req_body_len, resp_body, &payload_len);

    if (status != MU_STATUS_GOOD) {
        /* Always answer: send a ServiceFault rather than letting the client time out. */
        payload_len = server->config.send_buffer_size - body_offset;
        if (mu_write_service_fault(resp_body, &payload_len, 0, status) == MU_STATUS_GOOD) {
            status = MU_STATUS_GOOD;
        } else {
            payload_len = 0;
        }
    }

    if (status == MU_STATUS_GOOD && payload_len > 0) {
        opcua_uint32_t out_seq = ++server->secure_channel.out_sequence_number;
        size_t total = 0;
        if (is_opn) {
            status = mu_uasc_finalize_asymmetric_none(server->config.send_buffer, server->config.send_buffer_size,
                                                      server->secure_channel.channel_id, out_seq, seq.request_id,
                                                      payload_len, &total);
        } else {
            status = mu_uasc_finalize_symmetric(server->config.send_buffer, server->config.send_buffer_size,
                                                server->secure_channel.channel_id, server->secure_channel.token_id,
                                                out_seq, seq.request_id, payload_len, &total);
        }
        if (status == MU_STATUS_GOOD) send_buffer_chunk(server, total);
    }
}

#ifdef MICRO_OPCUA_SECURITY
/* Secured (or secure-capable) OPN/MSG handling: a crypto adapter is present, so
   OPN chunks are unwrapped/wrapped asymmetrically (None or Basic256Sha256) and
   MSG chunks symmetrically once the channel has keys. The decrypted request and
   the response body live in local scratch; the chunk modules frame the wire. */
static void handle_data_chunk_secure(mu_server_t *server, const opcua_byte_t *msg, size_t msg_len, bool is_opn)
{
    const mu_crypto_adapter_t *crypto = server->config.crypto_adapter;
    opcua_byte_t reqscratch[MU_SECURE_BODY_MAX];
    opcua_byte_t respbody[MU_SECURE_BODY_MAX];
    size_t req_len = 0;
    opcua_uint32_t response_request_id = 0;
    const opcua_byte_t *client_cert = NULL;
    size_t client_cert_len = 0;

    if (is_opn) {
        mu_asym_chunk_info_t ai;
        memset(&ai, 0, sizeof(ai));
        if (mu_asym_chunk_unwrap(crypto, msg, msg_len, reqscratch, sizeof(reqscratch), &req_len, &ai) != MU_STATUS_GOOD) {
            return;
        }
        /* Record the negotiated policy before dispatch so the OPN handler can
           derive channel keys; the client cert is needed to encrypt the reply. */
        server->secure_channel.policy = ai.policy;
        response_request_id = ai.request_id;
        client_cert = ai.sender_cert;
        client_cert_len = ai.sender_cert_len;
    } else if (server->secure_channel.policy == MU_SECURITY_POLICY_NONE_ID) {
        /* The client chose the None endpoint; MSG chunks are plaintext. */
        handle_data_chunk_plaintext(server, msg, msg_len, false);
        return;
    } else {
        if (!server->secure_channel.keys_valid) return;
        mu_sym_chunk_info_t si;
        memset(&si, 0, sizeof(si));
        if (mu_sym_chunk_unwrap(crypto, server->secure_channel.mode, &server->secure_channel.client_keys,
                                msg, msg_len, reqscratch, sizeof(reqscratch), &req_len, &si) != MU_STATUS_GOOD) {
            return;
        }
        response_request_id = si.request_id;
    }

    /* reqscratch holds the service body: [request-type NodeId][RequestHeader][...]. */
    mu_binary_reader_t rr;
    mu_binary_reader_init(&rr, reqscratch, req_len);
    mu_nodeid_t request_type;
    if (mu_binary_read_nodeid(&rr, &request_type) != MU_STATUS_GOOD) return;
    const opcua_byte_t *req_body = reqscratch + rr.position;
    size_t req_body_len = req_len - rr.position;

    size_t resp_len = sizeof(respbody);
    opcua_statuscode_t status = mu_service_dispatch(server, request_type.identifier.numeric,
                                                    req_body, req_body_len, respbody, &resp_len);
    if (status != MU_STATUS_GOOD) {
        resp_len = sizeof(respbody);
        if (mu_write_service_fault(respbody, &resp_len, 0, status) == MU_STATUS_GOOD) {
            status = MU_STATUS_GOOD;
        } else {
            resp_len = 0;
        }
    }
    if (status != MU_STATUS_GOOD || resp_len == 0) return;

    opcua_uint32_t out_seq = ++server->secure_channel.out_sequence_number;
    size_t total = 0;
    opcua_statuscode_t ws;
    if (is_opn) {
        ws = mu_asym_chunk_wrap(crypto, server->secure_channel.policy, server->secure_channel.channel_id,
                                out_seq, response_request_id, client_cert, client_cert_len,
                                respbody, resp_len, server->config.send_buffer, server->config.send_buffer_size, &total);
    } else {
        ws = mu_sym_chunk_wrap(crypto, server->secure_channel.mode, &server->secure_channel.server_keys, "MSG",
                               server->secure_channel.channel_id, server->secure_channel.token_id,
                               out_seq, response_request_id, respbody, resp_len,
                               server->config.send_buffer, server->config.send_buffer_size, &total);
    }
    if (ws == MU_STATUS_GOOD) send_buffer_chunk(server, total);
}
#endif /* MICRO_OPCUA_SECURITY */

/* Process one complete message (HELLO during connect, otherwise an OPN/MSG/CLO
   chunk) and send any response. Sets client_handle to NULL if the connection is
   closed (HELLO failure or CloseSecureChannel). */
static void process_message(mu_server_t *server, const opcua_byte_t *msg, size_t msg_len)
{
    opcua_statuscode_t status;

    if (server->tcp_conn.state == MU_TCP_STATE_CLOSED) {
        size_t ack_length = server->config.send_buffer_size;
        status = mu_tcp_process_hello(&server->tcp_conn, msg, msg_len, &server->config,
                                      server->config.send_buffer, &ack_length);
        if (status == MU_STATUS_GOOD) {
            size_t bytes_written;
            server->config.tcp_adapter.write(server->config.tcp_adapter.context, server->client_handle,
                                             server->config.send_buffer, ack_length, &bytes_written);
        } else {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server->client_handle);
            server->client_handle = NULL;
        }
        return;
    }

    mu_message_header_t header;
    status = mu_parse_message_header(msg, msg_len, &header);
    if (status != MU_STATUS_GOOD) return;

    bool is_opn = header.message_type[0] == 'O' && header.message_type[1] == 'P' && header.message_type[2] == 'N';
    bool is_msg = header.message_type[0] == 'M' && header.message_type[1] == 'S' && header.message_type[2] == 'G';
    bool is_clo = header.message_type[0] == 'C' && header.message_type[1] == 'L' && header.message_type[2] == 'O';

    if (is_clo) {
        mu_secure_channel_close(&server->secure_channel);
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server->client_handle);
        server->client_handle = NULL;
        return;
    }
    if (!is_opn && !is_msg) return; /* unsupported chunk type: ignore */

#ifdef MICRO_OPCUA_SECURITY
    if (server->config.crypto_adapter != NULL) {
        handle_data_chunk_secure(server, msg, msg_len, is_opn);
        return;
    }
#endif
    handle_data_chunk_plaintext(server, msg, msg_len, is_opn);
}

opcua_statuscode_t mu_server_poll(mu_server_t *server)
{
    if (server == NULL || !server->is_running) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* 1. Accept connections if none active */
    if (server->client_handle == NULL) {
        opcua_statuscode_t status = server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &server->client_handle);
        if (status == MU_STATUS_GOOD && server->client_handle != NULL) {
            mu_tcp_connection_init(&server->tcp_conn);
            mu_secure_channel_init(&server->secure_channel);
            mu_session_init(&server->session);
            server->rx_len = 0;
        }
        return MU_STATUS_GOOD;
    } else {
        /* Check if there's another connection waiting to be rejected */
        void *second_handle = NULL;
        opcua_statuscode_t status = server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &second_handle);
        if (status == MU_STATUS_GOOD && second_handle != NULL) {
            /* We don't have resources. OPC UA 10000-6 7.1.5: immediately close or wait for HEL and send ERR.
               Wait for HEL (simulate it by just trying to read once) */
            opcua_byte_t buf[256];
            size_t bytes_read = 0;
            status = server->config.tcp_adapter.read(server->config.tcp_adapter.context, second_handle, buf, sizeof(buf), &bytes_read);
            
            if (status == MU_STATUS_GOOD && bytes_read >= 8 && buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'L') {
                size_t err_len = sizeof(buf);
                if (mu_tcp_create_error_message(MU_STATUS_BAD_TCPNOTENOUGHRESOURCES, "Server full", buf, &err_len) == MU_STATUS_GOOD) {
                    size_t bytes_written;
                    server->config.tcp_adapter.write(server->config.tcp_adapter.context, second_handle, buf, err_len, &bytes_written);
                }
            }
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, second_handle);
        }
    }

    /* 2. Read from the active connection, appending to the receive buffer. */
    if (server->rx_len < server->config.receive_buffer_size) {
        size_t bytes_read = 0;
        opcua_statuscode_t status = server->config.tcp_adapter.read(
            server->config.tcp_adapter.context, server->client_handle,
            server->config.receive_buffer + server->rx_len,
            server->config.receive_buffer_size - server->rx_len, &bytes_read);

        if (status != MU_STATUS_GOOD) {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server->client_handle);
            server->client_handle = NULL;
            server->rx_len = 0;
            return MU_STATUS_GOOD;
        }
        server->rx_len += bytes_read;
    }

    /* 3. Process every complete message in the buffer, reassembling the stream:
       a single read() may carry several messages or only part of one. */
    while (server->client_handle != NULL && server->rx_len >= 8) {
        const opcua_byte_t *b = server->config.receive_buffer;
        /* MessageSize is a UInt32 at byte offset 4 of every TCP/UASC message. */
        size_t msg_size = (size_t)b[4] | ((size_t)b[5] << 8) | ((size_t)b[6] << 16) | ((size_t)b[7] << 24);

        if (msg_size < 8 || msg_size > server->config.receive_buffer_size) {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server->client_handle);
            server->client_handle = NULL;
            server->rx_len = 0;
            return MU_STATUS_GOOD;
        }
        if (msg_size > server->rx_len) {
            break; /* incomplete: wait for more bytes on a later poll */
        }

        process_message(server, server->config.receive_buffer, msg_size);

        if (server->client_handle == NULL) {
            server->rx_len = 0;
            break;
        }

        size_t remaining = server->rx_len - msg_size;
        if (remaining > 0) {
            memmove(server->config.receive_buffer, server->config.receive_buffer + msg_size, remaining);
        }
        server->rx_len = remaining;
    }

    return MU_STATUS_GOOD;
}

void mu_server_close(mu_server_t *server)
{
    if (server != NULL) {
        server->is_running = false;
        if (server->config.tcp_adapter.shutdown != NULL) {
            server->config.tcp_adapter.shutdown(server->config.tcp_adapter.context);
        }
    }
}
