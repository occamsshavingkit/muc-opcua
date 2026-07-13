/* src/core/server/poll.c */
#include "common.h"
#include <string.h>

static void poll_close_connection(mu_server_t *server, void **handle_ptr, size_t *rx_len_ptr, size_t *read_pos_ptr) {
    if (*handle_ptr != NULL) {
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, *handle_ptr);
        *handle_ptr = NULL;
        *rx_len_ptr = 0;
        *read_pos_ptr = 0;
    }
}

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
static void poll_send_error_and_close(mu_server_t *server, void *handle) {
    opcua_byte_t buf[256];
    size_t err_len = sizeof(buf);
    if (mu_tcp_create_error_message(MU_STATUS_BAD_TCPNOTENOUGHRESOURCES, "Server full", buf, &err_len) ==
        MU_STATUS_GOOD) {
        size_t bytes_written;
        server->config.tcp_adapter.write(server->config.tcp_adapter.context, handle, buf, err_len, &bytes_written);
    }
    server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, handle);
}
#endif /* MUC_OPCUA_CU_MULTIPLE_CONNECTIONS */

static bool poll_check_idle_timeout(mu_server_t *server, void **handle_ptr, size_t *rx_len_ptr, size_t *read_pos_ptr,
                                    opcua_uint64_t *last_activity_ptr, const mu_secure_channel_t *channel) {
    opcua_uint64_t now = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
    opcua_uint64_t limit =
        channel->is_open ? (opcua_uint64_t)channel->revised_lifetime : (opcua_uint64_t)MU_CONNECT_TIMEOUT_MS;
    if (now > *last_activity_ptr && (now - *last_activity_ptr) > limit) {
        poll_close_connection(server, handle_ptr, rx_len_ptr, read_pos_ptr);
        return true;
    }
    return false;
}

static size_t poll_msg_size(const opcua_byte_t *buffer, size_t offset) {
    const opcua_byte_t *b = buffer + offset;
    return (size_t)b[4] | ((size_t)b[5] << 8) | ((size_t)b[6] << 16) | ((size_t)b[7] << 24);
}

static void poll_process_buffer(mu_server_t *server, void **handle_ptr, size_t *read_pos_ptr, size_t *rx_len_ptr,
                                opcua_byte_t *buffer, size_t buffer_size) {
    size_t consumed = 0;
    while (*handle_ptr != NULL && (*rx_len_ptr - consumed) >= 8) {
        size_t msg_size = poll_msg_size(buffer, *read_pos_ptr + consumed);
        if (msg_size < 8 || msg_size > buffer_size) {
            poll_close_connection(server, handle_ptr, rx_len_ptr, read_pos_ptr);
            return;
        }
        if (msg_size > (*rx_len_ptr - consumed)) {
            break;
        }

        process_message(server, buffer + *read_pos_ptr + consumed, msg_size);

        if (*handle_ptr == NULL) {
            *rx_len_ptr = 0;
            *read_pos_ptr = 0;
            return;
        }
        consumed += msg_size;
    }

    *rx_len_ptr -= consumed;
    *read_pos_ptr += consumed;

    /* Avoid O(N) memmove on every poll cycle: only shift when buffer is
       nearly full or all data consumed (HP8, FR-011). */
    if (*rx_len_ptr == 0) {
        *read_pos_ptr = 0;
    } else if ((*read_pos_ptr + *rx_len_ptr + 64) > buffer_size) {
        size_t remaining = *rx_len_ptr;
        if (remaining > 0) {
            (void)memmove(buffer, buffer + *read_pos_ptr, remaining);
        }
        *read_pos_ptr = 0;
    }
}

