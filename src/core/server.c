/* src/core/server.c */
#include "micro_opcua/server.h"
#include "micro_opcua/encoding.h"
#include "server_internal.h"
#include "message_chunk.h"
#include "service_message.h"
#include "uasc.h"
#include "../services/secure_channel.h"
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
    } else {
        /* Check if there's another connection waiting to be rejected */
        void *second_handle = NULL;
        opcua_statuscode_t status = server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &second_handle);
        if (status == MU_STATUS_GOOD && second_handle != NULL) {
            /* We don't have resources. OPC UA 10000-6 7.1.5: immediately close or wait for HEL and send ERR.
               Wait for HEL (simulate it by just trying to read once) */
            opcua_byte_t buf[256];
            size_t bytes_read = 0;
            status = server->config.tcp_adapter.read(server->config.tcp_adapter.context, second_handle, buf, sizeof(buf), &bytes_read);
            
            if (status == MU_STATUS_GOOD && bytes_read >= 8 && buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'L') {
                size_t err_len = sizeof(buf);
                if (mu_tcp_create_error_message(MU_STATUS_BAD_TCPNOTENOUGHRESOURCES, "Server full", buf, &err_len) == MU_STATUS_GOOD) {
                    size_t bytes_written;
                    server->config.tcp_adapter.write(server->config.tcp_adapter.context, second_handle, buf, err_len, &bytes_written);
                }
            }
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, second_handle);
        }
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

        bool is_opn = header.message_type[0] == 'O' && header.message_type[1] == 'P' && header.message_type[2] == 'N';
        bool is_msg = header.message_type[0] == 'M' && header.message_type[1] == 'S' && header.message_type[2] == 'G';
        bool is_clo = header.message_type[0] == 'C' && header.message_type[1] == 'L' && header.message_type[2] == 'O';

        if (is_clo) {
            /* CloseSecureChannel: drop the channel and the connection. */
            mu_secure_channel_close(&server->secure_channel);
            server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, server->client_handle);
            server->client_handle = NULL;
            return MU_STATUS_GOOD;
        }

        if (!is_opn && !is_msg) {
            return MU_STATUS_GOOD; /* unsupported chunk type: ignore */
        }

        /* Read the security header to locate the SequenceHeader. */
        mu_binary_reader_t reader;
        mu_binary_reader_init(&reader, server->config.receive_buffer, bytes_read);
        reader.position = 12; /* MessageHeader(8) + SecureChannelId(4) */

        if (is_opn) {
            mu_string_t policy_uri;
            mu_bytestring_t sender_cert, receiver_thumbprint;
            status = mu_binary_read_string(&reader, &policy_uri);          if (status != MU_STATUS_GOOD) return status;
            status = mu_binary_read_bytestring(&reader, &sender_cert);     if (status != MU_STATUS_GOOD) return status;
            status = mu_binary_read_bytestring(&reader, &receiver_thumbprint); if (status != MU_STATUS_GOOD) return status;
        } else {
            opcua_uint32_t token_id;
            status = mu_binary_read_uint32(&reader, &token_id);            if (status != MU_STATUS_GOOD) return status;
        }

        mu_sequence_header_t seq;
        status = mu_binary_read_uint32(&reader, &seq.sequence_number);    if (status != MU_STATUS_GOOD) return status;
        status = mu_binary_read_uint32(&reader, &seq.request_id);         if (status != MU_STATUS_GOOD) return status;

        mu_nodeid_t request_type;
        status = mu_binary_read_nodeid(&reader, &request_type);           if (status != MU_STATUS_GOOD) return status;

        const opcua_byte_t *req_body = server->config.receive_buffer + reader.position;
        size_t req_body_len = bytes_read - reader.position;

        /* Reserve the chunk header; the service writes the body after it. */
        size_t body_offset = is_opn ? MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE : MU_UASC_SYMMETRIC_HEADER_SIZE;
        if (body_offset >= server->config.send_buffer_size) return MU_STATUS_BAD_RESPONSETOOLARGE;

        opcua_byte_t *resp_body = server->config.send_buffer + body_offset;
        size_t payload_len = server->config.send_buffer_size - body_offset;

        status = mu_service_dispatch(server, request_type.identifier.numeric, req_body, req_body_len, resp_body, &payload_len);

        if (status == MU_STATUS_GOOD && payload_len > 0) {
            opcua_uint32_t out_seq = ++server->secure_channel.out_sequence_number;
            size_t total = 0;
            if (is_opn) {
                status = mu_uasc_finalize_asymmetric_none(server->config.send_buffer, server->config.send_buffer_size,
                                                          server->secure_channel.channel_id, out_seq, seq.request_id,
                                                          payload_len, &total);
            } else {
                status = mu_uasc_finalize_symmetric(server->config.send_buffer, server->config.send_buffer_size,
                                                    server->secure_channel.channel_id, server->secure_channel.token_id,
                                                    out_seq, seq.request_id, payload_len, &total);
            }
            if (status == MU_STATUS_GOOD) {
                size_t bytes_written;
                server->config.tcp_adapter.write(server->config.tcp_adapter.context, server->client_handle,
                                                 server->config.send_buffer, total, &bytes_written);
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
