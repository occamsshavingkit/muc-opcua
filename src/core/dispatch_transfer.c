/* src/core/dispatch_transfer.c
 *
 * TransferSubscriptions handler (OPC-10000-4 §5.14.7).
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_REDUNDANCY

#include "muc_opcua/server.h"
#include "muc_opcua/types.h"
#include "server_internal.h"
#include <string.h>

opcua_statuscode_t handle_transfer_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length) {
    (void)response_length;
    opcua_int32_t subscription_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &subscription_count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (subscription_count < 0)
        return MU_STATUS_BAD_DECODINGERROR;

    opcua_uint32_t sub_ids[16];
    opcua_uint32_t count = (subscription_count > 0) ? (opcua_uint32_t)subscription_count : 0u;
    if (count > 16)
        count = 16;

    for (opcua_uint32_t i = 0; i < count; ++i) {
        s = mu_binary_read_uint32(r, &sub_ids[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }
    /* Skip send_initial_values (bool) */
    opcua_byte_t send_initial;
    s = mu_binary_read_byte(r, &send_initial);
    if (s != MU_STATUS_GOOD)
        return s;

    /* Write results: for each subscription ID, status code + available_sequence_numbers */
    s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (opcua_uint32_t i = 0; i < count; ++i) {
        mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, sub_ids[i]);
        opcua_statuscode_t status = (sub != NULL) ? MU_STATUS_GOOD : MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
        if (sub != NULL)
            sub->session_id = server->active_session->session_id;

        s = mu_binary_write_uint32(w, (opcua_uint32_t)status);
        if (s != MU_STATUS_GOOD)
            return s;
        s = mu_binary_write_int32(w, 0); /* no available sequence numbers */
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0); /* no diagnostic infos */
    return s;
}

#endif /* MUC_OPCUA_REDUNDANCY */