static opcua_statuscode_t poll_read_data(mu_server_t *server, void **handle_ptr, size_t *read_pos_ptr,
                                         size_t *rx_len_ptr, opcua_uint64_t *last_activity_ptr, opcua_byte_t *buffer,
                                         size_t buffer_size) {
    size_t write_pos = *read_pos_ptr + *rx_len_ptr;
    if (write_pos >= buffer_size) {
        return MU_STATUS_GOOD;
    }
    size_t bytes_read = 0;
    opcua_statuscode_t status = server->config.tcp_adapter.read(
        server->config.tcp_adapter.context, *handle_ptr, buffer + write_pos, buffer_size - write_pos, &bytes_read);
    if (status != MU_STATUS_GOOD) {
        poll_close_connection(server, handle_ptr, rx_len_ptr, read_pos_ptr);
        return MU_STATUS_GOOD;
    }
    if (bytes_read > 0) {
        *rx_len_ptr += bytes_read;
        *last_activity_ptr = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
    }
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
static void poll_conn_read_and_process(mu_server_t *server, mu_connection_t *conn) {
    poll_read_data(server, &conn->client_handle, &conn->rx_read_pos, &conn->rx_len, &conn->last_activity_ms,
                   conn->rx_buffer, sizeof(conn->rx_buffer));

    server->active_conn = conn;
    poll_process_buffer(server, &conn->client_handle, &conn->rx_read_pos, &conn->rx_len, conn->rx_buffer,
                        sizeof(conn->rx_buffer));
    server->active_conn = NULL;
}
#else
static void poll_single_read_and_process(mu_server_t *server) {
    poll_read_data(server, &server_client_handle, &server_rx_read_pos, &server_rx_len, &server_last_activity_ms,
                   server->config.receive_buffer, server->config.receive_buffer_size);

    poll_process_buffer(server, &server_client_handle, &server_rx_read_pos, &server_rx_len,
                        server->config.receive_buffer, server->config.receive_buffer_size);
}
#endif

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
/* Try to accept a new connection into a free slot. Returns true when an accept
   occurred (and the caller should immediately return mu_server_poll_background). */
static bool poll_try_accept_multi(mu_server_t *server) {
    int free_slot = -1;
    for (size_t i = 0; i < MU_INTERN_MAX_CONNECTIONS; ++i) {
        if (server->conns[i].client_handle == NULL) {
            free_slot = (int)i;
            break;
        }
    }

    void *new_handle = NULL;
    opcua_statuscode_t status = server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &new_handle);
    if (status == MU_STATUS_GOOD && new_handle != NULL) {
        if (free_slot != -1) {
            mu_connection_t *conn = &server->conns[free_slot];
            conn->client_handle = new_handle;
            conn->rx_len = 0;
            conn->rx_read_pos = 0;
            conn->last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
            mu_tcp_connection_init(&conn->tcp_conn);
            mu_secure_channel_init(&conn->secure_channel);
        } else {
            poll_send_error_and_close(server, new_handle);
        }
        return true;
    }
    return false;
}
#else
/* Try to accept a new connection when no client is connected. Returns true when
   the function handled the poll cycle (caller should return immediately). */
static bool poll_try_accept_single(mu_server_t *server) {
    if (server_client_handle != NULL) {
        return false;
    }
    opcua_statuscode_t status =
        server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &server_client_handle);
    if (status == MU_STATUS_GOOD && server_client_handle != NULL) {
        mu_tcp_connection_init(&server_tcp_conn);
        mu_secure_channel_init(&server_secure_channel);
#ifdef MUC_OPCUA_CU_MULTI_CHUNK
        mu_chunk_assembler_init(&server->chunk_assembly);
#endif
        for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
            mu_session_init(&server->sessions[i]);
        }
        server->active_session = NULL;
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
        mu_subscriptions_init(&server->subs);
#endif
        server_rx_len = 0;
        server_rx_read_pos = 0;
        server_last_activity_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
    }
    return true;
}

