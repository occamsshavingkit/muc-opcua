/* src/services/subscription.c */
#include "subscription.h"
#include <string.h>

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
#include <math.h>
#endif

#if MICRO_OPCUA_SUBSCRIPTIONS

#include "../core/server_internal.h"
#include "micro_opcua/address_space.h"
#include "service_header.h"

#define MU_PUBLISH_BODY_BYTES 512u
#define MU_ID_DATACHANGENOTIFICATION_ENCODING_DEFAULTBINARY 811u

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
/* OPC-10000-4 §7.20.1 / OPC-10000-6 StatusCode: InfoType=DataValue + Overflow. */
#define MU_STATUSCODE_INFOTYPE_DATAVALUE_OVERFLOW 0x00000480u
#ifndef MU_STATUS_BAD_TOOMANYOPERATIONS
/* OPC UA Bad_TooManyOperations. */
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS ((opcua_statuscode_t)0x80100000u)
#else
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS MU_STATUS_BAD_TOOMANYOPERATIONS
#endif
#endif

static bool variant_scalar_equal(const mu_variant_t *a, const mu_variant_t *b)
{
    if (a->type != b->type || a->is_array != b->is_array) {
        return false;
    }

    if (a->is_array) {
        return false;
    }

    switch (a->type) {
        case MU_TYPE_BOOLEAN:
            return a->value.b == b->value.b;
        case MU_TYPE_SBYTE:
            return a->value.sb == b->value.sb;
        case MU_TYPE_BYTE:
            return a->value.by == b->value.by;
        case MU_TYPE_INT16:
            return a->value.i16 == b->value.i16;
        case MU_TYPE_UINT16:
            return a->value.ui16 == b->value.ui16;
        case MU_TYPE_INT32:
            return a->value.i32 == b->value.i32;
        case MU_TYPE_UINT32:
            return a->value.ui32 == b->value.ui32;
        case MU_TYPE_INT64:
            return a->value.i64 == b->value.i64;
        case MU_TYPE_UINT64:
            return a->value.ui64 == b->value.ui64;
        case MU_TYPE_FLOAT:
            return memcmp(&a->value.f, &b->value.f, sizeof(a->value.f)) == 0;
        case MU_TYPE_DOUBLE:
            return memcmp(&a->value.d, &b->value.d, sizeof(a->value.d)) == 0;
        case MU_TYPE_DATETIME:
            return a->value.dt == b->value.dt;
        default:
            return false;
    }
}

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static bool variant_numeric_to_double(const mu_variant_t *value, opcua_double_t *out)
{
    if (value->is_array) {
        return false;
    }

    switch (value->type) {
        case MU_TYPE_SBYTE:
            *out = (opcua_double_t)value->value.sb;
            return true;
        case MU_TYPE_BYTE:
            *out = (opcua_double_t)value->value.by;
            return true;
        case MU_TYPE_INT16:
            *out = (opcua_double_t)value->value.i16;
            return true;
        case MU_TYPE_UINT16:
            *out = (opcua_double_t)value->value.ui16;
            return true;
        case MU_TYPE_INT32:
            *out = (opcua_double_t)value->value.i32;
            return true;
        case MU_TYPE_UINT32:
            *out = (opcua_double_t)value->value.ui32;
            return true;
        case MU_TYPE_INT64:
            *out = (opcua_double_t)value->value.i64;
            return true;
        case MU_TYPE_UINT64:
            *out = (opcua_double_t)value->value.ui64;
            return true;
        case MU_TYPE_FLOAT:
            *out = (opcua_double_t)value->value.f;
            return true;
        case MU_TYPE_DOUBLE:
            *out = value->value.d;
            return true;
        default:
            return false;
    }
}

static bool monitored_item_change_reportable(const mu_monitored_item_t *item,
                                             const mu_variant_t *cur,
                                             opcua_statuscode_t status)
{
    opcua_double_t numeric = 0.0;
    opcua_double_t reported_numeric = 0.0;

    if (!item->has_reported || status != item->last_status) {
        return true;
    }

    if (item->deadband_type != MU_DEADBAND_TYPE_ABSOLUTE) {
        return true;
    }

    if (!variant_numeric_to_double(cur, &numeric) ||
        !variant_numeric_to_double(&item->last_value, &reported_numeric)) {
        return true;
    }

    (void)reported_numeric;
    /* OPC-10000-4 §7.22.2: Absolute deadband suppresses same-status
       numeric changes smaller than |new - last reported| >= deadbandValue. */
    return fabs((double)numeric - (double)item->last_reported_numeric) >=
           (double)item->deadband_value;
}

