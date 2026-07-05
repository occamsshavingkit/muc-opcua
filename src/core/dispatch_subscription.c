/* src/core/dispatch_subscription.c
 * Subscription service-set handlers extracted from service_dispatch.c (T012).
 * Implements CreateSubscription (OPC-10000-4 5.14.2), ModifySubscription
 * (5.14.3), SetPublishingMode (5.14.4), Publish (5.14.5), Republish (5.14.6),
 * and DeleteSubscriptions (5.14.8). The dispatch table in service_dispatch.c
 * references these handlers by name; their extern prototypes live in
 * server_internal.h under MUC_OPCUA_SUBSCRIPTIONS. */
#include "../services/service_header.h"
#include "../services/subscription.h"
#include "muc_opcua/encoding.h"
#include "server_internal.h"
#include "service_dispatch.h"
#include <stddef.h>
#include <string.h>

#if MUC_OPCUA_SUBSCRIPTIONS
#define MU_DISPATCH_MAX_PUBLISH_ACKS MU_MAX_PUBLISH_ACKS

typedef struct {
    opcua_boolean_t publishing_enabled;
} mu_set_publishing_mode_context_t;

/* SetPublishingMode per-id callback: flip publishing_enabled on the matching
   session-owned Subscription. Per OPC-10000-4 §5.14.4 state table row 19,
   enabling publishing MUST reset the lifetime counter and clear the
   MoreNotifications flag. */
static opcua_statuscode_t set_publishing_mode_result(mu_server_t *server, opcua_uint32_t subscription_id,
                                                     void *context) {
    const mu_set_publishing_mode_context_t *mode = (const mu_set_publishing_mode_context_t *)context;
    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    sub->publishing_enabled = mode->publishing_enabled;
    if (mode->publishing_enabled) {
        sub->lifetime_counter = 0u;
        sub->more_notifications = false;
    }
    return MU_STATUS_GOOD;
}

/* DeleteSubscriptions per-id callback. */
static opcua_statuscode_t delete_subscription_result(mu_server_t *server, opcua_uint32_t subscription_id,
                                                     void *context) {
    (void)context;
    return mu_subscription_delete(&server->subs, server->active_session->session_id, subscription_id);
}

/* CreateSubscription (OPC 10000-4 5.14.2): decode the request parameters, let the
   fixed-size engine revise counts, and echo the revised values. */
opcua_statuscode_t handle_create_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint64_t requested_interval_bits;
    opcua_uint32_t requested_lifetime_count;
    opcua_uint32_t requested_max_keep_alive_count;
    opcua_uint32_t max_notifications_per_publish;
    opcua_boolean_t publishing_enabled;
    opcua_byte_t priority;

    mu_binary_read_uint64(r, &requested_interval_bits);
    mu_binary_read_uint32(r, &requested_lifetime_count);
    mu_binary_read_uint32(r, &requested_max_keep_alive_count);
    mu_binary_read_uint32(r, &max_notifications_per_publish);
    mu_binary_read_boolean(r, &publishing_enabled);
    mu_binary_read_byte(r, &priority);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    opcua_uint32_t publishing_interval_ms = publishing_interval_bits_to_ms(requested_interval_bits);
    opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);

    mu_subscription_t *sub = NULL;
    s = mu_subscription_create(&server->subs, server->active_session->session_id, publishing_interval_ms,
                               requested_lifetime_count, requested_max_keep_alive_count, max_notifications_per_publish,
                               priority, publishing_enabled, now_ms, &sub);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_CREATESUBSCRIPTIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_write_uint32(w, sub->subscription_id);
    mu_binary_write_uint64(w, publishing_interval_ms_to_bits(sub->publishing_interval_ms));
    mu_binary_write_uint32(w, sub->lifetime_count);
    mu_binary_write_uint32(w, sub->max_keep_alive_count);
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* ModifySubscription (OPC 10000-4 5.14.3): update a session-owned Subscription and
   return the revised interval/lifetime/keep-alive values. */
