/* src/core/server/common.h */
#ifndef MUC_OPCUA_SERVER_COMMON_H
#define MUC_OPCUA_SERVER_COMMON_H

#include "../server_internal.h"

/* Connect-phase idle timeout: a peer that connects but does not open a secure
   channel within this window is dropped so the single slot can be reused. */
#define MU_CONNECT_TIMEOUT_MS 30000

/* poll.c → process_message.c */
void process_message(struct mu_server *server, opcua_byte_t *msg, size_t msg_len);

/* data_chunk.c → process_message.c */
void send_buffer_chunk(struct mu_server *server, size_t total);
void send_tcp_error_chunk(struct mu_server *server, opcua_statuscode_t error_code);
void abort_connection(struct mu_server *server);
bool accept_inbound_chunk(struct mu_server *server, bool is_opn, opcua_uint32_t channel_id, opcua_uint32_t token_id,
                          opcua_uint32_t sequence_number);
bool is_ns0_numeric_nodeid(const mu_nodeid_t *nodeid);
void handle_data_chunk_plaintext(struct mu_server *server, const opcua_byte_t *msg, size_t msg_len, bool is_opn);
#ifdef MUC_OPCUA_SECURITY
void handle_data_chunk_secure(struct mu_server *server, opcua_byte_t *msg, size_t msg_len, bool is_opn);
#endif

#endif /* MUC_OPCUA_SERVER_COMMON_H */