static void monitored_item_record_reported(mu_monitored_item_t *item,
                                           const mu_variant_t *cur)
{
    opcua_double_t numeric = 0.0;

    if (variant_numeric_to_double(cur, &numeric)) {
        item->last_reported_numeric = numeric;
    }
    item->has_reported = true;
}

static opcua_uint32_t monitored_item_effective_queue_size(mu_monitored_item_t *item)
{
    opcua_uint32_t queue_size = item->queue_size;

    if (queue_size == 0u) {
        queue_size = 1u;
    }
    if (queue_size > MU_MONITORED_QUEUE_DEPTH) {
        queue_size = MU_MONITORED_QUEUE_DEPTH;
    }

    item->queue_size = queue_size;
    if (item->queue_head >= queue_size) {
        item->queue_head = 0u;
    }
    if (item->queue_tail >= queue_size) {
        item->queue_tail = 0u;
    }
    if (item->queue_count > queue_size) {
        item->queue_count = (opcua_byte_t)queue_size;
    }
    return queue_size;
}

static opcua_byte_t monitored_item_queue_next(opcua_byte_t index,
                                              opcua_uint32_t queue_size)
{
    ++index;
    if (index >= queue_size) {
        index = 0u;
    }
    return index;
}

static opcua_byte_t monitored_item_queue_previous(opcua_byte_t index,
                                                  opcua_uint32_t queue_size)
{
    if (index == 0u) {
        return (opcua_byte_t)(queue_size - 1u);
    }
    return (opcua_byte_t)(index - 1u);
}

static void monitored_item_mark_overflow(mu_monitored_item_t *item,
                                         opcua_byte_t index)
{
    item->queue[index].status |= MU_STATUSCODE_INFOTYPE_DATAVALUE_OVERFLOW;
    item->queue_overflow = true;
}

static void monitored_item_enqueue_report(mu_monitored_item_t *item,
                                          const mu_variant_t *cur,
                                          opcua_statuscode_t status)
{
    opcua_uint32_t queue_size = monitored_item_effective_queue_size(item);

    /* OPC-10000-4 §5.13.2 queueSize/discardOldest, §7.20.1 overflow. */
    if (item->queue_count < queue_size) {
        item->queue[item->queue_tail].value = *cur;
        item->queue[item->queue_tail].status = status;
        item->queue_tail = monitored_item_queue_next(item->queue_tail,
                                                     queue_size);
        ++item->queue_count;
    } else if (item->discard_oldest) {
        item->queue_head = monitored_item_queue_next(item->queue_head,
                                                     queue_size);
        item->queue[item->queue_tail].value = *cur;
        item->queue[item->queue_tail].status = status;
        item->queue_tail = monitored_item_queue_next(item->queue_tail,
                                                     queue_size);
        monitored_item_mark_overflow(item, item->queue_head);
    } else {
        opcua_byte_t newest =
            monitored_item_queue_previous(item->queue_tail, queue_size);
        item->queue[newest].value = *cur;
        item->queue[newest].status = status;
        monitored_item_mark_overflow(item, newest);
    }

    monitored_item_record_reported(item, cur);
    item->pending = item->queue_count > 0u;
}

static void monitored_item_prepare_pending_queue(mu_monitored_item_t *item)
{
    if (item->pending && item->queue_count == 0u && item->has_value) {
        monitored_item_enqueue_report(item, &item->last_value, item->last_status);
    } else if (item->has_value && !item->has_reported) {
        monitored_item_record_reported(item, &item->last_value);
    }
}
#endif

static opcua_statuscode_t read_monitored_item_value(const mu_monitored_item_t *item,
                                                    mu_variant_t *cur)
{
    const mu_node_t *node = item->resolved_node;

    if (node == NULL || node->value == NULL) {
        return MU_STATUS_BAD_NODEIDUNKNOWN;
    }

    return mu_value_source_read(node->value, &item->node_id, cur);
}

static bool monitored_item_sample_changed(const mu_monitored_item_t *item,
                                          const mu_variant_t *cur,
                                          opcua_statuscode_t status)
{
    switch (item->trigger) {
        case MU_DATACHANGE_TRIGGER_STATUS:
            return status != item->last_status;
        case MU_DATACHANGE_TRIGGER_STATUS_VALUE:
        case MU_DATACHANGE_TRIGGER_STATUS_VALUE_TIMESTAMP:
        default:
            return status != item->last_status ||
                   !variant_scalar_equal(cur, &item->last_value);
    }
}