/* Check for a second connection attempt and reject it with an error message. */
static void poll_reject_second_connection(mu_server_t *server) {
    void *second_handle = NULL;
    opcua_statuscode_t status = server->config.tcp_adapter.accept(server->config.tcp_adapter.context, &second_handle);
    if (status == MU_STATUS_GOOD && second_handle != NULL) {
        opcua_byte_t buf[256];
        size_t bytes_read = 0;
        status = server->config.tcp_adapter.read(server->config.tcp_adapter.context, second_handle, buf, sizeof(buf),
                                                 &bytes_read);

        if (status == MU_STATUS_GOOD && bytes_read >= 8 && buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'L') {
            size_t err_len = sizeof(buf);
            if (mu_tcp_create_error_message(MU_STATUS_BAD_TCPNOTENOUGHRESOURCES, "Server full", buf, &err_len) ==
                MU_STATUS_GOOD) {
                size_t bytes_written;
                server->config.tcp_adapter.write(server->config.tcp_adapter.context, second_handle, buf, err_len,
                                                 &bytes_written);
            }
        }
        server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context, second_handle);
    }
}
#endif /* MUC_OPCUA_CU_MULTIPLE_CONNECTIONS (else branch helpers) */

static opcua_statuscode_t mu_server_poll_background(mu_server_t *server) {
    (void)server;

#ifdef MUC_OPCUA_CU_PUBSUB
    opcua_statuscode_t status = mu_pubsub_poll(server);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
#endif

#if defined(MUC_OPCUA_CU_SESSION_TIMEOUT)
    {
        opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        if (now_ms != 0u) {
            for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
                mu_session_t *session = &server->sessions[i];
                if (session->state == MU_SESSION_STATE_CLOSED) {
                    continue;
                }
                if (session->revised_session_timeout_ms == 0u || session->last_activity_ms == 0u) {
                    continue;
                }
                if (now_ms > session->last_activity_ms &&
                    (now_ms - session->last_activity_ms) > session->revised_session_timeout_ms) {
                    mu_session_close_timeout(session);
                    mu_diagnostics_session_timeout(server);
                    mu_diagnostics_session_closed(server);
#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
                    for (size_t j = 0; j < MU_INTERN_MAX_SUBSCRIPTIONS; ++j) {
                        mu_subscription_t *sub = &server->subs.subscriptions[j];
                        if (sub->in_use && sub->session_id == session->session_id) {
                            if (mu_subscription_delete(&server->subs, session->session_id, sub->subscription_id) ==
                                MU_STATUS_GOOD) {
                                mu_diagnostics_subscription_closed(server);
                            }
                        }
                    }
#endif
                    if (server->active_session == session) {
                        server->active_session = NULL;
                    }
                }
            }
        }
    }
#endif /* defined(MUC_OPCUA_CU_SESSION_TIMEOUT) */

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC
    mu_subscriptions_tick(server, server->config.time_adapter.get_tick_ms(server->config.time_adapter.context));
#endif

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_server_poll(mu_server_t *server) {
    if (server == NULL || !server->is_running) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
    if (poll_try_accept_multi(server)) {
        return mu_server_poll_background(server);
    }

    for (size_t i = 0; i < MU_INTERN_MAX_CONNECTIONS; ++i) {
        mu_connection_t *conn = &server->conns[i];
        if (conn->client_handle == NULL) {
            continue;
        }

        if (poll_check_idle_timeout(server, &conn->client_handle, &conn->rx_len, &conn->rx_read_pos,
                                    &conn->last_activity_ms, &conn->secure_channel)) {
            continue;
        }
        poll_conn_read_and_process(server, conn);
    }
#else
    if (poll_try_accept_single(server)) {
        return mu_server_poll_background(server);
    }

    if (poll_check_idle_timeout(server, &server_client_handle, &server_rx_len, &server_rx_read_pos,
                                &server_last_activity_ms, &server_secure_channel)) {
        return mu_server_poll_background(server);
    }

    poll_reject_second_connection(server);

    poll_single_read_and_process(server);
#endif

    return mu_server_poll_background(server);
}
