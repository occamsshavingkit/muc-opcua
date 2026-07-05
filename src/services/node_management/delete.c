#include "common.h"

#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT

static void mu_dynamic_reference_move(mu_dynamic_address_space_t *space, size_t dest_index, size_t source_index) {
    const mu_dynamic_reference_t moved = space->references[source_index];

    mu_dynamic_nodeid_copy(&space->references[dest_index].source_node_id,
                           space->reference_source_nodeid_storage[dest_index], &moved.source_node_id);
    mu_dynamic_nodeid_copy(&space->references[dest_index].ref.reference_type_id,
                           space->reference_type_nodeid_storage[dest_index], &moved.ref.reference_type_id);
    space->references[dest_index].ref.is_forward = moved.ref.is_forward;
    mu_dynamic_nodeid_copy(&space->references[dest_index].ref.target_id,
                           space->reference_target_nodeid_storage[dest_index], &moved.ref.target_id);
}

static void mu_dynamic_node_move(mu_dynamic_address_space_t *space, size_t dest_index, size_t source_index) {
    mu_node_t moved = space->nodes[source_index];
    const mu_nodeid_type_t nodeid_type = moved.node_id.identifier_type;
    const opcua_int32_t string_nodeid_length =
        nodeid_type == MU_NODEID_STRING ? moved.node_id.identifier.string.length : -1;
    const opcua_int32_t browse_name_length = moved.browse_name.length;
    const opcua_int32_t display_name_length = moved.display_name.length;

    space->nodes[dest_index] = moved;
    if (nodeid_type == MU_NODEID_STRING) {
        if (string_nodeid_length == -1) {
            space->nodes[dest_index].node_id.identifier.string.data = NULL;
        } else {
            if (string_nodeid_length > 0) {
                (void)memcpy(space->string_nodeid_storage[dest_index], space->string_nodeid_storage[source_index],
                             (size_t)string_nodeid_length);
            }
            space->nodes[dest_index].node_id.identifier.string.data = space->string_nodeid_storage[dest_index];
        }
    }

    if (browse_name_length == -1) {
        space->nodes[dest_index].browse_name.data = NULL;
    } else {
        if (browse_name_length > 0) {
            (void)memcpy(space->browse_name_storage[dest_index], space->browse_name_storage[source_index],
                         (size_t)browse_name_length);
        }
        space->nodes[dest_index].browse_name.data = space->browse_name_storage[dest_index];
    }

    if (display_name_length == -1) {
        space->nodes[dest_index].display_name.data = NULL;
    } else {
        if (display_name_length > 0) {
            (void)memcpy(space->display_name_storage[dest_index], space->display_name_storage[source_index],
                         (size_t)display_name_length);
        }
        space->nodes[dest_index].display_name.data = space->display_name_storage[dest_index];
    }
}

opcua_statuscode_t mu_delete_nodes_request_decode(mu_binary_reader_t *r, mu_delete_nodes_item_t *items,
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
        s = mu_binary_read_nodeid(r, &items[i].node_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        opcua_byte_t b;
        s = mu_binary_read_byte(r, &b);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        items[i].delete_target_references = b ? true : false;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_delete_nodes_response_encode(mu_binary_writer_t *w, const opcua_statuscode_t *results,
                                                   size_t count) {
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (size_t i = 0; i < count; ++i) {
        s = mu_binary_write_statuscode(w, results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    /* DiagnosticInfos */
    return mu_binary_write_int32(w, 0);
}

opcua_statuscode_t mu_delete_references_request_decode(mu_binary_reader_t *r, mu_delete_references_item_t *items,
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

        s = mu_binary_read_expanded_nodeid(r, &items[i].target_node_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_byte(r, &b);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        items[i].delete_bidirectional = b ? true : false;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_delete_references_response_encode(mu_binary_writer_t *w, const opcua_statuscode_t *results,
                                                        size_t count) {
    opcua_statuscode_t s = mu_binary_write_int32(w, (opcua_int32_t)count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (size_t i = 0; i < count; ++i) {
        s = mu_binary_write_statuscode(w, results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    /* DiagnosticInfos */
    return mu_binary_write_int32(w, 0);
}

opcua_statuscode_t mu_delete_nodes_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w) {
    if (!server->config.allow_node_management) {
        return MU_STATUS_BAD_USERACCESSDENIED;
    }

    mu_delete_nodes_item_t items[8];
    size_t count = 0;
    opcua_statuscode_t s = mu_delete_nodes_request_decode(r, items, 8, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_statuscode_t results[8];
    for (size_t i = 0; i < count; ++i) {
        opcua_boolean_t found = false;
        for (size_t j = 0; j < server->dynamic_address_space.nodes_count; ++j) {
            if (mu_nodeid_equal(&server->dynamic_address_space.nodes[j].node_id, &items[i].node_id)) {
                /* Remove node by swapping with last element */
                size_t last = --server->dynamic_address_space.nodes_count;
                if (j != last) {
                    mu_dynamic_node_move(&server->dynamic_address_space, j, last);
                }
                found = true;
                break;
            }
        }

        if (found) {
            if (items[i].delete_target_references) {
                /* T020: Ensure DeleteNodes implementation also deletes all references to the deleted node */
                size_t r_idx = 0;
                while (r_idx < server->dynamic_address_space.references_count) {
                    mu_dynamic_reference_t *dyn_ref = &server->dynamic_address_space.references[r_idx];

                    if (mu_nodeid_equal(&dyn_ref->source_node_id, &items[i].node_id) ||
                        mu_nodeid_equal(&dyn_ref->ref.target_id, &items[i].node_id)) {

                        size_t last = --server->dynamic_address_space.references_count;
                        if (r_idx != last) {
                            mu_dynamic_reference_move(&server->dynamic_address_space, r_idx, last);
                        }
                        /* Do not increment r_idx since we swapped in a new element to check */
                    } else {
                        r_idx++;
                    }
                }
            }

            results[i] = MU_STATUS_GOOD;
        } else {
            results[i] = MU_STATUS_BAD_NODEIDUNKNOWN;
        }
    }

    return mu_delete_nodes_response_encode(w, results, count);
}

opcua_statuscode_t mu_delete_references_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w) {
    if (!server->config.allow_node_management) {
        return MU_STATUS_BAD_USERACCESSDENIED;
    }

    mu_delete_references_item_t items[8];
    size_t count = 0;
    opcua_statuscode_t s = mu_delete_references_request_decode(r, items, 8, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

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
                    mu_dynamic_reference_move(&server->dynamic_address_space, j, last);
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

#endif /* MUC_OPCUA_SERVICE_NODEMANAGEMENT */
