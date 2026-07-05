/* service_dispatch/transfer_handler.c */
#include "common.h"

#if MUC_OPCUA_REDUNDANCY

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
    opcua_byte_t send_initial;
    s = mu_binary_read_byte(r, &send_initial);
    if (s != MU_STATUS_GOOD)
        return s;

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
        s = mu_binary_write_int32(w, 0);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    return s;
}

#endif /* MUC_OPCUA_REDUNDANCY */
