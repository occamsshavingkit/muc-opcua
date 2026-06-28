/* src/core/server_internal.h */
#ifndef MICRO_OPCUA_SERVER_INTERNAL_H
#define MICRO_OPCUA_SERVER_INTERNAL_H

#include "micro_opcua/server.h"
#include "tcp_connection.h"
#include "../services/secure_channel.h"
#include "../services/session.h"
#include "../services/subscription.h"
#include "../address_space/base_nodes.h"
#include "service_dispatch.h"

#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
typedef struct {
    void *client_handle;
    size_t rx_len;
    opcua_uint64_t last_activity_ms;
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;
    opcua_byte_t rx_buffer[2048];
} mu_connection_t;
#endif

struct mu_server {
    mu_server_config_t config;
    mu_base_runtime_nodes_t runtime_base;
    mu_address_space_index_t user_address_space_index;
    opcua_boolean_t is_running;
    
#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
    mu_connection_t conns[MU_MAX_CONNECTIONS];
    mu_connection_t *active_conn;
#endif
    void *client_handle;
    size_t rx_len;              /* bytes accumulated in config.receive_buffer (stream reassembly) */
    opcua_uint64_t last_activity_ms; /* monotonic tick of last inbound traffic (idle timeout) */
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;

#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
#define server_secure_channel (*(server->active_conn ? &server->active_conn->secure_channel : &server->secure_channel))
#define server_tcp_conn (*(server->active_conn ? &server->active_conn->tcp_conn : &server->tcp_conn))
#define server_client_handle (*(server->active_conn ? &server->active_conn->client_handle : &server->client_handle))
#define server_rx_len (*(server->active_conn ? &server->active_conn->rx_len : &server->rx_len))
#define server_last_activity_ms (*(server->active_conn ? &server->active_conn->last_activity_ms : &server->last_activity_ms))
#else
#define server_secure_channel (server->secure_channel)
#define server_tcp_conn (server->tcp_conn)
#define server_client_handle (server->client_handle)
#define server_rx_len (server->rx_len)
#define server_last_activity_ms (server->last_activity_ms)
#endif

    mu_string_t opn_pending_security_policy;
#ifdef MICRO_OPCUA_SECURITY
    opcua_byte_t secure_scratch[MU_SECURE_SCRATCH_SIZE];
#endif
    mu_session_t sessions[MU_MAX_SESSIONS];
    mu_session_t *active_session;
#if MICRO_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_t subs;
    opcua_uint32_t current_request_id;
#endif
};

#if MICRO_OPCUA_SUBSCRIPTIONS
opcua_statuscode_t mu_server_emit_message(mu_server_t *server,
                                          opcua_uint32_t request_id,
                                          const opcua_byte_t *body,
                                          size_t body_len);
#endif

#endif /* MICRO_OPCUA_SERVER_INTERNAL_H */
