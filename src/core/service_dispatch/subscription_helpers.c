#include "common.h"

/* Spec 057: advertised MaxMonitoredItemsPerCall (base_nodes.c) must match the
 * per-call subscription-operation bound enforced here. */
_Static_assert(MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS == MU_MAX_MONITORED_ITEMS_PER_CALL,
               "advertised MaxMonitoredItemsPerCall must match the enforced dispatch bound");

#if MUC_OPCUA_SUBSCRIPTIONS

opcua_statuscode_t read_monitored_item_create_body(mu_binary_reader_t *r, mu_monitored_item_create_body_t *body) {
    opcua_statuscode_t s;
    mu_string_t index_range;
    opcua_uint16_t data_encoding_namespace_index;
    mu_string_t data_encoding_name;
    mu_nodeid_t filter_type;
    size_t filter_length;

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    body->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
    body->deadband_type = MU_DEADBAND_TYPE_NONE;
    body->deadband_value = 0.0;
    body->filter_result = MU_STATUS_GOOD;
    body->has_datachange_filter = false;
    body->has_aggregate = false;
    body->aggregate_type = 0u;
    body->processing_interval = 0.0;
#endif

    s = mu_binary_read_nodeid(r, &body->node_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_read_uint32(r, &body->attribute_id);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    s = mu_binary_read_string(r, &index_range);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_read_uint16(r, &data_encoding_namespace_index);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    s = mu_binary_read_string(r, &data_encoding_name);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    mu_binary_read_uint32(r, &body->monitoring_mode);
    mu_binary_read_uint32(r, &body->client_handle);
    mu_binary_read_uint64(r, &body->sampling_interval_bits);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    s = mu_binary_read_extension_object_header(r, &filter_type, &filter_length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    const size_t filter_body_start = r->position;
    s = ensure_reader_bytes_remaining(r, filter_length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (is_datachange_filter_binary_type(&filter_type)) {
        body->has_datachange_filter = true;
        s = read_datachange_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
    } else if (is_aggregate_filter_binary_type(&filter_type)) {
        s = read_aggregate_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
#ifdef MUC_OPCUA_EVENTS
    } else if (is_event_filter_binary_type(&filter_type)) {
        s = read_event_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
#endif
    } else if (filter_type.identifier_type == MU_NODEID_NUMERIC && filter_type.namespace_index == 0u &&
               filter_type.identifier.numeric == 0u && filter_length == 0u) {
    } else {
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
#else
#ifdef MUC_OPCUA_EVENTS
    if (is_event_filter_binary_type(&filter_type)) {
        s = read_event_filter_body(r, filter_length, body);
        if (s != MU_STATUS_GOOD)
            return s;
    } else {
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD)
            return s;
    }
#else
    s = skip_extension_object_body(r, filter_length);
    if (s != MU_STATUS_GOOD)
        return s;
#endif
#endif
    mu_binary_read_uint32(r, &body->queue_size);
    mu_binary_read_boolean(r, &body->discard_oldest);
    return r->status;
}

void copy_monitored_node_id(mu_monitored_item_t *item, const mu_nodeid_t *node_id) {
    item->node_id = *node_id;
    if (node_id->identifier_type == MU_NODEID_STRING) {
        opcua_int32_t length = node_id->identifier.string.length;
        if (length > (opcua_int32_t)sizeof(item->node_id_string)) {
            length = (opcua_int32_t)sizeof(item->node_id_string);
        }
        if (length > 0 && node_id->identifier.string.data != NULL) {
            memcpy(item->node_id_string, node_id->identifier.string.data, (size_t)length);
            item->node_id.identifier.string.data = item->node_id_string;
        } else {
            item->node_id.identifier.string.data = NULL;
        }
        item->node_id.identifier.string.length = length;
    }
}

opcua_statuscode_t write_null_extension_object(mu_binary_writer_t *w) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    return mu_binary_write_extension_object_header(w, &null_id, 0);
}

opcua_statuscode_t write_monitored_item_create_result(mu_binary_writer_t *w, opcua_statuscode_t result,
                                                      opcua_uint32_t monitored_item_id,
                                                      opcua_uint32_t sampling_interval_ms,
                                                      opcua_uint32_t revised_queue_size) {
    opcua_uint64_t sampling_bits = 0u;
    mu_binary_write_statuscode(w, result);
    mu_binary_write_uint32(w, monitored_item_id);
    if (result == MU_STATUS_GOOD) {
        sampling_bits = publishing_interval_ms_to_bits(sampling_interval_ms);
    }
    mu_binary_write_uint64(w, sampling_bits);
    mu_binary_write_uint32(w, revised_queue_size);
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }
    return write_null_extension_object(w);
}

mu_monitored_item_t *find_monitored_item(mu_server_t *server, opcua_uint32_t subscription_id,
                                         opcua_uint32_t monitored_item_id) {
    size_t active_checked = 0;
    for (size_t i = 0; i < MU_INTERN_MAX_MONITORED_ITEMS && active_checked < server->subs.active_monitored_items_count;
         ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (!item->in_use) {
            continue;
        }
        active_checked++;
        if (item->subscription_id == subscription_id && item->monitored_item_id == monitored_item_id) {
            return item;
        }
    }

    return NULL;
}

opcua_statuscode_t drive_subscription_id_status_array(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                      size_t *response_length, opcua_uint32_t response_type_id,
                                                      opcua_uint32_t request_handle, bool validate_subscription_id,
                                                      opcua_uint32_t subscription_id,
                                                      mu_subscription_id_result_fn item_result, void *context) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (count > 0 && (size_t)count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    if (validate_subscription_id) {
        mu_subscription_t *sub =
            mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
        if (sub == NULL) {
            return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
        }
    }

    if (count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, response_type_id, request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        opcua_uint32_t id;
        s = mu_binary_read_uint32(r, &id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_write_statuscode(w, item_result(server, id, context));
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t set_monitoring_mode_result(mu_server_t *server, opcua_uint32_t monitored_item_id, void *context) {
    mu_set_monitoring_mode_context_t *mode_ctx = (mu_set_monitoring_mode_context_t *)context;
    mu_monitored_item_t *item = find_monitored_item(server, mode_ctx->subscription_id, monitored_item_id);
    if (item == NULL) {
        return MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    }

    const opcua_byte_t previous_mode = item->monitoring_mode;
    item->monitoring_mode = (opcua_byte_t)mode_ctx->monitoring_mode;

    if (previous_mode == MU_MONITORING_MODE_DISABLED &&
        (item->monitoring_mode == MU_MONITORING_MODE_SAMPLING ||
         item->monitoring_mode == MU_MONITORING_MODE_REPORTING) &&
        item->attribute_id != 12u) {
        mu_variant_t cur;
        opcua_statuscode_t read_status = read_monitored_item_value(item, &cur);
        if (read_status == MU_STATUS_GOOD) {
            item->last_value = cur;
            item->last_status = MU_STATUS_GOOD;
            item->has_value = true;
            item->pending = true;
            opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
            item->next_sample_ms = now_ms + item->sampling_interval_ms;
        } else {
            item->last_status = read_status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t delete_monitored_item_result(mu_server_t *server, opcua_uint32_t monitored_item_id, void *context) {
    const mu_monitored_item_id_context_t *item_context = (const mu_monitored_item_id_context_t *)context;
    return mu_monitored_item_delete(&server->subs, item_context->subscription_id, monitored_item_id);
}

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
opcua_statuscode_t validate_create_item_filter(mu_server_t *server, const mu_monitored_item_create_body_t *body,
                                               const mu_node_t *node) {
    if (body->filter_result != MU_STATUS_GOOD)
        return body->filter_result;
    if (body->has_datachange_filter && body->attribute_id != MU_MONITORED_VALUE_ATTRIBUTE_ID)
        return MU_STATUS_BAD_FILTERNOTALLOWED;
    if (body->deadband_type == MU_DEADBAND_TYPE_ABSOLUTE && !monitored_node_has_numeric_static_value(node))
        return MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
#if MUC_OPCUA_DATA_ACCESS
    /* PercentDeadband applies only to items with an EURange Property; without one
       the server must reject with Bad_DeadbandFilterInvalid (OPC-10000-8 §7.3.2). */
    if (body->deadband_type == MU_DEADBAND_TYPE_PERCENT) {
        double span = 0.0;
        if (!mu_resolve_eurange_span(server->config.address_space, &server->user_address_space_index,
                                     &server->runtime_base.space, node, &span)) {
            return MU_STATUS_BAD_DEADBANDFILTERINVALID;
        }
    }
#else
    (void)server;
#endif
    if (body->has_aggregate && !monitored_node_has_numeric_static_value(node))
        return MU_STATUS_BAD_FILTERNOTALLOWED;
    return MU_STATUS_GOOD;
}
#endif

opcua_statuscode_t configure_monitored_item(mu_monitored_item_t *item, const mu_monitored_item_create_body_t *body,
                                            const mu_node_t *node, mu_subscription_t *sub,
                                            opcua_uint32_t timestamps_to_return, opcua_uint64_t now_ms,
                                            mu_server_t *server) {
    (void)server;
    copy_monitored_node_id(item, &body->node_id);
    item->resolved_node = node;
    item->attribute_id = body->attribute_id;
    item->client_handle = body->client_handle;
    item->monitoring_mode = (opcua_byte_t)body->monitoring_mode;
    item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
    item->timestamps_to_return = (opcua_byte_t)timestamps_to_return;
    if ((body->sampling_interval_bits & MU_DOUBLE_SIGN_BIT) != 0u) {
        item->sampling_interval_ms = sub->publishing_interval_ms;
    } else {
        item->sampling_interval_ms = publishing_interval_bits_to_ms(body->sampling_interval_bits);
    }
    item->next_sample_ms = now_ms + item->sampling_interval_ms;
    item->has_value = false;
    item->pending = false;
#ifdef MUC_OPCUA_EVENTS
    item->select_clauses_count = body->select_clauses_count;
    memcpy(item->select_clauses, body->select_clauses, sizeof(item->select_clauses));
#if MUC_OPCUA_EVENT_FILTER_WHERE
    item->where_clause = body->where_clause; /* compact, self-contained (blob-owned) */
#endif
#endif
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    item->trigger = (opcua_byte_t)body->trigger;
    item->deadband_type = (opcua_byte_t)body->deadband_type;
    item->deadband_value = body->deadband_value;
#if MUC_OPCUA_DATA_ACCESS
    /* Percent deadband threshold is a fraction of the EURange span; a percent item
       only reaches here after validate_create_item_filter confirmed the EURange
       Property exists (else it was rejected with Bad_DeadbandFilterInvalid). */
    item->deadband_span = 0.0;
    if (item->deadband_type == MU_DEADBAND_TYPE_PERCENT && node != NULL) {
        double span = 0.0;
        if (mu_resolve_eurange_span(server->config.address_space, &server->user_address_space_index,
                                    &server->runtime_base.space, node, &span)) {
            item->deadband_span = span;
        }
    }
#endif
    item->queue_size = body->queue_size;
    if (item->queue_size == 0u) {
        item->queue_size = 1u;
    }
    if (item->queue_size > MU_INTERN_MONITORED_QUEUE_DEPTH) {
        item->queue_size = MU_INTERN_MONITORED_QUEUE_DEPTH;
    }
    item->queue_head = 0u;
    item->queue_tail = 0u;
    item->queue_count = 0u;
    item->discard_oldest = body->discard_oldest;
    item->queue_overflow = false;
    item->has_aggregate = body->has_aggregate;
    if (item->has_aggregate) {
        item->aggregate_state.aggregate_type = body->aggregate_type;
        item->aggregate_state.processing_interval = body->processing_interval;
        item->aggregate_state.last_calculation = (opcua_datetime_t)now_ms;
        item->aggregate_state.sample_count = 0u;
        memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
    }
#endif
    if (body->attribute_id != 12u && node != NULL && node->value != NULL) {
        item->last_status = mu_value_source_read(node->value, &body->node_id, &item->last_value);
        if (item->last_status == MU_STATUS_GOOD) {
            item->has_value = true;
            item->pending = true;
        }
    }
    (void)body->queue_size;
    (void)body->discard_oldest;
    return MU_STATUS_GOOD;
}
#endif