static void advance_sample_timer(mu_monitored_item_t *item, opcua_uint64_t now_ms)
{
    opcua_uint64_t interval = item->sampling_interval_ms;
    const opcua_uint64_t max = ~(opcua_uint64_t)0;

    if (interval == 0u) {
        interval = 1u;
    }

    if (item->next_sample_ms <= now_ms) {
        opcua_uint64_t elapsed = now_ms - item->next_sample_ms;
        if (elapsed < interval) {
            /* Hot path: avoid Cortex-M0+ software 64-bit divide unless catching up. */
            if (item->next_sample_ms > max - interval) {
                item->next_sample_ms = max;
            } else {
                item->next_sample_ms += interval;
            }
            return;
        }

        opcua_uint64_t steps = (elapsed / interval) + 1u;
        opcua_uint64_t max_steps = (max - item->next_sample_ms) / interval;

        if (steps > max_steps) {
            item->next_sample_ms = max;
        } else {
            item->next_sample_ms += steps * interval;
        }
    }
}

static bool subscription_id_in_use(const mu_subscriptions_t *subs,
                                   opcua_uint32_t subscription_id)
{
    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        const mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use && sub->subscription_id == subscription_id) {
            return true;
        }
    }
    return false;
}

static opcua_uint32_t allocate_subscription_id(mu_subscriptions_t *subs)
{
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

static bool monitored_item_id_in_use(const mu_subscriptions_t *subs,
                                     opcua_uint32_t monitored_item_id)
{
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use && item->monitored_item_id == monitored_item_id) {
            return true;
        }
    }
    return false;
}

static opcua_uint32_t allocate_monitored_item_id(mu_subscriptions_t *subs)
{
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

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static mu_monitored_item_t *find_monitored_item_in_subscription(mu_subscriptions_t *subs,
                                                               opcua_uint32_t subscription_id,
                                                               opcua_uint32_t monitored_item_id)
{
    if (subs == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use &&
            item->subscription_id == subscription_id &&
            item->monitored_item_id == monitored_item_id) {
            return item;
        }
    }

    return NULL;
}
#endif

static void revise_subscription_counts(opcua_uint32_t requested_lifetime_count,
                                       opcua_uint32_t requested_max_keep_alive_count,
                                       opcua_uint32_t *revised_lifetime_count,
                                       opcua_uint32_t *revised_max_keep_alive_count)
{
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

static bool publish_request_dequeue(mu_subscriptions_t *subs,
                                    opcua_uint32_t session_id,
                                    mu_publish_request_t *out_request)
{
    bool found = false;
    size_t best = 0u;
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
    memset(&subs->publish_queue[best], 0, sizeof(subs->publish_queue[best]));
    return true;
}

static opcua_statuscode_t write_publish_response_prefix(mu_binary_writer_t *w,
                                                        opcua_uint32_t request_handle)
{
    mu_nodeid_t type = { 0, MU_NODEID_NUMERIC, { MU_ID_PUBLISHRESPONSE } };
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD) return s;

    mu_response_header_t rh;
    rh.timestamp = 0;
    rh.request_handle = request_handle;
    rh.service_result = MU_STATUS_GOOD;
    return mu_response_header_encode(w, &rh);
}

static void backpatch_int32(opcua_byte_t *buffer, size_t position, opcua_int32_t value)
{
    buffer[position] = (opcua_byte_t)(value & 0xFF);
    buffer[position + 1u] = (opcua_byte_t)((value >> 8) & 0xFF);
    buffer[position + 2u] = (opcua_byte_t)((value >> 16) & 0xFF);
    buffer[position + 3u] = (opcua_byte_t)((value >> 24) & 0xFF);
}

static bool monitored_item_reportable(const mu_monitored_item_t *item,
                                      const mu_subscription_t *sub)
{
    return item->in_use &&
           item->subscription_id == sub->subscription_id &&
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
           (item->pending || item->queue_count > 0u) &&
#else
           item->pending &&
#endif
           item->monitoring_mode == MU_MONITORING_MODE_REPORTING;
}

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static bool monitored_item_in_subscription(const mu_monitored_item_t *item,
                                           const mu_subscription_t *sub)
{
    return item->in_use && item->subscription_id == sub->subscription_id;
}

