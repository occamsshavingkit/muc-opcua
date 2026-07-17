/* src/core/server/data_chunk.c */
#include "../safe_mem.h"
#include "../service_message.h"
#include "../uasc.h"
#include "common.h"
#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
#include "../../security/asym_chunk.h"
#include "../../security/sym_chunk.h"
#endif
#include <string.h>

void mu_service_dispatch_set_opn_security_policy(mu_server_t *server, const mu_string_t *security_policy);
void mu_service_dispatch_set_opn_client_cert(mu_server_t *server, const mu_bytestring_t *client_cert);

#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
_Static_assert(MU_SECURE_RESP_MAX + MU_SECURE_OPN_REQ_MAX + MU_SECURE_SESSION_MAX <= MU_SECURE_SCRATCH_SIZE,
               "secure scratch must hold response, OPN request, and session-handshake buffers");
#endif

/* Send a fully-framed chunk from the send buffer. The transport is non-blocking,
   so a write error or short write would leave a half-framed chunk on the wire;
   drop the connection in that case rather than wedge the slot. */
void send_buffer_chunk(mu_server_t *server, size_t total) {
    size_t bytes_written = 0;
    opcua_statuscode_t s = server->config.tcp_adapter.write(server->config.tcp_adapter.context, server_client_handle,
                                                            server->config.send_buffer, total, &bytes_written);
    if (s != MU_STATUS_GOOD || bytes_written != total) {
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
        server_client_handle = NULL;
    }
}

void send_tcp_error_chunk(mu_server_t *server, opcua_statuscode_t error_code) {
    size_t err_len = server->config.send_buffer_size;
    if (mu_tcp_create_error_message(error_code, NULL, server->config.send_buffer, &err_len) == MU_STATUS_GOOD) {
        send_buffer_chunk(server, err_len);
    }
}

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
opcua_statuscode_t mu_server_emit_message(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *body,
                                          size_t body_len) {
    if (server == NULL || body == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
    /* If active_conn is NULL, find connection by active session's secure_channel_id */
    if (server->active_conn == NULL && server->active_session != NULL) {
        for (size_t i = 0; i < MU_INTERN_MAX_CONNECTIONS; ++i) {
            if (server->conns[i].client_handle != NULL &&
                server->conns[i].secure_channel.channel_id == server->active_session->secure_channel_id) {
                server->active_conn = &server->conns[i];
                break;
            }
        }
    }
    if (server->active_conn == NULL) {
        return MU_STATUS_BAD_SECURECHANNELIDINVALID;
    }
#endif

#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
    if (server_secure_channel.mode != MU_MESSAGE_SECURITY_MODE_NONE) {
        size_t total = 0;
        opcua_statuscode_t status;

        if (server->config.crypto_adapter == NULL || !server_secure_channel.keys_valid) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }

        opcua_uint32_t emit_seq = ++server_secure_channel.out_sequence_number;
#ifdef MUC_OPCUA_CU_SECURITY_ECC
        if (mu_security_policy_sym_mode(server_secure_channel.policy) == MU_SYM_MODE_AEAD_CHACHA20POLY1305) {
            status = mu_sym_chunk_wrap_aead(server->config.crypto_adapter, &server_secure_channel.server_keys, "MSG",
                                            server_secure_channel.channel_id, server_secure_channel.token_id, emit_seq,
                                            emit_seq - 1u, request_id, body, body_len, server->config.send_buffer,
                                            server->config.send_buffer_size, &total);
        } else
#endif
            status = mu_sym_chunk_wrap(server->config.crypto_adapter, server_secure_channel.mode,
                                       &server_secure_channel.server_keys, "MSG", server_secure_channel.channel_id,
                                       server_secure_channel.token_id, emit_seq, request_id, body, body_len,
                                       server->config.send_buffer, server->config.send_buffer_size, &total);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        send_buffer_chunk(server, total);
        return MU_STATUS_GOOD;
    }
#endif

    if (!mu_checked_memcpy_off(server->config.send_buffer, server->config.send_buffer_size,
                               MU_UASC_SYMMETRIC_HEADER_SIZE, body, body_len)) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }

    size_t total = 0;
    opcua_statuscode_t status = mu_uasc_finalize_symmetric(
        server->config.send_buffer, server->config.send_buffer_size, server_secure_channel.channel_id,
        server_secure_channel.token_id, ++server_secure_channel.out_sequence_number, request_id, body_len, &total);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    send_buffer_chunk(server, total);
    return MU_STATUS_GOOD;
}
#endif

