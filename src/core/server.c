/* src/core/server.c */
#include "muc_opcua/server.h"
#include "../services/secure_channel.h"
#include "message_chunk.h"
#include "muc_opcua/encoding.h"
#include "server_internal.h"
#include "service_message.h"
#include "uasc.h"
#ifdef MUC_OPCUA_SECURITY
#include "../security/asym_chunk.h"
#include "../security/sym_chunk.h"
#endif
#include <string.h>

void mu_service_dispatch_set_opn_security_policy(mu_server_t *server, const mu_string_t *security_policy);
void mu_service_dispatch_set_opn_client_cert(mu_server_t *server, const mu_bytestring_t *client_cert);

opcua_statuscode_t mu_server_config_validate(const mu_server_config_t *config) {
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

    /* Validate limits. An upper bound isn't just belt-and-suspenders: without it
       a caller requesting more sessions/channels than this build was compiled
       for (MU_MAX_SESSIONS / MU_MAX_CONNECTIONS size the static sessions[]/
       conns[] arrays) was previously accepted here and then silently capped at
       the compiled size by CreateSession / the connection accept path, with no
       indication the requested capacity was never actually available. */
    if (config->max_sessions == 0 || config->max_sessions > MU_MAX_SESSIONS) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (config->max_secure_channels == 0 || config->max_secure_channels > MU_MAX_CONNECTIONS) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (config->max_chunk_count == 0 || config->max_message_size < MU_MIN_CHUNK_SIZE) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate Platform Adapters */
    if (config->tcp_adapter.listen == NULL || config->tcp_adapter.accept == NULL || config->tcp_adapter.read == NULL ||
        config->tcp_adapter.write == NULL || config->tcp_adapter.close_connection == NULL ||
        config->tcp_adapter.shutdown == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (config->time_adapter.get_time == NULL || config->time_adapter.get_tick_ms == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (config->entropy_adapter.generate_random == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate the server's own certificate is within its validity window. */
    if (config->crypto_adapter != NULL) {
        const opcua_byte_t *own_cert = NULL;
        size_t own_cert_len = 0;
        if (config->crypto_adapter->get_own_certificate != NULL &&
            config->crypto_adapter->verify_certificate_validity != NULL) {
            opcua_statuscode_t cert_ret =
                config->crypto_adapter->get_own_certificate(config->crypto_adapter->context, &own_cert, &own_cert_len);
            if (cert_ret != MU_STATUS_GOOD) {
                return cert_ret;
            }
            opcua_statuscode_t cert_status = config->crypto_adapter->verify_certificate_validity(
                config->crypto_adapter->context, own_cert, own_cert_len);
            if (cert_status != MU_STATUS_GOOD) {
                return cert_status;
            }
        }
    }

    /* Validate address space if provided */
    if (config->address_space != NULL) {
        opcua_statuscode_t status = mu_address_space_validate(config->address_space);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        /* Feature 025 (F6): the user address-space sort index is a fixed
           order[MU_MAX_ADDRESS_SPACE_NODES] array. A larger space previously fell
           back to a silent O(n) linear scan AND made Query return nothing. Fail
           loudly at init instead (Constitution §VII: predictable failure over
           ambiguous degradation). Integrators needing more must raise
           MU_MAX_ADDRESS_SPACE_NODES, which resizes the index storage. */
        if (config->address_space->node_count > MU_MAX_ADDRESS_SPACE_NODES) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_server_init(void *storage, size_t storage_size, const mu_server_config_t *config,
                                  mu_server_t **out_server) {
    mu_server_t *server;
    opcua_statuscode_t status;

    if (storage == NULL || out_server == NULL || config == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* The server struct is placed directly in caller storage and contains pointers
       and size_t, so the buffer must be suitably aligned (word access to a
       misaligned struct faults on Cortex-M0+). */
    if (((uintptr_t)storage % _Alignof(struct mu_server)) != 0) {
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
    server->user_address_space_index = (mu_address_space_index_t){0};
    /* opn_pending_security_policy and opn_pending_client_cert are transient
       pointers into the receive buffer, valid only within a single
       mu_server_poll cycle (safe for single-threaded poll model). */
    server->opn_pending_security_policy.length = -1;
    server->opn_pending_security_policy.data = NULL;
    {
        opcua_datetime_t start = server->config.time_adapter.get_time
                                     ? server->config.time_adapter.get_time(server->config.time_adapter.context)
                                     : 0;
        mu_base_runtime_init(&server->runtime_base, &server->config.time_adapter, start);
    }
    server->is_running = true;
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    for (size_t i = 0; i < MU_MAX_CONNECTIONS; ++i) {
        server->conns[i].client_handle = NULL;
        mu_tcp_connection_init(&server->conns[i].tcp_conn);
        mu_secure_channel_init(&server->conns[i].secure_channel);
        server->conns[i].rx_len = 0;
    }
    server->active_conn = NULL;
#else
    server_client_handle = NULL;
    mu_tcp_connection_init(&server_tcp_conn);
    mu_secure_channel_init(&server_secure_channel);
#endif
    for (size_t i = 0; i < MU_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
    server->active_session = NULL;
#if MUC_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_init(&server->subs);
#endif
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
    memset(&server->dynamic_address_space, 0, sizeof(server->dynamic_address_space));
#endif
#ifdef MUC_OPCUA_SERVICE_QUERY
    for (size_t i = 0; i < MU_MAX_QUERY_CONTINUATION_POINTS; ++i) {
        server->query_context.continuation_points[i].id.length = -1;
        server->query_context.continuation_points[i].id.data = NULL;
        server->query_context.continuation_points[i].session_id = 0;
        server->query_context.continuation_points[i].next_index = 0;
        server->query_context.continuation_points[i].timestamp_ms = 0;
    }
#endif

    /* Initialize platform TCP adapter */
    status = server->config.tcp_adapter.listen(server->config.tcp_adapter.context, server->config.endpoint_url);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    *out_server = server;
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_SECURITY
_Static_assert(MU_SECURE_RESP_MAX + MU_SECURE_OPN_REQ_MAX + MU_SECURE_SESSION_MAX <= MU_SECURE_SCRATCH_SIZE,
               "secure scratch must hold response, OPN request, and session-handshake buffers");
#endif

/* Connect-phase idle timeout: a peer that connects but does not open a secure
   channel within this window is dropped so the single slot can be reused. */
#define MU_CONNECT_TIMEOUT_MS 30000

/* Send a fully-framed chunk from the send buffer. The transport is non-blocking,
   so a write error or short write would leave a half-framed chunk on the wire;
   drop the connection in that case rather than wedge the slot. */
static void send_buffer_chunk(mu_server_t *server, size_t total) {
    size_t bytes_written = 0;
    opcua_statuscode_t s = server->config.tcp_adapter.write(server->config.tcp_adapter.context, server_client_handle,
                                                            server->config.send_buffer, total, &bytes_written);
    if (s != MU_STATUS_GOOD || bytes_written != total) {
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
        server_client_handle = NULL;
    }
}

static void send_tcp_error_chunk(mu_server_t *server, opcua_statuscode_t error_code) {
    size_t err_len = server->config.send_buffer_size;
    if (mu_tcp_create_error_message(error_code, NULL, server->config.send_buffer, &err_len) == MU_STATUS_GOOD) {
        send_buffer_chunk(server, err_len);
    }
}

#if MUC_OPCUA_SUBSCRIPTIONS
opcua_statuscode_t mu_server_emit_message(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *body,
                                          size_t body_len) {
    if (server == NULL || body == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    /* If active_conn is NULL, find connection by active session's secure_channel_id */
    if (server->active_conn == NULL && server->active_session != NULL) {
        for (size_t i = 0; i < MU_MAX_CONNECTIONS; ++i) {
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

#ifdef MUC_OPCUA_SECURITY
    if (server_secure_channel.mode != MU_MESSAGE_SECURITY_MODE_NONE) {
        size_t total = 0;
        opcua_statuscode_t status;

        if (server->config.crypto_adapter == NULL || !server_secure_channel.keys_valid) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }

        status =
            mu_sym_chunk_wrap(server->config.crypto_adapter, server_secure_channel.mode,
                              &server_secure_channel.server_keys, "MSG", server_secure_channel.channel_id,
                              server_secure_channel.token_id, ++server_secure_channel.out_sequence_number, request_id,
                              body, body_len, server->config.send_buffer, server->config.send_buffer_size, &total);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        send_buffer_chunk(server, total);
        return MU_STATUS_GOOD;
    }
#endif

    if (body_len > server->config.send_buffer_size || MU_UASC_SYMMETRIC_HEADER_SIZE > server->config.send_buffer_size ||
        body_len > (server->config.send_buffer_size - MU_UASC_SYMMETRIC_HEADER_SIZE)) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }

    memcpy(server->config.send_buffer + MU_UASC_SYMMETRIC_HEADER_SIZE, body, body_len);

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
static void abort_connection(mu_server_t *server) {
    server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
    server_client_handle = NULL;
}

/* Validate an inbound chunk's sequencing and, for MSG, its channel/token binding.
   Returns false if the chunk replays/skips a SequenceNumber (OPC 10000-6 6.7.2.4)
   or is addressed to a different SecureChannel/Token — the caller must abort. */
static bool accept_inbound_chunk(mu_server_t *server, bool is_opn, opcua_uint32_t channel_id, opcua_uint32_t token_id,
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
static bool is_ns0_numeric_nodeid(const mu_nodeid_t *nodeid) {
    return nodeid->identifier_type == MU_NODEID_NUMERIC && nodeid->namespace_index == 0u;
}

/* Plaintext (SecurityPolicy None) OPN/MSG handling — the only path when no crypto
   adapter is configured. Dispatch writes the response body directly into the send
   buffer past the fixed UASC header, then a finalize step frames the chunk. */
static void handle_data_chunk_plaintext(mu_server_t *server, const opcua_byte_t *msg, size_t msg_len, bool is_opn) {
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

#if MUC_OPCUA_SUBSCRIPTIONS
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
        if (mu_write_service_fault(resp_body, &payload_len, 0, status) == MU_STATUS_GOOD) {
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

#ifdef MUC_OPCUA_SECURITY
/* Secured (or secure-capable) OPN/MSG handling: a crypto adapter is present, so
   OPN chunks are unwrapped/wrapped asymmetrically (None or Basic256Sha256) and
   MSG chunks symmetrically once the channel has keys. MSG chunks are decrypted in
   place in the receive buffer (no request scratch); only the response body needs
   a local buffer. `msg` is the mutable receive buffer. */
static void handle_data_chunk_secure(mu_server_t *server, opcua_byte_t *msg, size_t msg_len, bool is_opn) {
    const mu_crypto_adapter_t *crypto = server->config.crypto_adapter;

    /* MSG on a None channel is plaintext — handle before reserving secure scratch. */
    if (!is_opn && server_secure_channel.policy == MU_SECURITY_POLICY_NONE_ID) {
        handle_data_chunk_plaintext(server, msg, msg_len, false);
        return;
    }

    /* Single-connection server processes one chunk per poll, so shared secure
       scratch is not reentered while the request/response is in flight. */
    opcua_byte_t *respbody = server->secure_scratch;
    opcua_byte_t *opn_buf = server->secure_scratch + MU_SECURE_RESP_MAX; /* OPN request only */
    const opcua_byte_t *req_full = NULL;                                 /* [request-type NodeId][RequestHeader][...] */
    size_t req_len = 0;
    opcua_uint32_t response_request_id = 0;
    const opcua_byte_t *client_cert = NULL;
    size_t client_cert_len = 0;
    opcua_uint32_t in_channel_id = 0, in_token_id = 0, in_seq = 0;
    mu_string_t opn_security_policy = {-1, NULL};

    if (is_opn) {
        mu_asym_chunk_info_t ai;
        mu_binary_reader_t header_reader;
        opcua_uint32_t header_channel_id;

        mu_binary_reader_init(&header_reader, msg, msg_len);
        header_reader.position = 8;
        if (mu_binary_read_uint32(&header_reader, &header_channel_id) != MU_STATUS_GOOD) {
            return;
        }
        if (mu_binary_read_string(&header_reader, &opn_security_policy) != MU_STATUS_GOOD) {
            return;
        }

        memset(&ai, 0, sizeof(ai));
        if (mu_asym_chunk_unwrap(crypto, msg, msg_len, opn_buf, MU_SECURE_OPN_REQ_MAX, &req_len, server->secure_scratch,
                                 MU_SECURE_SCRATCH_SIZE, &ai) != MU_STATUS_GOOD) {
            return;
        }
        /* Record the negotiated policy before dispatch so the OPN handler can
           derive channel keys; the client cert is needed to encrypt the reply. */
        server_secure_channel.policy = ai.policy;
        response_request_id = ai.request_id;
        client_cert = ai.sender_cert;
        client_cert_len = ai.sender_cert_len;
        req_full = opn_buf;
        in_channel_id = ai.secure_channel_id;
        in_seq = ai.sequence_number;
    } else {
        if (!server_secure_channel.keys_valid) {
            return;
        }
        mu_sym_chunk_info_t si;
        memset(&si, 0, sizeof(si));
        /* Decrypts msg in place; req_full points into msg. */
        if (mu_sym_chunk_unwrap(crypto, server_secure_channel.mode, &server_secure_channel.client_keys, msg, msg_len,
                                &req_full, &req_len, &si) != MU_STATUS_GOOD) {
            return;
        }
        response_request_id = si.request_id;
        in_channel_id = si.secure_channel_id;
        in_token_id = si.token_id;
        in_seq = si.sequence_number;
    }

    if (!accept_inbound_chunk(server, is_opn, in_channel_id, in_token_id, in_seq)) {
        abort_connection(server);
        return;
    }

    mu_binary_reader_t rr;
    mu_binary_reader_init(&rr, req_full, req_len);
    mu_nodeid_t request_type;
    if (mu_binary_read_nodeid(&rr, &request_type) != MU_STATUS_GOOD) {
        return;
    }
    const opcua_byte_t *req_body = req_full + rr.position;
    size_t req_body_len = req_len - rr.position;

    size_t resp_len = MU_SECURE_RESP_MAX;
#if MUC_OPCUA_SUBSCRIPTIONS
    server->current_request_id = response_request_id;
#endif
    opcua_statuscode_t status;
    if (!is_ns0_numeric_nodeid(&request_type)) {
        status = MU_STATUS_BAD_DECODINGERROR;
    } else {
        if (is_opn) {
            mu_service_dispatch_set_opn_security_policy(server, &opn_security_policy);
            mu_bytestring_t cert_bs = {(opcua_int32_t)client_cert_len, client_cert};
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
        if (mu_write_service_fault(respbody, &resp_len, 0, status) == MU_STATUS_GOOD) {
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
        ws = mu_asym_chunk_wrap(crypto, server_secure_channel.policy, server_secure_channel.channel_id, out_seq,
                                response_request_id, client_cert, client_cert_len, respbody, resp_len,
                                server->config.send_buffer, server->config.send_buffer_size, &total);
    } else {
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
#endif /* MUC_OPCUA_SECURITY */

/* Process one complete message (HELLO during connect, otherwise an OPN/MSG/CLO
   chunk) and send any response. Sets client_handle to NULL if the connection is
   closed (HELLO failure or CloseSecureChannel). */
static void process_message(mu_server_t *server, opcua_byte_t *msg, size_t msg_len) {
    opcua_statuscode_t status;

    if (server_tcp_conn.state == MU_TCP_STATE_CLOSED) {
        size_t ack_length = server->config.send_buffer_size;
        status = mu_tcp_process_hello(&server_tcp_conn, msg, msg_len, &server->config, server->config.send_buffer,
                                      &ack_length);
        if (status == MU_STATUS_GOOD) {
            send_buffer_chunk(server, ack_length); /* closes on write failure */
        } else {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
            server_client_handle = NULL;
        }
        return;
    }

    mu_message_header_t header = {{0, 0, 0}, 0, 0, 0};
    status = mu_parse_message_header(msg, msg_len, &header);
    if (status != MU_STATUS_GOOD) {
        if (header.chunk_type == 'C') {
            send_tcp_error_chunk(server, status);
        }
        return;
    }

    bool is_opn = header.message_type[0] == 'O' && header.message_type[1] == 'P' && header.message_type[2] == 'N';
    bool is_msg = header.message_type[0] == 'M' && header.message_type[1] == 'S' && header.message_type[2] == 'G';
    bool is_clo = header.message_type[0] == 'C' && header.message_type[1] == 'L' && header.message_type[2] == 'O';

    if (is_clo) {
        mu_secure_channel_close(&server_secure_channel);
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
        server_client_handle = NULL;
        return;
    }
    if (!is_opn && !is_msg) {
        return; /* unsupported chunk type: ignore */
    }

#ifdef MUC_OPCUA_SECURITY
    if (server->config.crypto_adapter != NULL) {
        handle_data_chunk_secure(server, msg, msg_len, is_opn);
        return;
    }
#endif
    handle_data_chunk_plaintext(server, msg, msg_len, is_opn);
}

static opcua_statuscode_t mu_server_poll_background(mu_server_t *server) {
    (void)server;

#ifdef MUC_OPCUA_PUBSUB
    opcua_statuscode_t status = mu_pubsub_poll(server);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
#endif

#if MUC_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_tick(server, server->config.time_adapter.get_tick_ms(server->config.time_adapter.context));
#endif

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_server_poll(mu_server_t *server) {
    if (server == NULL || !server->is_running) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    /* 1. Try to accept a new connection if we have a free slot */
    int free_slot = -1;
    for (size_t i = 0; i < MU_MAX_CONNECTIONS; ++i) {
        if (server->conns[i].client_handle == NULL) {
            free_slot = (int)i;
            break;
        }
    }

    void *new_handle = NULL;
    opcua_statuscode_t status = server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &new_handle);
    if (status == MU_STATUS_GOOD && new_handle != NULL) {
        if (free_slot != -1) {
            mu_connection_t *conn = &server->conns[free_slot];
            conn->client_handle = new_handle;
            conn->rx_len = 0;
            conn->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
            mu_tcp_connection_init(&conn->tcp_conn);
            mu_secure_channel_init(&conn->secure_channel);
            return mu_server_poll_background(server);
        } else {
            /* Server full: send error message and close */
            opcua_byte_t buf[256];
            size_t err_len = sizeof(buf);
            if (mu_tcp_create_error_message(MU_STATUS_BAD_TCPNOTENOUGHRESOURCES, "Server full", buf, &err_len) ==
                MU_STATUS_GOOD) {
                size_t bytes_written;
                server->config.tcp_adapter.write(server->config.tcp_adapter.context, new_handle, buf, err_len,
                                                 &bytes_written);
            }
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, new_handle);
            return mu_server_poll_background(server);
        }
    }

    /* 2. Poll and read/process each active connection */
    for (size_t i = 0; i < MU_MAX_CONNECTIONS; ++i) {
        mu_connection_t *conn = &server->conns[i];
        if (conn->client_handle == NULL) {
            continue;
        }

        /* Check connection timeout */
        opcua_uint64_t now = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        opcua_uint64_t limit = conn->secure_channel.is_open ? (opcua_uint64_t)conn->secure_channel.revised_lifetime
                                                            : (opcua_uint64_t)MU_CONNECT_TIMEOUT_MS;
        if (now > conn->last_activity_ms && (now - conn->last_activity_ms) > limit) {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, conn->client_handle);
            conn->client_handle = NULL;
            conn->rx_len = 0;
            continue;
        }

        /* Read from connection */
        if (conn->rx_len < sizeof(conn->rx_buffer)) {
            size_t bytes_read = 0;
            status = server->config.tcp_adapter.read(server->config.tcp_adapter.context, conn->client_handle,
                                                     conn->rx_buffer + conn->rx_len,
                                                     sizeof(conn->rx_buffer) - conn->rx_len, &bytes_read);

            if (status != MU_STATUS_GOOD) {
                server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, conn->client_handle);
                conn->client_handle = NULL;
                conn->rx_len = 0;
                continue;
            }
            if (bytes_read > 0) {
                conn->rx_len += bytes_read;
                conn->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
            }
        }

        /* Set active connection context so dispatch / wrap / process_message uses it */
        server->active_conn = conn;

        /* Process complete messages in the buffer */
        size_t consumed = 0;
        while (conn->client_handle != NULL && (conn->rx_len - consumed) >= 8) {
            const opcua_byte_t *b = conn->rx_buffer + consumed;
            size_t msg_size = (size_t)b[4] | ((size_t)b[5] << 8) | ((size_t)b[6] << 16) | ((size_t)b[7] << 24);

            if (msg_size < 8 || msg_size > sizeof(conn->rx_buffer)) {
                server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, conn->client_handle);
                conn->client_handle = NULL;
                conn->rx_len = 0;
                break;
            }
            if (msg_size > (conn->rx_len - consumed)) {
                break;
            }

            process_message(server, conn->rx_buffer + consumed, msg_size);

            if (conn->client_handle == NULL) {
                conn->rx_len = 0;
                break;
            }

            consumed += msg_size;
        }

        if (conn->client_handle != NULL && consumed > 0) {
            size_t remaining = conn->rx_len - consumed;
            if (remaining > 0) {
                memmove(conn->rx_buffer, conn->rx_buffer + consumed, remaining);
            }
            conn->rx_len = remaining;
        }

        server->active_conn = NULL;
    }
#else
    /* 1. Accept connections if none active */
    if (server_client_handle == NULL) {
        opcua_statuscode_t status =
            server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &server_client_handle);
        if (status == MU_STATUS_GOOD && server_client_handle != NULL) {
            mu_tcp_connection_init(&server_tcp_conn);
            mu_secure_channel_init(&server_secure_channel);
            for (size_t i = 0; i < MU_MAX_SESSIONS; ++i) {
                mu_session_init(&server->sessions[i]);
            }
            server->active_session = NULL;
#if MUC_OPCUA_SUBSCRIPTIONS
            mu_subscriptions_init(&server->subs);
#endif
            server_rx_len = 0;
            server_last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        }
        return mu_server_poll_background(server);
    } else {
        /* Reclaim the single slot from an idle/stuck peer: a connection with no
           inbound traffic for its channel lifetime (or the connect timeout before
           a channel is open) is dropped. A monotonic tick of 0 (stub adapter)
           disables the timeout. */
        opcua_uint64_t now = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        opcua_uint64_t limit = server_secure_channel.is_open ? (opcua_uint64_t)server_secure_channel.revised_lifetime
                                                             : (opcua_uint64_t)MU_CONNECT_TIMEOUT_MS;
        if (now > server_last_activity_ms && (now - server_last_activity_ms) > limit) {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
            server_client_handle = NULL;
            server_rx_len = 0;
            return mu_server_poll_background(server);
        }
        /* Check if there's another connection waiting to be rejected */
        void *second_handle = NULL;
        opcua_statuscode_t status =
            server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &second_handle);
        if (status == MU_STATUS_GOOD && second_handle != NULL) {
            /* We don't have resources. OPC UA 10000-6 7.1.5: immediately close or wait for HEL and send ERR.
               Wait for HEL (simulate it by just trying to read once) */
            opcua_byte_t buf[256];
            size_t bytes_read = 0;
            status = server->config.tcp_adapter.read(server->config.tcp_adapter.context, second_handle, buf,
                                                     sizeof(buf), &bytes_read);

            if (status == MU_STATUS_GOOD && bytes_read >= 8 && buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'L') {
                size_t err_len = sizeof(buf);
                if (mu_tcp_create_error_message(MU_STATUS_BAD_TCPNOTENOUGHRESOURCES, "Server full", buf, &err_len) ==
                    MU_STATUS_GOOD) {
                    size_t bytes_written;
                    server->config.tcp_adapter.write(server->config.tcp_adapter.context, second_handle, buf, err_len,
                                                     &bytes_written);
                }
            }
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, second_handle);
        }
    }

    /* 2. Read from the active connection, appending to the receive buffer. */
    if (server_rx_len < server->config.receive_buffer_size) {
        size_t bytes_read = 0;
        opcua_statuscode_t status = server->config.tcp_adapter.read(
            server->config.tcp_adapter.context, server_client_handle, server->config.receive_buffer + server_rx_len,
            server->config.receive_buffer_size - server_rx_len, &bytes_read);

        if (status != MU_STATUS_GOOD) {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
            server_client_handle = NULL;
            server_rx_len = 0;
            return mu_server_poll_background(server);
        }
        if (bytes_read > 0) {
            server_rx_len += bytes_read;
            server_last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        }
    }

    /* 3. Process every complete message in the buffer, reassembling the stream:
       a single read() may carry several messages or only part of one. */
    size_t consumed = 0;
    while (server_client_handle != NULL && (server_rx_len - consumed) >= 8) {
        const opcua_byte_t *b = server->config.receive_buffer + consumed;
        /* MessageSize is a UInt32 at byte offset 4 of every TCP/UASC message. */
        size_t msg_size = (size_t)b[4] | ((size_t)b[5] << 8) | ((size_t)b[6] << 16) | ((size_t)b[7] << 24);

        if (msg_size < 8 || msg_size > server->config.receive_buffer_size) {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server_client_handle);
            server_client_handle = NULL;
            server_rx_len = 0;
            return mu_server_poll_background(server);
        }
        if (msg_size > (server_rx_len - consumed)) {
            break; /* incomplete: wait for more bytes on a later poll */
        }

        process_message(server, server->config.receive_buffer + consumed, msg_size);

        if (server_client_handle == NULL) {
            server_rx_len = 0;
            break;
        }

        consumed += msg_size;
    }

    if (server_client_handle != NULL && consumed > 0) {
        size_t remaining = server_rx_len - consumed;
        if (remaining > 0) {
            memmove(server->config.receive_buffer, server->config.receive_buffer + consumed, remaining);
        }
        server_rx_len = remaining;
    }
#endif

    return mu_server_poll_background(server);
}

void mu_server_close(mu_server_t *server) {
    if (server != NULL) {
        server->is_running = false;
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
        for (size_t i = 0; i < MU_MAX_CONNECTIONS; ++i) {
            if (server->conns[i].client_handle != NULL) {
                server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context,
                                                            server->conns[i].client_handle);
                mu_secure_channel_close(&server->conns[i].secure_channel);
                server->conns[i].client_handle = NULL;
            }
        }
#else
        mu_secure_channel_close(&server_secure_channel);
#endif
        if (server->config.tcp_adapter.shutdown != NULL) {
            server->config.tcp_adapter.shutdown(server->config.tcp_adapter.context);
        }
    }
}

#ifdef MUC_OPCUA_CUSTOM_METHODS
opcua_statuscode_t mu_server_register_method_callback(mu_server_t *server, const mu_nodeid_t *method_id,
                                                      mu_method_callback_t callback, void *context) {
    if (server == NULL || method_id == NULL || callback == NULL) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }

    if (server->registered_method_count >= MU_MAX_REGISTERED_METHODS) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    /* Check if already registered */
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (mu_nodeid_equal(&server->registered_methods[i].method_id, method_id)) {
            server->registered_methods[i].callback = callback;
            server->registered_methods[i].context = context;
            return MU_STATUS_GOOD;
        }
    }

    size_t idx = server->registered_method_count++;
    server->registered_methods[idx].method_id = *method_id;
    server->registered_methods[idx].callback = callback;
    server->registered_methods[idx].context = context;

    return MU_STATUS_GOOD;
}
#endif
