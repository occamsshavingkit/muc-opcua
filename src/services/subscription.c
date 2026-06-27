/* src/services/subscription.c */
#include "subscription.h"
#include <string.h>

#if MICRO_OPCUA_SUBSCRIPTIONS

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
    (void)server;
    (void)now_ms;
}

#endif /* MICRO_OPCUA_SUBSCRIPTIONS */
