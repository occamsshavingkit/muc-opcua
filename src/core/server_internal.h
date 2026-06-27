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

struct mu_server {
    mu_server_config_t config;
    mu_base_runtime_nodes_t runtime_base;
    opcua_boolean_t is_running;
    
    void *client_handle;
    size_t rx_len;              /* bytes accumulated in config.receive_buffer (stream reassembly) */
    opcua_uint64_t last_activity_ms; /* monotonic tick of last inbound traffic (idle timeout) */
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;
    mu_session_t session;
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