static bool monitored_item_reports_by_trigger(const struct mu_server *server,
                                              const mu_subscription_t *sub,
                                              const mu_monitored_item_t *linked_item)
{
    if (!monitored_item_in_subscription(linked_item, sub) ||
        linked_item->monitoring_mode != MU_MONITORING_MODE_SAMPLING ||
        linked_item->queue_count == 0u) {
        return false;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *triggering_item =
            &server->subs.monitored_items[i];
        if (!monitored_item_reportable(triggering_item, sub) ||
            triggering_item->triggered_count == 0u) {
            continue;
        }

        for (opcua_byte_t j = 0u; j < triggering_item->triggered_count; ++j) {
            if (triggering_item->triggered_items[j] ==
                linked_item->monitored_item_id) {
                /* OPC-10000-4 §5.13.1.6: a reporting triggering item releases
                   linked Sampling-mode items for this publishing cycle. */
                return true;
            }
        }
    }

    return false;
}

static bool count_queue_entries(opcua_int32_t *count,
                                opcua_uint32_t max_notifications,
                                opcua_byte_t queue_count)
{
    for (opcua_byte_t i = 0u; i < queue_count; ++i) {
        if (max_notifications != 0u &&
            (opcua_uint32_t)*count >= max_notifications) {
            return false;
        }
        ++(*count);
    }

    return true;
}
#endif

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static opcua_int32_t count_reportable_items(const struct mu_server *server,
                                            mu_subscription_t *sub)
{
    opcua_int32_t count = 0;
    opcua_uint32_t max_notifications = sub->max_notifications_per_publish;
    sub->more_notifications = false;

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        if (monitored_item_reportable(&server->subs.monitored_items[i], sub)) {
            const mu_monitored_item_t *item = &server->subs.monitored_items[i];
            if (!count_queue_entries(&count, max_notifications,
                                     item->queue_count)) {
                sub->more_notifications = true;
                return count;
            }
        }
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (monitored_item_reports_by_trigger(server, sub, item) &&
            !count_queue_entries(&count, max_notifications, item->queue_count)) {
            sub->more_notifications = true;
            return count;
        }
    }

    return count;
}
#else
static opcua_int32_t count_reportable_items(const struct mu_server *server,
                                            const mu_subscription_t *sub)
{
    opcua_int32_t count = 0;
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        if (monitored_item_reportable(&server->subs.monitored_items[i], sub)) {
            ++count;
        }
    }
    return count;
}
#endif

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
static void prepare_reportable_queues(struct mu_server *server,
                                      const mu_subscription_t *sub)
{
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (monitored_item_in_subscription(item, sub) &&
            item->monitoring_mode != MU_MONITORING_MODE_DISABLED) {
            monitored_item_prepare_pending_queue(item);
        }
    }
}

static void monitored_item_drain_reported_entries(mu_monitored_item_t *item,
                                                  opcua_int32_t *remaining)
{
    opcua_uint32_t queue_size = monitored_item_effective_queue_size(item);

    while (item->queue_count > 0u && *remaining > 0) {
        item->queue_head = monitored_item_queue_next(item->queue_head,
                                                     queue_size);
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

static void clear_reported_items(struct mu_server *server,
                                 const mu_subscription_t *sub,
                                 opcua_int32_t report_count)
{
    opcua_int32_t remaining = report_count;
    bool triggered_items[MU_MAX_MONITORED_ITEMS];

    memset(triggered_items, 0, sizeof(triggered_items));
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        triggered_items[i] =
            monitored_item_reports_by_trigger(server, sub,
                                              &server->subs.monitored_items[i]);
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS && remaining > 0; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (!monitored_item_reportable(item, sub)) {
            continue;
        }

        monitored_item_drain_reported_entries(item, &remaining);
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS && remaining > 0; ++i) {
        if (triggered_items[i]) {
            monitored_item_drain_reported_entries(&server->subs.monitored_items[i],
                                                  &remaining);
        }
    }
}
#else
static void clear_reported_items(struct mu_server *server, const mu_subscription_t *sub)
{
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (monitored_item_reportable(item, sub)) {
            item->pending = false;
        }
    }
}
#endif

static opcua_datetime_t publish_time_now(const struct mu_server *server)
{
    if (server->config.time_adapter.get_time == NULL) {
        return 0;
    }
    return server->config.time_adapter.get_time(server->config.time_adapter.context);
}

