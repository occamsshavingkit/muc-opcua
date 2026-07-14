#ifndef MU_SERVICES_HISTORY_MEMSTORE_H
#define MU_SERVICES_HISTORY_MEMSTORE_H

#include "muc_opcua/services/history.h"

#define MU_MEMSTORE_MAX_POINTS 64

typedef struct {
    mu_historical_data_point_t points[MU_MEMSTORE_MAX_POINTS];
    size_t count;
} mu_history_memstore_t;

opcua_statuscode_t mu_memstore_read_raw_modified(void *context, const mu_nodeid_t *node_id,
                                                 opcua_boolean_t is_read_modified, opcua_datetime_t start_time,
                                                 opcua_datetime_t end_time, opcua_uint32_t num_values_per_node,
                                                 opcua_boolean_t return_bounds, const opcua_byte_t *cp_in,
                                                 size_t cp_in_length, opcua_byte_t *cp_out, size_t *cp_out_length,
                                                 mu_historical_data_point_t *data_points, size_t max_data_points,
                                                 size_t *actual_data_points);

opcua_statuscode_t mu_memstore_update_data(void *context, const mu_nodeid_t *node_id,
                                           opcua_uint32_t perform_insert_replace,
                                           const mu_historical_data_point_t *data_points, size_t data_points_count,
                                           opcua_statuscode_t *results);

opcua_statuscode_t mu_memstore_delete_raw_modified(void *context, const mu_nodeid_t *node_id,
                                                   opcua_boolean_t is_delete_modified, opcua_datetime_t start_time,
                                                   opcua_datetime_t end_time);

#endif
