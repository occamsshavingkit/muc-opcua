#ifndef MICRO_OPCUA_SERVICES_HISTORY_INTERNAL_H
#define MICRO_OPCUA_SERVICES_HISTORY_INTERNAL_H

#include "micro_opcua/services/history.h"
#include "micro_opcua/encoding.h"

#ifdef MICRO_OPCUA_SERVICE_HISTORY

typedef struct {
    opcua_boolean_t is_read_modified;
    opcua_datetime_t start_time;
    opcua_datetime_t end_time;
    opcua_uint32_t num_values_per_node;
    opcua_boolean_t return_bounds;
} mu_read_raw_modified_details_t;

typedef struct {
    mu_nodeid_t node_id;
    mu_string_t index_range;
    mu_qualified_name_t data_encoding;
    mu_bytestring_t continuation_point;
} mu_history_read_value_id_t;

typedef struct {
    mu_read_raw_modified_details_t details;
    opcua_uint32_t timestamps_to_return;
    opcua_boolean_t release_continuation_points;
    size_t num_nodes_to_read;
    mu_history_read_value_id_t *nodes_to_read;
} mu_history_read_request_t;

typedef struct {
    size_t num_data_values;
    mu_datavalue_t *data_values;
} mu_history_data_t;

typedef struct {
    opcua_statuscode_t status_code;
    mu_bytestring_t continuation_point;
    mu_history_data_t history_data;
} mu_history_read_result_t;

typedef struct {
    size_t num_results;
    mu_history_read_result_t *results;
} mu_history_read_response_t;

opcua_statuscode_t mu_history_read_request_decode(mu_binary_reader_t *reader, mu_history_read_request_t *req,
                                                  mu_history_read_value_id_t *nodes_array, size_t max_nodes);

opcua_statuscode_t mu_history_read_response_encode(mu_binary_writer_t *writer, const mu_history_read_response_t *resp);

/* HistoryUpdate structures */
#define MU_MAX_HISTORY_UPDATE_VALUES_PER_DETAIL 10

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t perform_insert_replace;
    size_t num_values;
    mu_historical_data_point_t values[MU_MAX_HISTORY_UPDATE_VALUES_PER_DETAIL];
} mu_update_data_details_t;

typedef struct {
    mu_nodeid_t node_id;
    opcua_boolean_t is_delete_modified;
    opcua_datetime_t start_time;
    opcua_datetime_t end_time;
} mu_delete_raw_modified_details_t;

typedef enum {
    MU_HISTORY_UPDATE_TYPE_INVALID = 0,
    MU_HISTORY_UPDATE_TYPE_DATA = 1,
    MU_HISTORY_UPDATE_TYPE_DELETE = 2
} mu_history_update_details_type_t;

typedef struct {
    mu_history_update_details_type_t type;
    union {
        mu_update_data_details_t data;
        mu_delete_raw_modified_details_t delete_raw;
    } body;
} mu_history_update_item_t;

typedef struct {
    size_t num_items;
    mu_history_update_item_t *items;
} mu_history_update_request_t;

typedef struct {
    opcua_statuscode_t status_code;
    size_t num_operation_results;
    opcua_statuscode_t operation_results[MU_MAX_HISTORY_UPDATE_VALUES_PER_DETAIL];
} mu_history_update_result_t;

typedef struct {
    size_t num_results;
    mu_history_update_result_t *results;
} mu_history_update_response_t;

opcua_statuscode_t mu_history_update_request_decode(mu_binary_reader_t *reader, mu_history_update_request_t *req,
                                                    mu_history_update_item_t *items_array, size_t max_items);

opcua_statuscode_t mu_history_update_response_encode(mu_binary_writer_t *writer, const mu_history_update_response_t *resp);

#endif /* MICRO_OPCUA_SERVICE_HISTORY */

#endif /* MICRO_OPCUA_SERVICES_HISTORY_INTERNAL_H */
