/* src/core/tcp_connection.h */
#ifndef MICRO_OPCUA_TCP_CONNECTION_H
#define MICRO_OPCUA_TCP_CONNECTION_H

#include "micro_opcua/server.h"

typedef enum {
    MU_TCP_STATE_CLOSED = 0,
    MU_TCP_STATE_CONNECTED,
    MU_TCP_STATE_ESTABLISHED
} mu_tcp_state_t;

typedef struct {
    mu_tcp_state_t state;
    opcua_uint32_t receive_buffer_size;
    opcua_uint32_t send_buffer_size;
    opcua_uint32_t max_message_size;
    opcua_uint32_t max_chunk_count;
    opcua_uint32_t protocol_version;
} mu_tcp_connection_t;

void mu_tcp_connection_init(mu_tcp_connection_t *connection);

opcua_statuscode_t mu_tcp_process_hello(
    mu_tcp_connection_t *connection, 
    const opcua_byte_t *message, 
    size_t length, 
    const mu_server_config_t *config, 
    opcua_byte_t *ack_message, 
    size_t *ack_length);

opcua_statuscode_t mu_tcp_create_error_message(
    opcua_statuscode_t error_code,
    const char *reason,
    opcua_byte_t *err_message,
    size_t *err_length);

#endif /* MICRO_OPCUA_TCP_CONNECTION_H */
