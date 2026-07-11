/* service_dispatch/close_session.c - CloseSession service handler */
#include "common.h"

opcua_statuscode_t handle_close_session(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                        size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_boolean_t delete_subscriptions;
    s = mu_binary_read_boolean(r, &delete_subscriptions);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_statuscode_t close_result = MU_STATUS_BAD_SESSIONIDINVALID;
    if (server->active_session != NULL && req.authentication_token.identifier_type == MU_NODEID_NUMERIC) {
        close_result =
            mu_session_close(server->active_session, req.authentication_token.identifier.numeric, delete_subscriptions);
#if MUC_OPCUA_SUBSCRIPTIONS
        if (close_result == MU_STATUS_GOOD) {
            opcua_uint32_t session_id = server->active_session->session_id;
            for (size_t i = 0; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
                mu_subscription_t *sub = &server->subs.subscriptions[i];
                if (sub->in_use && sub->session_id == session_id) {
                    if (mu_subscription_delete(&server->subs, session_id, sub->subscription_id) == MU_STATUS_GOOD) {
                        mu_diagnostics_subscription_closed(server);
                    }
                }
            }
        }
#endif
        if (close_result == MU_STATUS_GOOD) {
            mu_diagnostics_session_closed(server);
            server->active_session = NULL;
        }
    }

    s = write_response_prefix(w, MU_ID_CLOSESESSIONRESPONSE, req.request_handle, close_result
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
