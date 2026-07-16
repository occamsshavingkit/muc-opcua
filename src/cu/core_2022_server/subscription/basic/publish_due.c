#include "services/subscription_publish/common.h"

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC

bool publish_request_dequeue(mu_subscriptions_t *subs, opcua_uint32_t session_id, mu_publish_request_t *out_request) {
    bool found = false;
    size_t best = 0;
    opcua_uint64_t best_enqueued = 0u;

    if (subs == NULL || out_request == NULL) {
        return false;
    }

    for (size_t i = 0; i < MU_INTERN_MAX_PUBLISH_REQUESTS; ++i) {
        const mu_publish_request_t *req = &subs->publish_queue[i];
        if (!req->in_use || req->session_id != session_id) {
            continue;
        }
        if (!found || req->enqueued_ms < best_enqueued) {
            found = true;
            best = i;
            best_enqueued = req->enqueued_ms;
        }
    }

    if (!found) {
        return false;
    }

    *out_request = subs->publish_queue[best];
    (void)memset(&subs->publish_queue[best], 0, sizeof(subs->publish_queue[best]));
    return true;
}

bool publish_request_dequeue_valid(struct mu_server *server, opcua_uint32_t session_id, opcua_uint64_t now_ms,
                                   mu_publish_request_t *out_request) {
    if (!publish_request_dequeue(&server->subs, session_id, out_request)) {
        return false;
    }

    if (out_request->timeout_hint == 0u || now_ms < out_request->enqueued_ms) {
        return true;
    }

    if ((now_ms - out_request->enqueued_ms) <= (opcua_uint64_t)out_request->timeout_hint) {
        return true;
    }

    opcua_byte_t fault_buf[MU_PUBLISH_BODY_BYTES];
    size_t fault_length = sizeof(fault_buf);
    opcua_statuscode_t s =
        mu_write_service_fault(fault_buf, &fault_length, out_request->request_handle, MU_STATUS_BAD_TIMEOUT
#ifdef MUC_OPCUA_CU_TIME_SYNC
                               ,
                               server
#endif
        );
    if (s == MU_STATUS_GOOD) {
        (void)mu_server_emit_message(server, out_request->request_id, fault_buf, fault_length);
    }
    return false;
}

bool publish_request_evict_oldest(struct mu_server *server, opcua_uint32_t session_id) {
    mu_publish_request_t evicted;
    if (server == NULL || !publish_request_dequeue(&server->subs, session_id, &evicted)) {
        return false;
    }

    opcua_byte_t fault_buf[MU_PUBLISH_BODY_BYTES];
    size_t fault_length = sizeof(fault_buf);
    opcua_statuscode_t s =
        mu_write_service_fault(fault_buf, &fault_length, evicted.request_handle, MU_STATUS_BAD_TOOMANYPUBLISHREQUESTS
#ifdef MUC_OPCUA_CU_TIME_SYNC
                               ,
                               server
#endif
        );
    if (s == MU_STATUS_GOOD) {
        (void)mu_server_emit_message(server, evicted.request_id, fault_buf, fault_length);
    }
    return true;
}

opcua_statuscode_t write_publish_response_prefix(mu_binary_writer_t *w, opcua_uint32_t request_handle) {
    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_PUBLISHRESPONSE}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = MU_STATUS_GOOD;
    rh.return_diagnostics = 0u;
    return mu_response_header_encode(w, &rh);
}

void backpatch_int32(opcua_byte_t *buffer, size_t position, opcua_int32_t value) {
    buffer[position] = (opcua_byte_t)(value & 0xFF);
    buffer[position + 1u] = (opcua_byte_t)((value >> 8) & 0xFF);
    buffer[position + 2u] = (opcua_byte_t)((value >> 16) & 0xFF);
    buffer[position + 3u] = (opcua_byte_t)((value >> 24) & 0xFF);
}

opcua_datetime_t publish_time_now(const struct mu_server *server) {
    if (server->config.time_adapter.get_time == NULL) {
        return 0;
    }
    return server->config.time_adapter.get_time(server->config.time_adapter.context);
}

