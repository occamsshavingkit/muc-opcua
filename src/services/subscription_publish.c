/* src/services/subscription_publish.c */
#include "subscription.h"
#include <string.h>

#if MUC_OPCUA_SUBSCRIPTIONS

#include "../core/server_internal.h"
#include "../core/service_dispatch.h"
#include "muc_opcua/address_space.h"
#include "service_header.h"

#define MU_PUBLISH_BODY_BYTES 512u
#define MU_ID_DATACHANGENOTIFICATION_ENCODING_DEFAULTBINARY 811u

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
#define MU_STATUSCODE_INFOTYPE_DATAVALUE_OVERFLOW 0x00000480u
#ifndef MU_STATUS_BAD_TOOMANYOPERATIONS
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS ((opcua_statuscode_t)0x80100000u)
#else
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS MU_STATUS_BAD_TOOMANYOPERATIONS
#endif
#endif

static bool subscription_id_in_use(const mu_subscriptions_t *subs, opcua_uint32_t subscription_id) {
    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        const mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use && sub->subscription_id == subscription_id) {
            return true;
        }
    }
    return false;
}

static opcua_uint32_t allocate_subscription_id(mu_subscriptions_t *subs) {
    for (size_t i = 0; i <= MU_MAX_SUBSCRIPTIONS; ++i) {
        opcua_uint32_t id = subs->next_subscription_id;
        if (id == 0u) {
            id = 1u;
        }

        subs->next_subscription_id = id + 1u;
        if (subs->next_subscription_id == 0u) {
            subs->next_subscription_id = 1u;
        }

        if (!subscription_id_in_use(subs, id)) {
            return id;
        }
    }

    return 0u;
}

static bool monitored_item_id_in_use(const mu_subscriptions_t *subs, opcua_uint32_t monitored_item_id) {
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use && item->monitored_item_id == monitored_item_id) {
            return true;
        }
    }
    return false;
}

static opcua_uint32_t allocate_monitored_item_id(mu_subscriptions_t *subs) {
    for (size_t i = 0; i <= MU_MAX_MONITORED_ITEMS; ++i) {
        opcua_uint32_t id = subs->next_monitored_item_id;
        if (id == 0u) {
            id = 1u;
        }

        subs->next_monitored_item_id = id + 1u;
        if (subs->next_monitored_item_id == 0u) {
            subs->next_monitored_item_id = 1u;
        }

        if (!monitored_item_id_in_use(subs, id)) {
            return id;
        }
    }

    return 0u;
}

static void revise_subscription_counts(opcua_uint32_t requested_lifetime_count,
                                       opcua_uint32_t requested_max_keep_alive_count,
                                       opcua_uint32_t *revised_lifetime_count,
                                       opcua_uint32_t *revised_max_keep_alive_count) {
    opcua_uint32_t revised_keep_alive = requested_max_keep_alive_count;
    if (revised_keep_alive == 0u) {
        revised_keep_alive = 1u;
    }
    if (revised_keep_alive > (0xFFFFFFFFu / 3u)) {
        revised_keep_alive = 0xFFFFFFFFu / 3u;
    }

    opcua_uint32_t min_lifetime = revised_keep_alive * 3u;
    opcua_uint32_t revised_lifetime = requested_lifetime_count;
    if (revised_lifetime < min_lifetime) {
        revised_lifetime = min_lifetime;
    }

    *revised_lifetime_count = revised_lifetime;
    *revised_max_keep_alive_count = revised_keep_alive;
}

