#include "core/service_dispatch/common.h"

#ifdef MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
#define MU_MAX_HISTORY_NODES_PER_READ 10
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
#ifdef MUC_OPCUA_CU_TIME_SYNC
                                  ,
                                  server
#endif
        );
        if (s == MU_STATUS_GOOD) {
            *response_length = w->position;
        }
        return s;
    }

    if (!server->config.history_adapter.read_raw_modified) {
        s = write_response_prefix(w, MU_ID_HISTORYREADRESPONSE, req_header.request_handle, MU_STATUS_BAD_NOTSUPPORTED
#ifdef MUC_OPCUA_CU_TIME_SYNC
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
    mu_datavalue_t dvals[MU_MAX_HISTORY_NODES_PER_READ][10];
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
        mu_historical_data_point_t data_points[10];
        memset(data_points, 0, sizeof(data_points));
        size_t actual_data_points = 0;

        res->status_code = server->config.history_adapter.read_raw_modified(
            server->config.history_adapter.context, &node->node_id, req.details.is_read_modified,
            req.details.start_time, req.details.end_time, req.details.num_values_per_node, req.details.return_bounds,
            node->continuation_point.data,
            node->continuation_point.length > 0 ? (size_t)node->continuation_point.length : 0, continuation_points[i],
            &cp_out_length, data_points, 10, &actual_data_points);

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
    }

    s = write_response_prefix(w, MU_ID_HISTORYREADRESPONSE, req_header.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_CU_TIME_SYNC
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
#ifdef MUC_OPCUA_CU_TIME_SYNC
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
                res->status_code = MU_STATUS_BAD_NOTSUPPORTED;
                continue;
            }

            res->num_operation_results = item->body.data.num_values;
            res->status_code = server->config.history_adapter.update_data(
                server->config.history_adapter.context, &item->body.data.node_id,
                item->body.data.perform_insert_replace, item->body.data.values, item->body.data.num_values,
                res->operation_results);
        } else if (item->type == MU_HISTORY_UPDATE_TYPE_DELETE) {
            if (!server->config.history_adapter.delete_raw_modified) {
                res->status_code = MU_STATUS_BAD_NOTSUPPORTED;
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
#ifdef MUC_OPCUA_CU_TIME_SYNC
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
#endif
