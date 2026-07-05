#include "common.h"

#if MUC_OPCUA_SUBSCRIPTIONS

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

opcua_int32_t count_reportable_items(const struct mu_server *server, mu_subscription_t *sub) {
    opcua_int32_t count = 0;
    opcua_uint32_t max_notifications = sub->max_notifications_per_publish;
    sub->more_notifications = false;

    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)mu_ctz_u32(bits);
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
            size_t i = w * 32u + (size_t)mu_ctz_u32(bits);
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
opcua_int32_t count_reportable_items(const struct mu_server *server, mu_subscription_t *sub) {
    opcua_int32_t count = 0;
    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)mu_ctz_u32(bits);
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
void prepare_reportable_queues(struct mu_server *server, const mu_subscription_t *sub) {
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (monitored_item_in_subscription(item, sub) && item->monitoring_mode != MU_MONITORING_MODE_DISABLED) {
            monitored_item_prepare_pending_queue(item);
        }
    }
}

void enqueue_resend_data(struct mu_server *server, mu_subscription_t *sub) {
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

void monitored_item_drain_reported_entries(mu_monitored_item_t *item, opcua_int32_t *remaining) {
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

void clear_reported_items(struct mu_server *server, const mu_subscription_t *sub, opcua_int32_t report_count) {
    opcua_int32_t remaining = report_count;
    bool triggered_items[MU_MAX_MONITORED_ITEMS];

    (void)memset(triggered_items, 0, sizeof(triggered_items));
    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)mu_ctz_u32(bits);
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
            size_t i = w * 32u + (size_t)mu_ctz_u32(bits);
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
            size_t i = w * 32u + (size_t)mu_ctz_u32(bits);
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
void clear_reported_items(struct mu_server *server, const mu_subscription_t *sub) {
    for (size_t w = 0; w < MU_REPORTABLE_BITMAP_WORDS; ++w) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[w];
        while (bits != 0u) {
            size_t i = w * 32u + (size_t)mu_ctz_u32(bits);
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

#endif /* MUC_OPCUA_SUBSCRIPTIONS */