static bool publish_request_dequeue(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                    mu_publish_request_t *out_request) {
    bool found = false;
    size_t best = 0;
    opcua_uint64_t best_enqueued = 0u;

    if (subs == NULL || out_request == NULL) {
        return false;
    }

    for (size_t i = 0; i < MU_MAX_PUBLISH_REQUESTS; ++i) {
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

static opcua_statuscode_t write_publish_response_prefix(mu_binary_writer_t *w, opcua_uint32_t request_handle) {
    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_PUBLISHRESPONSE}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = MU_STATUS_GOOD;
    return mu_response_header_encode(w, &rh);
}

static void backpatch_int32(opcua_byte_t *buffer, size_t position, opcua_int32_t value) {
    buffer[position] = (opcua_byte_t)(value & 0xFF);
    buffer[position + 1u] = (opcua_byte_t)((value >> 8) & 0xFF);
    buffer[position + 2u] = (opcua_byte_t)((value >> 16) & 0xFF);
    buffer[position + 3u] = (opcua_byte_t)((value >> 24) & 0xFF);
}

bool monitored_item_reportable(const mu_monitored_item_t *item, const mu_subscription_t *sub) {
    return item->in_use && item->subscription_id == sub->subscription_id &&
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
           (item->pending || item->queue_count > 0u) &&
#else
           item->pending &&
#endif
           item->monitoring_mode == MU_MONITORING_MODE_REPORTING;
}

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
static bool count_queue_entries(opcua_int32_t *count, opcua_uint32_t max_notifications, opcua_byte_t queue_count) {
    for (opcua_byte_t i = 0u; i < queue_count; ++i) {
        if (max_notifications != 0u && (opcua_uint32_t)*count >= max_notifications) {
            return false;
        }
        ++(*count);
    }

    return true;
}

static opcua_int32_t count_reportable_items(const struct mu_server *server, mu_subscription_t *sub) {
    opcua_int32_t count = 0;
    opcua_uint32_t max_notifications = sub->max_notifications_per_publish;
    sub->more_notifications = false;

    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            if (monitored_item_reportable(&server->subs.monitored_items[i], sub)) {
                const mu_monitored_item_t *item = &server->subs.monitored_items[i];
                if (!count_queue_entries(&count, max_notifications, item->queue_count)) {
                    sub->more_notifications = true;
                    return count;
                }
            }
        }
    }

    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            const mu_monitored_item_t *item = &server->subs.monitored_items[i];
            if (monitored_item_reports_by_trigger(server, sub, item) &&
                !count_queue_entries(&count, max_notifications, item->queue_count)) {
                sub->more_notifications = true;
                return count;
            }
        }
    }

    return count;
}
#else
static opcua_int32_t count_reportable_items(const struct mu_server *server, const mu_subscription_t *sub) {
    opcua_int32_t count = 0;
    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            if (monitored_item_reportable(&server->subs.monitored_items[i], sub)) {
                ++count;
            }
        }
    }
    return count;
}
#endif

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
static void prepare_reportable_queues(struct mu_server *server, const mu_subscription_t *sub) {
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (monitored_item_in_subscription(item, sub) && item->monitoring_mode != MU_MONITORING_MODE_DISABLED) {
            monitored_item_prepare_pending_queue(item);
        }
    }
}

static void enqueue_resend_data(struct mu_server *server, mu_subscription_t *sub) {
    if (!sub->resend_data_pending) {
        return;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (!monitored_item_in_subscription(item, sub) || item->monitoring_mode != MU_MONITORING_MODE_REPORTING) {
            continue;
        }

        mu_variant_t cur;
        memset(&cur, 0, sizeof(cur));
        opcua_statuscode_t status = read_monitored_item_value(item, &cur);
        if (status == MU_STATUS_GOOD) {
            item->last_value = cur;
            item->last_status = status;
            item->has_value = true;
        } else if (item->has_value) {
            cur = item->last_value;
            item->last_status = status;
        } else {
            cur.type = MU_TYPE_NULL;
            item->last_value = cur;
            item->last_status = status;
            item->has_value = true;
        }

        monitored_item_enqueue_report(item, &cur, status);
        server->subs.reportable_bitmap[i / 32u] |= (1u << (i % 32u));
    }

    sub->resend_data_pending = false;
}

static void monitored_item_drain_reported_entries(mu_monitored_item_t *item, opcua_int32_t *remaining) {
    opcua_uint32_t queue_size = monitored_item_effective_queue_size(item);

    while (item->queue_count > 0u && *remaining > 0) {
        item->queue_head = monitored_item_queue_next(item->queue_head, queue_size);
        --item->queue_count;
        --(*remaining);
    }

    if (item->queue_count == 0u) {
        item->queue_head = 0u;
        item->queue_tail = 0u;
        item->pending = false;
        item->queue_overflow = false;
    } else {
        item->pending = true;
    }
}

