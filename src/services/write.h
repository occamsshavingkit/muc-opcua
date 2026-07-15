/* src/services/write.h */
#ifndef MUC_OPCUA_SERVICES_WRITE_H
#define MUC_OPCUA_SERVICES_WRITE_H

#include "muc_opcua/encoding.h"
#include "muc_opcua/types.h"

#ifdef MUC_OPCUA_SERVICE_WRITE

#ifndef MU_ATTRIBUTEID_VALUE
#define MU_ATTRIBUTEID_VALUE 13
#endif

typedef struct {
    size_t num_nodes_to_write;
    mu_write_value_t *nodes_to_write;
} mu_write_request_t;

typedef struct {
    size_t num_results;
    opcua_statuscode_t *results;
} mu_write_response_t;

opcua_statuscode_t mu_write_request_decode(mu_binary_reader_t *reader, mu_write_request_t *req,
                                           mu_write_value_t *nodes_array, size_t max_nodes);

opcua_statuscode_t mu_write_response_encode(mu_binary_writer_t *writer, const mu_write_response_t *resp);

#endif /* MUC_OPCUA_SERVICE_WRITE */

#endif /* MUC_OPCUA_SERVICES_WRITE_H */
