#include "common.h"

#if MUC_OPCUA_SUBSCRIPTIONS
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD

opcua_statuscode_t read_set_triggering_uint32_array(mu_binary_reader_t *r, opcua_uint32_t *items,
                                                    opcua_int32_t *count) {
    opcua_int32_t decoded_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &decoded_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (decoded_count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (decoded_count > 0 && (size_t)decoded_count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    s = ensure_array_items_min_remaining(r, decoded_count, 4u);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (decoded_count <= 0) {
        *count = 0;
        return MU_STATUS_GOOD;
    }

    for (opcua_int32_t i = 0; i < decoded_count; ++i) {
        s = mu_binary_read_uint32(r, &items[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    *count = decoded_count;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t handle_set_triggering(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint32_t subscription_id;
    opcua_uint32_t triggering_item_id;
    opcua_uint32_t links_to_add[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_uint32_t links_to_remove[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_statuscode_t add_results[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_statuscode_t remove_results[MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS];
    opcua_int32_t add_count = 0;
    opcua_int32_t remove_count = 0;

    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &triggering_item_id);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    s = read_set_triggering_uint32_array(r, links_to_add, &add_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = read_set_triggering_uint32_array(r, links_to_remove, &remove_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if ((size_t)add_count + (size_t)remove_count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    (void)sub;

    if (add_count == 0 && remove_count == 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    for (opcua_int32_t i = 0; i < add_count; ++i) {
        add_results[i] =
            mu_monitored_item_add_trigger_link(&server->subs, subscription_id, triggering_item_id, links_to_add[i]);
    }
    for (opcua_int32_t i = 0; i < remove_count; ++i) {
        remove_results[i] = mu_monitored_item_remove_trigger_link(&server->subs, subscription_id, triggering_item_id,
                                                                  links_to_remove[i]);
    }

    s = write_response_prefix(w, MU_ID_SETTRIGGERINGRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, add_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < add_count; ++i) {
        s = mu_binary_write_statuscode(w, add_results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, remove_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < remove_count; ++i) {
        s = mu_binary_write_statuscode(w, remove_results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_SUBSCRIPTIONS_STANDARD */
#endif /* MUC_OPCUA_SUBSCRIPTIONS */
