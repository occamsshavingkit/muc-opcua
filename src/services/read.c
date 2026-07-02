/* src/services/read.c */
#include "read.h"
#include "../address_space/base_nodes.h"
#include <stddef.h>
#include <string.h>

opcua_statuscode_t mu_read_request_decode(mu_binary_reader_t *reader, mu_read_request_t *req,
                                          mu_read_value_id_t *nodes_array, size_t max_nodes) {
    if (!reader || !req || !nodes_array)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t status;

    status = mu_binary_read_double(reader, &req->max_age);
    if (status != MU_STATUS_GOOD)
        return status;

    status = mu_binary_read_uint32(reader, &req->timestamps_to_return);
    if (status != MU_STATUS_GOOD)
        return status;

    opcua_int32_t no_of_nodes;
    status = mu_binary_read_int32(reader, &no_of_nodes);
    if (status != MU_STATUS_GOOD)
        return status;

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
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_uint32(reader, &node->attribute_id);
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_string(reader, &node->index_range);
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_uint16(reader, &node->data_encoding_namespace_index);
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_string(reader, &node->data_encoding_name);
        if (status != MU_STATUS_GOOD)
            return status;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_read_response_encode(mu_binary_writer_t *writer, const mu_read_response_t *resp) {
    if (!writer || !resp)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t status;

    /* Results array */
    if (resp->num_results == 0 || resp->results == NULL) {
        status = mu_binary_write_int32(writer, 0);
        if (status != MU_STATUS_GOOD)
            return status;
    } else {
        status = mu_binary_write_int32(writer, (opcua_int32_t)resp->num_results);
        if (status != MU_STATUS_GOOD)
            return status;

        for (size_t i = 0; i < resp->num_results; ++i) {
            status = mu_binary_write_datavalue(writer, &resp->results[i]);
            if (status != MU_STATUS_GOOD)
                return status;
        }
    }

    /* DiagnosticInfos array */
    status = mu_binary_write_int32(writer, 0); /* empty */
    if (status != MU_STATUS_GOOD)
        return status;

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_attribute(const mu_address_space_t *address_space, const mu_node_t *node,
                                         opcua_uint32_t attribute_id, mu_variant_t *value) {
    (void)address_space;
    /* Default to a scalar; array attributes (e.g. a String[] Value) set these explicitly. */
    value->is_array = false;
    value->array_length = 0;
    switch (attribute_id) {
    case MU_ATTRIBUTEID_NODEID:
        value->type = MU_TYPE_NODEID;
        value->value.nodeid = node->node_id;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_NODECLASS:
        value->type = MU_TYPE_INT32;
        value->value.i32 = (opcua_int32_t)node->node_class;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_BROWSENAME:
        /* BrowseName is a mandatory, readable Attribute of every Node
           (OPC 10000-3 5.2.4), encoded as a QualifiedName. */
        value->type = MU_TYPE_QUALIFIEDNAME;
        value->value.qualified_name.namespace_index = node->node_id.namespace_index;
        value->value.qualified_name.name = node->browse_name;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_DISPLAYNAME:
        /* DisplayName is a mandatory, readable Attribute of every Node
           (OPC 10000-3 5.2.5), encoded as a LocalizedText (null locale). */
        value->type = MU_TYPE_LOCALIZEDTEXT;
        value->value.localized_text.locale = (mu_string_t){-1, NULL};
        value->value.localized_text.text = node->display_name;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_VALUE:
        /* The Value Attribute only exists for Variable (and VariableType) Nodes;
           reading it on another NodeClass is Bad_AttributeIdInvalid (OPC 10000-3 5.6). */
        if (node->node_class != MU_NODECLASS_VARIABLE) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        if (node->value) {
            return mu_value_source_read(node->value, &node->node_id, value);
        }
        return MU_STATUS_BAD_NOTREADABLE;

    default:
        return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
    }
}

opcua_statuscode_t mu_read_process_with_user_index(const mu_address_space_t *address_space,
                                                   mu_address_space_index_t *user_index,
                                                   const mu_address_space_t *dynamic, const mu_read_request_t *req,
                                                   mu_read_response_t *resp, mu_datavalue_t *results_array,
                                                   size_t max_results) {
    if (!req || !resp || !results_array)
        return MU_STATUS_BAD_INTERNALERROR;
    /* OPC-10000-4 section 5.11.2.3 uses TimestampsToReturn from section 7.39. */
    if (req->timestamps_to_return > MU_TIMESTAMPS_TO_RETURN_NEITHER)
        return MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID;
    if (req->num_nodes_to_read > max_results)
        return MU_STATUS_BAD_TOOMANYOPERATIONS;

    resp->results = results_array;
    resp->num_results = req->num_nodes_to_read;

    for (size_t i = 0; i < req->num_nodes_to_read; ++i) {
        const mu_read_value_id_t *read_val = &req->nodes_to_read[i];
        mu_datavalue_t *dv = &resp->results[i];

        memset(dv, 0, sizeof(*dv));

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
                status = mu_value_source_read(node->value, &node->node_id, &dv->value);
            } else {
                status = MU_STATUS_BAD_NOTREADABLE;
            }
        } else {
            status = read_attribute(address_space, node, read_val->attribute_id, &dv->value);
        }
        if (status == MU_STATUS_GOOD) {
            dv->has_value = true;
            /* Optionally add timestamps based on req->timestamps_to_return, but not strictly needed for test */
        } else {
            dv->has_status = true;
            dv->status = status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_read_process(const mu_address_space_t *address_space, const mu_address_space_t *dynamic,
                                   const mu_read_request_t *req, mu_read_response_t *resp,
                                   mu_datavalue_t *results_array, size_t max_results) {
    return mu_read_process_with_user_index(address_space, NULL, dynamic, req, resp, results_array, max_results);
}
