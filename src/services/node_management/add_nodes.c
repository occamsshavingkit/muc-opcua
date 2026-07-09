#include "common.h"

#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT

opcua_boolean_t mu_dynamic_string_fits(const mu_string_t *name, size_t max_length) {
    return name->length == -1 || (name->length >= 0 && (size_t)name->length <= max_length);
}

void mu_dynamic_string_copy(mu_string_t *target, opcua_byte_t *dest, const mu_string_t *source) {
    if (source->length == -1) {
        target->length = -1;
        target->data = NULL;
        return;
    }

    if (source->length > 0) {
        (void)memcpy(dest, source->data, (size_t)source->length);
    }

    target->length = source->length;
    target->data = dest;
}

void mu_dynamic_nodeid_copy(mu_nodeid_t *target, opcua_byte_t *dest, const mu_nodeid_t *source) {
    *target = *source;
    if (source->identifier_type == MU_NODEID_STRING) {
        mu_dynamic_string_copy(&target->identifier.string, dest, &source->identifier.string);
    }
}

static opcua_boolean_t mu_dynamic_browse_name_fits(const mu_string_t *name) {
    return mu_dynamic_string_fits(name, MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH);
}

static opcua_boolean_t mu_dynamic_display_name_fits(const mu_string_t *name) {
    return mu_dynamic_string_fits(name, MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH);
}

static opcua_boolean_t mu_dynamic_string_nodeid_fits(const mu_nodeid_t *node_id) {
    if (node_id->identifier_type != MU_NODEID_STRING) {
        return true;
    }

    return mu_dynamic_string_fits(&node_id->identifier.string, MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH);
}

static void mu_dynamic_browse_name_copy(mu_dynamic_address_space_t *space, size_t node_index,
                                        const mu_string_t *source) {
    mu_dynamic_string_copy(&space->nodes[node_index].browse_name, space->browse_name_storage[node_index], source);
}

static void mu_dynamic_display_name_copy(mu_dynamic_address_space_t *space, size_t node_index,
                                         const mu_string_t *source) {
    mu_dynamic_string_copy(&space->nodes[node_index].display_name, space->display_name_storage[node_index], source);
}

static void mu_dynamic_string_nodeid_copy(mu_dynamic_address_space_t *space, size_t node_index,
                                          const mu_nodeid_t *source) {
    mu_dynamic_nodeid_copy(&space->nodes[node_index].node_id, space->string_nodeid_storage[node_index], source);
}

static opcua_boolean_t mu_nodeid_is_numeric0(const mu_nodeid_t *node_id, opcua_uint32_t numeric_id) {
    return node_id->namespace_index == 0 && node_id->identifier_type == MU_NODEID_NUMERIC &&
           node_id->identifier.numeric == numeric_id;
}