static opcua_statuscode_t write_data_change_notification(mu_binary_writer_t *w,
                                                         const struct mu_server *server,
                                                         const mu_subscription_t *sub,
                                                         opcua_int32_t report_count)
{
    mu_nodeid_t type_id = {
        0,
        MU_NODEID_NUMERIC,
        { MU_ID_DATACHANGENOTIFICATION_ENCODING_DEFAULTBINARY }
    };
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type_id);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_byte(w, 1u);
    if (s != MU_STATUS_GOOD) return s;

    size_t length_pos = w->position;
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) return s;
    size_t body_start = w->position;

    s = mu_binary_write_int32(w, report_count);
    if (s != MU_STATUS_GOOD) return s;

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (!monitored_item_reportable(item, sub)) {
            continue;
        }

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
        opcua_uint32_t queue_size = item->queue_size;
        opcua_byte_t entry_index = item->queue_head;
        if (queue_size == 0u) {
            queue_size = 1u;
        } else if (queue_size > MU_MONITORED_QUEUE_DEPTH) {
            queue_size = MU_MONITORED_QUEUE_DEPTH;
        }

        for (opcua_byte_t queued = 0u;
             queued < item->queue_count && report_count > 0;
             ++queued) {
            const mu_variant_t *value = &item->queue[entry_index].value;
            opcua_statuscode_t status = item->queue[entry_index].status;

            s = mu_binary_write_uint32(w, item->client_handle);
            if (s != MU_STATUS_GOOD) return s;

            mu_datavalue_t dv;
            memset(&dv, 0, sizeof(dv));
            dv.has_value = true;
            dv.value = *value;
            if (status != MU_STATUS_GOOD) {
                dv.has_status = true;
                dv.status = status;
            }
            s = mu_binary_write_datavalue(w, &dv);
            if (s != MU_STATUS_GOOD) return s;

            --report_count;
            entry_index = monitored_item_queue_next(entry_index, queue_size);
        }
#else
        s = mu_binary_write_uint32(w, item->client_handle);
        if (s != MU_STATUS_GOOD) return s;

        mu_datavalue_t dv;
        memset(&dv, 0, sizeof(dv));
        dv.has_value = true;
        dv.value = item->last_value;
        if (item->last_status != MU_STATUS_GOOD) {
            dv.has_status = true;
            dv.status = item->last_status;
        }
        s = mu_binary_write_datavalue(w, &dv);
        if (s != MU_STATUS_GOOD) return s;
#endif
    }

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS && report_count > 0; ++i) {
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

        for (opcua_byte_t queued = 0u;
             queued < item->queue_count && report_count > 0;
             ++queued) {
            const mu_variant_t *value = &item->queue[entry_index].value;
            opcua_statuscode_t status = item->queue[entry_index].status;

            s = mu_binary_write_uint32(w, item->client_handle);
            if (s != MU_STATUS_GOOD) return s;

            mu_datavalue_t dv;
            memset(&dv, 0, sizeof(dv));
            dv.has_value = true;
            dv.value = *value;
            if (status != MU_STATUS_GOOD) {
                dv.has_status = true;
                dv.status = status;
            }
            s = mu_binary_write_datavalue(w, &dv);
            if (s != MU_STATUS_GOOD) return s;

            --report_count;
            entry_index = monitored_item_queue_next(entry_index, queue_size);
        }
    }
#endif

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) return s;

    size_t body_length = w->position - body_start;
    if (body_length > (size_t)0x7FFFFFFF) {
        return MU_STATUS_BAD_RESPONSETOOLARGE;
    }
    backpatch_int32(w->buffer, length_pos, (opcua_int32_t)body_length);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t build_publish_response(struct mu_server *server,
                                                 const mu_subscription_t *sub,
                                                 const mu_publish_request_t *request,
                                                 opcua_uint32_t sequence_number,
                                                 bool include_data,
                                                 opcua_int32_t report_count,
                                                 opcua_datetime_t publish_time,
                                                 opcua_byte_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *out_length,
                                                 size_t *out_message_start,
                                                 size_t *out_message_end)
{
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, buffer_size);
    if (out_message_start != NULL) {
        *out_message_start = 0u;
    }
    if (out_message_end != NULL) {
        *out_message_end = 0u;
    }

    opcua_statuscode_t s =
        write_publish_response_prefix(&w, request->request_handle);
    if (s != MU_STATUS_GOOD) return s;

    s = mu_binary_write_uint32(&w, sub->subscription_id);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(&w, 0);
    if (s != MU_STATUS_GOOD) return s;
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    s = mu_binary_write_boolean(&w, include_data && sub->more_notifications);
#else
    s = mu_binary_write_boolean(&w, false);