static void clear_reported_items(struct mu_server *server, const mu_subscription_t *sub, opcua_int32_t report_count) {
    opcua_int32_t remaining = report_count;
    bool triggered_items[MU_MAX_MONITORED_ITEMS];

    (void)memset(triggered_items, 0, sizeof(triggered_items));
    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            triggered_items[i] = monitored_item_reports_by_trigger(server, sub, &server->subs.monitored_items[i]);
        }
    }

    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u && remaining > 0) {
            size_t i = w * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            mu_monitored_item_t *item = &server->subs.monitored_items[i];
            if (!monitored_item_reportable(item, sub)) {
                continue;
            }

            monitored_item_drain_reported_entries(item, &remaining);
            if (!item->pending && item->queue_count == 0u) {
                server->subs.reportable_bitmap[i / 32u] &= ~(1u << (i % 32u));
            }
        }
    }

    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u && remaining > 0) {
            size_t i = w * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            if (triggered_items[i]) {
                monitored_item_drain_reported_entries(&server->subs.monitored_items[i], &remaining);
                if (!server->subs.monitored_items[i].pending && server->subs.monitored_items[i].queue_count == 0u) {
                    server->subs.reportable_bitmap[i / 32u] &= ~(1u << (i % 32u));
                }
            }
        }
    }
}
#else
static void clear_reported_items(struct mu_server *server, const mu_subscription_t *sub) {
    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            mu_monitored_item_t *item = &server->subs.monitored_items[i];
            if (monitored_item_reportable(item, sub)) {
                item->pending = false;
                server->subs.reportable_bitmap[i / 32u] &= ~(1u << (i % 32u));
            }
        }
    }
}
#endif

static opcua_datetime_t publish_time_now(const struct mu_server *server) {
    if (server->config.time_adapter.get_time == NULL) {
        return 0;
    }
    return server->config.time_adapter.get_time(server->config.time_adapter.context);
}

static opcua_statuscode_t write_data_change_notification(mu_binary_writer_t *w, const struct mu_server *server,
                                                         const mu_subscription_t *sub, opcua_int32_t report_count) {
    mu_nodeid_t type_id = {0, MU_NODEID_NUMERIC, {MU_ID_DATACHANGENOTIFICATION_ENCODING_DEFAULTBINARY}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_byte(w, 1u);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    size_t length_pos = w->position;
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    size_t body_start = w->position;

    s = mu_binary_write_int32(w, report_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (size_t bw = 0; bw < MU_REPORTABLE_BITMAP_WORDS; ++bw) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[bw];
        while (bits != 0u) {
            size_t i = bw * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            const mu_monitored_item_t *item = &server->subs.monitored_items[i];
            if (!monitored_item_reportable(item, sub)) {
                continue;
            }

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
            opcua_uint32_t queue_size = item->queue_size;
            opcua_byte_t entry_index = item->queue_head;
            if (queue_size == 0u) {
                queue_size = 1u;
            } else if (queue_size > MU_MONITORED_QUEUE_DEPTH) {
                queue_size = MU_MONITORED_QUEUE_DEPTH;
            }

            for (opcua_byte_t queued = 0u; queued < item->queue_count && report_count > 0; ++queued) {
                const mu_variant_t *value = &item->queue[entry_index].value;
                opcua_statuscode_t status = item->queue[entry_index].status;

                s = mu_binary_write_uint32(w, item->client_handle);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }

                mu_datavalue_t dv;
                memset(&dv, 0, sizeof(dv));
                dv.has_value = true;
                dv.value = *value;
                if (status != MU_STATUS_GOOD) {
                    dv.has_status = true;
                    dv.status = status;
                }
                s = mu_binary_write_datavalue(w, &dv);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }

                --report_count;
                entry_index = monitored_item_queue_next(entry_index, queue_size);
            }
#else
            s = mu_binary_write_uint32(w, item->client_handle);
            if (s != MU_STATUS_GOOD) {
                return s;
            }

            mu_datavalue_t dv;
            (void)memset(&dv, 0, sizeof(dv));
            dv.has_value = true;
            dv.value = item->last_value;
            if (item->last_status != MU_STATUS_GOOD) {
                dv.has_status = true;
                dv.status = item->last_status;
            }
            s = mu_binary_write_datavalue(w, &dv);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
#endif
        }
    }

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    for (size_t bw = 0; bw < MU_REPORTABLE_BITMAP_WORDS && report_count > 0; ++bw) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[bw];
        while (bits != 0u && report_count > 0) {
            size_t i = bw * 32u + (size_t)__builtin_ctz(bits);
            bits &= (bits - 1u);
            if (i >= MU_MAX_MONITORED_ITEMS) {
                break;
            }
            const mu_monitored_item_t *item = &server->subs.monitored_items[i];
            if (!monitored_item_reports_by_trigger(server, sub, item)) {
                continue;
            }

            opcua_uint32_t queue_size = item->queue_size;
            opcua_byte_t entry_index = item->queue_head;
            if (queue_size == 0u) {
                queue_size = 1u;
            } else if (queue_size > MU_MONITORED_QUEUE_DEPTH) {
                queue_size = MU_MONITORED_QUEUE_DEPTH;
            }

            for (opcua_byte_t queued = 0u; queued < item->queue_count && report_count > 0; ++queued) {
                const mu_variant_t *value = &item->queue[entry_index].value;
                opcua_statuscode_t status = item->queue[entry_index].status;

                s = mu_binary_write_uint32(w, item->client_handle);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }

                mu_datavalue_t dv;
                memset(&dv, 0, sizeof(dv));
                dv.has_value = true;
                dv.value = *value;
                if (status != MU_STATUS_GOOD) {
                    dv.has_status = true;
                    dv.status = status;
                }
                s = mu_binary_write_datavalue(w, &dv);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }

                --report_count;
                entry_index = monitored_item_queue_next(entry_index, queue_size);
            }
        }
    }
