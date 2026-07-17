/* service_dispatch/view_handler.c - Browse-family handlers */
#include "core/service_dispatch/common.h"

#define MU_DISPATCH_MAX_BROWSE_NODES 8
/* Spec 057: advertised MaxNodesPerBrowse (base_nodes.c) must match enforcement. */
_Static_assert(MU_DISPATCH_MAX_BROWSE_NODES == MU_MAX_NODES_PER_BROWSE,
               "advertised MaxNodesPerBrowse must match the enforced dispatch bound");
#define MU_DISPATCH_MAX_BROWSE_REFS 32
#define MU_DISPATCH_MAX_BROWSE_CONTINUATION_POINTS 32
#define MU_DISPATCH_MAX_TRANSLATE_BROWSE_PATHS 16
#define MU_DISPATCH_MAX_TRANSLATE_ELEMENTS 8

#ifdef MUC_OPCUA_CU_VIEW_BASIC_2
/* mu_browse_process_with_user_index is defined in src/services/browse.c. It is
    not declared in browse.h so forward-declare it here for the dispatch handler.
    OPC-10000-4 5.9.2. */
opcua_statuscode_t mu_browse_process_with_user_index(const mu_address_space_t *address_space,
                                                     const mu_address_space_index_t *user_index,
                                                     const mu_address_space_t *dynamic,
#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
                                                     const mu_dynamic_reference_t *dyn_refs, size_t dyn_refs_count,
#endif
                                                     const mu_browse_request_t *req, mu_browse_result_t *results,
                                                     size_t max_results, mu_reference_description_t *ref_pool,
                                                     size_t max_total_refs);
#endif

#ifdef MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH
/* Compare two QualifiedNames (OPC-10000-4 §7.30): both the namespace_index
 * and the name string must match. Static address-space nodes do not store a
 * browse_name namespace_index, so callers pass 0 (the default namespace) for
 * the node side. */
static opcua_boolean_t browse_name_equals(opcua_uint16_t node_namespace_index, const mu_string_t *node_name,
                                          opcua_uint16_t target_namespace_index, const mu_string_t *target_name) {
    if (node_namespace_index != target_namespace_index) {
        return false;
    }
    if (node_name->length != target_name->length) {
        return false;
    }
    if (node_name->length <= 0) {
        return node_name->length == 0;
    }
    if (node_name->data == NULL || target_name->data == NULL) {
        return false;
    }
    return memcmp(node_name->data, target_name->data, (size_t)node_name->length) == 0;
}

typedef struct {
    mu_nodeid_t reference_type_id;
    opcua_boolean_t is_inverse;
    opcua_boolean_t include_subtypes;
    opcua_uint16_t target_namespace_index;
    mu_string_t target_name;
} translate_path_element_t;

typedef struct {
    mu_nodeid_t starting_node;
    opcua_int32_t element_count;
    translate_path_element_t elements[MU_DISPATCH_MAX_TRANSLATE_ELEMENTS];
} translate_path_request_t;

/* TranslateBrowsePathsToNodeIds (OPC 10000-4 5.9.4): resolve each RelativePath
   over the static address space and encode BrowsePathResult[] directly. */
