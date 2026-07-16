#include "core/service_dispatch/common.h"

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC

opcua_statuscode_t write_create_monitored_items_prefix(mu_binary_writer_t *w, opcua_uint32_t request_handle,
                                                       opcua_int32_t items_to_create, const mu_server_t *server) {
#ifndef MUC_OPCUA_CU_TIME_SYNC
    (void)server;
#endif
    opcua_statuscode_t s = write_response_prefix(w, MU_ID_CREATEMONITOREDITEMSRESPONSE, request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_CU_TIME_SYNC
                                                 ,
                                                 server
#endif
    );
    if (s != MU_STATUS_GOOD)
        return s;
    return mu_binary_write_int32(w, items_to_create);
}

bool resolve_and_validate_create_item(mu_server_t *server, mu_monitored_item_create_body_t *body, mu_binary_writer_t *w,
                                      const mu_node_t **node_out) {
    const mu_node_t *node = mu_resolve_node(server->config.address_space, &server->user_address_space_index,
                                            &server->runtime_base.space, &body->node_id);
    if (node == NULL) {
        opcua_statuscode_t s = write_monitored_item_create_result(w, MU_STATUS_BAD_NODEIDUNKNOWN, 0u, 0u, 0u);
        (void)s;
        return false;
    }

    if (body->index_range_start == -2) {
        /* CU 5208 / OPC-10000-4 §7.22: a syntactically invalid IndexRange. */
        opcua_statuscode_t s = write_monitored_item_create_result(w, MU_STATUS_BAD_INDEXRANGEINVALID, 0u, 0u, 0u);
        (void)s;
        return false;
    }

#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    {
        opcua_statuscode_t filter_status = validate_create_item_filter(server, body, node);
        if (filter_status != MU_STATUS_GOOD) {
            opcua_statuscode_t s = write_monitored_item_create_result(w, filter_status, 0u, 0u, 0u);
            (void)s;
            return false;
        }
    }
