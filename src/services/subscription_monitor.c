/* src/services/subscription_monitor.c */
#include "subscription.h"
#include <string.h>

#if MUC_OPCUA_SUBSCRIPTIONS

#include "../core/server_internal.h"
#include "../core/service_dispatch.h"
#include "muc_opcua/address_space.h"
#include "service_header.h"

static bool variant_scalar_equal(const mu_variant_t *a, const mu_variant_t *b) {
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

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
#define MU_STATUSCODE_INFOTYPE_DATAVALUE_OVERFLOW 0x00000480u
#ifndef MU_STATUS_BAD_TOOMANYOPERATIONS
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS ((opcua_statuscode_t)0x80100000u)
#else
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS MU_STATUS_BAD_TOOMANYOPERATIONS
#endif

bool variant_numeric_to_double(const mu_variant_t *value, opcua_double_t *out) {
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

bool monitored_item_change_reportable(const mu_monitored_item_t *item, const mu_variant_t *cur,
                                      opcua_statuscode_t status) {
    opcua_double_t numeric = 0.0;
    opcua_double_t reported_numeric = 0.0;

    if (!item->has_reported || status != item->last_status) {
        return true;
    }

    if (!variant_numeric_to_double(cur, &numeric) || !variant_numeric_to_double(&item->last_value, &reported_numeric)) {
        return true;
    }

    (void)reported_numeric;
    double diff = (double)numeric - (double)item->last_reported_numeric;
    if (diff < 0.0) {
        diff = -diff;
    }

    if (item->deadband_type == MU_DEADBAND_TYPE_ABSOLUTE) {
        return diff >= (double)item->deadband_value;
    }

    return diff > 0.0;
}

static void monitored_item_record_reported(mu_monitored_item_t *item, const mu_variant_t *cur) {
    opcua_double_t numeric = 0.0;

    if (variant_numeric_to_double(cur, &numeric)) {
        item->last_reported_numeric = numeric;
    }
    item->has_reported = true;
}

opcua_uint32_t monitored_item_effective_queue_size(mu_monitored_item_t *item) {
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

opcua_byte_t monitored_item_queue_next(opcua_byte_t index, opcua_uint32_t queue_size) {
    ++index;
    if (index >= queue_size) {
        index = 0u;
    }
    return index;
}

static opcua_byte_t monitored_item_queue_previous(opcua_byte_t index, opcua_uint32_t queue_size) {
    if (index == 0u) {
        return (opcua_byte_t)(queue_size - 1u);
    }
    return (opcua_byte_t)(index - 1u);
}

static void monitored_item_mark_overflow(mu_monitored_item_t *item, opcua_byte_t index) {
    item->queue[index].status |= MU_STATUSCODE_INFOTYPE_DATAVALUE_OVERFLOW;
    item->queue_overflow = true;
}

void monitored_item_enqueue_report(mu_monitored_item_t *item, const mu_variant_t *cur,
                                   opcua_statuscode_t status) {
    opcua_uint32_t queue_size = monitored_item_effective_queue_size(item);

    if (item->queue_count < queue_size) {
        item->queue[item->queue_tail].value = *cur;
        item->queue[item->queue_tail].status = status;
        item->queue_tail = monitored_item_queue_next(item->queue_tail, queue_size);
        ++item->queue_count;
    } else if (item->discard_oldest) {
        item->queue_head = monitored_item_queue_next(item->queue_head, queue_size);
        item->queue[item->queue_tail].value = *cur;
        item->queue[item->queue_tail].status = status;
        item->queue_tail = monitored_item_queue_next(item->queue_tail, queue_size);
        monitored_item_mark_overflow(item, item->queue_head);
    } else {
        opcua_byte_t newest = monitored_item_queue_previous(item->queue_tail, queue_size);
        item->queue[newest].value = *cur;
        item->queue[newest].status = status;
        monitored_item_mark_overflow(item, newest);
    }

    monitored_item_record_reported(item, cur);
    item->pending = item->queue_count > 0u;
}

void monitored_item_prepare_pending_queue(mu_monitored_item_t *item) {
    if (item->pending && item->queue_count == 0u && item->has_value) {
        monitored_item_enqueue_report(item, &item->last_value, item->last_status);
    } else if (item->has_value && !item->has_reported) {
        monitored_item_record_reported(item, &item->last_value);
    }
}

#endif

opcua_statuscode_t read_monitored_item_value(const mu_monitored_item_t *item, mu_variant_t *cur) {
    const mu_node_t *node = item->resolved_node;

    if (node == NULL || node->value == NULL) {
        return MU_STATUS_BAD_NODEIDUNKNOWN;
    }

    return mu_value_source_read(node->value, &item->node_id, cur);
}

bool monitored_item_sample_changed(const mu_monitored_item_t *item, const mu_variant_t *cur,
                                   opcua_statuscode_t status) {
    switch (item->trigger) {
    case MU_DATACHANGE_TRIGGER_STATUS:
        return status != item->last_status;
    case MU_DATACHANGE_TRIGGER_STATUS_VALUE:
    case MU_DATACHANGE_TRIGGER_STATUS_VALUE_TIMESTAMP:
    default:
        return status != item->last_status || !variant_scalar_equal(cur, &item->last_value);
    }
}

void advance_sample_timer(mu_monitored_item_t *item, opcua_uint64_t now_ms) {
    opcua_uint64_t interval = item->sampling_interval_ms;
    const opcua_uint64_t max = ~(opcua_uint64_t)0;

    if (interval == 0u) {
        interval = 1u;
    }

    if (item->next_sample_ms <= now_ms) {
        opcua_uint64_t elapsed = now_ms - item->next_sample_ms;
        if (elapsed < interval) {
            if (item->next_sample_ms > max - interval) {
                item->next_sample_ms = max;
            } else {
                item->next_sample_ms += interval;
            }
            return;
        }

        opcua_uint64_t steps = 0u;
        while ((elapsed >= interval) && (steps < 10u)) {
            elapsed -= interval;
            steps++;
        }

        opcua_uint64_t advance = steps + 1u;
        while (advance > 0u) {
            if (item->next_sample_ms > max - interval) {
                item->next_sample_ms = max;
                break;
            }
            item->next_sample_ms += interval;
            advance--;
        }
    }
}

#endif /* MUC_OPCUA_SUBSCRIPTIONS */
