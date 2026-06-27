/* src/services/subscription.c */
#include "subscription.h"
#include <string.h>

#if MICRO_OPCUA_SUBSCRIPTIONS

#include "../address_space/base_nodes.h"
#include "../core/server_internal.h"
#include "micro_opcua/address_space.h"

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

static opcua_statuscode_t read_monitored_item_value(const struct mu_server *server,
                                                    const mu_monitored_item_t *item,
                                                    mu_variant_t *cur)
{
    const mu_node_t *node =
        mu_resolve_node(server->config.address_space,
                        &server->runtime_base.space,
                        &item->node_id);

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

void mu_subscriptions_init(mu_subscriptions_t *subs)
{
    if (subs != NULL) {
        memset(subs, 0, sizeof(*subs));
        subs->next_subscription_id = 1u;
        subs->next_monitored_item_id = 1u;
    }
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

    memset(slot, 0, sizeof(*slot));
    slot->in_use = true;
    slot->subscription_id = subscription_id;
    slot->session_id = session_id;
    slot->publishing_interval_ms = publishing_interval_ms;
    slot->max_keep_alive_count = revised_keep_alive;
    slot->lifetime_count = revised_lifetime;
    slot->max_notifications_per_publish = max_notifications_per_publish;
    slot->priority = priority;
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
        opcua_statuscode_t status = read_monitored_item_value(server, item, &cur);

        if (!item->has_value) {
            item->last_value = cur;
            item->last_status = status;
            item->has_value = true;
            item->pending = true;
        } else if (monitored_item_sample_changed(item, &cur, status)) {
            item->last_value = cur;
            item->last_status = status;
            item->pending = true;
        }

        advance_sample_timer(item, now_ms);
    }
}

#endif /* MICRO_OPCUA_SUBSCRIPTIONS */
