/* src/services/subscription_aggregate.c */
#include "subscription.h"
#include <string.h>

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD

#include "../core/server_internal.h"
#include "../core/service_dispatch.h"
#include "muc_opcua/address_space.h"
#include "service_header.h"

#define MU_STATUSCODE_INFOTYPE_DATAVALUE_OVERFLOW 0x00000480u
#ifndef MU_STATUS_BAD_TOOMANYOPERATIONS
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS ((opcua_statuscode_t)0x80100000u)
#else
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS MU_STATUS_BAD_TOOMANYOPERATIONS
#endif

void monitored_item_accumulate_aggregate(mu_monitored_item_t *item, const mu_variant_t *cur) {
    opcua_double_t val_double = 0.0;
    if (!variant_numeric_to_double(cur, &val_double)) {
        return;
    }

    if (item->aggregate_state.aggregate_type == MU_ID_AGGREGATETYPE_AVERAGE) {
        item->aggregate_state.accumulator.avg.sum += val_double;
    } else if (item->aggregate_state.aggregate_type == MU_ID_AGGREGATETYPE_MINIMUM) {
        if (item->aggregate_state.sample_count == 0u) {
            item->aggregate_state.accumulator.min.min_val = *cur;
        } else {
            opcua_double_t min_double = 0.0;
            if (variant_numeric_to_double(&item->aggregate_state.accumulator.min.min_val, &min_double)) {
                if (val_double < min_double) {
                    item->aggregate_state.accumulator.min.min_val = *cur;
                }
            } else {
                item->aggregate_state.accumulator.min.min_val = *cur;
            }
        }
    } else if (item->aggregate_state.aggregate_type == MU_ID_AGGREGATETYPE_MAXIMUM) {
        if (item->aggregate_state.sample_count == 0u) {
            item->aggregate_state.accumulator.max.max_val = *cur;
        } else {
            opcua_double_t max_double = 0.0;
            if (variant_numeric_to_double(&item->aggregate_state.accumulator.max.max_val, &max_double)) {
                if (val_double > max_double) {
                    item->aggregate_state.accumulator.max.max_val = *cur;
                }
            } else {
                item->aggregate_state.accumulator.max.max_val = *cur;
            }
        }
    }

    item->aggregate_state.sample_count++;
}

void monitored_item_publish_aggregate(mu_monitored_item_t *item, opcua_uint64_t now_ms) {
    mu_variant_t calc_val;
    (void)memset(&calc_val, 0, sizeof(calc_val));
    opcua_statuscode_t calc_status = MU_STATUS_GOOD;

    if (item->aggregate_state.sample_count == 0u) {
        if (item->has_value) {
            calc_val = item->last_value;
            calc_status = item->last_status;
        } else {
            calc_status = MU_STATUS_BAD_NODATA;
        }
    } else {
        if (item->aggregate_state.aggregate_type == MU_ID_AGGREGATETYPE_AVERAGE) {
            calc_val.type = MU_TYPE_DOUBLE;
            calc_val.value.d = item->aggregate_state.accumulator.avg.sum / item->aggregate_state.sample_count;
            calc_val.is_array = false;
        } else if (item->aggregate_state.aggregate_type == MU_ID_AGGREGATETYPE_MINIMUM) {
            calc_val = item->aggregate_state.accumulator.min.min_val;
        } else if (item->aggregate_state.aggregate_type == MU_ID_AGGREGATETYPE_MAXIMUM) {
            calc_val = item->aggregate_state.accumulator.max.max_val;
        } else {
            calc_status = MU_STATUS_BAD_INTERNALERROR;
        }
    }

    monitored_item_enqueue_report(item, &calc_val, calc_status);

    item->aggregate_state.sample_count = 0u;
    (void)memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
    item->aggregate_state.last_calculation = (opcua_datetime_t)now_ms;
}

