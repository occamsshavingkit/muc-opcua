#include "core/service_dispatch/common.h"
#include "muc_opcua/opcua_ids.h"

#ifdef MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
#define MU_MAX_HISTORY_NODES_PER_READ 10
#define MU_MAX_HISTORY_DATA_POINTS 10

#include <math.h>

static bool aggregate_status_is_bad_cu(opcua_statuscode_t status) {
    return (status & 0xC0000000u) == 0x80000000u;
}
static bool variant_numeric_to_double_hist(const mu_variant_t *value, opcua_double_t *out);
static opcua_statuscode_t compute_history_aggregate(const mu_historical_data_point_t *points, size_t num_points,
                                                     opcua_uint32_t aggregate_type, mu_variant_t *result,
                                                     opcua_statuscode_t *result_status);

opcua_statuscode_t handle_history_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t s = mu_request_header_decode(r, &req_header);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_history_read_value_id_t nodes_to_read[MU_MAX_HISTORY_NODES_PER_READ];
    mu_history_read_request_t req;
    s = mu_history_read_request_decode(r, &req, nodes_to_read, MU_MAX_HISTORY_NODES_PER_READ);
    if (s != MU_STATUS_GOOD) {
        s = write_response_prefix(w, MU_ID_HISTORYREADRESPONSE, req_header.request_handle, s
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                  ,
                                  server
#endif
        );
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return s;
    }

    mu_history_read_result_t results[MU_MAX_HISTORY_NODES_PER_READ];
    mu_datavalue_t dvals[MU_MAX_HISTORY_NODES_PER_READ][MU_MAX_HISTORY_DATA_POINTS];
    opcua_byte_t continuation_points[MU_MAX_HISTORY_NODES_PER_READ][MU_MAX_HISTORY_READ_CONTINUATION_POINT_LENGTH];
    memset(dvals, 0, sizeof(dvals));
    mu_history_read_response_t resp;
    resp.num_results = req.num_nodes_to_read;
    resp.results = results;

    for (size_t i = 0; i < req.num_nodes_to_read; i++) {
        mu_history_read_value_id_t *node = &req.nodes_to_read[i];
        mu_history_read_result_t *res = &results[i];

        res->continuation_point.length = -1;
        res->continuation_point.data = NULL;
        res->history_data.num_data_values = 0;
        res->history_data.data_values = NULL;

        size_t cp_out_length = sizeof(continuation_points[i]);
        mu_historical_data_point_t data_points[MU_MAX_HISTORY_DATA_POINTS];
        memset(data_points, 0, sizeof(data_points));
        size_t actual_data_points = 0;

        if (req.details_type == MU_HISTORY_READ_TYPE_RAW_MODIFIED) {
            if (!server->config.history_adapter.read_raw_modified) {
                res->status_code = MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
            } else {
                res->status_code = server->config.history_adapter.read_raw_modified(
                    server->config.history_adapter.context, &node->node_id,
                    req.details.raw_modified.is_read_modified, req.details.raw_modified.start_time,
                    req.details.raw_modified.end_time, req.details.raw_modified.num_values_per_node,
                    req.details.raw_modified.return_bounds, node->continuation_point.data,
                    node->continuation_point.length > 0 ? (size_t)node->continuation_point.length : 0,
                    continuation_points[i], &cp_out_length, data_points, MU_MAX_HISTORY_DATA_POINTS,
                    &actual_data_points);
            }

            if (res->status_code == MU_STATUS_GOOD) {
                if (cp_out_length > 0 && cp_out_length <= sizeof(continuation_points[i])) {
                    res->continuation_point.length = (opcua_int32_t)cp_out_length;
                    res->continuation_point.data = continuation_points[i];
                }
                for (size_t j = 0; j < actual_data_points; j++) {
                    dvals[i][j].has_value = true;
                    dvals[i][j].value = data_points[j].value;
                    dvals[i][j].has_source_timestamp = true;
                    dvals[i][j].source_timestamp = data_points[j].source_timestamp;
                    dvals[i][j].has_server_timestamp = true;
                    dvals[i][j].server_timestamp = data_points[j].server_timestamp;
                    dvals[i][j].has_status = true;
                    dvals[i][j].status = data_points[j].status;
                }
                res->history_data.num_data_values = actual_data_points;
                res->history_data.data_values = dvals[i];
            }
        } else {
            /* ReadProcessedDetails: read raw data, compute aggregate */
            if (!server->config.history_adapter.read_raw_modified) {
                res->status_code = MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
            } else {
                opcua_boolean_t is_raw = false;
                res->status_code = server->config.history_adapter.read_raw_modified(
                    server->config.history_adapter.context, &node->node_id, is_raw,
                    req.details.processed.start_time, req.details.processed.end_time, 0,
                    false, NULL, 0, NULL, NULL, data_points, MU_MAX_HISTORY_DATA_POINTS,
                    &actual_data_points);
            }

            if (res->status_code == MU_STATUS_GOOD) {
                mu_variant_t agg_result;
                opcua_statuscode_t agg_status;
                opcua_uint32_t agg_type = (req.details.processed.num_aggregate_types > 0)
                                              ? req.details.processed.aggregate_types[0]
                                              : MU_ID_AGGREGATETYPE_AVERAGE;

                opcua_statuscode_t comp_status =
                    compute_history_aggregate(data_points, actual_data_points, agg_type, &agg_result, &agg_status);
                if (comp_status == MU_STATUS_GOOD) {
                    dvals[i][0].has_value = true;
                    dvals[i][0].value = agg_result;
                    dvals[i][0].has_status = true;
                    dvals[i][0].status = agg_status;
                    res->history_data.num_data_values = 1;
                    res->history_data.data_values = dvals[i];
                } else {
                    res->status_code = comp_status;
                }
            }
        }
    }

    s = write_response_prefix(w, MU_ID_HISTORYREADRESPONSE, req_header.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_history_read_response_encode(w, &resp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#define MU_MAX_HISTORY_UPDATE_ITEMS 5
opcua_statuscode_t handle_history_update(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t s = mu_request_header_decode(r, &req_header);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_history_update_item_t items[MU_MAX_HISTORY_UPDATE_ITEMS];
    mu_history_update_request_t req;
    s = mu_history_update_request_decode(r, &req, items, MU_MAX_HISTORY_UPDATE_ITEMS);
    if (s != MU_STATUS_GOOD) {
        s = write_response_prefix(w, MU_ID_HISTORYUPDATERESPONSE, req_header.request_handle, s
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                  ,
                                  server
#endif
        );
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return s;
    }

    mu_history_update_result_t results[MU_MAX_HISTORY_UPDATE_ITEMS];
    mu_history_update_response_t resp;
    resp.num_results = req.num_items;
    resp.results = results;

    for (size_t i = 0; i < req.num_items; ++i) {
        mu_history_update_item_t *item = &req.items[i];
        mu_history_update_result_t *res = &results[i];
        res->status_code = MU_STATUS_GOOD;
        res->num_operation_results = 0;

        if (item->type == MU_HISTORY_UPDATE_TYPE_DATA) {
            if (!server->config.history_adapter.update_data) {
                res->status_code = MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
                continue;
            }

            res->status_code = server->config.history_adapter.update_data(
                server->config.history_adapter.context, &item->body.data.node_id,
                item->body.data.perform_insert_replace, item->body.data.values, item->body.data.num_values,
                res->operation_results);
            if (res->status_code == MU_STATUS_GOOD) {
                res->num_operation_results = item->body.data.num_values;
            }
        } else if (item->type == MU_HISTORY_UPDATE_TYPE_DELETE) {
            if (!server->config.history_adapter.delete_raw_modified) {
                res->status_code = MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
                continue;
            }

            res->status_code = server->config.history_adapter.delete_raw_modified(
                server->config.history_adapter.context, &item->body.delete_raw.node_id,
                item->body.delete_raw.is_delete_modified, item->body.delete_raw.start_time,
                item->body.delete_raw.end_time);
        } else {
            res->status_code = MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
        }
    }

    s = write_response_prefix(w, MU_ID_HISTORYUPDATERESPONSE, req_header.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_history_update_response_encode(w, &resp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

static bool variant_numeric_to_double_hist(const mu_variant_t *value, opcua_double_t *out) {
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

static opcua_statuscode_t compute_history_aggregate(const mu_historical_data_point_t *points, size_t num_points,
                                                     opcua_uint32_t aggregate_type, mu_variant_t *result,
                                                     opcua_statuscode_t *result_status) {
    if (num_points == 0) {
        *result_status = MU_STATUS_BAD_NODATA;
        memset(result, 0, sizeof(*result));
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_AVERAGE) {
        opcua_double_t sum = 0.0;
        size_t count = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    sum += val;
                    count++;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (count > 0) ? sum / (opcua_double_t)count : 0.0;
        result->is_array = false;
        *result_status = (count > 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_MINIMUM) {
        bool found = false;
        opcua_double_t min_val = 0.0;
        mu_variant_t min_var;
        memset(&min_var, 0, sizeof(min_var));
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!found || val < min_val) {
                        min_val = val;
                        min_var = points[i].value;
                        found = true;
                    }
                }
            }
        }
        if (found) {
            *result = min_var;
            *result_status = MU_STATUS_GOOD;
        } else {
            *result_status = MU_STATUS_BAD_NODATA;
            memset(result, 0, sizeof(*result));
        }
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_MAXIMUM) {
        bool found = false;
        opcua_double_t max_val = 0.0;
        mu_variant_t max_var;
        memset(&max_var, 0, sizeof(max_var));
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!found || val > max_val) {
                        max_val = val;
                        max_var = points[i].value;
                        found = true;
                    }
                }
            }
        }
        if (found) {
            *result = max_var;
            *result_status = MU_STATUS_GOOD;
        } else {
            *result_status = MU_STATUS_BAD_NODATA;
            memset(result, 0, sizeof(*result));
        }
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_COUNT) {
        size_t count = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    count++;
                }
            }
        }
        result->type = MU_TYPE_INT64;
        result->value.i64 = (opcua_int64_t)count;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_START || aggregate_type == MU_ID_AGGREGATETYPE_START_BOUND) {
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    *result = points[i].value;
                    *result_status = MU_STATUS_GOOD;
                    return MU_STATUS_GOOD;
                }
            }
        }
        *result_status = MU_STATUS_BAD_NODATA;
        memset(result, 0, sizeof(*result));
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_END || aggregate_type == MU_ID_AGGREGATETYPE_END_BOUND) {
        for (size_t i = num_points; i > 0; i--) {
            size_t idx = i - 1;
            if (!aggregate_status_is_bad_cu(points[idx].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[idx].value, &val)) {
                    *result = points[idx].value;
                    *result_status = MU_STATUS_GOOD;
                    return MU_STATUS_GOOD;
                }
            }
        }
        *result_status = MU_STATUS_BAD_NODATA;
        memset(result, 0, sizeof(*result));
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_DELTA || aggregate_type == MU_ID_AGGREGATETYPE_DELTA_BOUNDS) {
        opcua_double_t first = 0.0, last = 0.0;
        bool have_first = false, have_last = false;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!have_first) {
                        first = val;
                        have_first = true;
                    }
                    last = val;
                    have_last = true;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (have_first && have_last) ? last - first : 0.0;
        result->is_array = false;
        *result_status = (have_first && have_last) ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_RANGE || aggregate_type == MU_ID_AGGREGATETYPE_RANGE_2) {
        opcua_double_t min_r = 0.0, max_r = 0.0;
        bool have = false;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!have) {
                        min_r = max_r = val;
                        have = true;
                    } else {
                        if (val < min_r) min_r = val;
                        if (val > max_r) max_r = val;
                    }
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = have ? max_r - min_r : 0.0;
        result->is_array = false;
        *result_status = have ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_TIME_AVERAGE || aggregate_type == MU_ID_AGGREGATETYPE_TIME_AVERAGE_2) {
        opcua_double_t sum = 0.0;
        size_t count = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    sum += val;
                    count++;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (count > 0) ? sum / (opcua_double_t)count : 0.0;
        result->is_array = false;
        *result_status = (count > 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_TOTAL || aggregate_type == MU_ID_AGGREGATETYPE_TOTAL_2) {
        opcua_double_t total = 0.0;
        bool have = false;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    total += val;
                    have = true;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = total;
        result->is_array = false;
        *result_status = have ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_MINIMUM_2) {
        bool found = false;
        opcua_double_t min_val = 0.0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!found || val < min_val) {
                        min_val = val;
                        found = true;
                    }
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = found ? min_val : 0.0;
        result->is_array = false;
        *result_status = found ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_MAXIMUM_2) {
        bool found = false;
        opcua_double_t max_val = 0.0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!found || val > max_val) {
                        max_val = val;
                        found = true;
                    }
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = found ? max_val : 0.0;
        result->is_array = false;
        *result_status = found ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_WORST_QUALITY || aggregate_type == MU_ID_AGGREGATETYPE_WORST_QUALITY_2) {
        opcua_statuscode_t worst = 0;
        if (num_points > 0) {
            worst = points[0].status;
            for (size_t i = 1; i < num_points; i++) {
                if ((points[i].status & 0xC0000000u) > (worst & 0xC0000000u)) {
                    worst = points[i].status;
                }
            }
        }
        result->type = MU_TYPE_STATUSCODE;
        result->value.status = worst;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_DURATION_GOOD) {
        opcua_uint64_t duration_ms = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                if (i > 0 && !aggregate_status_is_bad_cu(points[i - 1].status)) {
                    if (points[i].source_timestamp > points[i - 1].source_timestamp) {
                        duration_ms += (opcua_uint64_t)(points[i].source_timestamp - points[i - 1].source_timestamp);
                    }
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (opcua_double_t)duration_ms;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_DURATION_BAD) {
        opcua_uint64_t duration_ms = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (aggregate_status_is_bad_cu(points[i].status)) {
                if (i > 0 && aggregate_status_is_bad_cu(points[i - 1].status)) {
                    if (points[i].source_timestamp > points[i - 1].source_timestamp) {
                        duration_ms += (opcua_uint64_t)(points[i].source_timestamp - points[i - 1].source_timestamp);
                    }
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (opcua_double_t)duration_ms;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_DURATION_IN_STATE_ZERO) {
        opcua_uint64_t duration_ms = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val) && val == 0.0) {
                    if (i > 0) {
                        opcua_double_t prev_val;
                        if (!aggregate_status_is_bad_cu(points[i - 1].status) &&
                            variant_numeric_to_double_hist(&points[i - 1].value, &prev_val) && prev_val == 0.0) {
                            if (points[i].source_timestamp > points[i - 1].source_timestamp) {
                                duration_ms +=
                                    (opcua_uint64_t)(points[i].source_timestamp - points[i - 1].source_timestamp);
                            }
                        }
                    }
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (opcua_double_t)duration_ms;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_DURATION_IN_STATE_NON_ZERO) {
        opcua_uint64_t duration_ms = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val) && val != 0.0) {
                    if (i > 0) {
                        opcua_double_t prev_val;
                        if (!aggregate_status_is_bad_cu(points[i - 1].status) &&
                            variant_numeric_to_double_hist(&points[i - 1].value, &prev_val) && prev_val != 0.0) {
                            if (points[i].source_timestamp > points[i - 1].source_timestamp) {
                                duration_ms +=
                                    (opcua_uint64_t)(points[i].source_timestamp - points[i - 1].source_timestamp);
                            }
                        }
                    }
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (opcua_double_t)duration_ms;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_PERCENT_GOOD) {
        opcua_uint32_t good = 0, bad = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (aggregate_status_is_bad_cu(points[i].status)) {
                bad++;
            } else {
                good++;
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (good + bad > 0) ? (opcua_double_t)good * 100.0 / (opcua_double_t)(good + bad) : 0.0;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_PERCENT_BAD) {
        opcua_uint32_t good = 0, bad = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (aggregate_status_is_bad_cu(points[i].status)) {
                bad++;
            } else {
                good++;
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (good + bad > 0) ? (opcua_double_t)bad * 100.0 / (opcua_double_t)(good + bad) : 0.0;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_MINIMUM_ACTUAL_TIME ||
        aggregate_type == MU_ID_AGGREGATETYPE_MINIMUM_ACTUAL_TIME_2) {
        bool found = false;
        opcua_double_t min_val = 0.0;
        mu_variant_t min_var;
        memset(&min_var, 0, sizeof(min_var));
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!found || val < min_val) {
                        min_val = val;
                        min_var = points[i].value;
                        found = true;
                    }
                }
            }
        }
        if (found) {
            *result = min_var;
            *result_status = MU_STATUS_GOOD;
        } else {
            *result_status = MU_STATUS_BAD_NODATA;
            memset(result, 0, sizeof(*result));
        }
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_MAXIMUM_ACTUAL_TIME ||
        aggregate_type == MU_ID_AGGREGATETYPE_MAXIMUM_ACTUAL_TIME_2) {
        bool found = false;
        opcua_double_t max_val = 0.0;
        mu_variant_t max_var;
        memset(&max_var, 0, sizeof(max_var));
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    if (!found || val > max_val) {
                        max_val = val;
                        max_var = points[i].value;
                        found = true;
                    }
                }
            }
        }
        if (found) {
            *result = max_var;
            *result_status = MU_STATUS_GOOD;
        } else {
            *result_status = MU_STATUS_BAD_NODATA;
            memset(result, 0, sizeof(*result));
        }
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_NUMBER_OF_TRANSITIONS) {
        opcua_uint32_t transitions = 0;
        bool last_was_zero = false;
        bool has_last = false;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    bool is_zero = (val == 0.0);
                    if (!has_last) {
                        last_was_zero = is_zero;
                        has_last = true;
                    } else if (is_zero != last_was_zero) {
                        transitions++;
                        last_was_zero = is_zero;
                    }
                }
            }
        }
        result->type = MU_TYPE_INT64;
        result->value.i64 = (opcua_int64_t)transitions;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_STANDARD_DEVIATION_SAMPLE) {
        opcua_uint32_t count = 0;
        opcua_double_t mean = 0.0;
        opcua_double_t m2 = 0.0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    count++;
                    opcua_double_t delta = val - mean;
                    mean += delta / (opcua_double_t)count;
                    opcua_double_t delta2 = val - mean;
                    m2 += delta * delta2;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (count > 1) ? sqrt(m2 / (opcua_double_t)(count - 1)) : 0.0;
        result->is_array = false;
        *result_status = (count > 1) ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_VARIANCE_SAMPLE) {
        opcua_uint32_t count = 0;
        opcua_double_t mean = 0.0;
        opcua_double_t m2 = 0.0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    count++;
                    opcua_double_t delta = val - mean;
                    mean += delta / (opcua_double_t)count;
                    opcua_double_t delta2 = val - mean;
                    m2 += delta * delta2;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (count > 1) ? m2 / (opcua_double_t)(count - 1) : 0.0;
        result->is_array = false;
        *result_status = (count > 1) ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_STANDARD_DEVIATION_POPULATION) {
        opcua_uint32_t count = 0;
        opcua_double_t mean = 0.0;
        opcua_double_t m2 = 0.0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    count++;
                    opcua_double_t delta = val - mean;
                    mean += delta / (opcua_double_t)count;
                    opcua_double_t delta2 = val - mean;
                    m2 += delta * delta2;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (count > 0) ? sqrt(m2 / (opcua_double_t)count) : 0.0;
        result->is_array = false;
        *result_status = (count > 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_VARIANCE_POPULATION) {
        opcua_uint32_t count = 0;
        opcua_double_t mean = 0.0;
        opcua_double_t m2 = 0.0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    count++;
                    opcua_double_t delta = val - mean;
                    mean += delta / (opcua_double_t)count;
                    opcua_double_t delta2 = val - mean;
                    m2 += delta * delta2;
                }
            }
        }
        result->type = MU_TYPE_DOUBLE;
        result->value.d = (count > 0) ? m2 / (opcua_double_t)count : 0.0;
        result->is_array = false;
        *result_status = (count > 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_NODATA;
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_INTERPOLATIVE) {
        for (size_t i = num_points; i > 0; i--) {
            size_t idx = i - 1;
            if (!aggregate_status_is_bad_cu(points[idx].status)) {
                *result = points[idx].value;
                *result_status = MU_STATUS_GOOD;
                return MU_STATUS_GOOD;
            }
        }
        *result_status = MU_STATUS_BAD_NODATA;
        memset(result, 0, sizeof(*result));
        return MU_STATUS_GOOD;
    }

    if (aggregate_type == MU_ID_AGGREGATETYPE_ANNOTATION_COUNT) {
        size_t count = 0;
        for (size_t i = 0; i < num_points; i++) {
            if (!aggregate_status_is_bad_cu(points[i].status)) {
                opcua_double_t val;
                if (variant_numeric_to_double_hist(&points[i].value, &val)) {
                    count++;
                }
            }
        }
        result->type = MU_TYPE_INT64;
        result->value.i64 = (opcua_int64_t)count;
        result->is_array = false;
        *result_status = MU_STATUS_GOOD;
        return MU_STATUS_GOOD;
    }

    *result_status = MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
    memset(result, 0, sizeof(*result));
    return MU_STATUS_GOOD;
}

#endif