#endif
    if (s != MU_STATUS_GOOD) return s;

    size_t message_start = w.position;
    s = mu_binary_write_uint32(&w, sequence_number);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int64(&w, publish_time);
    if (s != MU_STATUS_GOOD) return s;
    s = mu_binary_write_int32(&w, include_data ? 1 : 0);
    if (s != MU_STATUS_GOOD) return s;

    if (include_data) {
        s = write_data_change_notification(&w, server, sub, report_count);
        if (s != MU_STATUS_GOOD) return s;
    }
    size_t message_end = w.position;

    opcua_uint32_t ack_count = request->ack_count;
    if (ack_count > MU_MAX_PUBLISH_ACKS) {
        ack_count = MU_MAX_PUBLISH_ACKS;
    }
    s = mu_binary_write_int32(&w, (opcua_int32_t)ack_count);
    if (s != MU_STATUS_GOOD) return s;
    for (opcua_uint32_t i = 0; i < ack_count; ++i) {
        s = mu_binary_write_statuscode(&w, request->ack_results[i]);
        if (s != MU_STATUS_GOOD) return s;
    }
    s = mu_binary_write_int32(&w, 0);
    if (s != MU_STATUS_GOOD) return s;

    *out_length = w.position;
    if (out_message_start != NULL) {
        *out_message_start = message_start;
    }
    if (out_message_end != NULL) {
        *out_message_end = message_end;
    }
    return MU_STATUS_GOOD;
}

static opcua_uint32_t current_sequence_number(mu_subscription_t *sub)
{
    if (sub->sequence_number == 0u) {
        sub->sequence_number = 1u;
    }
    return sub->sequence_number;
}

static void advance_sequence_number(mu_subscription_t *sub)
{
    ++sub->sequence_number;
    if (sub->sequence_number == 0u) {
        sub->sequence_number = 1u;
    }
}

static void store_retransmit(mu_subscription_t *sub,
                             opcua_uint32_t sequence_number,
                             opcua_datetime_t publish_time,
                             const opcua_byte_t *body,
                             size_t body_length)
{
    sub->retransmit.valid = false;
    sub->retransmit.message_len = 0u;

    if (body_length > MU_RETRANSMIT_BYTES) {
        return;
    }

    sub->retransmit.sequence_number = sequence_number;
    sub->retransmit.publish_time = publish_time;
    memcpy(sub->retransmit.message, body, body_length);
    sub->retransmit.message_len = body_length;
    sub->retransmit.valid = true;
}

static void advance_publish_timer(mu_subscription_t *sub, opcua_uint64_t now_ms)
{
    opcua_uint64_t interval = sub->publishing_interval_ms;
    const opcua_uint64_t max = ~(opcua_uint64_t)0;

    if (interval == 0u) {
        interval = 1u;
    }

    do {
        if (sub->next_publish_ms > max - interval) {
            sub->next_publish_ms = max;
            break;
        }
        sub->next_publish_ms += interval;
    } while (sub->next_publish_ms <= now_ms);
}

