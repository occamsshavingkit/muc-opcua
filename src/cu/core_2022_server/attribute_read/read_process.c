/* src/services/read/read_process.c */
#include "address_space/base_nodes.h"
#include "services/read/common.h"
#include <stddef.h>

opcua_statuscode_t mu_read_request_decode(mu_binary_reader_t *reader, mu_read_request_t *req,
                                          mu_read_value_id_t *nodes_array, size_t max_nodes) {
    if (!reader || !req || !nodes_array) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;

    status = mu_binary_read_double(reader, &req->max_age);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_uint32(reader, &req->timestamps_to_return);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    opcua_int32_t no_of_nodes;
    status = mu_binary_read_int32(reader, &no_of_nodes);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (no_of_nodes < 0) {
        req->num_nodes_to_read = 0;
        return MU_STATUS_GOOD;
    }
    if ((size_t)no_of_nodes > max_nodes) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    req->num_nodes_to_read = (size_t)no_of_nodes;
    req->nodes_to_read = nodes_array;

    for (size_t i = 0; i < req->num_nodes_to_read; ++i) {
        mu_read_value_id_t *node = &req->nodes_to_read[i];

        status = mu_binary_read_nodeid(reader, &node->node_id);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_uint32(reader, &node->attribute_id);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_string(reader, &node->index_range);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_uint16(reader, &node->data_encoding_namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_string(reader, &node->data_encoding_name);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_read_response_encode(mu_binary_writer_t *writer, const mu_read_response_t *resp) {
    if (!writer || !resp) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;

    if (resp->num_results == 0 || resp->results == NULL) {
        status = mu_binary_write_int32(writer, 0);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    } else {
        status = mu_binary_write_int32(writer, (opcua_int32_t)resp->num_results);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        for (size_t i = 0; i < resp->num_results; ++i) {
            status = mu_binary_write_datavalue(writer, &resp->results[i]);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
        }
    }

    status = mu_binary_write_int32(writer, 0);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_read_process_with_user_index(const mu_address_space_t *address_space,
                                                   mu_address_space_index_t *user_index,
                                                   const mu_address_space_t *dynamic, const mu_read_request_t *req,
                                                   opcua_datetime_t now, mu_read_response_t *resp,
                                                   mu_datavalue_t *results_array, size_t max_results,
                                                   mu_read_cache_t *cache) {
    if (!req || !resp || !results_array) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (req->timestamps_to_return > MU_TIMESTAMPS_TO_RETURN_NEITHER) {
        return MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID;
    }
    if (req->num_nodes_to_read > max_results) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    resp->results = results_array;
    resp->num_results = req->num_nodes_to_read;

    for (size_t i = 0; i < req->num_nodes_to_read; ++i) {
        const mu_read_value_id_t *read_val = &req->nodes_to_read[i];
        mu_datavalue_t *dv = &resp->results[i];

        dv->has_value = false;
        dv->has_status = false;
        dv->has_source_timestamp = false;
        dv->has_source_picoseconds = false;
        dv->has_server_timestamp = false;
        dv->has_server_picoseconds = false;

        const mu_node_t *node = mu_resolve_node(address_space, user_index, dynamic, &read_val->node_id);
        if (!node) {
            dv->has_status = true;
            dv->status = MU_STATUS_BAD_NODEIDUNKNOWN;
            continue;
        }

        opcua_statuscode_t status;
        if (read_val->attribute_id == MU_ATTRIBUTEID_VALUE) {
            dv->value.is_array = false;
            dv->value.array_length = 0;
            if (node->node_class != MU_NODECLASS_VARIABLE) {
                status = MU_STATUS_BAD_ATTRIBUTEIDINVALID;
            } else if (node->value) {
#if MUC_OPCUA_READ_CACHE
                if (mu_read_cache_lookup(cache, &read_val->node_id, req->max_age, now, &dv->value)) {
                    status = MU_STATUS_GOOD;
                } else {
                    status = mu_value_source_read(node->value, &node->node_id, &dv->value);
                    if (status == MU_STATUS_GOOD) {
                        mu_read_cache_store(cache, &read_val->node_id, &dv->value, now);
                    }
                }
#else
                /* No value cache: always return a fresh "best effort" value.
                   maxAge is decoded for wire compatibility but not acted upon,
                   which OPC-10000-4 5.11.2 permits (fresh always satisfies it). */
                (void)cache;
                status = mu_value_source_read(node->value, &node->node_id, &dv->value);
#endif
            } else {
                status = MU_STATUS_BAD_NOTREADABLE;
            }
        } else {
            status = read_attribute(address_space, node, read_val->attribute_id, &dv->value);
        }
        if (status == MU_STATUS_GOOD) {
#ifdef MUC_OPCUA_CU_MULTI_CHUNK
            status = apply_index_range(&read_val->index_range, &dv->value);
#endif
        }
        if (status == MU_STATUS_GOOD) {
            dv->has_value = true;
            opcua_uint32_t ttr = req->timestamps_to_return;
            if (ttr == MU_TIMESTAMPS_TO_RETURN_SOURCE || ttr == MU_TIMESTAMPS_TO_RETURN_BOTH) {
                dv->has_source_timestamp = true;
                dv->source_timestamp = now;
            }
            if (ttr == MU_TIMESTAMPS_TO_RETURN_SERVER || ttr == MU_TIMESTAMPS_TO_RETURN_BOTH) {
                dv->has_server_timestamp = true;
                dv->server_timestamp = now;
            }
        } else {
            dv->has_status = true;
            dv->status = status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_read_process(const mu_address_space_t *address_space, const mu_address_space_t *dynamic,
                                   const mu_read_request_t *req, opcua_datetime_t now, mu_read_response_t *resp,
                                   mu_datavalue_t *results_array, size_t max_results, mu_read_cache_t *cache) {
    return mu_read_process_with_user_index(address_space, NULL, dynamic, req, now, resp, results_array, max_results,
                                           cache);
}
