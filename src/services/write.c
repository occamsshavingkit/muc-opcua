/* src/services/write.c */
#include "write.h"
#include <stddef.h>

#ifdef MICRO_OPCUA_SERVICE_WRITE

opcua_statuscode_t mu_write_request_decode(mu_binary_reader_t *reader,
                                           mu_write_request_t *req,
                                           mu_write_value_t *nodes_array,
                                           size_t max_nodes)
{
    if (!reader || !req || !nodes_array) return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t status;
    opcua_int32_t no_of_nodes;
    status = mu_binary_read_int32(reader, &no_of_nodes);
    if (status != MU_STATUS_GOOD) return status;

    if (no_of_nodes < 0) {
        req->num_nodes_to_write = 0;
        req->nodes_to_write = NULL;
        return MU_STATUS_GOOD;
    }
    if ((size_t)no_of_nodes > max_nodes) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    req->num_nodes_to_write = (size_t)no_of_nodes;
    req->nodes_to_write = nodes_array;

    for (size_t i = 0; i < req->num_nodes_to_write; ++i) {
        mu_write_value_t *node = &req->nodes_to_write[i];

        status = mu_binary_read_nodeid(reader, &node->node_id);
        if (status != MU_STATUS_GOOD) return status;

        status = mu_binary_read_uint32(reader, &node->attribute_id);
        if (status != MU_STATUS_GOOD) return status;

        status = mu_binary_read_string(reader, &node->index_range);
        if (status != MU_STATUS_GOOD) return status;

        status = mu_binary_read_datavalue(reader, &node->value);
        if (status != MU_STATUS_GOOD) return status;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_response_encode(mu_binary_writer_t *writer,
                                            const mu_write_response_t *resp)
{
    if (!writer || !resp) return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t status;
    status = mu_binary_write_int32(writer, (opcua_int32_t)resp->num_results);
    if (status != MU_STATUS_GOOD) return status;

    for (size_t i = 0; i < resp->num_results; ++i) {
        status = mu_binary_write_statuscode(writer, resp->results[i]);
        if (status != MU_STATUS_GOOD) return status;
    }

    return MU_STATUS_GOOD;
}

#endif /* MICRO_OPCUA_SERVICE_WRITE */
