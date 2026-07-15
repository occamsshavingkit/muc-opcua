/* src/services/write.c */
#include "services/write.h"
#include <stddef.h>

#ifdef MUC_OPCUA_SERVICE_WRITE

opcua_statuscode_t mu_write_request_decode(mu_binary_reader_t *reader, mu_write_request_t *req,
                                           mu_write_value_t *nodes_array, size_t max_nodes) {
    if (!reader || !req || !nodes_array) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;
    opcua_int32_t no_of_nodes;
    status = mu_binary_read_int32(reader, &no_of_nodes);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (no_of_nodes < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (no_of_nodes == -1) {
        req->num_nodes_to_write = 0;
        req->nodes_to_write = NULL;
        return MU_STATUS_GOOD;
    }
    if ((size_t)no_of_nodes > max_nodes) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    size_t node_count = (size_t)no_of_nodes;
    mu_write_value_t original_nodes[node_count == 0 ? 1 : node_count];
    for (size_t i = 0; i < node_count; ++i) {
        original_nodes[i] = nodes_array[i];
    }

    for (mu_write_value_t *node = nodes_array, *end = nodes_array + node_count; node != end; ++node) {
        status = mu_binary_read_nodeid(reader, &node->node_id);
        if (status != MU_STATUS_GOOD) {
            for (size_t i = 0; i < node_count; ++i) {
                nodes_array[i] = original_nodes[i];
            }
            return status;
        }

        status = mu_binary_read_uint32(reader, &node->attribute_id);
        if (status != MU_STATUS_GOOD) {
            for (size_t i = 0; i < node_count; ++i) {
                nodes_array[i] = original_nodes[i];
            }
            return status;
        }

        status = mu_binary_read_string(reader, &node->index_range);
        if (status != MU_STATUS_GOOD) {
            for (size_t i = 0; i < node_count; ++i) {
                nodes_array[i] = original_nodes[i];
            }
            return status;
        }

        status = mu_binary_read_datavalue(reader, &node->value);
        if (status != MU_STATUS_GOOD) {
            for (size_t i = 0; i < node_count; ++i) {
                nodes_array[i] = original_nodes[i];
            }
            return status;
        }
    }

    req->num_nodes_to_write = node_count;
    req->nodes_to_write = nodes_array;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_write_response_encode(mu_binary_writer_t *writer, const mu_write_response_t *resp) {
    if (!writer || !resp) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;
    size_t result_count = resp->num_results;
    status = mu_binary_write_int32(writer, (opcua_int32_t)result_count);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    const opcua_statuscode_t *results = resp->results;
    for (size_t i = 0; i < result_count; ++i) {
        status = mu_binary_write_statuscode(writer, results[i]);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    /* WriteResponse DiagnosticInfos[] — null array per OPC-10000-6 §5.2.2.15 */
    status = mu_binary_write_int32(writer, -1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_SERVICE_WRITE */