/* Abort the active connection (a security/protocol violation). The poll loop
   reclaims the slot when client_handle becomes NULL. */
void abort_connection(mu_server_t *server) {
    server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
    server_client_handle = NULL;
}

/* Validate an inbound chunk's sequencing and, for MSG, its channel/token binding.
   Returns false if the chunk replays/skips a SequenceNumber (OPC 10000-6 6.7.2.4)
   or is addressed to a different SecureChannel/Token — the caller must abort. */
bool accept_inbound_chunk(mu_server_t *server, bool is_opn, opcua_uint32_t channel_id, opcua_uint32_t token_id,
                          opcua_uint32_t sequence_number) {
    if (!is_opn) {
        if (channel_id != server_secure_channel.channel_id || token_id != server_secure_channel.token_id) {
            return false;
        }
    }
    return mu_sequence_validate(&server_secure_channel.sequence, sequence_number) == MU_STATUS_GOOD;
}

/* OPC-10000-6 §5.2.2.9 and OPC-10000-4 §7.38.2: request type ids are
   well-known ns=0 numeric NodeIds before the request body is dispatched. */
bool is_ns0_numeric_nodeid(const mu_nodeid_t *nodeid) {
    return nodeid->identifier_type == MU_NODEID_NUMERIC && nodeid->namespace_index == 0u;
}

/* Plaintext (SecurityPolicy None) OPN/MSG handling — the only path when no crypto
   adapter is configured. Dispatch writes the response body directly into the send
   buffer past the fixed UASC header, then a finalize step frames the chunk. */