#endif

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    size_t body_length = w->position - body_start;
    if (body_length > (size_t)0x7FFFFFFF) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    backpatch_int32(w->buffer, length_pos, (opcua_int32_t)body_length);
    return MU_STATUS_GOOD;
}

#ifdef MUC_OPCUA_EVENTS
static opcua_statuscode_t write_event_notification_list(mu_binary_writer_t *w, struct mu_server *server,
                                                        const mu_subscription_t *sub) {
    mu_nodeid_t type_id = {0, MU_NODEID_NUMERIC, {914}};

    opcua_int32_t event_monitored_item_count = 0;
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (item->in_use && item->subscription_id == sub->subscription_id && item->attribute_id == 12u &&
            item->monitoring_mode == MU_MONITORING_MODE_REPORTING) {
            event_monitored_item_count++;
        }
    }

    if (event_monitored_item_count == 0) {
        return mu_binary_write_extension_object_header(w, &type_id, 0);
    }

    opcua_byte_t body_buf[1024];
    mu_binary_writer_t sub_w;
    mu_binary_writer_init(&sub_w, body_buf, sizeof(body_buf));

    opcua_int32_t total_field_lists = event_monitored_item_count * (opcua_int32_t)sub->event_queue.count;
    opcua_statuscode_t s = mu_binary_write_int32(&sub_w, total_field_lists);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (item->in_use && item->subscription_id == sub->subscription_id && item->attribute_id == 12u &&
            item->monitoring_mode == MU_MONITORING_MODE_REPORTING) {

            size_t idx = sub->event_queue.head;
            for (size_t k = 0; k < sub->event_queue.count; ++k) {
                const mu_event_notification_t *ev = &sub->event_queue.queue[idx];
                idx = (idx + 1) % MU_MAX_EVENT_QUEUE_SIZE;

                s = mu_binary_write_uint32(&sub_w, item->client_handle);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }

                s = mu_binary_write_int32(&sub_w, (opcua_int32_t)item->select_clauses_count);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }

                for (opcua_byte_t j = 0; j < item->select_clauses_count; ++j) {
                    opcua_byte_t field_type = item->select_clauses[j];
                    mu_variant_t var;
                    (void)memset(&var, 0, sizeof(var));

                    switch (field_type) {
                    case 1:
                        var.type = MU_TYPE_BYTESTRING;
                        var.value.bytestr = ev->event_id;
                        break;
                    case 2:
                        var.type = MU_TYPE_NODEID;
                        var.value.nodeid = ev->event_type;
                        break;
                    case 3:
                        var.type = MU_TYPE_DATETIME;
                        var.value.dt = ev->time;
                        break;
                    case 4: {
                        var.type = MU_TYPE_LOCALIZEDTEXT;
                        mu_localized_text_t lt = {{-1, NULL}, ev->message};
                        var.value.localized_text = lt;
                    } break;
                    case 5:
                        var.type = MU_TYPE_UINT16;
                        var.value.ui16 = ev->severity;
                        break;
                    default:
                        var.type = MU_TYPE_NULL;
                        break;
                    }
                    s = mu_binary_write_variant(&sub_w, &var);
                    if (s != MU_STATUS_GOOD) {
                        return s;
                    }
                }
            }
        }
    }

    s = mu_binary_write_extension_object_header(w, &type_id, sub_w.position);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (w->position + sub_w.position > w->length) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    (void)memcpy(w->buffer + w->position, body_buf, sub_w.position);
    w->position += sub_w.position;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_server_trigger_event(mu_server_t *server, const mu_event_notification_t *event) {
    if (server == NULL || event == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &server->subs.subscriptions[i];
        if (sub->in_use) {
            mu_event_queue_t *eq = &sub->event_queue;
            if (eq->count >= MU_MAX_EVENT_QUEUE_SIZE) {
                eq->head = (eq->head + 1) % MU_MAX_EVENT_QUEUE_SIZE;
                eq->count--;
            }
            eq->queue[eq->tail] = *event;
            eq->tail = (eq->tail + 1) % MU_MAX_EVENT_QUEUE_SIZE;
            eq->count++;
        }
    }
    return MU_STATUS_GOOD;
}
#endif