opcua_statuscode_t handle_translate_browse_paths(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                                 size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t browse_path_count;
    s = mu_binary_read_int32(r, &browse_path_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (browse_path_count == -1) {
        browse_path_count = 0;
    }
    if (browse_path_count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if ((size_t)browse_path_count > MU_DISPATCH_MAX_TRANSLATE_BROWSE_PATHS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    translate_path_request_t paths[MU_DISPATCH_MAX_TRANSLATE_BROWSE_PATHS];
    size_t path_count = (size_t)browse_path_count;
    for (size_t path_index = 0; path_index < path_count; ++path_index) {
        s = mu_binary_read_nodeid(r, &paths[path_index].starting_node);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        opcua_int32_t element_count;
        s = mu_binary_read_int32(r, &element_count);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (element_count == -1) {
            element_count = 0;
        }
        if (element_count < -1) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        if ((size_t)element_count > MU_DISPATCH_MAX_TRANSLATE_ELEMENTS) {
            return MU_STATUS_BAD_TOOMANYOPERATIONS;
        }
        paths[path_index].element_count = element_count;

        size_t element_total = (size_t)element_count;
        for (size_t element_index = 0; element_index < element_total; ++element_index) {
            translate_path_element_t *element = &paths[path_index].elements[element_index];

            s = mu_binary_read_nodeid(r, &element->reference_type_id);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            mu_binary_read_boolean(r, &element->is_inverse);
            mu_binary_read_boolean(r, &element->include_subtypes);
            mu_binary_read_uint16(r, &element->target_namespace_index);
            if (r->status != MU_STATUS_GOOD) {
                return r->status;
            }
            s = mu_binary_read_string(r, &element->target_name);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
    }

    s = write_response_prefix(w, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, browse_path_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    const mu_address_space_t *address_space = server->config.address_space;
    for (size_t path_index = 0; path_index < path_count; ++path_index) {
        const mu_node_t *current = NULL;
        if (address_space != NULL) {
            current = mu_address_space_find_node(address_space, &server->user_address_space_index,
                                                 &paths[path_index].starting_node);
        }

        size_t element_total = (size_t)paths[path_index].element_count;
        opcua_statuscode_t path_status = MU_STATUS_GOOD;
        for (size_t element_index = 0; element_index < element_total; ++element_index) {
            const translate_path_element_t *element = &paths[path_index].elements[element_index];

            /* T018 (OPC-10000-4 §5.9.4.1): the last RelativePathElement must
             * carry a targetName. Empty/null is "missing" -> Bad_BrowseNameInvalid. */
            if (element_index == element_total - 1U &&
                (element->target_name.length <= 0 || element->target_name.data == NULL)) {
                path_status = MU_STATUS_BAD_BROWSENAMEINVALID;
                current = NULL;
                break;
            }

            if (current != NULL) {
                const mu_node_t *next = NULL;
                for (size_t ref_index = 0; ref_index < current->reference_count; ++ref_index) {
                    const mu_reference_t *ref = &current->references[ref_index];
                    opcua_boolean_t direction_matches = element->is_inverse ? !ref->is_forward : ref->is_forward;
                    opcua_boolean_t type_matches;

                    if (!direction_matches) {
                        continue;
                    }

                    if (element->include_subtypes) {
                        type_matches = ref_type_is_subtype_of(&ref->reference_type_id, &element->reference_type_id);
                    } else {
                        type_matches = mu_nodeid_equal(&ref->reference_type_id, &element->reference_type_id);
                    }
                    if (!type_matches) {
                        continue;
                    }

                    const mu_node_t *target =
                        mu_address_space_find_node(address_space, &server->user_address_space_index, &ref->target_id);
                    if (target == NULL) {
                        continue;
                    }
                    /* T017 (OPC-10000-4 §7.30): QualifiedName match must include
                     * namespace_index. Static nodes implicitly use namespace 0. */
                    if (!browse_name_equals(0, &target->browse_name, element->target_namespace_index,
                                            &element->target_name)) {
                        continue;
                    }

                    next = target;
                    break;
                }
                current = next;
            }
        }

        if (path_status == MU_STATUS_BAD_BROWSENAMEINVALID) {
            mu_binary_write_statuscode(w, MU_STATUS_BAD_BROWSENAMEINVALID);
            mu_binary_write_int32(w, 0);
            if (w->status != MU_STATUS_GOOD) {
                return w->status;
            }
        } else if (current != NULL) {
            mu_binary_write_statuscode(w, MU_STATUS_GOOD);
            mu_binary_write_int32(w, 1);
            if (w->status != MU_STATUS_GOOD) {
                return w->status;
            }
            s = mu_binary_write_nodeid(w, &current->node_id);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            mu_binary_write_uint32(w, 0xFFFFFFFFu);
            if (w->status != MU_STATUS_GOOD) {
                return w->status;
            }
        } else {
            mu_binary_write_statuscode(w, MU_STATUS_BAD_NOMATCH);
            mu_binary_write_int32(w, 0);
            if (w->status != MU_STATUS_GOOD) {
                return w->status;
            }
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH */

#ifdef MUC_OPCUA_CU_VIEW_BASIC_2
/* Browse (OPC 10000-4 5.9.2): decode the request after the RequestHeader, traverse
   references in the address space, and encode the BrowseResponse. */
opcua_statuscode_t handle_browse(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                 size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_browse_request_t breq;
    mu_browse_description_t descs[MU_DISPATCH_MAX_BROWSE_NODES];
    s = mu_browse_request_decode(r, &breq, descs, MU_DISPATCH_MAX_BROWSE_NODES);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_browse_result_t results[MU_DISPATCH_MAX_BROWSE_NODES];
    mu_reference_description_t ref_pool[MU_DISPATCH_MAX_BROWSE_REFS];

#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
    const mu_dynamic_reference_t *dyn_refs = NULL;
    size_t dyn_refs_count = 0;
    dyn_refs = server->dynamic_address_space.references;
    dyn_refs_count = server->dynamic_address_space.references_count;
#endif

    s = mu_browse_process_with_user_index(
        server->config.address_space, &server->user_address_space_index, &server->runtime_base.space,
#ifdef MUC_OPCUA_CU_NODEMANAGEMENT
        dyn_refs, dyn_refs_count,
#endif
        &breq, results, MU_DISPATCH_MAX_BROWSE_NODES, ref_pool, MU_DISPATCH_MAX_BROWSE_REFS);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_BROWSERESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_browse_response_t bresp;
    bresp.results = results;
    bresp.num_results = breq.num_nodes_to_browse;
    s = mu_browse_response_encode(w, &bresp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* BrowseNext (OPC 10000-4 5.9.3, 7.9): this server never creates
   ContinuationPoints, so every supplied point is unknown. */
opcua_statuscode_t handle_browse_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_boolean_t release_continuation_points;
    mu_binary_read_boolean(r, &release_continuation_points);

    opcua_int32_t count;
    mu_binary_read_int32(r, &count);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }
    (void)release_continuation_points;
    if (count == -1) {
        count = 0;
    }
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if ((size_t)count > MU_DISPATCH_MAX_BROWSE_CONTINUATION_POINTS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_bytestring_t continuation_point;
        s = mu_binary_read_bytestring(r, &continuation_point);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = write_response_prefix(w, MU_ID_BROWSENEXTRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < count; ++i) {
        mu_bytestring_t null_continuation_point = {-1, NULL};

        s = mu_binary_write_statuscode(w, MU_STATUS_BAD_CONTINUATIONPOINTINVALID);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_binary_write_bytestring(w, &null_continuation_point);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_binary_write_int32(w, 0);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_CU_VIEW_BASIC_2 */