void handle_data_chunk_plaintext(mu_server_t *server, const opcua_byte_t *msg, size_t msg_len, bool is_opn) {
    opcua_statuscode_t status;
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, msg, msg_len);
    reader.position = 8; /* MessageHeader(8); SecureChannelId follows */
    opcua_uint32_t channel_id = 0;
    status = mu_binary_read_uint32(&reader, &channel_id);
    if (status != MU_STATUS_GOOD) {
        return; /* SecureChannelId */
    }

    mu_string_t opn_security_policy = {-1, NULL};
    mu_bytestring_t opn_sender_cert = {-1, NULL};
    opcua_uint32_t token_id = 0;
    if (is_opn) {
        mu_bytestring_t receiver_thumbprint;
        status = mu_binary_read_string(&reader, &opn_security_policy);
        if (status != MU_STATUS_GOOD) {
            return;
        }
        status = mu_binary_read_bytestring(&reader, &opn_sender_cert);
        if (status != MU_STATUS_GOOD) {
            return;
        }
        status = mu_binary_read_bytestring(&reader, &receiver_thumbprint);
        if (status != MU_STATUS_GOOD) {
            return;
        }
    } else {
        status = mu_binary_read_uint32(&reader, &token_id);
        if (status != MU_STATUS_GOOD) {
            return;
        }
    }

    mu_sequence_header_t seq;
    status = mu_binary_read_uint32(&reader, &seq.sequence_number);
    if (status != MU_STATUS_GOOD) {
        return;
    }
    status = mu_binary_read_uint32(&reader, &seq.request_id);
    if (status != MU_STATUS_GOOD) {
        return;
    }

    if (!accept_inbound_chunk(server, is_opn, channel_id, token_id, seq.sequence_number)) {
        abort_connection(server);
        return;
    }

    mu_nodeid_t request_type;
    status = mu_binary_read_nodeid(&reader, &request_type);
    if (status != MU_STATUS_GOOD) {
        return;
    }

    const opcua_byte_t *req_body = msg + reader.position;
    size_t req_body_len = msg_len - reader.position;

    size_t body_offset = is_opn ? MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE : MU_UASC_SYMMETRIC_HEADER_SIZE;
    if (body_offset >= server->config.send_buffer_size) {
        return;
    }

    opcua_byte_t *resp_body = server->config.send_buffer + body_offset;
    size_t payload_len = server->config.send_buffer_size - body_offset;

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    server->current_request_id = seq.request_id;
#endif
    if (!is_ns0_numeric_nodeid(&request_type)) {
        status = MU_STATUS_BAD_DECODINGERROR;
    } else {
        if (is_opn) {
            mu_service_dispatch_set_opn_security_policy(server, &opn_security_policy);
            mu_service_dispatch_set_opn_client_cert(server, &opn_sender_cert);
        }
        status = mu_service_dispatch(server, request_type.identifier.numeric, req_body, req_body_len, resp_body,
                                     &payload_len);
        if (is_opn) {
            mu_service_dispatch_set_opn_security_policy(server, NULL);
            mu_service_dispatch_set_opn_client_cert(server, NULL);
        }
    }

    bool opn_rejected = ((is_opn) && (status != MU_STATUS_GOOD));
    if (status != MU_STATUS_GOOD) {
        /* Always answer: send a ServiceFault rather than letting the client time out. */
        payload_len = server->config.send_buffer_size - body_offset;
        if (mu_write_service_fault(resp_body, &payload_len, 0, status
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
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
        if (is_opn) {
            status = mu_uasc_finalize_asymmetric_none(server->config.send_buffer, server->config.send_buffer_size,
                                                      server_secure_channel.channel_id, out_seq, seq.request_id,
                                                      payload_len, &total);
        } else {
            status = mu_uasc_finalize_symmetric(server->config.send_buffer, server->config.send_buffer_size,
                                                server_secure_channel.channel_id, server_secure_channel.token_id,
                                                out_seq, seq.request_id, payload_len, &total);
        }
        if (status == MU_STATUS_GOOD) {
            send_buffer_chunk(server, total);
        }
    }
    if (opn_rejected && server_secure_channel.is_open) {
        (void)mu_secure_channel_close(&server_secure_channel);
    }
}

#ifdef MUC_OPCUA_SECURE_CHANNEL_CRYPTO
/* Unwrap an incoming asymmetric OPN chunk and extract its metadata. */
static bool secure_unwrap_opn(mu_server_t *server, const mu_crypto_adapter_t *crypto, opcua_byte_t *msg, size_t msg_len,
                              opcua_byte_t *opn_buf, mu_string_t *out_security_policy,
                              const opcua_byte_t **out_req_full, size_t *out_req_len, opcua_uint32_t *out_request_id,
                              opcua_uint32_t *out_channel_id, opcua_uint32_t *out_seq,
                              const opcua_byte_t **out_client_cert, size_t *out_client_cert_len) {
    mu_asym_chunk_info_t ai;
    mu_binary_reader_t header_reader;
    opcua_uint32_t header_channel_id;

    mu_binary_reader_init(&header_reader, msg, msg_len);
    header_reader.position = 8;
    if (mu_binary_read_uint32(&header_reader, &header_channel_id) != MU_STATUS_GOOD) {
        return false;
    }
    if (mu_binary_read_string(&header_reader, out_security_policy) != MU_STATUS_GOOD) {
        return false;
    }

    (void)memset(&ai, 0, sizeof(ai));
    if (mu_asym_chunk_unwrap(&(mu_asym_unwrap_params_t){.crypto = crypto,
                                                        .chunk = msg,
                                                        .chunk_len = msg_len,
                                                        .out_body = opn_buf,
                                                        .out_cap = MU_SECURE_OPN_REQ_MAX,
                                                        .out_body_len = out_req_len,
                                                        .scratch = server->secure_scratch,
                                                        .scratch_len = MU_SECURE_SCRATCH_SIZE,
                                                        .info = &ai}) != MU_STATUS_GOOD) {
        return false;
    }
    server_secure_channel.policy = ai.policy;
    *out_request_id = ai.request_id;
    *out_client_cert = ai.sender_cert;
    *out_client_cert_len = ai.sender_cert_len;
    *out_req_full = opn_buf;
    *out_channel_id = ai.secure_channel_id;
    *out_seq = ai.sequence_number;
    return true;
}

/* Unwrap an incoming symmetric MSG chunk and extract its metadata. */
static bool secure_unwrap_msg(mu_server_t *server, const mu_crypto_adapter_t *crypto, opcua_byte_t *msg, size_t msg_len,
                              const opcua_byte_t **out_req_full, size_t *out_req_len, opcua_uint32_t *out_request_id,
                              opcua_uint32_t *out_channel_id, opcua_uint32_t *out_token_id, opcua_uint32_t *out_seq) {
    if (!server_secure_channel.keys_valid) {
        return false;
    }
    mu_sym_chunk_info_t si;
    (void)memset(&si, 0, sizeof(si));
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    if (mu_security_policy_sym_mode(server_secure_channel.policy) == MU_SYM_MODE_AEAD_CHACHA20POLY1305) {
        /* ECC-curve25519 AEAD: the per-chunk nonce derives from the previous
           inbound SequenceNumber (Table 69). */
        if (mu_sym_chunk_unwrap_aead(crypto, &server_secure_channel.client_keys, msg, msg_len,
                                     server_secure_channel.in_last_sequence_number, out_req_full, out_req_len,
                                     &si) != MU_STATUS_GOOD) {
            return false;
        }
    } else
#endif
        if (mu_sym_chunk_unwrap(crypto, server_secure_channel.mode, &server_secure_channel.client_keys, msg, msg_len,
                                out_req_full, out_req_len, &si) != MU_STATUS_GOOD) {
        return false;
    }
    *out_request_id = si.request_id;
    *out_channel_id = si.secure_channel_id;
    *out_token_id = si.token_id;
    *out_seq = si.sequence_number;
    return true;
}

/* Secured (or secure-capable) OPN/MSG handling: a crypto adapter is present, so
   OPN chunks are unwrapped/wrapped asymmetrically (None or Basic256Sha256) and
   MSG chunks symmetrically once the channel has keys. MSG chunks are decrypted in
   place in the receive buffer (no request scratch); only the response body needs
   a local buffer. `msg` is the mutable receive buffer. */
void handle_data_chunk_secure(mu_server_t *server, opcua_byte_t *msg, size_t msg_len, bool is_opn) {
    const mu_crypto_adapter_t *crypto = server->config.crypto_adapter;

    if (!is_opn && server_secure_channel.policy == MU_SECURITY_POLICY_NONE_ID) {
        handle_data_chunk_plaintext(server, msg, msg_len, false);
        return;
    }

    opcua_byte_t *respbody = server->secure_scratch;
    opcua_byte_t *opn_buf = server->secure_scratch + MU_SECURE_RESP_MAX;
    const opcua_byte_t *req_full = NULL;
    size_t req_len = 0;
    opcua_uint32_t response_request_id = 0;
    const opcua_byte_t *client_cert = NULL;
    size_t client_cert_len = 0;
    opcua_uint32_t in_channel_id = 0, in_token_id = 0, in_seq = 0;
    mu_string_t opn_security_policy = {-1, NULL};

    if (is_opn) {
        if (!secure_unwrap_opn(server, crypto, msg, msg_len, opn_buf, &opn_security_policy, &req_full, &req_len,
                               &response_request_id, &in_channel_id, &in_seq, &client_cert, &client_cert_len)) {
            return;
        }
    } else {
        if (!secure_unwrap_msg(server, crypto, msg, msg_len, &req_full, &req_len, &response_request_id, &in_channel_id,
                               &in_token_id, &in_seq)) {
            return;
        }
    }

    if (!accept_inbound_chunk(server, is_opn, in_channel_id, in_token_id, in_seq)) {
        abort_connection(server);
        return;
    }
    /* Record this inbound SequenceNumber for the next AEAD chunk's nonce. */
    server_secure_channel.in_last_sequence_number = in_seq;

    mu_binary_reader_t rr;
    mu_binary_reader_init(&rr, req_full, req_len);
    mu_nodeid_t request_type;
    if (mu_binary_read_nodeid(&rr, &request_type) != MU_STATUS_GOOD) {
        return;
    }
    const opcua_byte_t *req_body = req_full + rr.position;
    size_t req_body_len = req_len - rr.position;

    size_t resp_len = MU_SECURE_RESP_MAX;
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    server->current_request_id = response_request_id;
#endif
    opcua_statuscode_t status;
    if (!is_ns0_numeric_nodeid(&request_type)) {
        status = MU_STATUS_BAD_DECODINGERROR;
    } else {
        if (is_opn) {
            mu_service_dispatch_set_opn_security_policy(server, &opn_security_policy);
            mu_bytestring_t cert_bs = {
                (opcua_int32_t)(client_cert_len > (size_t)INT32_MAX ? INT32_MAX : client_cert_len), client_cert};
            mu_service_dispatch_set_opn_client_cert(server, &cert_bs);
        }
        status =
            mu_service_dispatch(server, request_type.identifier.numeric, req_body, req_body_len, respbody, &resp_len);
        if (is_opn) {
            mu_service_dispatch_set_opn_security_policy(server, NULL);
            mu_service_dispatch_set_opn_client_cert(server, NULL);
        }
    }
    bool opn_rejected = ((is_opn) && (status != MU_STATUS_GOOD));
    if (status != MU_STATUS_GOOD) {
        resp_len = MU_SECURE_RESP_MAX;
        if (mu_write_service_fault(respbody, &resp_len, 0, status
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                   ,
                                   server
#endif
                                   ) == MU_STATUS_GOOD) {
            status = MU_STATUS_GOOD;
        } else {
            resp_len = 0;
        }
    }
    if ((status != MU_STATUS_GOOD) || (resp_len == 0u)) {
        return;
    }

    opcua_uint32_t out_seq = ++server_secure_channel.out_sequence_number;
    size_t total = 0;
    opcua_statuscode_t ws;
    if (is_opn) {
        mu_asym_wrap_params_t awp = {.crypto = crypto,
                                     .policy = server_secure_channel.policy,
                                     .secure_channel_id = server_secure_channel.channel_id,
                                     .sequence_number = out_seq,
                                     .request_id = response_request_id,
                                     .receiver_cert = client_cert,
                                     .receiver_cert_len = client_cert_len,
                                     .body = respbody,
                                     .body_len = resp_len,
                                     .out = server->config.send_buffer,
                                     .out_cap = server->config.send_buffer_size,
                                     .out_len = &total};
        ws = mu_asym_chunk_wrap(&awp);
    }
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    else if (mu_security_policy_sym_mode(server_secure_channel.policy) == MU_SYM_MODE_AEAD_CHACHA20POLY1305) {
        /* ECC-curve25519 AEAD outbound: nonce uses the previous outbound
           SequenceNumber (out_seq - 1). */
        ws =
            mu_sym_chunk_wrap_aead(crypto, &server_secure_channel.server_keys, "MSG", server_secure_channel.channel_id,
                                   server_secure_channel.token_id, out_seq, out_seq - 1u, response_request_id, respbody,
                                   resp_len, server->config.send_buffer, server->config.send_buffer_size, &total);
    }
#endif
    else {
        ws = mu_sym_chunk_wrap(crypto, server_secure_channel.mode, &server_secure_channel.server_keys, "MSG",
                               server_secure_channel.channel_id, server_secure_channel.token_id, out_seq,
                               response_request_id, respbody, resp_len, server->config.send_buffer,
                               server->config.send_buffer_size, &total);
    }
    if (ws == MU_STATUS_GOOD) {
        send_buffer_chunk(server, total);
    }
    if (opn_rejected && server_secure_channel.is_open) {
        (void)mu_secure_channel_close(&server_secure_channel);
    }
}
#endif /* MUC_OPCUA_SECURE_CHANNEL_CRYPTO */