static opcua_statuscode_t build_publish_response(struct mu_server *server, const mu_subscription_t *sub,
                                                 const mu_publish_request_t *request, opcua_uint32_t sequence_number,
                                                 bool include_data, opcua_int32_t report_count,
                                                 opcua_datetime_t publish_time, opcua_byte_t *buffer,
                                                 size_t buffer_size, size_t *out_length, size_t *out_message_start,
                                                 size_t *out_message_end) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, buffer_size);
    if (out_message_start != NULL) {
        *out_message_start = 0u;
    }
    if (out_message_end != NULL) {
        *out_message_end = 0u;
    }

    opcua_statuscode_t s = write_publish_response_prefix(&w, request->request_handle);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_uint32(&w, sub->subscription_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(&w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    s = mu_binary_write_boolean(&w, include_data && sub->more_notifications);
#else
    s = mu_binary_write_boolean(&w, false);
#endif
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    size_t message_start = w.position;
    s = mu_binary_write_uint32(&w, sequence_number);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int64(&w, publish_time);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t notification_data_count = 0;
    if (include_data) {
        if (report_count > 0) {
            notification_data_count++;
        }
#ifdef MUC_OPCUA_EVENTS
        if (sub->event_queue.count > 0) {
            notification_data_count++;
        }
#endif
    }
    s = mu_binary_write_int32(&w, notification_data_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (include_data) {
        if (report_count > 0) {
            s = write_data_change_notification(&w, server, sub, report_count);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
#ifdef MUC_OPCUA_EVENTS
        if (sub->event_queue.count > 0) {
            s = write_event_notification_list(&w, server, sub);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
#endif
    }
    size_t message_end = w.position;

    opcua_uint32_t ack_count = request->ack_count;
    if (ack_count > MU_MAX_PUBLISH_ACKS) {
        ack_count = MU_MAX_PUBLISH_ACKS;
    }
    s = mu_binary_write_int32(&w, (opcua_int32_t)ack_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_uint32_t i = 0; i < ack_count; ++i) {
        s = mu_binary_write_statuscode(&w, request->ack_results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
    s = mu_binary_write_int32(&w, 0);
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

static opcua_uint32_t current_sequence_number(mu_subscription_t *sub) {
    if (sub->sequence_number == 0u) {
        sub->sequence_number = 1u;
    }
    return sub->sequence_number;
}

static void advance_sequence_number(mu_subscription_t *sub) {
    ++sub->sequence_number;
    if (sub->sequence_number == 0u) {
        sub->sequence_number = 1u;
    }
}

static void store_retransmit(mu_subscription_t *sub, opcua_uint32_t sequence_number, opcua_datetime_t publish_time,
                             const opcua_byte_t *body, size_t body_length) {
    sub->retransmit.valid = false;
    sub->retransmit.message_len = 0u;

    if (body_length > MU_RETRANSMIT_BYTES) {
        return;
    }

    sub->retransmit.sequence_number = sequence_number;
    sub->retransmit.publish_time = publish_time;
    (void)memcpy(sub->retransmit.message, body, body_length);
    sub->retransmit.message_len = body_length;
    sub->retransmit.valid = true;
}

static bool publish_response_oversize_status(opcua_statuscode_t status) {
    return status == MU_STATUS_BAD_ENCODINGERROR || status == MU_STATUS_BAD_OUTOFMEMORY ||
           status == MU_STATUS_BAD_RESPONSETOOLARGE;
}

static opcua_statuscode_t emit_publish_response_too_large_fault(struct mu_server *server,
                                                                const mu_publish_request_t *request,
                                                                opcua_byte_t *buffer, size_t buffer_size) {
    size_t fault_length = buffer_size;
    opcua_statuscode_t s =
        mu_write_service_fault(buffer, &fault_length, request->request_handle, MU_STATUS_BAD_RESPONSETOOLARGE);
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

static void publish_due(struct mu_server *server, opcua_uint64_t now_ms) {
    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &server->subs.subscriptions[i];
        if (!sub->in_use || sub->next_publish_ms > now_ms) {
            continue;
        }

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
        mu_connection_t *conn_match = NULL;
        mu_session_t *session = NULL;
        for (size_t s_idx = 0; s_idx < MU_MAX_SESSIONS; ++s_idx) {
            if (server->sessions[s_idx].state != MU_SESSION_STATE_CLOSED &&
                server->sessions[s_idx].session_id == sub->session_id) {
                session = &server->sessions[s_idx];
                break;
            }
        }
        if (session != NULL) {
            for (size_t c = 0; c < MU_MAX_CONNECTIONS; ++c) {
                if (server->conns[c].client_handle != NULL &&
                    server->conns[c].secure_channel.channel_id == session->secure_channel_id) {
                    conn_match = &server->conns[c];
                    break;
                }
            }
        }
        if (conn_match == NULL) {
            continue;
        }
        server->active_conn = conn_match;
#endif

        opcua_int32_t report_count = 0;
        bool has_data = false;
        bool has_events = false;
#ifdef MUC_OPCUA_EVENTS
        has_events = (sub->event_queue.count > 0);
#endif
        if (sub->publishing_enabled) {
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
            prepare_reportable_queues(server, sub);
            enqueue_resend_data(server, sub);
#endif
            report_count = count_reportable_items(server, sub);
            has_data = (report_count > 0) || has_events;
        }

        if (has_data) {
            mu_publish_request_t request;
            if (publish_request_dequeue(&server->subs, sub->session_id, &request)) {
                opcua_byte_t body[MU_PUBLISH_BODY_BYTES];
                size_t body_length = 0;
                size_t message_start = 0;
                size_t message_end = 0;
                opcua_uint32_t sequence_number = current_sequence_number(sub);
                opcua_datetime_t publish_time = publish_time_now(server);
                bool sent_response_too_large_fault = false;
                opcua_statuscode_t s =
                    build_publish_response(server, sub, &request, sequence_number, true, report_count, publish_time,
                                           body, sizeof(body), &body_length, &message_start, &message_end);
                if (s == MU_STATUS_GOOD) {
                    s = mu_server_emit_message(server, request.request_id, body, body_length);
                }
                if (publish_response_oversize_status(s)) {
                    s = emit_publish_response_too_large_fault(server, &request, body, sizeof(body));
                    sent_response_too_large_fault = (s == MU_STATUS_GOOD);
                }
                if (s == MU_STATUS_GOOD && !sent_response_too_large_fault) {
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
                    clear_reported_items(server, sub, report_count);
#else
                    clear_reported_items(server, sub);
#endif
#ifdef MUC_OPCUA_EVENTS
                    if (has_events) {
                        mu_subscription_t *mutable_sub = (mu_subscription_t *)sub;
                        mutable_sub->event_queue.head = 0;
                        mutable_sub->event_queue.tail = 0;
                        mutable_sub->event_queue.count = 0;
                    }
#endif
                    sub->keep_alive_counter = 0u;
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
                if (publish_request_dequeue(&server->subs, sub->session_id, &request)) {
                    opcua_byte_t body[MU_PUBLISH_BODY_BYTES];
                    size_t body_length = 0;
                    opcua_uint32_t sequence_number = current_sequence_number(sub);
                    opcua_datetime_t publish_time = publish_time_now(server);
                    bool sent_response_too_large_fault = false;
                    opcua_statuscode_t s =
                        build_publish_response(server, sub, &request, sequence_number, false, 0, publish_time, body,
                                               sizeof(body), &body_length, NULL, NULL);
                    if (s == MU_STATUS_GOOD) {
                        s = mu_server_emit_message(server, request.request_id, body, body_length);
                    }
                    if (publish_response_oversize_status(s)) {
                        s = emit_publish_response_too_large_fault(server, &request, body, sizeof(body));
                        sent_response_too_large_fault = (s == MU_STATUS_GOOD);
                    }
                    if (s == MU_STATUS_GOOD && !sent_response_too_large_fault) {
                        sub->keep_alive_counter = 0u;
                    }
                }
            }
        }

        advance_publish_timer(sub, now_ms);

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
        server->active_conn = NULL;
#endif
    }
}

void mu_subscriptions_init(mu_subscriptions_t *subs) {
    if (subs != NULL) {
        (void)memset(subs, 0, sizeof(*subs));
        subs->next_subscription_id = 1u;
        subs->next_monitored_item_id = 1u;
    }
}

opcua_statuscode_t mu_publish_request_enqueue(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                              opcua_uint32_t request_id, opcua_uint32_t request_handle,
                                              opcua_uint64_t now_ms, mu_publish_request_t **out_req) {
    if (out_req != NULL) {
        *out_req = NULL;
    }
    if (subs == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    for (size_t i = 0; i < MU_MAX_PUBLISH_REQUESTS; ++i) {
        mu_publish_request_t *slot = &subs->publish_queue[i];
        if (!slot->in_use) {
            (void)memset(slot, 0, sizeof(*slot));
            slot->in_use = true;
            slot->session_id = session_id;
            slot->request_id = request_id;
            slot->request_handle = request_handle;
            slot->enqueued_ms = now_ms;
            slot->ack_count = 0u;
            if (out_req != NULL) {
                *out_req = slot;
            }
            return MU_STATUS_GOOD;
        }
    }

    return MU_STATUS_BAD_TOOMANYPUBLISHREQUESTS;
}

opcua_statuscode_t mu_subscription_create(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                          opcua_uint32_t publishing_interval_ms,
                                          opcua_uint32_t requested_lifetime_count,
                                          opcua_uint32_t requested_max_keep_alive_count,
                                          opcua_uint32_t max_notifications_per_publish, opcua_byte_t priority,
                                          bool publishing_enabled, opcua_uint64_t now_ms, mu_subscription_t **out_sub) {
    if (subs == NULL || out_sub == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    mu_subscription_t *slot = NULL;
    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        if (!subs->subscriptions[i].in_use) {
            slot = &subs->subscriptions[i];
            break;
        }
    }

    if (slot == NULL) {
        *out_sub = NULL;
        return MU_STATUS_BAD_TOOMANYSUBSCRIPTIONS;
    }

    opcua_uint32_t subscription_id = allocate_subscription_id(subs);
    if (subscription_id == 0u) {
        *out_sub = NULL;
        return MU_STATUS_BAD_TOOMANYSUBSCRIPTIONS;
    }

    memset(slot, 0, sizeof(*slot));
    slot->in_use = true;
    slot->subscription_id = subscription_id;
    slot->session_id = session_id;
    mu_subscription_apply_parameters(slot, publishing_interval_ms, requested_lifetime_count,
                                     requested_max_keep_alive_count, max_notifications_per_publish, priority);
    slot->publishing_enabled = publishing_enabled;
    slot->sequence_number = 1u;
    slot->keep_alive_counter = 0u;
    slot->lifetime_counter = 0u;
    slot->next_publish_ms = now_ms + publishing_interval_ms;
    slot->more_notifications = false;
    slot->retransmit.valid = false;

    *out_sub = slot;
    return MU_STATUS_GOOD;
}

void mu_subscription_apply_parameters(mu_subscription_t *sub, opcua_uint32_t publishing_interval_ms,
                                      opcua_uint32_t requested_lifetime_count,
                                      opcua_uint32_t requested_max_keep_alive_count,
                                      opcua_uint32_t max_notifications_per_publish, opcua_byte_t priority) {
    if (sub == NULL) {
        return;
    }

    opcua_uint32_t revised_lifetime = 0u;
    opcua_uint32_t revised_keep_alive = 0u;
    revise_subscription_counts(requested_lifetime_count, requested_max_keep_alive_count, &revised_lifetime,
                               &revised_keep_alive);

    sub->publishing_interval_ms = publishing_interval_ms;
    sub->max_keep_alive_count = revised_keep_alive;
    sub->lifetime_count = revised_lifetime;
    sub->max_notifications_per_publish = max_notifications_per_publish;
    sub->priority = priority;
}

mu_subscription_t *mu_subscription_find(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                        opcua_uint32_t subscription_id) {
    if (subs == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use && sub->session_id == session_id && sub->subscription_id == subscription_id) {
            return sub;
        }
    }

    return NULL;
}

opcua_statuscode_t mu_subscription_acknowledge(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                               opcua_uint32_t subscription_id, opcua_uint32_t sequence_number) {
    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    if (sub->retransmit.valid && sub->retransmit.sequence_number == sequence_number) {
        sub->retransmit.valid = false;
        sub->retransmit.message_len = 0u;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_SEQUENCENUMBERUNKNOWN;
}

opcua_statuscode_t mu_subscription_republish(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                             opcua_uint32_t subscription_id, opcua_uint32_t sequence_number,
                                             const opcua_byte_t **out_msg, size_t *out_len) {
    if (out_msg == NULL || out_len == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    *out_msg = NULL;
    *out_len = 0u;

    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    if (sub->retransmit.valid && sub->retransmit.sequence_number == sequence_number) {
        *out_msg = sub->retransmit.message;
        *out_len = sub->retransmit.message_len;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_MESSAGENOTAVAILABLE;
}

opcua_statuscode_t mu_subscription_delete(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                          opcua_uint32_t subscription_id) {
    if (subs == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use && item->subscription_id == subscription_id) {
            memset(item, 0, sizeof(*item));
            if (subs->active_monitored_items_count > 0) {
                subs->active_monitored_items_count--;
            }
        }
    }

    memset(sub, 0, sizeof(*sub));
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_monitored_item_alloc(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                           mu_monitored_item_t **out_item) {
    if (subs == NULL || out_item == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    *out_item = NULL;

    mu_monitored_item_t *slot = NULL;
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        if (!subs->monitored_items[i].in_use) {
            slot = &subs->monitored_items[i];
            break;
        }
    }

    if (slot == NULL) {
        return MU_STATUS_BAD_TOOMANYMONITOREDITEMS;
    }

    opcua_uint32_t monitored_item_id = allocate_monitored_item_id(subs);
    if (monitored_item_id == 0u) {
        return MU_STATUS_BAD_TOOMANYMONITOREDITEMS;
    }

    memset(slot, 0, sizeof(*slot));
    slot->in_use = true;
    slot->subscription_id = subscription_id;
    slot->monitored_item_id = monitored_item_id;
    subs->active_monitored_items_count++;

    *out_item = slot;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_monitored_item_delete(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                            opcua_uint32_t monitored_item_id) {
    if (subs == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use && item->subscription_id == subscription_id && item->monitored_item_id == monitored_item_id) {
            memset(item, 0, sizeof(*item));
            if (subs->active_monitored_items_count > 0) {
                subs->active_monitored_items_count--;
            }
            return MU_STATUS_GOOD;
        }
    }

    return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
}

void mu_subscriptions_tick(struct mu_server *server, opcua_uint64_t now_ms) {
    if (server == NULL) {
        return;
    }

    (void)memset(server->subs.reportable_bitmap, 0, sizeof(server->subs.reportable_bitmap));

    size_t active_checked = 0;
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS && active_checked < server->subs.active_monitored_items_count; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (!item->in_use) {
            continue;
        }
        active_checked++;

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        if (item->pending || item->queue_count > 0u)
#else
        if (item->pending)
#endif
        {
            server->subs.reportable_bitmap[i / 32u] |= (1u << (i % 32u));
        }

        if (item->monitoring_mode == MU_MONITORING_MODE_DISABLED) {
            continue;
        }

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        if (item->has_aggregate) {
            if ((opcua_double_t)(now_ms - (opcua_uint64_t)item->aggregate_state.last_calculation) >=
                item->aggregate_state.processing_interval) {
                monitored_item_publish_aggregate(item, now_ms);
                if (item->pending || item->queue_count > 0u) {
                    server->subs.reportable_bitmap[i / 32u] |= (1u << (i % 32u));
                }
            }
        }
#endif

        if (now_ms < item->next_sample_ms) {
            continue;
        }

#ifdef MUC_OPCUA_EVENTS
        if (item->attribute_id == 12u) {
            continue;
        }
#endif

        mu_variant_t cur;
        memset(&cur, 0, sizeof(cur));
        opcua_statuscode_t status = read_monitored_item_value(item, &cur);

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        if (item->has_aggregate) {
            if (status == MU_STATUS_GOOD) {
                monitored_item_accumulate_aggregate(item, &cur);
            }
            item->last_value = cur;
            item->last_status = status;
            item->has_value = true;
            advance_sample_timer(item, now_ms);
            continue;
        }
#endif

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        if (item->has_value && (!item->has_reported || (item->pending && item->queue_count == 0u))) {
            monitored_item_prepare_pending_queue(item);
        }
#endif

        if (!item->has_value) {
            item->last_value = cur;
            item->last_status = status;
            item->has_value = true;
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
            monitored_item_enqueue_report(item, &cur, status);
#else
            item->pending = true;
#endif
        } else if (monitored_item_sample_changed(item, &cur, status)) {
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
            if (monitored_item_change_reportable(item, &cur, status)) {
                item->last_value = cur;
                item->last_status = status;
                monitored_item_enqueue_report(item, &cur, status);
            }
#else
            item->last_value = cur;
            item->last_status = status;
            item->pending = true;
#endif
        }

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
        if (item->pending || item->queue_count > 0u)
#else
        if (item->pending)
#endif
        {
            server->subs.reportable_bitmap[i / 32u] |= (1u << (i % 32u));
        }

        advance_sample_timer(item, now_ms);
    }

    publish_due(server, now_ms);
}

#endif /* MUC_OPCUA_SUBSCRIPTIONS */
