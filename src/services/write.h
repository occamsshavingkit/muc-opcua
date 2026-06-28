/* src/services/write.h */
#ifndef MICRO_OPCUA_SERVICES_WRITE_H
#define MICRO_OPCUA_SERVICES_WRITE_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/encoding.h"

#ifdef MICRO_OPCUA_SERVICE_WRITE

#ifndef MU_ATTRIBUTEID_VALUE
#define MU_ATTRIBUTEID_VALUE 13
#endif

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t attribute_id;
    mu_string_t index_range;
    mu_datavalue_t value;
} mu_write_value_t;

typedef struct {
    size_t num_nodes_to_write;
    mu_write_value_t *nodes_to_write;
} mu_write_request_t;

typedef struct {
    size_t num_results;
    opcua_statuscode_t *results;
} mu_write_response_t;

opcua_statuscode_t mu_write_request_decode(mu_binary_reader_t *reader,
                                           mu_write_request_t *req,
                                           mu_write_value_t *nodes_array,
                                           size_t max_nodes);

opcua_statuscode_t mu_write_response_encode(mu_binary_writer_t *writer,
                                            const mu_write_response_t *resp);

#endif /* MICRO_OPCUA_SERVICE_WRITE */

#endif /* MICRO_OPCUA_SERVICES_WRITE_H */