static opcua_statuscode_t mu_decode_localized_text_text(mu_binary_reader_t *reader, mu_string_t *text) {
    opcua_byte_t encoding_mask = 0;
    opcua_statuscode_t status = mu_binary_read_byte(reader, &encoding_mask);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    if ((encoding_mask & (opcua_byte_t)~0x03u) != 0u) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    text->length = -1;
    text->data = NULL;

    if ((encoding_mask & 0x01u) != 0u) {
        mu_string_t ignored_locale;
        status = mu_binary_read_string(reader, &ignored_locale);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }
    if ((encoding_mask & 0x02u) != 0u) {
        status = mu_binary_read_string(reader, text);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mu_add_nodes_item_display_name(const mu_add_nodes_item_t *item, mu_string_t *display_name,
                                                         opcua_boolean_t *has_display_name) {
    *has_display_name = false;
    display_name->length = -1;
    display_name->data = NULL;

    if (item->node_attributes_len == 0 || item->node_attributes == NULL) {
        return MU_STATUS_GOOD;
    }

    if (!mu_nodeid_is_numeric0(&item->node_attributes_type_id, MU_OBJECTATTRIBUTES_ENCODING_DEFAULTBINARY_ID)) {
        return MU_STATUS_GOOD;
    }

    mu_binary_reader_t body;
    mu_binary_reader_init(&body, item->node_attributes, item->node_attributes_len);

    opcua_uint32_t specified_attributes = 0;
    opcua_statuscode_t status = mu_binary_read_uint32(&body, &specified_attributes);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    /* OPC-10000-4 sections 5.8.2.2 and 7.24.2: ObjectAttributes carries
       DisplayName as LocalizedText when NodeAttributes bit 6 is specified. */
    if ((specified_attributes & MU_NODEATTRIBUTES_DISPLAYNAME_BIT) == 0u) {
        return MU_STATUS_GOOD;
    }

    status = mu_decode_localized_text_text(&body, display_name);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    *has_display_name = true;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_add_nodes_request_decode(mu_binary_reader_t *r, mu_add_nodes_item_t *items, size_t max_items,
                                               size_t *out_count) {
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
        s = mu_binary_read_nodeid(r, &items[i].parent_node_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_nodeid(r, &items[i].reference_type_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_expanded_nodeid(r, &items[i].requested_new_node_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_qualified_name(r, &items[i].browse_name);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_uint32(r, &items[i].node_class);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = mu_binary_read_extension_object_header(r, &items[i].node_attributes_type_id, &items[i].node_attributes_len);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        if (items[i].node_attributes_len > 0) {
            if (r->position > r->length || items[i].node_attributes_len > (r->length - r->position)) {
                r->status = MU_STATUS_BAD_DECODINGERROR;
                return MU_STATUS_BAD_DECODINGERROR;
            }
            items[i].node_attributes = r->buffer + r->position;
            r->position += items[i].node_attributes_len;
        } else {
            items[i].node_attributes = NULL;
        }

        s = mu_binary_read_expanded_nodeid(r, &items[i].type_definition);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_add_nodes_response_encode(mu_binary_writer_t *w, const mu_add_nodes_result_t *results,
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

        s = mu_binary_write_nodeid(w, &results[i].added_node_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
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
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_add_nodes_result_t results[8];
    for (size_t i = 0; i < count; ++i) {
        (void)memset(&results[i], 0, sizeof(results[i]));
        results[i].added_node_id.identifier_type = MU_NODEID_NUMERIC;
        results[i].added_node_id.namespace_index = 0;
        results[i].added_node_id.identifier.numeric = 0;

        /* Basic validation */
        if (items[i].node_class == 0) {
            results[i].status_code = MU_STATUS_BAD_NODECLASSINVALID;
            continue;
        }

        /* Check capacity */
        if (server->dynamic_address_space.nodes_count >= MU_INTERN_MAX_DYNAMIC_NODES) {
            results[i].status_code = MU_STATUS_BAD_OUTOFMEMORY;
            continue;
        }

        /* OPC-10000-4 5.8.2.2 AddNodesItem.browseName becomes persistent Node
           state here; this embedded build owns a bounded copy instead of
           retaining transient request-buffer storage. */
        if (!mu_dynamic_browse_name_fits(&items[i].browse_name.name)) {
            results[i].status_code = MU_STATUS_BAD_OUTOFMEMORY;
            continue;
        }

        mu_string_t display_name = items[i].browse_name.name;
        opcua_boolean_t has_display_name = false;
        s = mu_add_nodes_item_display_name(&items[i], &display_name, &has_display_name);
        if (s != MU_STATUS_GOOD) {
            results[i].status_code = s;
            continue;
        }
        if (!has_display_name) {
            display_name = items[i].browse_name.name;
        }
        if (!mu_dynamic_display_name_fits(&display_name)) {
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
        /* OPC-10000-4 5.8.2.2 AddNodesItem.requestedNewNodeId becomes the
           persisted NodeId; bounded storage avoids retaining request bytes. */
        if (!mu_dynamic_string_nodeid_fits(&new_id)) {
            results[i].status_code = MU_STATUS_BAD_OUTOFMEMORY;
            continue;
        }

        /* OPC-10000-4 §5.8.2.2 / §7.38.2: reject a requestedNewNodeId that already
           exists in the configured base space or the dynamic space. */
        {
            opcua_boolean_t node_exists = false;
            if ((server->config.address_space != NULL) &&
                (mu_address_space_find_node(server->config.address_space, (mu_address_space_index_t *)0, &new_id) !=
                 NULL)) {
                node_exists = true;
            }
            for (size_t j = 0; (!node_exists) && (j < server->dynamic_address_space.nodes_count); ++j) {
                if (mu_nodeid_equal(&server->dynamic_address_space.nodes[j].node_id, &new_id)) {
                    node_exists = true;
                }
            }
            if (node_exists) {
                results[i].status_code = MU_STATUS_BAD_NODEIDEXISTS;
                continue;
            }
        }

        /* Add to dynamic address space */
        size_t idx = server->dynamic_address_space.nodes_count++;
        mu_node_t *node = &server->dynamic_address_space.nodes[idx];
        (void)memset(node, 0, sizeof(mu_node_t));
        mu_dynamic_string_nodeid_copy(&server->dynamic_address_space, idx, &new_id);
        node->node_class = (mu_node_class_t)items[i].node_class;
        mu_dynamic_browse_name_copy(&server->dynamic_address_space, idx, &items[i].browse_name.name);
        mu_dynamic_display_name_copy(&server->dynamic_address_space, idx, &display_name);

        results[i].status_code = MU_STATUS_GOOD;
        results[i].added_node_id = node->node_id;
    }

    return mu_add_nodes_response_encode(w, results, count);
}

#endif /* MUC_OPCUA_SERVICE_NODEMANAGEMENT */
