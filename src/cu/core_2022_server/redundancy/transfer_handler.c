/* service_dispatch/transfer_handler.c */
#include "core/service_dispatch/common.h"

#if MUC_OPCUA_CU_REDUNDANCY

opcua_statuscode_t handle_transfer_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length) {
    (void)response_length;
    mu_request_header_t req;
    opcua_int32_t subscription_count;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_read_int32(r, &subscription_count);
    if (s != MU_STATUS_GOOD)
        return s;
    if (subscription_count < 0) {
        s = write_response_prefix(w, MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, req.request_handle,
                                  MU_STATUS_BAD_DECODINGERROR
#ifdef MUC_OPCUA_CU_TIME_SYNC
                                  ,
                                  server
#endif
        );
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (subscription_count == 0) {
        /* Empty subscriptionIds -> service-level Bad_NothingToDo (OPC-10000-4 §5.14.7.3);
           results[] and diagnosticInfos[] are empty. */
        s = write_response_prefix(w, MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, req.request_handle, MU_STATUS_BAD_NOTHINGTODO
#ifdef MUC_OPCUA_CU_TIME_SYNC
                                  ,
                                  server
#endif
        );
        if (s == MU_STATUS_GOOD) {
            s = mu_binary_write_int32(w, 0); /* results[] */
        }
        if (s == MU_STATUS_GOOD) {
            s = mu_binary_write_int32(w, 0); /* diagnosticInfos[] */
        }
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return s;
    }

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

    s = write_response_prefix(w, MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_CU_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (opcua_uint32_t i = 0; i < count; ++i) {
        opcua_statuscode_t status;
        opcua_uint32_t available_seq = 0u;
        bool have_available = false;

        /* Find the Subscription regardless of owner (OPC-10000-4 §5.14.7). */
        mu_subscription_t *sub = mu_subscription_find_any(&server->subs, sub_ids[i]);
        if (sub == NULL) {
            status = MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
        } else if (sub->session_id == server->active_session->session_id) {
            /* SessionChanged() == FALSE: already owned by the calling Session. */
            status = MU_STATUS_BAD_NOTHINGTODO;
        } else {
            /* Locate the current owner to evaluate ClientValidated() (same user). */
            mu_session_t *owner = NULL;
            for (size_t si = 0; si < MU_INTERN_MAX_SESSIONS; ++si) {
                if (server->sessions[si].state != MU_SESSION_STATE_CLOSED &&
                    server->sessions[si].session_id == sub->session_id) {
                    owner = &server->sessions[si];
                    break;
                }
            }
            if (owner != NULL && !mu_session_same_user(owner, server->active_session)) {
                status = MU_STATUS_BAD_USERACCESSDENIED;
            } else {
                /* Row 23: report the sequence numbers still available for Republish, notify
                   the OLD Session (Good_SubscriptionTransferred), then SetSession(). */
                if (sub->retransmit.valid) {
                    available_seq = sub->retransmit.sequence_number;
                    have_available = true;
                }
                issue_status_change_notification(server, sub, MU_STATUS_GOOD_SUBSCRIPTIONTRANSFERRED);
                if (send_initial) {
                    /* sendInitialValues: re-send current values on the next Publish. */
                    for (size_t mi = 0; mi < MU_INTERN_MAX_MONITORED_ITEMS; ++mi) {
                        mu_monitored_item_t *item = &server->subs.monitored_items[mi];
                        if (item->in_use && item->subscription_id == sub->subscription_id) {
                            item->has_reported = false;
                        }
                    }
                }
                (void)mu_subscription_transfer(&server->subs, sub, server->active_session->session_id);
                status = MU_STATUS_GOOD;
            }
        }

        s = mu_binary_write_uint32(w, (opcua_uint32_t)status);
        if (s != MU_STATUS_GOOD)
            return s;
        /* availableSequenceNumbers[] */
        if (have_available) {
            s = mu_binary_write_int32(w, 1);
            if (s == MU_STATUS_GOOD)
                s = mu_binary_write_uint32(w, available_seq);
        } else {
            s = mu_binary_write_int32(w, 0);
        }
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s == MU_STATUS_GOOD) {
        *response_length = w->position;
    }
    return s;
}

#endif /* MUC_OPCUA_CU_REDUNDANCY */
