/* src/services/read.h */
#ifndef MUC_OPCUA_SERVICES_READ_H
#define MUC_OPCUA_SERVICES_READ_H

#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/status.h"

typedef struct mu_read_cache mu_read_cache_t;

typedef enum {
    MU_ATTRIBUTEID_NODEID = 1,
    MU_ATTRIBUTEID_NODECLASS = 2,
    MU_ATTRIBUTEID_BROWSENAME = 3,
    MU_ATTRIBUTEID_DISPLAYNAME = 4,
    MU_ATTRIBUTEID_DESCRIPTION = 5,
    MU_ATTRIBUTEID_WRITEMASK = 6,
    MU_ATTRIBUTEID_USERWRITEMASK = 7,
    MU_ATTRIBUTEID_ISABSTRACT = 8,
    MU_ATTRIBUTEID_SYMMETRIC = 9,
    MU_ATTRIBUTEID_INVERSENAME = 10,
    MU_ATTRIBUTEID_CONTAINSNOLOOPS = 11,
    MU_ATTRIBUTEID_EVENTNOTIFIER = 12,
    MU_ATTRIBUTEID_VALUE = 13,
    MU_ATTRIBUTEID_DATATYPE = 14,
    MU_ATTRIBUTEID_VALUERANK = 15,
    MU_ATTRIBUTEID_ARRAYDIMENSIONS = 16,
    MU_ATTRIBUTEID_ACCESSLEVEL = 17,
    MU_ATTRIBUTEID_USERACCESSLEVEL = 18,
    MU_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL = 19,
    MU_ATTRIBUTEID_HISTORIZING = 20,
    MU_ATTRIBUTEID_EXECUTABLE = 21,
    MU_ATTRIBUTEID_USEREXECUTABLE = 22
} mu_attribute_id_t;

typedef enum {
    MU_TIMESTAMPS_TO_RETURN_SOURCE = 0,
    MU_TIMESTAMPS_TO_RETURN_SERVER = 1,
    MU_TIMESTAMPS_TO_RETURN_BOTH = 2,
    MU_TIMESTAMPS_TO_RETURN_NEITHER = 3,
    MU_TIMESTAMPS_TO_RETURN_INVALID = 4
} mu_timestamps_to_return_t;

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t attribute_id;
    mu_string_t index_range;
    opcua_uint16_t data_encoding_namespace_index;
    mu_string_t data_encoding_name;
} mu_read_value_id_t;

typedef struct {
    opcua_double_t max_age;
    opcua_uint32_t timestamps_to_return;
    mu_read_value_id_t *nodes_to_read;
    size_t num_nodes_to_read;
} mu_read_request_t;

typedef struct {
    mu_datavalue_t *results;
    size_t num_results;
} mu_read_response_t;

opcua_statuscode_t mu_read_request_decode(mu_binary_reader_t *reader, mu_read_request_t *req,
                                          mu_read_value_id_t *nodes_array, size_t max_nodes);

opcua_statuscode_t mu_read_response_encode(mu_binary_writer_t *writer, const mu_read_response_t *resp);

/* Process a Read request (OPC-10000-4 5.11.2). `now` is the current server UTC
 * time (100-ns intervals since 1601-01-01) used to populate sourceTimestamp
 * and/or serverTimestamp per req->timestamps_to_return (5.11.2.2 Table 47,
 * 7.39 Table 180). Pass 0 if no time source is available. */
opcua_statuscode_t mu_read_process(const mu_address_space_t *address_space, const mu_address_space_t *dynamic,
                                   const mu_read_request_t *req, opcua_datetime_t now, mu_read_response_t *resp,
                                   mu_datavalue_t *results_array, size_t max_results, mu_read_cache_t *cache);

typedef struct {
    size_t hits;
    size_t misses;
} mu_read_cache_stats_t;

void mu_read_cache_get_stats(mu_read_cache_stats_t *out);

#endif /* MUC_OPCUA_SERVICES_READ_H */