static mu_monitored_item_t *find_monitored_item_in_subscription(mu_subscriptions_t *subs,
                                                                opcua_uint32_t subscription_id,
                                                                opcua_uint32_t monitored_item_id) {
    if (subs == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use && item->subscription_id == subscription_id && item->monitored_item_id == monitored_item_id) {
            return item;
        }
    }

    return NULL;
}

bool monitored_item_in_subscription(const mu_monitored_item_t *item, const mu_subscription_t *sub) {
    return item->in_use && item->subscription_id == sub->subscription_id;
}

bool monitored_item_reports_by_trigger(const struct mu_server *server, const mu_subscription_t *sub,
                                       const mu_monitored_item_t *linked_item) {
    if (!monitored_item_in_subscription(linked_item, sub) ||
        linked_item->monitoring_mode != MU_MONITORING_MODE_SAMPLING || linked_item->queue_count == 0u) {
        return false;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *triggering_item = &server->subs.monitored_items[i];
        if (!monitored_item_reportable(triggering_item, sub) || triggering_item->triggered_count == 0u) {
            continue;
        }

        for (opcua_byte_t j = 0u; j < triggering_item->triggered_count; ++j) {
            if (triggering_item->triggered_items[j] == linked_item->monitored_item_id) {
                return true;
            }
        }
    }

    return false;
}

opcua_statuscode_t mu_monitored_item_add_trigger_link(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                                      opcua_uint32_t triggering_item_id,
                                                      opcua_uint32_t linked_item_id) {
    mu_monitored_item_t *triggering_item =
        find_monitored_item_in_subscription(subs, subscription_id, triggering_item_id);
    mu_monitored_item_t *linked_item = find_monitored_item_in_subscription(subs, subscription_id, linked_item_id);

    if (triggering_item == NULL || linked_item == NULL) {
        return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    }

    for (opcua_byte_t i = 0u; i < triggering_item->triggered_count; ++i) {
        if (triggering_item->triggered_items[i] == linked_item_id) {
            return MU_STATUS_GOOD;
        }
    }

    if (triggering_item->triggered_count >= MU_MAX_TRIGGER_LINKS) {
        return MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS;
    }

    triggering_item->triggered_items[triggering_item->triggered_count] = linked_item_id;
    ++triggering_item->triggered_count;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_monitored_item_remove_trigger_link(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                                         opcua_uint32_t triggering_item_id,
                                                         opcua_uint32_t linked_item_id) {
    mu_monitored_item_t *triggering_item =
        find_monitored_item_in_subscription(subs, subscription_id, triggering_item_id);
    mu_monitored_item_t *linked_item = find_monitored_item_in_subscription(subs, subscription_id, linked_item_id);

    if (triggering_item == NULL || linked_item == NULL) {
        return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    }

    for (opcua_byte_t i = 0u; i < triggering_item->triggered_count; ++i) {
        if (triggering_item->triggered_items[i] != linked_item_id) {
            continue;
        }

        while ((opcua_byte_t)(i + 1u) < triggering_item->triggered_count) {
            triggering_item->triggered_items[i] = triggering_item->triggered_items[i + 1u];
            ++i;
        }
        --triggering_item->triggered_count;
        triggering_item->triggered_items[triggering_item->triggered_count] = 0u;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
}

opcua_statuscode_t mu_subscription_get_monitored_items(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                                       opcua_uint32_t subscription_id, opcua_uint32_t *server_handles,
                                                       opcua_uint32_t *client_handles, size_t max_handles,
                                                       size_t *out_count) {
    if (subs == NULL || server_handles == NULL || client_handles == NULL || out_count == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    *out_count = 0u;
    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &subs->monitored_items[i];
        if (!item->in_use || item->subscription_id != subscription_id) {
            continue;
        }
        if (*out_count >= max_handles) {
            return MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS;
        }
        server_handles[*out_count] = item->monitored_item_id;
        client_handles[*out_count] = item->client_handle;
        ++(*out_count);
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_subscription_request_resend_data(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                                       opcua_uint32_t subscription_id) {
    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    sub->resend_data_pending = true;
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_SUBSCRIPTIONS_STANDARD */
