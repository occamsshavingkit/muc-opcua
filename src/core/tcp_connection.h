/* src/core/tcp_connection.h */
#ifndef MUC_OPCUA_TCP_CONNECTION_H
#define MUC_OPCUA_TCP_CONNECTION_H

#include "muc_opcua/server.h"

typedef enum { MU_TCP_STATE_CLOSED = 0, MU_TCP_STATE_CONNECTED, MU_TCP_STATE_ESTABLISHED } mu_tcp_state_t;

typedef struct {
    mu_tcp_state_t state;
    opcua_uint32_t receive_buffer_size;
    opcua_uint32_t send_buffer_size;
    opcua_uint32_t max_message_size;
    opcua_uint32_t max_chunk_count;
    opcua_uint32_t protocol_version;
} mu_tcp_connection_t;

void mu_tcp_connection_init(mu_tcp_connection_t *connection);

opcua_statuscode_t mu_tcp_process_hello(mu_tcp_connection_t *connection, const opcua_byte_t *message, size_t length,
                                        const mu_server_config_t *config, opcua_byte_t *ack_message,
                                        size_t *ack_length);

opcua_statuscode_t mu_tcp_create_error_message(opcua_statuscode_t error_code, const char *reason,
                                               opcua_byte_t *err_message, size_t *err_length);

#if MUC_OPCUA_REVERSE_CONNECT
/* Build the ReverseHello (RHE) message a server-initiated connection must send as its
   first Message (OPC-10000-6 §7.1.3 / §7.1.2.6). Header 'RHE'+'F'+MessageSize, then body
   ServerUri = config->application_uri and EndpointUrl = config->endpoint_url (each an OPC UA
   String whose encoded value shall be < 4096 bytes). On success *length is the RHE size. */
opcua_statuscode_t mu_tcp_create_reverse_hello(const mu_server_config_t *config, opcua_byte_t *buffer, size_t *length);
#endif

#endif /* MUC_OPCUA_TCP_CONNECTION_H */
