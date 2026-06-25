/* src/core/tcp_connection.c */
#include "tcp_connection.h"
#include "micro_opcua/encoding.h"

void mu_tcp_connection_init(mu_tcp_connection_t *connection) {
    if (connection) {
        connection->state = MU_TCP_STATE_CLOSED;
        connection->receive_buffer_size = 0;
        connection->send_buffer_size = 0;
        connection->max_message_size = 0;
        connection->max_chunk_count = 0;
        connection->protocol_version = 0;
    }
}

opcua_statuscode_t mu_tcp_process_hello(
    mu_tcp_connection_t *connection, 
    const opcua_byte_t *message, 
    size_t length, 
    const mu_server_config_t *config, 
    opcua_byte_t *ack_message, 
    size_t *ack_length)
{
    if (!connection || !message || !config || !ack_message || !ack_length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (length < 8) return MU_STATUS_BAD_TCPMESSAGETOOLARGE;

    if (message[0] != 'H' || message[1] != 'E' || message[2] != 'L' || message[3] != 'F') {
        return MU_STATUS_BAD_TCPMESSAGETYPEINVALID;
    }

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, message, length);
    reader.position = 4; // Skip message type and chunk type
    
    opcua_int32_t message_size;
    opcua_statuscode_t status = mu_binary_read_int32(&reader, &message_size);
    if (status != MU_STATUS_GOOD) return status;
    
    if (message_size > (opcua_int32_t)length) return MU_STATUS_BAD_TCPMESSAGETOOLARGE;
    
    opcua_uint32_t protocol_version, receive_buffer_size, send_buffer_size;
    opcua_uint32_t max_message_size, max_chunk_count;
    mu_string_t endpoint_url;
    
    status = mu_binary_read_uint32(&reader, &protocol_version);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_read_uint32(&reader, &receive_buffer_size);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_read_uint32(&reader, &send_buffer_size);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_read_uint32(&reader, &max_message_size);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_read_uint32(&reader, &max_chunk_count);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_read_string(&reader, &endpoint_url);
    if (status != MU_STATUS_GOOD) return status;

    /* Minimum values */
    if (receive_buffer_size < 8192) receive_buffer_size = 8192;
    if (send_buffer_size < 8192) send_buffer_size = 8192;

    /* Negotiate values */
    connection->protocol_version = protocol_version;
    connection->receive_buffer_size = receive_buffer_size < (opcua_uint32_t)config->send_buffer_size ? receive_buffer_size : (opcua_uint32_t)config->send_buffer_size;
    connection->send_buffer_size = send_buffer_size < (opcua_uint32_t)config->receive_buffer_size ? send_buffer_size : (opcua_uint32_t)config->receive_buffer_size;
    connection->max_message_size = max_message_size < config->max_message_size ? max_message_size : config->max_message_size;
    connection->max_chunk_count = max_chunk_count < config->max_chunk_count ? max_chunk_count : config->max_chunk_count;

    if (connection->max_message_size == 0) connection->max_message_size = config->max_message_size;
    if (connection->max_chunk_count == 0) connection->max_chunk_count = config->max_chunk_count;

    /* Write ACK */
    if (*ack_length < 28) return MU_STATUS_BAD_INTERNALERROR;
    
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, ack_message, *ack_length);
    
    status = mu_binary_write_byte(&writer, 'A');
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_byte(&writer, 'C');
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_byte(&writer, 'K');
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_byte(&writer, 'F');
    if (status != MU_STATUS_GOOD) return status;
    
    status = mu_binary_write_int32(&writer, 28); /* ACK size */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&writer, 0); /* ProtocolVersion */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&writer, connection->send_buffer_size); /* Server's receive is client's send */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&writer, connection->receive_buffer_size); /* Server's send is client's receive */
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&writer, connection->max_message_size);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_uint32(&writer, connection->max_chunk_count);
    if (status != MU_STATUS_GOOD) return status;
    
    *ack_length = writer.position;
    connection->state = MU_TCP_STATE_ESTABLISHED;
    
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_tcp_create_error_message(
    opcua_statuscode_t error_code,
    const char *reason,
    opcua_byte_t *err_message,
    size_t *err_length)
{
    if (!err_message || !err_length) return MU_STATUS_BAD_INTERNALERROR;
    
    size_t reason_len = reason ? strlen(reason) : 0;
    size_t total_len = 8 + 4 + 4 + reason_len;
    
    if (*err_length < total_len) return MU_STATUS_BAD_INTERNALERROR;
    
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, err_message, *err_length);
    
    opcua_statuscode_t status = mu_binary_write_byte(&writer, 'E');
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_byte(&writer, 'R');
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_byte(&writer, 'R');
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_write_byte(&writer, 'F');
    if (status != MU_STATUS_GOOD) return status;
    
    status = mu_binary_write_int32(&writer, (opcua_int32_t)total_len);
    if (status != MU_STATUS_GOOD) return status;
    
    status = mu_binary_write_uint32(&writer, (opcua_uint32_t)error_code);
    if (status != MU_STATUS_GOOD) return status;
    
    mu_string_t str_reason;
    str_reason.length = (opcua_int32_t)reason_len;
    str_reason.data = (opcua_byte_t*)reason;
    status = mu_binary_write_string(&writer, &str_reason);
    if (status != MU_STATUS_GOOD) return status;
    
    *err_length = writer.position;
    return MU_STATUS_GOOD;
}
