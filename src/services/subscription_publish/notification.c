#include "common.h"

#if MUC_OPCUA_SUBSCRIPTIONS

static void datavalue_apply_timestamps(mu_datavalue_t *dv, const mu_monitored_item_t *item, opcua_datetime_t now) {
    opcua_byte_t mode = item->timestamps_to_return;
    if (mode == MU_TIMESTAMPS_TO_RETURN_SOURCE || mode == MU_TIMESTAMPS_TO_RETURN_BOTH) {
        dv->has_source_timestamp = true;
        dv->source_timestamp = now;
    }
    if (mode == MU_TIMESTAMPS_TO_RETURN_SERVER || mode == MU_TIMESTAMPS_TO_RETURN_BOTH) {
        dv->has_server_timestamp = true;
        dv->server_timestamp = now;
    }
}

static opcua_statuscode_t write_dcn_object_header(mu_binary_writer_t *w, opcua_int32_t report_count,
                                                  size_t *length_pos) {
    mu_nodeid_t type_id = {0, MU_NODEID_NUMERIC, {MU_ID_DATACHANGENOTIFICATION_ENCODING_DEFAULTBINARY}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_byte(w, 1u);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *length_pos = w->position;
    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, report_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t write_dcn_item_value(mu_binary_writer_t *w, const mu_monitored_item_t *item,
                                               const mu_variant_t *value, opcua_statuscode_t status,
                                               opcua_datetime_t now) {
    opcua_statuscode_t s = mu_binary_write_uint32(w, item->client_handle);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_datavalue_t dv;
    (void)memset(&dv, 0, sizeof(dv));
    dv.has_value = true;
    dv.value = *value;
    if (status != MU_STATUS_GOOD) {
        dv.has_status = true;
        dv.status = status;
    }
    datavalue_apply_timestamps(&dv, item, now);
    return mu_binary_write_datavalue(w, &dv);
}

static opcua_statuscode_t write_dcn_object_end(mu_binary_writer_t *w, size_t length_pos, size_t body_start) {
    opcua_statuscode_t s = mu_binary_write_int32(w, 0);
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

opcua_statuscode_t write_data_change_notification(mu_binary_writer_t *w, const struct mu_server *server,
                                                  const mu_subscription_t *sub, opcua_int32_t report_count) {
    size_t length_pos = 0;
    opcua_statuscode_t s = write_dcn_object_header(w, report_count, &length_pos);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    size_t body_start = w->position;

    opcua_datetime_t now = publish_time_now(server);

    for (size_t bw = 0; bw < MU_REPORTABLE_BITMAP_WORDS; ++bw) {
        opcua_uint32_t bits = server->subs.reportable_bitmap[bw];
        while (bits != 0u) {
            size_t i = bw * 32u + (size_t)mu_ctz_u32(bits);
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
                s = write_dcn_item_value(w, item, &item->queue[entry_index].value, item->queue[entry_index].status,
                                         now);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }
                --report_count;
                entry_index = monitored_item_queue_next(entry_index, queue_size);
            }
#else
            s = write_dcn_item_value(w, item, &item->last_value, item->last_status, now);
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
            size_t i = bw * 32u + (size_t)mu_ctz_u32(bits);
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
                s = write_dcn_item_value(w, item, &item->queue[entry_index].value, item->queue[entry_index].status,
                                         now);
                if (s != MU_STATUS_GOOD) {
                    return s;
                }
                --report_count;
                entry_index = monitored_item_queue_next(entry_index, queue_size);
            }
        }
    }
#endif

    return write_dcn_object_end(w, length_pos, body_start);
}

opcua_statuscode_t write_status_change_notification(mu_binary_writer_t *w, opcua_statuscode_t status) {
    mu_nodeid_t type_id = {0, MU_NODEID_NUMERIC, {MU_ID_STATUSCHANGENOTIFICATION_ENCODING_DEFAULTBINARY}};
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

    s = mu_binary_write_statuscode(w, status);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_byte(w, 0u);
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
opcua_statuscode_t write_event_notification_list(mu_binary_writer_t *w, struct mu_server *server,
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

#if MUC_OPCUA_EVENT_FILTER_WHERE
                if (item->where_clause_count > 0) {
                    mu_event_fields_t fields;
                    memset(&fields, 0, sizeof(fields));
                    fields.event_id = ev->event_id;
                    fields.event_type = ev->event_type;
                    fields.time = ev->time;
                    fields.message = ev->message;
                    fields.severity = ev->severity;
                    if (!mu_evaluate_event_filter_where(&fields, item->where_operators, item->where_field_indices,
                                                        item->where_values, item->where_clause_count)) {
                        continue;
                    }
                }
#endif

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

#endif /* MUC_OPCUA_SUBSCRIPTIONS */
