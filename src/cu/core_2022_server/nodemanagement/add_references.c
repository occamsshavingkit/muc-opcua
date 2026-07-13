#include "services/node_management/common.h"

#ifdef MUC_OPCUA_CU_NODEMANAGEMENT

static opcua_boolean_t mu_dynamic_reference_string_nodeid_fits(const mu_nodeid_t *node_id) {
    if (node_id->identifier_type != MU_NODEID_STRING) {
        return true;
    }

    return mu_dynamic_string_fits(&node_id->identifier.string, MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH);
}

static opcua_boolean_t mu_dynamic_reference_nodeids_fit(const mu_add_references_item_t *item) {
    return mu_dynamic_reference_string_nodeid_fits(&item->source_node_id) &&
           mu_dynamic_reference_string_nodeid_fits(&item->reference_type_id) &&
           mu_dynamic_reference_string_nodeid_fits(&item->target_node_id.node_id);
}

static void mu_dynamic_reference_copy(mu_dynamic_address_space_t *space, size_t reference_index,
                                      const mu_add_references_item_t *item) {
    mu_dynamic_reference_t *dyn_ref = &space->references[reference_index];

    mu_dynamic_nodeid_copy(&dyn_ref->source_node_id, space->reference_source_nodeid_storage[reference_index],
                           &item->source_node_id);
    mu_dynamic_nodeid_copy(&dyn_ref->ref.reference_type_id, space->reference_type_nodeid_storage[reference_index],
                           &item->reference_type_id);
    dyn_ref->ref.is_forward = item->is_forward;
    mu_dynamic_nodeid_copy(&dyn_ref->ref.target_id, space->reference_target_nodeid_storage[reference_index],
                           &item->target_node_id.node_id);
}

opcua_statuscode_t mu_add_references_request_decode(mu_binary_reader_t *r, mu_add_references_item_t *items,
                                                    size_t max_items, size_t *out_count) {
    opcua_int32_t count = 0;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (count < 0) {
        *out_count = 0;
        return MU_STATUS_GOOD;
    }

    if ((size_t)count > max_items) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    *out_count = (size_t)count;

    for (size_t i = 0; i < *out_count; ++i) {
        s = mu_binary_read_nodeid(r, &items[i].source_node_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_nodeid(r, &items[i].reference_type_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        opcua_byte_t b;
        s = mu_binary_read_byte(r, &b);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        items[i].is_forward = b ? true : false;

        s = mu_binary_read_string(r, &items[i].target_server_uri);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_expanded_nodeid(r, &items[i].target_node_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_uint32(r, &items[i].target_node_class);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_add_references_response_encode(mu_binary_writer_t *w, const mu_add_references_result_t *results,
                                                     size_t count) {
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (size_t i = 0; i < count; ++i) {
        s = mu_binary_write_statuscode(w, results[i].status_code);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    /* DiagnosticInfos */
    return mu_binary_write_int32(w, 0);
}

opcua_statuscode_t mu_add_references_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w) {
    if (!server->config.allow_node_management) {
        return MU_STATUS_BAD_USERACCESSDENIED;
    }

    mu_add_references_item_t items[8];
    size_t count = 0;
    opcua_statuscode_t s = mu_add_references_request_decode(r, items, 8, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_add_references_result_t results[8];
    for (size_t i = 0; i < count; ++i) {
        if (server->dynamic_address_space.references_count >= MU_INTERN_MAX_DYNAMIC_REFERENCES) {
            results[i].status_code = MU_STATUS_BAD_OUTOFMEMORY;
            continue;
        }

        /* OPC-10000-4 5.8.3.2 AddReferencesItem stores SourceNode,
           ReferenceType, and TargetNode identifiers as persistent Reference
           state; string NodeIds are copied into fixed per-reference storage. */
        if (!mu_dynamic_reference_nodeids_fit(&items[i])) {
            results[i].status_code = MU_STATUS_BAD_OUTOFMEMORY;
            continue;
        }

        size_t idx = server->dynamic_address_space.references_count++;
        mu_dynamic_reference_copy(&server->dynamic_address_space, idx, &items[i]);

        results[i].status_code = MU_STATUS_GOOD;
    }

    return mu_add_references_response_encode(w, results, count);
}

#endif /* MUC_OPCUA_CU_NODEMANAGEMENT */
