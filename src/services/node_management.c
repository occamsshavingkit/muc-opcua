/* src/services/node_management.c */
#include "node_management.h"
#include <string.h>

#ifdef MICRO_OPCUA_SERVICE_NODEMANAGEMENT

/* Decoder / Encoder implementations */
opcua_statuscode_t mu_add_nodes_request_decode(mu_binary_reader_t *r, mu_add_nodes_item_t *items, size_t max_items,
                                               size_t *out_count) {
    opcua_int32_t count = 0;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;

    if (count < 0) {
        *out_count = 0;
        return MU_STATUS_GOOD;
    }

    if ((size_t)count > max_items) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    *out_count = (size_t)count;

    for (size_t i = 0; i < *out_count; ++i) {
        s = mu_binary_read_nodeid(r, &items[i].parent_node_id);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_nodeid(r, &items[i].reference_type_id);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_expanded_nodeid(r, &items[i].requested_new_node_id);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_qualified_name(r, &items[i].browse_name);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_uint32(r, &items[i].node_class);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_extension_object_header(r, &items[i].node_attributes_type_id, &items[i].node_attributes_len);
        if (s != MU_STATUS_GOOD)
            return s;

        if (items[i].node_attributes_len > 0) {
            items[i].node_attributes = r->buffer + r->position;
            r->position += items[i].node_attributes_len;
        } else {
            items[i].node_attributes = NULL;
        }

        s = mu_binary_read_expanded_nodeid(r, &items[i].type_definition);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_add_nodes_response_encode(mu_binary_writer_t *w, const mu_add_nodes_result_t *results,
                                                size_t count) {
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (size_t i = 0; i < count; ++i) {
        s = mu_binary_write_statuscode(w, results[i].status_code);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_write_nodeid(w, &results[i].added_node_id);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* DiagnosticInfos */
    return mu_binary_write_int32(w, 0);
}

opcua_statuscode_t mu_add_references_request_decode(mu_binary_reader_t *r, mu_add_references_item_t *items,
                                                    size_t max_items, size_t *out_count) {
    opcua_int32_t count = 0;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;

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
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_nodeid(r, &items[i].reference_type_id);
        if (s != MU_STATUS_GOOD)
            return s;

        opcua_byte_t b;
        s = mu_binary_read_byte(r, &b);
        if (s != MU_STATUS_GOOD)
            return s;
        items[i].is_forward = b ? true : false;

        s = mu_binary_read_string(r, &items[i].target_server_uri);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_expanded_nodeid(r, &items[i].target_node_id);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_uint32(r, &items[i].target_node_class);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_add_references_response_encode(mu_binary_writer_t *w, const mu_add_references_result_t *results,
                                                     size_t count) {
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (size_t i = 0; i < count; ++i) {
        s = mu_binary_write_statuscode(w, results[i].status_code);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* DiagnosticInfos */
    return mu_binary_write_int32(w, 0);
}

opcua_statuscode_t mu_delete_nodes_request_decode(mu_binary_reader_t *r, mu_delete_nodes_item_t *items,
                                                  size_t max_items, size_t *out_count) {
    opcua_int32_t count = 0;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;

    if (count < 0) {
        *out_count = 0;
        return MU_STATUS_GOOD;
    }

    if ((size_t)count > max_items) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    *out_count = (size_t)count;

    for (size_t i = 0; i < *out_count; ++i) {
        s = mu_binary_read_nodeid(r, &items[i].node_id);
        if (s != MU_STATUS_GOOD)
            return s;

        opcua_byte_t b;
        s = mu_binary_read_byte(r, &b);
        if (s != MU_STATUS_GOOD)
            return s;
        items[i].delete_target_references = b ? true : false;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_delete_nodes_response_encode(mu_binary_writer_t *w, const opcua_statuscode_t *results,
                                                   size_t count) {
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (size_t i = 0; i < count; ++i) {
        s = mu_binary_write_statuscode(w, results[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* DiagnosticInfos */
    return mu_binary_write_int32(w, 0);
}

opcua_statuscode_t mu_delete_references_request_decode(mu_binary_reader_t *r, mu_delete_references_item_t *items,
                                                       size_t max_items, size_t *out_count) {
    opcua_int32_t count = 0;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD)
        return s;

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
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_nodeid(r, &items[i].reference_type_id);
        if (s != MU_STATUS_GOOD)
            return s;

        opcua_byte_t b;
        s = mu_binary_read_byte(r, &b);
        if (s != MU_STATUS_GOOD)
            return s;
        items[i].is_forward = b ? true : false;

        s = mu_binary_read_expanded_nodeid(r, &items[i].target_node_id);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_byte(r, &b);
        if (s != MU_STATUS_GOOD)
            return s;
        items[i].delete_bidirectional = b ? true : false;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_delete_references_response_encode(mu_binary_writer_t *w, const opcua_statuscode_t *results,
                                                        size_t count) {
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD)
        return s;

    for (size_t i = 0; i < count; ++i) {
        s = mu_binary_write_statuscode(w, results[i]);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    /* DiagnosticInfos */
    return mu_binary_write_int32(w, 0);
}

opcua_statuscode_t mu_add_nodes_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w) {
    if (!server->config.allow_node_management) {
        return MU_STATUS_BAD_USERACCESSDENIED;
    }

    mu_add_nodes_item_t items[8]; /* Arbitrary local max for processing */
    size_t count = 0;
    opcua_statuscode_t s = mu_add_nodes_request_decode(r, items, 8, &count);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_add_nodes_result_t results[8];
    for (size_t i = 0; i < count; ++i) {
        memset(&results[i], 0, sizeof(results[i]));
        results[i].added_node_id.identifier_type = MU_NODEID_NUMERIC;
        results[i].added_node_id.namespace_index = 0;
        results[i].added_node_id.identifier.numeric = 0;

        /* Basic validation */
        if (items[i].node_class == 0) {
            results[i].status_code = MU_STATUS_BAD_NODECLASSINVALID;
            continue;
        }

        /* Check capacity */
        if (server->dynamic_address_space.nodes_count >= MU_MAX_DYNAMIC_NODES) {
            results[i].status_code = MU_STATUS_BAD_OUTOFMEMORY;
            continue;
        }

        /* Assign NodeId if not provided (simplified) */
        mu_nodeid_t new_id = items[i].requested_new_node_id.node_id;
        if (new_id.identifier_type == MU_NODEID_NUMERIC && new_id.identifier.numeric == 0) {
            new_id.namespace_index = 1;
            /* Simple auto-increment strategy based on count */
            new_id.identifier.numeric = 1000 + (opcua_uint32_t)server->dynamic_address_space.nodes_count;
        }

        /* Add to dynamic address space */
        size_t idx = server->dynamic_address_space.nodes_count++;
        mu_node_t *node = &server->dynamic_address_space.nodes[idx];
        memset(node, 0, sizeof(mu_node_t));
        node->node_id = new_id;
        node->node_class = (mu_node_class_t)items[i].node_class;
        node->browse_name = items[i].browse_name.name;
        node->display_name = items[i].browse_name.name;

        results[i].status_code = MU_STATUS_GOOD;
        results[i].added_node_id = new_id;
    }

    return mu_add_nodes_response_encode(w, results, count);
}

opcua_statuscode_t mu_delete_nodes_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w) {
    if (!server->config.allow_node_management) {
        return MU_STATUS_BAD_USERACCESSDENIED;
    }

    mu_delete_nodes_item_t items[8];
    size_t count = 0;
    opcua_statuscode_t s = mu_delete_nodes_request_decode(r, items, 8, &count);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_statuscode_t results[8];
    for (size_t i = 0; i < count; ++i) {
        opcua_boolean_t found = false;
        for (size_t j = 0; j < server->dynamic_address_space.nodes_count; ++j) {
            if (mu_nodeid_equal(&server->dynamic_address_space.nodes[j].node_id, &items[i].node_id)) {
                /* Remove node by swapping with last element */
                size_t last = --server->dynamic_address_space.nodes_count;
                if (j != last) {
                    server->dynamic_address_space.nodes[j] = server->dynamic_address_space.nodes[last];
                }
                found = true;
                break;
            }
        }

        if (found) {
            /* T020: Ensure DeleteNodes implementation also deletes all references to the deleted node */
            size_t r_idx = 0;
            while (r_idx < server->dynamic_address_space.references_count) {
                mu_dynamic_reference_t *dyn_ref = &server->dynamic_address_space.references[r_idx];

                if (mu_nodeid_equal(&dyn_ref->source_node_id, &items[i].node_id) ||
                    mu_nodeid_equal(&dyn_ref->ref.target_id, &items[i].node_id)) {

                    size_t last = --server->dynamic_address_space.references_count;
                    if (r_idx != last) {
                        server->dynamic_address_space.references[r_idx] =
                            server->dynamic_address_space.references[last];
                    }
                    /* Do not increment r_idx since we swapped in a new element to check */
                } else {
                    r_idx++;
                }
            }

            results[i] = MU_STATUS_GOOD;
        } else {
            results[i] = MU_STATUS_BAD_NODEIDUNKNOWN;
        }
    }

    return mu_delete_nodes_response_encode(w, results, count);
}

opcua_statuscode_t mu_add_references_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w) {
    if (!server->config.allow_node_management) {
        return MU_STATUS_BAD_USERACCESSDENIED;
    }

    mu_add_references_item_t items[8];
    size_t count = 0;
    opcua_statuscode_t s = mu_add_references_request_decode(r, items, 8, &count);
    if (s != MU_STATUS_GOOD)
        return s;

    mu_add_references_result_t results[8];
    for (size_t i = 0; i < count; ++i) {
        if (server->dynamic_address_space.references_count >= MU_MAX_DYNAMIC_REFERENCES) {
            results[i].status_code = MU_STATUS_BAD_OUTOFMEMORY;
            continue;
        }

        size_t idx = server->dynamic_address_space.references_count++;
        mu_dynamic_reference_t *dyn_ref = &server->dynamic_address_space.references[idx];

        dyn_ref->source_node_id = items[i].source_node_id;
        dyn_ref->ref.reference_type_id = items[i].reference_type_id;
        dyn_ref->ref.is_forward = items[i].is_forward;
        dyn_ref->ref.target_id = items[i].target_node_id.node_id;

        results[i].status_code = MU_STATUS_GOOD;
    }

    return mu_add_references_response_encode(w, results, count);
}

opcua_statuscode_t mu_delete_references_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w) {
    if (!server->config.allow_node_management) {
        return MU_STATUS_BAD_USERACCESSDENIED;
    }

    mu_delete_references_item_t items[8];
    size_t count = 0;
    opcua_statuscode_t s = mu_delete_references_request_decode(r, items, 8, &count);
    if (s != MU_STATUS_GOOD)
        return s;

    opcua_statuscode_t results[8];
    for (size_t i = 0; i < count; ++i) {
        opcua_boolean_t found = false;

        for (size_t j = 0; j < server->dynamic_address_space.references_count; ++j) {
            mu_dynamic_reference_t *dyn_ref = &server->dynamic_address_space.references[j];

            if (mu_nodeid_equal(&dyn_ref->source_node_id, &items[i].source_node_id) &&
                mu_nodeid_equal(&dyn_ref->ref.reference_type_id, &items[i].reference_type_id) &&
                mu_nodeid_equal(&dyn_ref->ref.target_id, &items[i].target_node_id.node_id) &&
                dyn_ref->ref.is_forward == items[i].is_forward) {

                size_t last = --server->dynamic_address_space.references_count;
                if (j != last) {
                    server->dynamic_address_space.references[j] = server->dynamic_address_space.references[last];
                }
                found = true;
                break;
            }
        }

        if (found) {
            results[i] = MU_STATUS_GOOD;
        } else {
            results[i] = MU_STATUS_BAD_NOTFOUND; /* Assuming Bad_NotFound for reference not found */
        }
    }

    return mu_delete_references_response_encode(w, results, count);
}

#endif /* MICRO_OPCUA_SERVICE_NODEMANAGEMENT */
