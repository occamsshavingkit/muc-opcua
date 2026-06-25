/* src/core/server_internal.h */
#ifndef MICRO_OPCUA_SERVER_INTERNAL_H
#define MICRO_OPCUA_SERVER_INTERNAL_H

#include "micro_opcua/server.h"
#include "tcp_connection.h"
#include "../services/secure_channel.h"
#include "../services/session.h"
#include "service_dispatch.h"

struct mu_server {
    mu_server_config_t config;
    opcua_boolean_t is_running;
    
    void *client_handle;
    mu_tcp_connection_t tcp_conn;
    mu_secure_channel_t secure_channel;
    mu_session_t session;
};

#endif /* MICRO_OPCUA_SERVER_INTERNAL_H */
