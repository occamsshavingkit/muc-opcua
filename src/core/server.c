/* src/core/server.c */
#include "micro_opcua/server.h"
#include "server_internal.h"
#include "message_chunk.h"
#include "service_message.h"
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
        }
        return MU_STATUS_GOOD;
    }

    /* 2. Read from active connection */
    size_t bytes_read = 0;
    opcua_statuscode_t status = server->config.tcp_adapter.read(
        server->config.tcp_adapter.context, 
        server->client_handle, 
        server->config.receive_buffer, 
        server->config.receive_buffer_size, 
        &bytes_read);
        
    if (status != MU_STATUS_GOOD) {
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server->client_handle);
        server->client_handle = NULL;
        return MU_STATUS_GOOD;
    }
    
    if (bytes_read == 0) {
        return MU_STATUS_GOOD;
    }

    /* 3. Process data */
    if (server->tcp_conn.state == MU_TCP_STATE_CLOSED) {
        size_t ack_length = server->config.send_buffer_size;
        status = mu_tcp_process_hello(&server->tcp_conn, 
                                      server->config.receive_buffer, bytes_read, 
                                      &server->config, 
                                      server->config.send_buffer, &ack_length);
        if (status == MU_STATUS_GOOD) {
            size_t bytes_written;
            server->config.tcp_adapter.write(server->config.tcp_adapter.context, server->client_handle, server->config.send_buffer, ack_length, &bytes_written);
        } else {
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server->client_handle);
            server->client_handle = NULL;
        }
    } else {
        mu_message_header_t header;
        status = mu_parse_message_header(server->config.receive_buffer, bytes_read, &header);
        if (status != MU_STATUS_GOOD) return status;
        
        size_t offset = 8;
        if ((header.message_type[0] == 'O' && header.message_type[1] == 'P' && header.message_type[2] == 'N') ||
            (header.message_type[0] == 'M' && header.message_type[1] == 'S' && header.message_type[2] == 'G')) {
            
            mu_sequence_header_t seq;
            status = mu_parse_sequence_header(server->config.receive_buffer, bytes_read, &offset, &seq);
            if (status != MU_STATUS_GOOD) return status;
            
            mu_nodeid_t request_id;
            status = mu_parse_service_prefix(server->config.receive_buffer, bytes_read, &offset, &request_id);
            if (status != MU_STATUS_GOOD) return status;
            
            size_t req_body_len = bytes_read - offset;
            const opcua_byte_t *req_body = server->config.receive_buffer + offset;
            
            size_t resp_len = server->config.send_buffer_size;
            size_t resp_offset = 8 + 8 + 4;
            size_t payload_max = resp_len - resp_offset;
            size_t payload_len = payload_max;
            
            opcua_byte_t *resp_body = server->config.send_buffer + resp_offset;
            
            status = mu_service_dispatch(server, request_id.identifier.numeric, req_body, req_body_len, resp_body, &payload_len);
            
            if (status == MU_STATUS_GOOD) {
                size_t actual_resp_len = resp_offset + payload_len;
                mu_message_header_t resp_hdr;
                resp_hdr.message_type[0] = header.message_type[0];
                resp_hdr.message_type[1] = header.message_type[1];
                resp_hdr.message_type[2] = header.message_type[2];
                resp_hdr.chunk_type = 'F';
                resp_hdr.message_size = (opcua_uint32_t)actual_resp_len;
                resp_hdr.secure_channel_id = header.secure_channel_id;
                
                mu_write_message_header(server->config.send_buffer, resp_len, &resp_hdr);
                
                size_t w_offset = 8;
                mu_sequence_header_t resp_seq;
                resp_seq.sequence_number = seq.sequence_number + 1; /* Dummy sequence logic */
                resp_seq.request_id = seq.request_id;
                mu_write_sequence_header(server->config.send_buffer, resp_len, &w_offset, &resp_seq);
                
                const mu_service_handler_t *handler = mu_get_service_handler(request_id.identifier.numeric);
                mu_nodeid_t resp_node = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = handler ? handler->response_id : 0};
                mu_write_service_prefix(server->config.send_buffer, resp_len, &w_offset, &resp_node);
                
                size_t bytes_written;
                server->config.tcp_adapter.write(server->config.tcp_adapter.context, server->client_handle, server->config.send_buffer, actual_resp_len, &bytes_written);
            }
        }
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