static void publish_due(struct mu_server *server, opcua_uint64_t now_ms)
{
    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &server->subs.subscriptions[i];
        if (!sub->in_use || sub->next_publish_ms > now_ms) {
            continue;
        }

        opcua_int32_t report_count = 0;
        bool has_data = false;
        if (sub->publishing_enabled) {
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
            prepare_reportable_queues(server, sub);
#endif
            report_count = count_reportable_items(server, sub);
            has_data = report_count > 0;
        }

        if (has_data) {
            mu_publish_request_t request;
            if (publish_request_dequeue(&server->subs, sub->session_id, &request)) {
                opcua_byte_t body[MU_PUBLISH_BODY_BYTES];
                size_t body_length = 0u;
                size_t message_start = 0u;
                size_t message_end = 0u;
                opcua_uint32_t sequence_number = current_sequence_number(sub);
                opcua_datetime_t publish_time = publish_time_now(server);
                opcua_statuscode_t s =
                    build_publish_response(server, sub, &request, sequence_number,
                                           true, report_count, publish_time,
                                           body, sizeof(body), &body_length,
                                           &message_start, &message_end);
                if (s == MU_STATUS_GOOD) {
                    s = mu_server_emit_message(server, request.request_id, body, body_length);
                }
                if (s == MU_STATUS_GOOD) {
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
                    clear_reported_items(server, sub, report_count);
#else
                    clear_reported_items(server, sub);
#endif
                    sub->keep_alive_counter = 0u;
                    if (message_end >= message_start && message_end <= body_length) {
                        store_retransmit(sub, sequence_number, publish_time,
                                         body + message_start,
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
                    size_t body_length = 0u;
                    opcua_uint32_t sequence_number = current_sequence_number(sub);
                    opcua_datetime_t publish_time = publish_time_now(server);
                    opcua_statuscode_t s =
                        build_publish_response(server, sub, &request, sequence_number,
                                               false, 0, publish_time,
                                               body, sizeof(body), &body_length,
                                               NULL, NULL);
                    if (s == MU_STATUS_GOOD) {
                        s = mu_server_emit_message(server, request.request_id, body, body_length);
                    }
                    if (s == MU_STATUS_GOOD) {
                        sub->keep_alive_counter = 0u;
                    }
                }
            }
        }

        advance_publish_timer(sub, now_ms);
    }
}

void mu_subscriptions_init(mu_subscriptions_t *subs)
{
    if (subs != NULL) {
        memset(subs, 0, sizeof(*subs));
        subs->next_subscription_id = 1u;
        subs->next_monitored_item_id = 1u;
    }
}

opcua_statuscode_t mu_publish_request_enqueue(mu_subscriptions_t *subs,
                                              opcua_uint32_t session_id,
                                              opcua_uint32_t request_id,
                                              opcua_uint32_t request_handle,
                                              opcua_uint64_t now_ms,
                                              mu_publish_request_t **out_req)
{
    if (out_req != NULL) {
        *out_req = NULL;
    }
    if (subs == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    for (size_t i = 0; i < MU_MAX_PUBLISH_REQUESTS; ++i) {
        mu_publish_request_t *slot = &subs->publish_queue[i];
        if (!slot->in_use) {
            memset(slot, 0, sizeof(*slot));
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

opcua_statuscode_t mu_subscription_create(mu_subscriptions_t *subs,
                                          opcua_uint32_t session_id,
                                          opcua_uint32_t publishing_interval_ms,
                                          opcua_uint32_t requested_lifetime_count,
                                          opcua_uint32_t requested_max_keep_alive_count,
                                          opcua_uint32_t max_notifications_per_publish,
                                          opcua_byte_t priority,
                                          bool publishing_enabled,
                                          opcua_uint64_t now_ms,
                                          mu_subscription_t **out_sub)
{
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
    mu_subscription_apply_parameters(slot,
                                     publishing_interval_ms,
                                     requested_lifetime_count,
                                     requested_max_keep_alive_count,
                                     max_notifications_per_publish,
                                     priority);
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

void mu_subscription_apply_parameters(mu_subscription_t *sub,
                                      opcua_uint32_t publishing_interval_ms,
                                      opcua_uint32_t requested_lifetime_count,
                                      opcua_uint32_t requested_max_keep_alive_count,
                                      opcua_uint32_t max_notifications_per_publish,
                                      opcua_byte_t priority)
{
    if (sub == NULL) {
        return;
    }

    opcua_uint32_t revised_lifetime = 0u;
    opcua_uint32_t revised_keep_alive = 0u;
    revise_subscription_counts(requested_lifetime_count,
                               requested_max_keep_alive_count,
                               &revised_lifetime,
                               &revised_keep_alive);

    sub->publishing_interval_ms = publishing_interval_ms;
    sub->max_keep_alive_count = revised_keep_alive;
    sub->lifetime_count = revised_lifetime;
    sub->max_notifications_per_publish = max_notifications_per_publish;
    sub->priority = priority;
}

mu_subscription_t *mu_subscription_find(mu_subscriptions_t *subs,
                                        opcua_uint32_t session_id,
                                        opcua_uint32_t subscription_id)
{
    if (subs == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use &&
            sub->session_id == session_id &&
            sub->subscription_id == subscription_id) {
            return sub;
        }
    }

    return NULL;
}

opcua_statuscode_t mu_subscription_acknowledge(mu_subscriptions_t *subs,
                                               opcua_uint32_t session_id,
                                               opcua_uint32_t subscription_id,
                                               opcua_uint32_t sequence_number)
{
    mu_subscription_t *sub =
        mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    if (sub->retransmit.valid &&
        sub->retransmit.sequence_number == sequence_number) {
        sub->retransmit.valid = false;
        sub->retransmit.message_len = 0u;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_SEQUENCENUMBERUNKNOWN;
}

opcua_statuscode_t mu_subscription_republish(mu_subscriptions_t *subs,
                                             opcua_uint32_t session_id,
                                             opcua_uint32_t subscription_id,
                                             opcua_uint32_t sequence_number,
                                             const opcua_byte_t **out_msg,
                                             size_t *out_len)
{
    if (out_msg == NULL || out_len == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    *out_msg = NULL;
    *out_len = 0u;

    mu_subscription_t *sub =
        mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    if (sub->retransmit.valid &&
        sub->retransmit.sequence_number == sequence_number) {
        *out_msg = sub->retransmit.message;
        *out_len = sub->retransmit.message_len;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_MESSAGENOTAVAILABLE;
}

opcua_statuscode_t mu_subscription_delete(mu_subscriptions_t *subs,
                                          opcua_uint32_t session_id,
                                          opcua_uint32_t subscription_id)
{
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
        }
    }

    memset(sub, 0, sizeof(*sub));
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_monitored_item_alloc(mu_subscriptions_t *subs,
                                           opcua_uint32_t subscription_id,
                                           mu_monitored_item_t **out_item)
{
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

    *out_item = slot;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_monitored_item_delete(mu_subscriptions_t *subs,
                                            opcua_uint32_t subscription_id,
                                            opcua_uint32_t monitored_item_id)
{
    if (subs == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use &&
            item->subscription_id == subscription_id &&
            item->monitored_item_id == monitored_item_id) {
            memset(item, 0, sizeof(*item));
            return MU_STATUS_GOOD;
        }
    }

    return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
}

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
opcua_statuscode_t mu_monitored_item_add_trigger_link(mu_subscriptions_t *subs,
                                                      opcua_uint32_t subscription_id,
                                                      opcua_uint32_t triggering_item_id,
                                                      opcua_uint32_t linked_item_id)
{
    mu_monitored_item_t *triggering_item =
        find_monitored_item_in_subscription(subs, subscription_id, triggering_item_id);
    mu_monitored_item_t *linked_item =
        find_monitored_item_in_subscription(subs, subscription_id, linked_item_id);

    if (triggering_item == NULL || linked_item == NULL) {
        return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    }

    for (opcua_byte_t i = 0u; i < triggering_item->triggered_count; ++i) {
        if (triggering_item->triggered_items[i] == linked_item_id) {
            return MU_STATUS_GOOD;
        }
    }

    /* OPC-10000-4 §5.13.5 SetTriggering link list. */
    if (triggering_item->triggered_count >= MU_MAX_TRIGGER_LINKS) {
        return MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS;
    }

    triggering_item->triggered_items[triggering_item->triggered_count] =
        linked_item_id;
    ++triggering_item->triggered_count;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_monitored_item_remove_trigger_link(mu_subscriptions_t *subs,
                                                         opcua_uint32_t subscription_id,
                                                         opcua_uint32_t triggering_item_id,
                                                         opcua_uint32_t linked_item_id)
{
    mu_monitored_item_t *triggering_item =
        find_monitored_item_in_subscription(subs, subscription_id, triggering_item_id);
    mu_monitored_item_t *linked_item =
        find_monitored_item_in_subscription(subs, subscription_id, linked_item_id);

    if (triggering_item == NULL || linked_item == NULL) {
        return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    }

    for (opcua_byte_t i = 0u; i < triggering_item->triggered_count; ++i) {
        if (triggering_item->triggered_items[i] != linked_item_id) {
            continue;
        }

        while ((opcua_byte_t)(i + 1u) < triggering_item->triggered_count) {
            triggering_item->triggered_items[i] =
                triggering_item->triggered_items[i + 1u];
            ++i;
        }
        --triggering_item->triggered_count;
        triggering_item->triggered_items[triggering_item->triggered_count] = 0u;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
}
#endif

void mu_subscriptions_tick(struct mu_server *server, opcua_uint64_t now_ms)
{
    if (server == NULL) {
        return;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (!item->in_use ||
            item->monitoring_mode == MU_MONITORING_MODE_DISABLED ||
            now_ms < item->next_sample_ms) {
            continue;
        }

        mu_variant_t cur;
        memset(&cur, 0, sizeof(cur));
        opcua_statuscode_t status = read_monitored_item_value(item, &cur);

#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
        if (item->has_value &&
            (!item->has_reported || (item->pending && item->queue_count == 0u))) {
            monitored_item_prepare_pending_queue(item);
        }
#endif

        if (!item->has_value) {
            item->last_value = cur;
            item->last_status = status;
            item->has_value = true;
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
            monitored_item_enqueue_report(item, &cur, status);
#else
            item->pending = true;
#endif
        } else if (monitored_item_sample_changed(item, &cur, status)) {
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
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

        advance_sample_timer(item, now_ms);
    }

    publish_due(server, now_ms);
}

#endif /* MICRO_OPCUA_SUBSCRIPTIONS */
