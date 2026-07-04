/* src/core/server_internal.h */
#ifndef MUC_OPCUA_SERVER_INTERNAL_H
#define MUC_OPCUA_SERVER_INTERNAL_H

#include "../address_space/base_nodes.h"
#include "../services/secure_channel.h"
#include "../services/session.h"
#include "../services/subscription.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/alarms_conditions.h"
#include "service_dispatch.h"
#include "tcp_connection.h"

/* Scratch layout within server->secure_scratch (all under MUC_OPCUA_SECURITY):
   [0,                  MU_SECURE_RESP_MAX)           response body
   [MU_SECURE_RESP_MAX, MU_SECURE_RESP_MAX + OPN_MAX) OPN request unwrap
   [RESP_MAX + OPN_MAX, end)                          session handshake buffers
   Details and _Static_asserts in server.c. */
#ifdef MUC_OPCUA_SECURITY
#define MU_SECURE_RESP_MAX 11264
#define MU_SECURE_OPN_REQ_MAX 1024
#define MU_SECURE_SESSION_OFFSET (MU_SECURE_RESP_MAX + MU_SECURE_OPN_REQ_MAX)

/* Session-handshake scratch region — used by CreateSession (to_sign/sig) and
   ActivateSession (verify_buf/decrypt_buf). Handler writes the response body into
   respbody BEFORE signing/verification, so the session region MUST NOT overlap. */
#define MU_SECURE_SESSION_MAX 2048
#define MU_SESSION_TO_SIGN_SIZE 1536
#define MU_SESSION_SIG_SIZE 512
#endif

#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
typedef struct {
    mu_nodeid_t source_node_id;
    mu_reference_t ref;
} mu_dynamic_reference_t;

typedef struct {
    mu_node_t nodes[MU_MAX_DYNAMIC_NODES];
    opcua_byte_t browse_name_storage[MU_MAX_DYNAMIC_NODES][MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH];
    opcua_byte_t display_name_storage[MU_MAX_DYNAMIC_NODES][MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH];
    opcua_byte_t string_nodeid_storage[MU_MAX_DYNAMIC_NODES][MU_MAX_DYNAMIC_STRING_NODEID_LENGTH];
    size_t nodes_count;

    mu_dynamic_reference_t references[MU_MAX_DYNAMIC_REFERENCES];
    opcua_byte_t reference_source_nodeid_storage[MU_MAX_DYNAMIC_REFERENCES]
                                                [MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH];
    opcua_byte_t reference_type_nodeid_storage[MU_MAX_DYNAMIC_REFERENCES]
                                              [MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH];
    opcua_byte_t reference_target_nodeid_storage[MU_MAX_DYNAMIC_REFERENCES]
                                                [MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH];
    size_t references_count;
} mu_dynamic_address_space_t;
#endif

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
typedef struct {
    void *client_handle;
    size_t rx_len;
    opcua_uint64_t last_activity_ms;
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;
    opcua_byte_t rx_buffer[MU_CONNECTION_RX_BUFFER_SIZE];
} mu_connection_t;

_Static_assert(sizeof(mu_connection_t) <= (MU_CONNECTION_RX_BUFFER_SIZE + MU_CONNECTION_BASE_STORAGE_BYTES),
               "MU_CONNECTION_BASE_STORAGE_BYTES must cover mu_connection_t fields outside rx_buffer");
#endif

struct mu_server {
    mu_server_config_t config;
    mu_base_runtime_nodes_t runtime_base;
    mu_address_space_index_t user_address_space_index;
    opcua_boolean_t is_running;

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    mu_connection_t conns[MU_MAX_CONNECTIONS];
    mu_connection_t *active_conn;
#endif
    void *client_handle;
    size_t rx_len;                   /* bytes accumulated in config.receive_buffer (stream reassembly) */
    opcua_uint64_t last_activity_ms; /* monotonic tick of last inbound traffic (idle timeout) */
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
#define server_secure_channel (*(server->active_conn ? &server->active_conn->secure_channel : &server->secure_channel))
#define server_tcp_conn (*(server->active_conn ? &server->active_conn->tcp_conn : &server->tcp_conn))
#define server_client_handle (*(server->active_conn ? &server->active_conn->client_handle : &server->client_handle))
#define server_rx_len (*(server->active_conn ? &server->active_conn->rx_len : &server->rx_len))
#define server_last_activity_ms                                                                                        \
    (*(server->active_conn ? &server->active_conn->last_activity_ms : &server->last_activity_ms))
#else
#define server_secure_channel (server->secure_channel)
#define server_tcp_conn (server->tcp_conn)
#define server_client_handle (server->client_handle)
#define server_rx_len (server->rx_len)
#define server_last_activity_ms (server->last_activity_ms)
#endif

    mu_string_t opn_pending_security_policy;
    mu_bytestring_t opn_pending_client_cert;
#ifdef MUC_OPCUA_SECURITY
    opcua_byte_t secure_scratch[MU_SECURE_SCRATCH_SIZE];
    /* Persistent copy of the current SecureChannel's client application-instance
       certificate, populated at OpenSecureChannel and used to verify the
       ActivateSession ClientSignature (OPC-10000-4 §5.7.3). Needed because the OPN
       sender certificate points into transient receive-buffer memory. */
    opcua_byte_t channel_client_cert[MU_MAX_CLIENT_CERT_SIZE];
    size_t channel_client_cert_len;
#endif
    mu_session_t sessions[MU_MAX_SESSIONS];
    mu_session_t *active_session;
#if MUC_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_t subs;
    opcua_uint32_t current_request_id;
#endif
#ifdef MUC_OPCUA_CUSTOM_METHODS
#define MU_MAX_REGISTERED_METHODS 8
    struct {
        mu_nodeid_t method_id;
        mu_method_callback_t callback;
        void *context;
    } registered_methods[MU_MAX_REGISTERED_METHODS];
    size_t registered_method_count;
#endif

#ifdef MUC_OPCUA_PUBSUB
#define MU_MAX_WRITER_GROUPS 2
    mu_pubsub_writer_group_t writer_groups[MU_MAX_WRITER_GROUPS];
    size_t writer_group_count;
#endif

#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
    mu_dynamic_address_space_t dynamic_address_space;
#endif

#ifdef MUC_OPCUA_SERVICE_QUERY
    struct {
        struct {
            opcua_byte_t id_buf[8];
            mu_string_t id;
            opcua_uint32_t session_id;
            size_t next_index; /* Index into address space to resume from */
            opcua_uint64_t timestamp_ms;
        } continuation_points[MU_MAX_QUERY_CONTINUATION_POINTS];
    } query_context;
#endif

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
    mu_condition_t conditions[MU_MAX_CONDITIONS];
    size_t condition_count;
#endif
};

#if MUC_OPCUA_SUBSCRIPTIONS
opcua_statuscode_t mu_server_emit_message(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *body,
                                          size_t body_len);
#endif

#endif /* MUC_OPCUA_SERVER_INTERNAL_H */
