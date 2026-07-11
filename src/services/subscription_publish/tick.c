#include "common.h"

#if MUC_OPCUA_SUBSCRIPTIONS

static bool subscription_id_in_use(const mu_subscriptions_t *subs, opcua_uint32_t subscription_id) {
    for (size_t i = 0; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
        const mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use && sub->subscription_id == subscription_id) {
            return true;
        }
    }
    return false;
}

static opcua_uint32_t allocate_subscription_id(mu_subscriptions_t *subs) {
    for (size_t i = 0; i <= MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
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
    for (size_t i = 0; i < MU_INTERN_MAX_MONITORED_ITEMS; ++i) {
        const mu_monitored_item_t *item = &subs->monitored_items[i];
        if (item->in_use && item->monitored_item_id == monitored_item_id) {
            return true;
        }
    }
    return false;
}

static opcua_uint32_t allocate_monitored_item_id(mu_subscriptions_t *subs) {
    for (size_t i = 0; i <= MU_INTERN_MAX_MONITORED_ITEMS; ++i) {
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

size_t mu_subscriptions_count_for_session(const mu_subscriptions_t *subs, opcua_uint32_t session_id) {
    size_t count = 0u;
    for (size_t i = 0u; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
        const mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use && sub->session_id == session_id) {
            ++count;
        }
    }
    return count;
}

void mu_publish_request_queue_clear(mu_subscriptions_t *subs, opcua_uint32_t session_id) {
    if (subs == NULL) {
        return;
    }
    for (size_t i = 0u; i < MU_INTERN_MAX_PUBLISH_REQUESTS; ++i) {
        if (subs->publish_queue[i].in_use && subs->publish_queue[i].session_id == session_id) {
            (void)memset(&subs->publish_queue[i], 0, sizeof(subs->publish_queue[i]));
        }
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

    for (size_t i = 0; i < MU_INTERN_MAX_PUBLISH_REQUESTS; ++i) {
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
    for (size_t i = 0; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
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

    for (size_t i = 0; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use && sub->session_id == session_id && sub->subscription_id == subscription_id) {
            return sub;
        }
    }

    return NULL;
}

#if MUC_OPCUA_REDUNDANCY
mu_subscription_t *mu_subscription_find_any(mu_subscriptions_t *subs, opcua_uint32_t subscription_id) {
    if (subs == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < MU_INTERN_MAX_SUBSCRIPTIONS; ++i) {
        mu_subscription_t *sub = &subs->subscriptions[i];
        if (sub->in_use && sub->subscription_id == subscription_id) {
            return sub;
        }
    }
    return NULL;
}

opcua_statuscode_t mu_subscription_transfer(mu_subscriptions_t *subs, mu_subscription_t *sub,
                                            opcua_uint32_t new_session_id) {
    if (subs == NULL || sub == NULL || !sub->in_use) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    /* SetSession() (OPC-10000-4 §5.14.1.4): re-home the Subscription to the new owner.
       Future notifications route to the new session (publish routing resolves via
       sub->session_id). Drop the old owner's parked Publish requests once it holds no
       more subscriptions, so queued publishes (keyed by session_id) don't strand. */
    opcua_uint32_t old_session_id = sub->session_id;
    sub->session_id = new_session_id;
    if (old_session_id != new_session_id && mu_subscriptions_count_for_session(subs, old_session_id) == 0u) {
        mu_publish_request_queue_clear(subs, old_session_id);
    }
    return MU_STATUS_GOOD;
}
#endif

opcua_statuscode_t mu_subscription_delete(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                          opcua_uint32_t subscription_id) {
    if (subs == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    for (size_t i = 0; i < MU_INTERN_MAX_MONITORED_ITEMS; ++i) {
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
    for (size_t i = 0; i < MU_INTERN_MAX_MONITORED_ITEMS; ++i) {
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

    for (size_t i = 0; i < MU_INTERN_MAX_MONITORED_ITEMS; ++i) {
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
    for (size_t i = 0; i < MU_INTERN_MAX_MONITORED_ITEMS && active_checked < server->subs.active_monitored_items_count;
         ++i) {
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
