/* src/core/server/init.c */
#include "../safe_mem.h"
#include "common.h"
#include <string.h>

opcua_statuscode_t mu_server_config_validate(const mu_server_config_t *config) {
    if (config == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate Endpoints */
#if MUC_OPCUA_REVERSE_CONNECT
    if (config->reverse_connect_url != NULL) {
        if (strncmp(config->reverse_connect_url, "opc.tcp://", 10) != 0) {
            return MU_STATUS_BAD_TCPENDPOINTURLINVALID;
        }
    } else
#endif
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

    /* Validate limits. An upper bound isn't just belt-and-suspenders: without it
       a caller requesting more sessions/channels than this build was compiled
       for (MU_MAX_SESSIONS / MU_MAX_CONNECTIONS size the static sessions[]/
       conns[] arrays) was previously accepted here and then silently capped at
       the compiled size by CreateSession / the connection accept path, with no
       indication the requested capacity was never actually available. */
    if (config->max_sessions == 0 || config->max_sessions > MU_MAX_SESSIONS) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (config->max_secure_channels == 0 || config->max_secure_channels > MU_MAX_CONNECTIONS) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (config->max_chunk_count == 0 || config->max_message_size < MU_MIN_CHUNK_SIZE) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Validate Platform Adapters */
#if MUC_OPCUA_REVERSE_CONNECT
    if (config->reverse_connect_url != NULL) {
        if (config->tcp_adapter.connect == NULL || config->tcp_adapter.accept == NULL ||
            config->tcp_adapter.read == NULL || config->tcp_adapter.write == NULL ||
            config->tcp_adapter.close_connection == NULL || config->tcp_adapter.shutdown == NULL) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
    } else
#endif
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

    /* Validate the server's own certificate is within its validity window. */
    if (config->crypto_adapter != NULL) {
        const opcua_byte_t *own_cert = NULL;
        size_t own_cert_len = 0;
        if (config->crypto_adapter->get_own_certificate != NULL &&
            config->crypto_adapter->verify_certificate_validity != NULL) {
            opcua_statuscode_t cert_ret =
                config->crypto_adapter->get_own_certificate(config->crypto_adapter->context, &own_cert, &own_cert_len);
            if (cert_ret != MU_STATUS_GOOD) {
                return cert_ret;
            }
            opcua_statuscode_t cert_status = config->crypto_adapter->verify_certificate_validity(
                config->crypto_adapter->context, own_cert, own_cert_len);
            if (cert_status != MU_STATUS_GOOD) {
                return cert_status;
            }
        }
    }

    /* Validate address space if provided */
    if (config->address_space != NULL) {
        opcua_statuscode_t status = mu_address_space_validate(config->address_space);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        /* Feature 025 (F6): the user address-space sort index is a fixed
           order[MU_MAX_ADDRESS_SPACE_NODES] array. A larger space previously fell
           back to a silent O(n) linear scan AND made Query return nothing. Fail
           loudly at init instead (Constitution §VII: predictable failure over
           ambiguous degradation). Integrators needing more must raise
           MU_MAX_ADDRESS_SPACE_NODES, which resizes the index storage. */
        if (config->address_space->node_count > MU_MAX_ADDRESS_SPACE_NODES) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_server_init(void *storage, size_t storage_size, const mu_server_config_t *config,
                                  mu_server_t **out_server) {
    mu_server_t *server;
    opcua_statuscode_t status;

    if (storage == NULL || out_server == NULL || config == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* The server struct is placed directly in caller storage and contains pointers
       and size_t, so the buffer must be suitably aligned (word access to a
       misaligned struct faults on Cortex-M0+). */
    if (((uintptr_t)storage % _Alignof(struct mu_server)) != 0) {
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
    (void)memset(server, 0, sizeof(struct mu_server));
    server->config = *config;
    server->user_address_space_index = (mu_address_space_index_t){0};
    /* opn_pending_security_policy and opn_pending_client_cert are transient
       pointers into the receive buffer, valid only within a single
       mu_server_poll cycle (safe for single-threaded poll model). */
    server->opn_pending_security_policy.length = -1;
    server->opn_pending_security_policy.data = NULL;
    {
        opcua_datetime_t start = server->config.time_adapter.get_time
                                     ? server->config.time_adapter.get_time(server->config.time_adapter.context)
                                     : 0;
        mu_base_runtime_init(&server->runtime_base, &server->config.time_adapter, start);
    }
    server->is_running = true;
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    for (size_t i = 0; i < MU_MAX_CONNECTIONS; ++i) {
        server->conns[i].client_handle = NULL;
        mu_tcp_connection_init(&server->conns[i].tcp_conn);
        mu_secure_channel_init(&server->conns[i].secure_channel);
        server->conns[i].rx_len = 0;
        server->conns[i].rx_read_pos = 0;
    }
    server->active_conn = NULL;
#else
    server_client_handle = NULL;
    mu_tcp_connection_init(&server_tcp_conn);
    mu_secure_channel_init(&server_secure_channel);
#ifdef MUC_OPCUA_MULTI_CHUNK
    mu_chunk_assembler_init(&server->chunk_assembly);
#endif
#endif
    for (size_t i = 0; i < MU_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
    server->active_session = NULL;
#if MUC_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_init(&server->subs);
#endif
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
    (void)memset(&server->dynamic_address_space, 0, sizeof(server->dynamic_address_space));
#endif
#ifdef MUC_OPCUA_SERVICE_QUERY
    for (size_t i = 0; i < MU_MAX_QUERY_CONTINUATION_POINTS; ++i) {
        server->query_context.continuation_points[i].id.length = -1;
        server->query_context.continuation_points[i].id.data = NULL;
        server->query_context.continuation_points[i].session_id = 0;
        server->query_context.continuation_points[i].next_index = 0;
        server->query_context.continuation_points[i].timestamp_ms = 0;
    }
#endif

    /* Initialize platform TCP adapter */
#if MUC_OPCUA_REVERSE_CONNECT
    if (config->reverse_connect_url != NULL) {
        void *handle = NULL;
        status = server->config.tcp_adapter.connect(server->config.tcp_adapter.context, config->reverse_connect_url,
                                                    &handle);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
        server->conns[0].client_handle = handle;
        server->active_conn = &server->conns[0];
#else
        server->client_handle = handle;
#endif
    } else
#endif
    {
        status = server->config.tcp_adapter.listen(server->config.tcp_adapter.context, server->config.endpoint_url);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    *out_server = server;
    return MU_STATUS_GOOD;
}

void mu_server_close(mu_server_t *server) {
    if (server != NULL) {
        server->is_running = false;
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
        for (size_t i = 0; i < MU_MAX_CONNECTIONS; ++i) {
            if (server->conns[i].client_handle != NULL) {
                server->config.tcp_adapter.close_connection(server->config.tcp_adapter.context,
                                                            server->conns[i].client_handle);
                mu_secure_channel_close(&server->conns[i].secure_channel);
                server->conns[i].client_handle = NULL;
            }
        }
#else
        mu_secure_channel_close(&server_secure_channel);
#endif
        if (server->config.tcp_adapter.shutdown != NULL) {
            server->config.tcp_adapter.shutdown(server->config.tcp_adapter.context);
        }
    }
}