static opcua_statuscode_t write_publish_response_header(mu_binary_writer_t *w, const mu_subscription_t *sub,
                                                        opcua_uint32_t request_handle, bool include_data,
                                                        opcua_uint32_t sequence_number, opcua_datetime_t publish_time,
                                                        size_t *message_start) {
    opcua_statuscode_t s = write_publish_response_prefix(w, request_handle);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_uint32(w, sub->subscription_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    s = mu_binary_write_boolean(w, include_data && sub->more_notifications);
#else
    (void)include_data;
    s = mu_binary_write_boolean(w, false);
#endif
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *message_start = w->position;
    s = mu_binary_write_uint32(w, sequence_number);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int64(w, publish_time);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t write_publish_response_results(mu_binary_writer_t *w, const mu_publish_request_t *request) {
    opcua_uint32_t ack_count = request->ack_count;
    if (ack_count > MU_MAX_PUBLISH_ACKS) {
        ack_count = MU_MAX_PUBLISH_ACKS;
    }
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)ack_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_uint32_t i = 0; i < ack_count; ++i) {
        s = mu_binary_write_statuscode(w, request->ack_results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t build_publish_response(struct mu_server *server, const mu_subscription_t *sub,
                                                 const mu_publish_request_t *request, opcua_uint32_t sequence_number,
                                                 bool include_data, opcua_int32_t report_count,
                                                 opcua_statuscode_t status_change, opcua_datetime_t publish_time,
                                                 opcua_byte_t *buffer, size_t buffer_size, size_t *out_length,
                                                 size_t *out_message_start, size_t *out_message_end) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, buffer_size);
    if (out_message_start != NULL) {
        *out_message_start = 0u;
    }
    if (out_message_end != NULL) {
        *out_message_end = 0u;
    }

    size_t message_start;
    opcua_statuscode_t s = write_publish_response_header(&w, sub, request->request_handle, include_data,
                                                         sequence_number, publish_time, &message_start);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t notification_data_count = 0;
    if (status_change != MU_STATUS_GOOD) {
        notification_data_count++;
    } else if (include_data) {
        if (report_count > 0) {
            notification_data_count++;
        }
#ifdef MUC_OPCUA_CU_EVENTS
        if (sub->event_queue.count > 0) {
            notification_data_count++;
        }
#endif
    }
    s = mu_binary_write_int32(&w, notification_data_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (status_change != MU_STATUS_GOOD) {
        s = write_status_change_notification(&w, status_change);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    } else if (include_data) {
        if (report_count > 0) {
            s = write_data_change_notification(&w, server, sub, report_count);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
#ifdef MUC_OPCUA_CU_EVENTS
        if (sub->event_queue.count > 0) {
            s = write_event_notification_list(&w, server, sub);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
#endif
    }
    size_t message_end = w.position;

    s = write_publish_response_results(&w, request);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *out_length = w.position;
    if (out_message_start != NULL) {
        *out_message_start = message_start;
    }
    if (out_message_end != NULL) {
        *out_message_end = message_end;
    }
    return MU_STATUS_GOOD;
}

opcua_uint32_t current_sequence_number(mu_subscription_t *sub) {
    if (sub->sequence_number == 0u) {
        sub->sequence_number = 1u;
    }
    return sub->sequence_number;
}

void advance_sequence_number(mu_subscription_t *sub) {
    ++sub->sequence_number;
    if (sub->sequence_number == 0u) {
        sub->sequence_number = 1u;
    }
}

static opcua_statuscode_t emit_publish_response_too_large_fault(struct mu_server *server,
                                                                const mu_publish_request_t *request,
                                                                opcua_byte_t *buffer, size_t buffer_size) {
    size_t fault_length = buffer_size;
    opcua_statuscode_t s =
        mu_write_service_fault(buffer, &fault_length, request->request_handle, MU_STATUS_BAD_RESPONSETOOLARGE
#ifdef MUC_OPCUA_CU_TIME_SYNC
                               ,
                               server
#endif
        );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return mu_server_emit_message(server, request->request_id, buffer, fault_length);
}

void advance_publish_timer(mu_subscription_t *sub, opcua_uint64_t now_ms) {
    opcua_uint64_t interval = sub->publishing_interval_ms;
    const opcua_uint64_t max = ~(opcua_uint64_t)0;

    if (interval == 0u) {
        interval = 1u;
    }

    unsigned safety = 0u;
    do {
        if (sub->next_publish_ms > max - interval) {
            sub->next_publish_ms = max;
            break;
        }
        sub->next_publish_ms += interval;
        safety++;
    } while ((sub->next_publish_ms <= now_ms) && (safety < 100u));
}

void issue_status_change_notification(struct mu_server *server, mu_subscription_t *sub, opcua_statuscode_t status) {
    mu_publish_request_t request;
    if (!publish_request_dequeue(&server->subs, sub->session_id, &request)) {
        return;
    }

    opcua_byte_t body[MU_PUBLISH_BODY_BYTES];
    size_t body_length = 0;
    size_t message_start = 0;
    size_t message_end = 0;
    opcua_uint32_t sequence_number = current_sequence_number(sub);
    opcua_datetime_t publish_time = publish_time_now(server);
    bool sent_response_too_large_fault = false;
    opcua_statuscode_t s =
        build_publish_response(server, sub, &request, sequence_number, false, 0, status, publish_time, body,
                               sizeof(body), &body_length, &message_start, &message_end);
    if (s == MU_STATUS_GOOD) {
        s = mu_server_emit_message(server, request.request_id, body, body_length);
    }
    if (publish_response_oversize_status(s)) {
        s = emit_publish_response_too_large_fault(server, &request, body, sizeof(body));
        sent_response_too_large_fault = (s == MU_STATUS_GOOD);
    }
    if (s == MU_STATUS_GOOD && !sent_response_too_large_fault) {
        if (message_end >= message_start && message_end <= body_length) {
            store_retransmit(sub, sequence_number, publish_time, body + message_start, message_end - message_start);
        }
        advance_sequence_number(sub);
    }
}

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
static bool select_publish_subscription(struct mu_server *server, const mu_subscription_t *sub) {
    mu_connection_t *conn_match = NULL;
    mu_session_t *session = NULL;
    for (size_t s_idx = 0; s_idx < MU_INTERN_MAX_SESSIONS; ++s_idx) {
        if (server->sessions[s_idx].state != MU_SESSION_STATE_CLOSED &&
            server->sessions[s_idx].session_id == sub->session_id) {
            session = &server->sessions[s_idx];
            break;
        }
    }
    if (session != NULL) {
        for (size_t c = 0; c < MU_INTERN_MAX_CONNECTIONS; ++c) {
            if (server->conns[c].client_handle != NULL &&
                server->conns[c].secure_channel.channel_id == session->secure_channel_id) {
                conn_match = &server->conns[c];
                break;
            }
        }
    }
    if (conn_match == NULL) {
        return false;
    }
    server->active_conn = conn_match;
    return true;
}
#endif

void prepare_publish_notification_data(struct mu_server *server, mu_subscription_t *sub, opcua_int32_t *report_count,
                                       bool *has_data, bool *has_events) {
    *report_count = 0;
    *has_data = false;
    *has_events = false;
#ifdef MUC_OPCUA_CU_EVENTS
    *has_events = (sub->event_queue.count > 0);
#endif
    if (sub->publishing_enabled) {
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
        prepare_reportable_queues(server, sub);
        enqueue_resend_data(server, sub);
#endif
        *report_count = count_reportable_items(server, sub);
        *has_data = (*report_count > 0) || *has_events;
    }
}

void publish_response_encode_and_emit(struct mu_server *server, mu_subscription_t *sub, bool has_data,
                                      opcua_int32_t report_count, bool has_events, opcua_uint64_t now_ms) {
    (void)has_events;
    if (has_data) {
        mu_publish_request_t request;
        if (publish_request_dequeue_valid(server, sub->session_id, now_ms, &request)) {
            opcua_byte_t body[MU_PUBLISH_BODY_BYTES];
            size_t body_length = 0;
            size_t message_start = 0;
            size_t message_end = 0;
            opcua_uint32_t sequence_number = current_sequence_number(sub);
            opcua_datetime_t publish_time = publish_time_now(server);
            bool sent_response_too_large_fault = false;
            opcua_statuscode_t s =
                build_publish_response(server, sub, &request, sequence_number, true, report_count, MU_STATUS_GOOD,
                                       publish_time, body, sizeof(body), &body_length, &message_start, &message_end);
            if (s == MU_STATUS_GOOD) {
                s = mu_server_emit_message(server, request.request_id, body, body_length);
            }
            if (publish_response_oversize_status(s)) {
                s = emit_publish_response_too_large_fault(server, &request, body, sizeof(body));
                sent_response_too_large_fault = (s == MU_STATUS_GOOD);
            }
            if (s == MU_STATUS_GOOD && !sent_response_too_large_fault) {
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
                clear_reported_items(server, sub, report_count);
#else
                clear_reported_items(server, sub);
#endif
#ifdef MUC_OPCUA_CU_EVENTS
                if (has_events) {
                    sub->event_queue.head = 0;
                    sub->event_queue.tail = 0;
                    sub->event_queue.count = 0;
                }
#endif
                sub->keep_alive_counter = 0u;
                sub->lifetime_counter = 0u;
                if (message_end >= message_start && message_end <= body_length) {
                    store_retransmit(sub, sequence_number, publish_time, body + message_start,
                                     message_end - message_start);
                } else {
                    sub->retransmit.valid = false;
                    sub->retransmit.message_len = 0u;
                }
                advance_sequence_number(sub);
            }
        }
    } else {
        ++sub->keep_alive_counter;
        if (sub->keep_alive_counter >= sub->max_keep_alive_count) {
            mu_publish_request_t request;
            if (publish_request_dequeue_valid(server, sub->session_id, now_ms, &request)) {
                opcua_byte_t body[MU_PUBLISH_BODY_BYTES];
                size_t body_length = 0;
                opcua_uint32_t sequence_number = current_sequence_number(sub);
                opcua_datetime_t publish_time = publish_time_now(server);
                bool sent_response_too_large_fault = false;
                opcua_statuscode_t s =
                    build_publish_response(server, sub, &request, sequence_number, false, 0, MU_STATUS_GOOD,
                                           publish_time, body, sizeof(body), &body_length, NULL, NULL);
                if (s == MU_STATUS_GOOD) {
                    s = mu_server_emit_message(server, request.request_id, body, body_length);
                }
                if (publish_response_oversize_status(s)) {
                    s = emit_publish_response_too_large_fault(server, &request, body, sizeof(body));
                    sent_response_too_large_fault = (s == MU_STATUS_GOOD);
                }
                if (s == MU_STATUS_GOOD && !sent_response_too_large_fault) {
                    sub->keep_alive_counter = 0u;
                    sub->lifetime_counter = 0u;
                }
            }
        }
    }
}

void publish_due(struct mu_server *server, opcua_uint64_t now_ms) {
    for (size_t i = 0; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &server->subs.subscriptions[i];
        if (!sub->in_use || sub->next_publish_ms > now_ms) {
            continue;
        }

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
        if (!select_publish_subscription(server, sub)) {
            continue;
        }
#endif

        opcua_int32_t report_count;
        bool has_data;
        bool has_events;
        prepare_publish_notification_data(server, sub, &report_count, &has_data, &has_events);

        publish_response_encode_and_emit(server, sub, has_data, report_count, has_events, now_ms);

        advance_publish_timer(sub, now_ms);
        ++sub->lifetime_counter;
        if (sub->lifetime_counter >= sub->lifetime_count) {
            issue_status_change_notification(server, sub, MU_STATUS_BAD_TIMEOUT);
            mu_subscription_delete(&server->subs, sub->session_id, sub->subscription_id);
        }

#ifdef MUC_OPCUA_CU_MULTIPLE_CONNECTIONS
        server->active_conn = NULL;
#endif
    }
}

#endif /* MUC_OPCUA_CU_SUBSCRIPTION_BASIC */
