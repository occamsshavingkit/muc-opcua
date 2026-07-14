#include "muc_opcua/services/history_memstore.h"
#include "muc_opcua/status.h"
#include <string.h>

static mu_history_memstore_t *store(void *context) {
    return (mu_history_memstore_t *)context;
}

static size_t find_timestamp(const mu_history_memstore_t *s, opcua_datetime_t source_timestamp) {
    for (size_t i = 0; i < s->count; i++) {
        if (s->points[i].source_timestamp == source_timestamp)
            return i;
    }
    return s->count;
}

static opcua_boolean_t point_in_range(const mu_historical_data_point_t *point, opcua_datetime_t start_time,
                                      opcua_datetime_t end_time) {
    return point->source_timestamp >= start_time && point->source_timestamp <= end_time;
}

opcua_statuscode_t mu_memstore_read_raw_modified(void *context, const mu_nodeid_t *node_id,
                                                 opcua_boolean_t is_read_modified, opcua_datetime_t start_time,
                                                 opcua_datetime_t end_time, opcua_uint32_t num_values_per_node,
                                                 opcua_boolean_t return_bounds, const opcua_byte_t *cp_in,
                                                 size_t cp_in_length, opcua_byte_t *cp_out, size_t *cp_out_length,
                                                 mu_historical_data_point_t *data_points, size_t max_data_points,
                                                 size_t *actual_data_points) {
    (void)node_id;
    (void)is_read_modified;
    (void)return_bounds;
    mu_history_memstore_t *s = store(context);
    size_t written = 0;

    if (cp_out_length) {
        *cp_out_length = 0;
    }

    /* Continuation point: skip past previously-returned points */
    size_t skip = 0;
    if (cp_in && cp_in_length == sizeof(size_t)) {
        memcpy(&skip, cp_in, sizeof(size_t));
    }

    size_t per_page =
        (num_values_per_node > 0 && num_values_per_node < max_data_points) ? num_values_per_node : max_data_points;

    size_t scan = skip;
    for (; scan < s->count && written < per_page; scan++) {
        if (!point_in_range(&s->points[scan], start_time, end_time)) {
            continue;
        }
        memcpy(&data_points[written], &s->points[scan], sizeof(mu_historical_data_point_t));
        written++;
    }

    *actual_data_points = written;

    if (written == per_page && scan < s->count && cp_out && cp_out_length) {
        size_t next = scan;
        memcpy(cp_out, &next, sizeof(size_t));
        *cp_out_length = sizeof(size_t);
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_memstore_update_data(void *context, const mu_nodeid_t *node_id,
                                           opcua_uint32_t perform_insert_replace,
                                           const mu_historical_data_point_t *data_points, size_t data_points_count,
                                           opcua_statuscode_t *results) {
    (void)node_id;
    mu_history_memstore_t *s = store(context);

    for (size_t i = 0; i < data_points_count; i++) {
        size_t existing = find_timestamp(s, data_points[i].source_timestamp);
        if (perform_insert_replace == 1) { /* Insert */
            if (existing < s->count) {
                results[i] = MU_STATUS_BAD_ENTRYEXISTS;
                continue;
            }
            if (s->count >= MU_MEMSTORE_MAX_POINTS) {
                results[i] = MU_STATUS_BAD_OUTOFMEMORY;
                continue;
            }
            memcpy(&s->points[s->count], &data_points[i], sizeof(mu_historical_data_point_t));
            s->count++;
            results[i] = MU_STATUS_GOOD;
        } else if (perform_insert_replace == 2) { /* Replace */
            if (existing < s->count) {
                memcpy(&s->points[existing], &data_points[i], sizeof(mu_historical_data_point_t));
                results[i] = MU_STATUS_GOOD;
            } else {
                results[i] = MU_STATUS_BAD_NOENTRYEXISTS;
            }
        } else {
            results[i] = MU_STATUS_BAD_INVALIDARGUMENT;
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_memstore_delete_raw_modified(void *context, const mu_nodeid_t *node_id,
                                                   opcua_boolean_t is_delete_modified, opcua_datetime_t start_time,
                                                   opcua_datetime_t end_time) {
    (void)node_id;
    (void)is_delete_modified;
    mu_history_memstore_t *s = store(context);
    size_t write = 0;
    size_t deleted = 0;

    for (size_t i = 0; i < s->count; i++) {
        if (s->points[i].source_timestamp >= start_time && s->points[i].source_timestamp <= end_time) {
            deleted++;
        } else {
            if (write != i) {
                memcpy(&s->points[write], &s->points[i], sizeof(mu_historical_data_point_t));
            }
            write++;
        }
    }
    s->count = write;
    (void)deleted;
    return MU_STATUS_GOOD;
}
