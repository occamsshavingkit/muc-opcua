#include "muc_opcua/pubsub.h"
#include "core/server_internal.h"
#include <string.h>

opcua_statuscode_t mu_server_add_writer_group(mu_server_t *server, const mu_pubsub_writer_group_t *wg) {
    if (!server || !wg) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
#ifdef MUC_OPCUA_CU_PUBSUB
    if (server->writer_group_count >= MU_MAX_WRITER_GROUPS) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    server->writer_groups[server->writer_group_count] = *wg;
    server->writer_groups[server->writer_group_count].last_publish_time_ms = 0;
    server->writer_group_count++;
    return MU_STATUS_GOOD;
#else
    return MU_STATUS_BAD_NOTSUPPORTED;
#endif
}

opcua_statuscode_t mu_pubsub_poll(mu_server_t *server) {
#ifdef MUC_OPCUA_CU_PUBSUB
    if (!server || !server->config.pubsub.enabled) {
        return MU_STATUS_GOOD;
    }
    if (!server->config.udp_adapter.send) {
        return MU_STATUS_GOOD;
    }
    if (!server->config.time_adapter.get_tick_ms) {
        return MU_STATUS_GOOD;
    }

    opcua_uint32_t now = (opcua_uint32_t)server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);

    for (size_t i = 0; i < server->writer_group_count; i++) {
        mu_pubsub_writer_group_t *wg = &server->writer_groups[i];
        if ((opcua_uint32_t)(now - wg->last_publish_time_ms) >= wg->publishing_interval_ms) {
            wg->last_publish_time_ms = now;
            const char *address = server->config.pubsub.address ? server->config.pubsub.address : "255.255.255.255";

            size_t bytes_written = 0;
            opcua_statuscode_t status =
                mu_encode_uadp_network_message(&server->config.pubsub, wg, server->config.send_buffer,
                                               server->config.send_buffer_size, &bytes_written);
            if (status == MU_STATUS_GOOD && bytes_written > 0) {
                status = server->config.udp_adapter.send(server->config.udp_adapter.context, server->config.send_buffer,
                                                         bytes_written, address, server->config.pubsub.port);
                if (status != MU_STATUS_GOOD) {
                    return status;
                }
            } else if (status != MU_STATUS_GOOD) {
                return status;
            }
        }
    }
    return MU_STATUS_GOOD;
#else
    (void)server;
    return MU_STATUS_GOOD;
#endif
}