#endif

    if (!(body->attribute_id == MU_MONITORED_VALUE_ATTRIBUTE_ID
#ifdef MUC_OPCUA_CU_EVENTS
          || body->attribute_id == 12u
#endif
          )) {
        opcua_statuscode_t s = write_monitored_item_create_result(w, MU_STATUS_BAD_ATTRIBUTEIDINVALID, 0u, 0u, 0u);
        (void)s;
        return false;
    }

    *node_out = node;
    return true;
}
opcua_statuscode_t allocate_and_write_create_item(mu_server_t *server, opcua_uint32_t subscription_id,
                                                  mu_subscription_t *sub, mu_monitored_item_create_body_t *body,
                                                  const mu_node_t *node, opcua_uint32_t timestamps_to_return,
                                                  opcua_uint64_t now_ms, mu_binary_writer_t *w) {
    mu_monitored_item_t *item = NULL;
    opcua_statuscode_t result = mu_monitored_item_alloc(&server->subs, subscription_id, &item);
    if (result != MU_STATUS_GOOD) {
        return write_monitored_item_create_result(w, result, 0u, 0u, 0u);
    }

    configure_monitored_item(item, body, node, sub, timestamps_to_return, now_ms, server);

#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    const opcua_uint32_t revised_queue_size = item->queue_size;
#else
    const opcua_uint32_t revised_queue_size = 1u;
#endif
    return write_monitored_item_create_result(w, MU_STATUS_GOOD, item->monitored_item_id, item->sampling_interval_ms,
                                              revised_queue_size);
}
opcua_statuscode_t handle_create_monitored_items(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint32_t subscription_id, timestamps_to_return;
    opcua_int32_t items_to_create;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &timestamps_to_return);
    mu_binary_read_int32(r, &items_to_create);
    if (r->status != MU_STATUS_GOOD)
        return r->status;

    if (timestamps_to_return > MU_TIMESTAMPS_TO_RETURN_NEITHER)
        return MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID;
    if (items_to_create > 0 && (size_t)items_to_create > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS)
        return MU_STATUS_BAD_TOOMANYOPERATIONS;

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL)
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    if (items_to_create <= 0)
        return MU_STATUS_BAD_NOTHINGTODO;

    s = write_create_monitored_items_prefix(w, req.request_handle, items_to_create, server);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);

    for (opcua_int32_t i = 0; i < items_to_create; ++i) {
        mu_monitored_item_create_body_t body;
        s = read_monitored_item_create_body(r, &body);
        if (s != MU_STATUS_GOOD)
            return s;

        const mu_node_t *node;
        if (!resolve_and_validate_create_item(server, &body, w, &node))
            continue;

        s = allocate_and_write_create_item(server, subscription_id, sub, &body, node, timestamps_to_return, now_ms, w);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD)
        return s;

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
opcua_statuscode_t write_monitored_item_modify_result(mu_binary_writer_t *w, opcua_statuscode_t result,
                                                      opcua_uint32_t revised_sampling_interval_ms,
                                                      opcua_uint32_t revised_queue_size) {
    opcua_uint64_t sampling_bits = 0u;
    mu_binary_write_statuscode(w, result);
    if (result == MU_STATUS_GOOD) {
        sampling_bits = publishing_interval_ms_to_bits(revised_sampling_interval_ms);
    }
    mu_binary_write_uint64(w, sampling_bits);
    mu_binary_write_uint32(w, revised_queue_size);
    if (w->status != MU_STATUS_GOOD) {
        return w->status;
    }
    return write_null_extension_object(w);
}
opcua_statuscode_t read_modify_item_filter(mu_binary_reader_t *r, opcua_statuscode_t *filter_result,
                                           bool *has_datachange_filter, bool *has_aggregate_filter
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
                                           ,
                                           mu_monitored_item_create_body_t *filter_body
#endif
) {
    mu_nodeid_t filter_type;
    size_t filter_length;
    opcua_statuscode_t s;

    s = mu_binary_read_extension_object_header(r, &filter_type, &filter_length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = ensure_reader_bytes_remaining(r, filter_length);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    if (is_datachange_filter_binary_type(&filter_type)) {
        const size_t filter_body_start = r->position;

        *has_datachange_filter = true;
        filter_body->has_datachange_filter = true;

        s = read_datachange_filter_body(r, filter_length, filter_body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        *filter_result = filter_body->filter_result;
    } else if (is_aggregate_filter_binary_type(&filter_type)) {
        const size_t filter_body_start = r->position;

        *has_aggregate_filter = true;
        s = read_aggregate_filter_body(r, filter_length, filter_body);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (r->position != filter_body_start + filter_length) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        *filter_result = filter_body->filter_result;
    } else if (!is_known_monitoring_filter_binary_type(&filter_type, filter_length)) {
        *filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    } else {
        s = skip_extension_object_body(r, filter_length);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }
#else
    (void)filter_result;
    (void)has_datachange_filter;
    (void)has_aggregate_filter;
    s = skip_extension_object_body(r, filter_length);
    if (s != MU_STATUS_GOOD)
        return s;
#endif
    return MU_STATUS_GOOD;
}
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
static opcua_uint32_t clamp_queue_size(opcua_uint32_t queue_size) {
    if (queue_size == 0u) {
        return 1u;
    }
    if (queue_size > MU_INTERN_MONITORED_QUEUE_DEPTH) {
        return MU_INTERN_MONITORED_QUEUE_DEPTH;
    }
    return queue_size;
}
#endif

#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
static opcua_statuscode_t apply_modify_item_filters(mu_monitored_item_t *item, mu_server_t *server,
                                                    bool has_datachange_filter, bool has_aggregate_filter,
                                                    mu_monitored_item_create_body_t *filter_body) {
    if (has_datachange_filter) {
        item->trigger = (opcua_byte_t)filter_body->trigger;
        item->deadband_type = (opcua_byte_t)filter_body->deadband_type;
        item->deadband_value = filter_body->deadband_value;
    }
    if (has_aggregate_filter) {
        item->has_aggregate = true;
        item->aggregate_state.aggregate_type = filter_body->aggregate_type;
        item->aggregate_state.processing_interval = filter_body->processing_interval;
        item->aggregate_state.sample_count = 0u;
        memset(&item->aggregate_state.accumulator, 0, sizeof(item->aggregate_state.accumulator));
        opcua_uint64_t now_ms = server->config.time_adapter.get_tick_ms(server->config.time_adapter.context);
        item->aggregate_state.last_calculation = (opcua_datetime_t)now_ms;
    }
    return MU_STATUS_GOOD;
}
#endif

opcua_statuscode_t apply_modify_item_fields(mu_server_t *server, opcua_uint32_t subscription_id,
                                            opcua_uint32_t monitored_item_id, opcua_uint32_t client_handle,
                                            opcua_uint64_t sampling_interval_bits, opcua_uint32_t timestamps_to_return,
                                            opcua_statuscode_t filter_result, bool has_datachange_filter,
                                            bool has_aggregate_filter, opcua_uint32_t queue_size,
                                            opcua_boolean_t discard_oldest, opcua_statuscode_t *result_out,
                                            opcua_uint32_t *revised_sampling_interval_ms_out,
                                            opcua_uint32_t *revised_queue_size_out
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
                                            ,
                                            mu_monitored_item_create_body_t *filter_body
#endif
) {
    mu_monitored_item_t *item = find_monitored_item(server, subscription_id, monitored_item_id);
    opcua_statuscode_t result = MU_STATUS_BAD_MONITOREDITEMIDINVALID;
    opcua_uint32_t revised_sampling_interval_ms = 0u;
    opcua_uint32_t revised_queue_size = 1u;
    (void)queue_size;
    (void)discard_oldest;

    if (item == NULL) {
        *result_out = result;
        *revised_sampling_interval_ms_out = revised_sampling_interval_ms;
        *revised_queue_size_out = revised_queue_size;
        return MU_STATUS_GOOD;
    }

    if (filter_result != MU_STATUS_GOOD) {
        *result_out = filter_result;
        *revised_sampling_interval_ms_out = revised_sampling_interval_ms;
        *revised_queue_size_out = revised_queue_size;
        return MU_STATUS_GOOD;
    }

    if ((has_datachange_filter || has_aggregate_filter) && item->attribute_id != MU_MONITORED_VALUE_ATTRIBUTE_ID) {
        *result_out = MU_STATUS_BAD_FILTERNOTALLOWED;
        *revised_sampling_interval_ms_out = revised_sampling_interval_ms;
        *revised_queue_size_out = revised_queue_size;
        return MU_STATUS_GOOD;
    }

    revised_sampling_interval_ms = publishing_interval_bits_to_ms(sampling_interval_bits);
    item->sampling_interval_ms = revised_sampling_interval_ms;
    item->client_handle = client_handle;
    item->timestamps_to_return = (opcua_byte_t)timestamps_to_return;

#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
    item->queue_size = clamp_queue_size(queue_size);
    item->discard_oldest = discard_oldest;
    revised_queue_size = item->queue_size;
    apply_modify_item_filters(item, server, has_datachange_filter, has_aggregate_filter, filter_body);
#else
    (void)has_datachange_filter;
    (void)has_aggregate_filter;
#endif

    *result_out = MU_STATUS_GOOD;
    *revised_sampling_interval_ms_out = revised_sampling_interval_ms;
    *revised_queue_size_out = revised_queue_size;
    return MU_STATUS_GOOD;
}
opcua_statuscode_t handle_modify_monitored_items(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_uint32_t subscription_id;
    opcua_uint32_t timestamps_to_return;
    opcua_int32_t count;
    mu_binary_read_uint32(r, &subscription_id);
    mu_binary_read_uint32(r, &timestamps_to_return);
    mu_binary_read_int32(r, &count);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    (void)timestamps_to_return;

    if (count > 0 && (size_t)count > MU_DISPATCH_MAX_SUBSCRIPTION_OPERATIONS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    mu_subscription_t *sub = mu_subscription_find(&server->subs, server->active_session->session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }
    (void)sub;

    if (count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    s = write_response_prefix(w, MU_ID_MODIFYMONITOREDITEMSRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_CU_TIME_SYNC
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
        opcua_uint32_t monitored_item_id;
        opcua_uint32_t client_handle;
        opcua_uint64_t sampling_interval_bits;
        opcua_statuscode_t filter_result = MU_STATUS_GOOD;
        bool has_datachange_filter = false;
        bool has_aggregate_filter = false;
        opcua_uint32_t queue_size;
        opcua_boolean_t discard_oldest;
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
        mu_monitored_item_create_body_t filter_body;
        filter_body.trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        filter_body.deadband_type = MU_DEADBAND_TYPE_NONE;
        filter_body.deadband_value = 0.0;
        filter_body.filter_result = MU_STATUS_GOOD;
        filter_body.has_datachange_filter = false;
        filter_body.has_aggregate = false;
        filter_body.aggregate_type = 0u;
        filter_body.processing_interval = 0.0;
#endif

        mu_binary_read_uint32(r, &monitored_item_id);
        mu_binary_read_uint32(r, &client_handle);
        mu_binary_read_uint64(r, &sampling_interval_bits);
        if (r->status != MU_STATUS_GOOD) {
            return r->status;
        }
        s = read_modify_item_filter(r, &filter_result, &has_datachange_filter, &has_aggregate_filter
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
                                    ,
                                    &filter_body
#endif
        );
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        mu_binary_read_uint32(r, &queue_size);
        mu_binary_read_boolean(r, &discard_oldest);
        if (r->status != MU_STATUS_GOOD) {
            return r->status;
        }

        opcua_statuscode_t result;
        opcua_uint32_t revised_sampling_interval_ms;
        opcua_uint32_t revised_queue_size;
        apply_modify_item_fields(server, subscription_id, monitored_item_id, client_handle, sampling_interval_bits,
                                 timestamps_to_return, filter_result, has_datachange_filter, has_aggregate_filter,
                                 queue_size, discard_oldest, &result, &revised_sampling_interval_ms, &revised_queue_size
#if MUC_OPCUA_CU_SUBSCRIPTION_STANDARD
                                 ,
                                 &filter_body
#endif
        );

        s = write_monitored_item_modify_result(w, result, revised_sampling_interval_ms, revised_queue_size);
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
opcua_statuscode_t handle_set_monitoring_mode(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                              size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_set_monitoring_mode_context_t context = {0u, 0u};
    mu_binary_read_uint32(r, &context.subscription_id);
    mu_binary_read_uint32(r, &context.monitoring_mode);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_SETMONITORINGMODERESPONSE,
                                              req.request_handle, true, context.subscription_id,
                                              set_monitoring_mode_result, &context);
}
opcua_statuscode_t handle_delete_monitored_items(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_monitored_item_id_context_t context = {0u};
    mu_binary_read_uint32(r, &context.subscription_id);

    return drive_subscription_id_status_array(server, r, w, response_length, MU_ID_DELETEMONITOREDITEMSRESPONSE,
                                              req.request_handle, true, context.subscription_id,
                                              delete_monitored_item_result, &context);
}

#endif /* MUC_OPCUA_CU_SUBSCRIPTION_BASIC */