opcua_statuscode_t handle_modify_subscription(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint32_t subscription_id;
    opcua_uint64_t requested_interval_bits;
    opcua_uint32_t requested_lifetime_count;
    opcua_uint32_t requested_max_keep_alive_count;
    opcua_uint32_t max_notifications_per_publish;
    opcua_byte_t priority;

    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint64(r, &requested_interval_bits);
    mu_binary_read_uint32(r, &requested_lifetime_count);
    mu_binary_read_uint32(r, &requested_max_keep_alive_count);
    mu_binary_read_uint32(r, &max_notifications_per_publish);
    mu_binary_read_byte(r, &priority);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    mu_subscription_apply_parameters(sub, publishing_interval_bits_to_ms(requested_interval_bits),
                                     requested_lifetime_count, requested_max_keep_alive_count,
                                     max_notifications_per_publish, priority);

    s = write_response_prefix(w, MU_ID_MODIFYSUBSCRIPTIONRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_write_uint64(w, publishing_interval_ms_to_bits(sub->publishing_interval_ms));
    mu_binary_write_uint32(w, sub->lifetime_count);
    mu_binary_write_uint32(w, sub->max_keep_alive_count);
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* SetPublishingMode (OPC 10000-4 5.14.4): toggle publishing per Subscription id and
   report per-id StatusCode results. */
opcua_statuscode_t handle_set_publishing_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_set_publishing_mode_context_t context = {false};
    mu_binary_read_boolean(r, &context.publishing_enabled);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_SETPUBLISHINGMODERESPONSE,
                                              req.request_handle, false, 0u, set_publishing_mode_result, &context);
}

/* DeleteSubscriptions (OPC 10000-4 5.14.8): service-level Good with per-id
   StatusCode results. DiagnosticInfos is empty, matching other array handlers. */
opcua_statuscode_t handle_delete_subscriptions(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                               size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_DELETESUBSCRIPTIONSRESPONSE,
                                              req.request_handle, false, 0u, delete_subscription_result, NULL);
}

/* Publish (OPC 10000-4 5.14.5): decode RequestHeader and acknowledgements, then
   park the request. The publishing timer emits the PublishResponse asynchronously. */
opcua_statuscode_t handle_publish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                  size_t *response_length) {
    (void)w;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t ack_count;
    s = mu_binary_read_int32(r, &ack_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (ack_count < 0) {
        ack_count = 0;
    }
    if ((size_t)ack_count > MU_DISPATCH_MAX_PUBLISH_ACKS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    opcua_uint32_t stored_ack_count = 0u;
    opcua_statuscode_t ack_results[MU_DISPATCH_MAX_PUBLISH_ACKS];
    for (opcua_int32_t i = 0; i < ack_count; ++i) {
        opcua_uint32_t subscription_id;
        opcua_uint32_t sequence_number;
        mu_binary_read_uint32(r, &subscription_id);
        mu_binary_read_uint32(r, &sequence_number);
        if (r->status != MU_STATUS_GOOD) {
            return r->status;
        }
        opcua_statuscode_t ack_result = mu_subscription_acknowledge(&server->subs, server->active_session->session_id,
                                                                    subscription_id, sequence_number);
        if (stored_ack_count < MU_DISPATCH_MAX_PUBLISH_ACKS) {
            ack_results[stored_ack_count] = ack_result;
            ++stored_ack_count;
        }
    }

    opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
    mu_publish_request_t *parked = NULL;
    s = mu_publish_request_enqueue(&server->subs, server->active_session->session_id, server->current_request_id,
                                   req.request_handle, now_ms, &parked);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (parked != NULL) {
        parked->ack_count = stored_ack_count;
        for (opcua_uint32_t i = 0u; i < stored_ack_count; ++i) {
            parked->ack_results[i] = ack_results[i];
        }
    }

    *response_length = 0;
    return MU_STATUS_GOOD;
}

/* Republish (OPC 10000-4 5.14.6): return a retained NotificationMessage body for
   the requested subscription sequence number. */
opcua_statuscode_t handle_republish(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                    size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint32_t subscription_id;
    opcua_uint32_t sequence_number;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &sequence_number);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    const opcua_byte_t *message = NULL;
    size_t message_len = 0u;
    s = mu_subscription_republish(&server->subs, server->active_session->session_id, subscription_id, sequence_number,
                                  &message, &message_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_REPUBLISHRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (message_len > w->length - w->position) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    if (message_len > 0u) {
        if (message == NULL) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        memcpy(w->buffer + w->position, message, message_len);
        w->position += message_len;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SUBSCRIPTIONS */
