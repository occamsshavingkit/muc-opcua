/* src/services/browse.c */
#include "browse.h"
#include "../address_space/base_nodes.h"
#include "../core/server_internal.h"
#include <stddef.h>

/* Immediate supertype of a well-known (ns=0) ReferenceType, or 0 at the root.
   Covers the standard hierarchy needed to resolve Browse includeSubtypes
   (OPC 10000-5 ReferenceTypes). */
static opcua_uint32_t ref_type_supertype(opcua_uint32_t id) {
    switch (id) {
    case 32:
    case 33:
        return 31; /* NonHierarchical/Hierarchical -> References */
    case 34:
    case 35:
    case 36:
        return 33; /* HasChild/Organizes/HasEventSource -> HierarchicalReferences */
    case 37:
    case 38:
    case 39:
    case 40:
        return 32; /* HasModellingRule/HasEncoding/HasDescription/HasTypeDefinition -> NonHierarchical */
    case 44:
    case 45:
        return 34; /* Aggregates/HasSubtype -> HasChild */
    case 46:
    case 47:
        return 44; /* HasProperty/HasComponent -> Aggregates */
    case 48:
        return 36; /* HasNotifier -> HasEventSource */
    default:
        return 0;
    }
}

opcua_boolean_t ref_type_is_subtype_of(const mu_nodeid_t *child, const mu_nodeid_t *parent) {
    if (mu_nodeid_equal(child, parent))
        return true;
    if (child->identifier_type != MU_NODEID_NUMERIC || child->namespace_index != 0)
        return false;
    if (parent->identifier_type != MU_NODEID_NUMERIC || parent->namespace_index != 0)
        return false;
    opcua_uint32_t c = child->identifier.numeric;
    opcua_uint32_t p = parent->identifier.numeric;
    /* The supertype table is static and acyclic, so this climb terminates. The
       iteration bound is defense-in-depth: a future edit that introduced a cycle
       would otherwise hang. 64 far exceeds the ReferenceType hierarchy depth. */
    for (int guard = 0; c != 0 && guard < 64; ++guard) {
        c = ref_type_supertype(c);
        if (c == p)
            return true;
    }
    return false;
}

/* Whether a reference passes the browse description's reference-type filter. */
static opcua_boolean_t reference_type_matches(const mu_browse_description_t *desc, const mu_reference_t *r) {
    mu_nodeid_t any = {0, MU_NODEID_NUMERIC, {0}};
    if (mu_nodeid_equal(&desc->reference_type_id, &any)) {
        return true; /* no filter: all reference types */
    }
    if (desc->include_subtypes) {
        return ref_type_is_subtype_of(&r->reference_type_id, &desc->reference_type_id);
    }
    return mu_nodeid_equal(&desc->reference_type_id, &r->reference_type_id);
}

opcua_statuscode_t mu_browse_request_decode(mu_binary_reader_t *reader, mu_browse_request_t *req,
                                            mu_browse_description_t *desc_array, size_t max_desc) {
    if (!reader || !req || !desc_array)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t status;

    /* ViewDescription */
    status = mu_binary_read_nodeid(reader, &req->view_id);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_read_int64(reader, &req->timestamp);
    if (status != MU_STATUS_GOOD)
        return status;
    status = mu_binary_read_uint32(reader, &req->view_version);
    if (status != MU_STATUS_GOOD)
        return status;

    /* RequestedMaxReferencesPerNode */
    status = mu_binary_read_uint32(reader, &req->requested_max_references_per_node);
    if (status != MU_STATUS_GOOD)
        return status;

    /* NodesToBrowse Array Length */
    opcua_int32_t no_of_nodes;
    status = mu_binary_read_int32(reader, &no_of_nodes);
    if (status != MU_STATUS_GOOD)
        return status;

    if (no_of_nodes < 0) {
        req->num_nodes_to_browse = 0;
        return MU_STATUS_GOOD; /* null array */
    }
    if ((size_t)no_of_nodes > max_desc) {
        return MU_STATUS_BAD_INTERNALERROR; /* Too many items for static array */
    }

    req->num_nodes_to_browse = (size_t)no_of_nodes;
    req->nodes_to_browse = desc_array;

    for (size_t i = 0; i < req->num_nodes_to_browse; ++i) {
        mu_browse_description_t *desc = &req->nodes_to_browse[i];

        status = mu_binary_read_nodeid(reader, &desc->node_id);
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_uint32(reader, &desc->browse_direction);
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_nodeid(reader, &desc->reference_type_id);
        if (status != MU_STATUS_GOOD)
            return status;

        opcua_boolean_t include_subtypes;
        status = mu_binary_read_boolean(reader, &include_subtypes);
        desc->include_subtypes = include_subtypes;
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_uint32(reader, &desc->node_class_mask);
        if (status != MU_STATUS_GOOD)
            return status;

        status = mu_binary_read_uint32(reader, &desc->result_mask);
        if (status != MU_STATUS_GOOD)
            return status;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_browse_response_encode(mu_binary_writer_t *writer, const mu_browse_response_t *resp) {
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
            mu_browse_result_t *res = &resp->results[i];

            status = mu_binary_write_statuscode(writer, res->status_code);
            if (status != MU_STATUS_GOOD)
                return status;

            /* ContinuationPoint: ByteString (null/empty = -1 length) */
            status = mu_binary_write_int32(writer, -1);
            if (status != MU_STATUS_GOOD)
                return status;

            /* References array */
            if (res->num_references == 0 || res->references == NULL) {
                status = mu_binary_write_int32(writer, 0);
                if (status != MU_STATUS_GOOD)
                    return status;
            } else {
                status = mu_binary_write_int32(writer, (opcua_int32_t)res->num_references);
                if (status != MU_STATUS_GOOD)
                    return status;

                for (size_t j = 0; j < res->num_references; ++j) {
                    mu_reference_description_t *ref = &res->references[j];

                    status = mu_binary_write_nodeid(writer, &ref->reference_type_id);
                    if (status != MU_STATUS_GOOD)
                        return status;

                    status = mu_binary_write_boolean(writer, ref->is_forward);
                    if (status != MU_STATUS_GOOD)
                        return status;

                    /* NodeId (ExpandedNodeId) */
                    status = mu_binary_write_nodeid(writer, &ref->node_id);
                    if (status != MU_STATUS_GOOD)
                        return status;

                    /* BrowseName (QualifiedName) */
                    status = mu_binary_write_uint16(writer, ref->browse_name_namespace_index);
                    if (status != MU_STATUS_GOOD)
                        return status;
                    status = mu_binary_write_string(writer, &ref->browse_name);
                    if (status != MU_STATUS_GOOD)
                        return status;

                    /* DisplayName (LocalizedText) */
                    opcua_byte_t lt_mask = 2; /* Text present */
                    status = mu_binary_write_byte(writer, lt_mask);
                    if (status != MU_STATUS_GOOD)
                        return status;
                    status = mu_binary_write_string(writer, &ref->display_name);
                    if (status != MU_STATUS_GOOD)
                        return status;

                    /* NodeClass */
                    status = mu_binary_write_uint32(writer, ref->node_class);
                    if (status != MU_STATUS_GOOD)
                        return status;

                    /* TypeDefinition (ExpandedNodeId) */
                    status = mu_binary_write_nodeid(writer, &ref->type_definition);
                    if (status != MU_STATUS_GOOD)
                        return status;
                }
            }
        }
    }

    /* DiagnosticInfos array */
    status = mu_binary_write_int32(writer, 0); /* 0 or -1 means empty */
    if (status != MU_STATUS_GOOD)
        return status;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_browse_process_with_user_index(const mu_address_space_t *address_space,
                                                     const mu_address_space_index_t *user_index,
                                                     const mu_address_space_t *dynamic,
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
                                                     const mu_dynamic_reference_t *dyn_refs, size_t dyn_refs_count,
#endif
                                                     const mu_browse_request_t *req, mu_browse_result_t *results,
                                                     size_t max_results, mu_reference_description_t *ref_pool,
                                                     size_t max_total_refs) {
    if (!req || !results || max_results < req->num_nodes_to_browse)
        return MU_STATUS_BAD_INTERNALERROR;

    size_t refs_used = 0;

    for (size_t i = 0; i < req->num_nodes_to_browse; ++i) {
        mu_browse_description_t *desc = &req->nodes_to_browse[i];
        mu_browse_result_t *res = &results[i];

        res->status_code = MU_STATUS_GOOD;
        res->num_references = 0;
        res->references = NULL;

        /* OPC-10000-4 sections 5.9.2.4 and 7.38.2 */
        if (desc->browse_direction > MU_BROWSE_DIRECTION_BOTH) {
            res->status_code = MU_STATUS_BAD_BROWSEDIRECTIONINVALID;
            continue;
        }

        const mu_node_t *node =
            mu_resolve_node(address_space, (mu_address_space_index_t *)user_index, dynamic, &desc->node_id);
        if (!node) {
            res->status_code = MU_STATUS_BAD_NODEIDUNKNOWN;
            continue;
        }

        size_t node_refs_start = refs_used;
        size_t match_count = 0;
        opcua_boolean_t too_many_refs = false;
        size_t refs_remaining = max_total_refs - node_refs_start;

        for (size_t j = 0; j < node->reference_count; ++j) {
            const mu_reference_t *r = &node->references[j];

            if (desc->browse_direction == MU_BROWSE_DIRECTION_FORWARD && !r->is_forward)
                continue;
            if (desc->browse_direction == MU_BROWSE_DIRECTION_INVERSE && r->is_forward)
                continue;

            if (!reference_type_matches(desc, r))
                continue;

            const mu_node_t *target =
                mu_resolve_node(address_space, (mu_address_space_index_t *)user_index, dynamic, &r->target_id);
            if (!target)
                continue;

            if (desc->node_class_mask != 0) {
                if (!((opcua_uint32_t)target->node_class & desc->node_class_mask))
                    continue;
            }

            if (req->requested_max_references_per_node > 0 && match_count >= req->requested_max_references_per_node) {
                too_many_refs = true;
                break;
            }

            if (match_count >= refs_remaining) {
                too_many_refs = true;
                break;
            }

            mu_reference_description_t *ref_desc = &ref_pool[node_refs_start + match_count++];
            ref_desc->reference_type_id = r->reference_type_id;
            ref_desc->is_forward = r->is_forward;
            ref_desc->node_id = target->node_id;
            ref_desc->browse_name_namespace_index = 0;
            ref_desc->browse_name = target->browse_name;
            ref_desc->display_name = target->display_name;
            ref_desc->node_class = target->node_class;

            /* Find HasTypeDefinition if present */
            ref_desc->type_definition =
                (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};
            for (size_t k = 0; k < target->reference_count; ++k) {
                if (target->references[k].is_forward &&
                    target->references[k].reference_type_id.identifier_type == MU_NODEID_NUMERIC &&
                    target->references[k].reference_type_id.identifier.numeric == 40) { /* HasTypeDefinition */
                    ref_desc->type_definition = target->references[k].target_id;
                    break;
                }
            }
        }
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
        /* Also iterate over dynamic references */
        if (dyn_refs && dyn_refs_count > 0) {
            for (size_t j = 0; j < dyn_refs_count; ++j) {
                const mu_dynamic_reference_t *dyn = &dyn_refs[j];

                if (!mu_nodeid_equal(&dyn->source_node_id, &desc->node_id))
                    continue;

                const mu_reference_t *r = &dyn->ref;

                if (desc->browse_direction == MU_BROWSE_DIRECTION_FORWARD && !r->is_forward)
                    continue;
                if (desc->browse_direction == MU_BROWSE_DIRECTION_INVERSE && r->is_forward)
                    continue;

                if (!reference_type_matches(desc, r))
                    continue;

                const mu_node_t *target =
                    mu_resolve_node(address_space, (mu_address_space_index_t *)user_index, dynamic, &r->target_id);
                if (!target)
                    continue;

                if (desc->node_class_mask != 0) {
                    if (!((opcua_uint32_t)target->node_class & desc->node_class_mask))
                        continue;
                }

                if (req->requested_max_references_per_node > 0 &&
                    match_count >= req->requested_max_references_per_node) {
                    too_many_refs = true;
                    break;
                }

                if (match_count >= refs_remaining) {
                    too_many_refs = true;
                    break;
                }

                mu_reference_description_t *ref_desc = &ref_pool[node_refs_start + match_count++];
                ref_desc->reference_type_id = r->reference_type_id;
                ref_desc->is_forward = r->is_forward;
                ref_desc->node_id = target->node_id;
                ref_desc->browse_name_namespace_index = 0;
                ref_desc->browse_name = target->browse_name;
                ref_desc->display_name = target->display_name;
                ref_desc->node_class = target->node_class;

                ref_desc->type_definition =
                    (mu_nodeid_t){.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};
                /* Optional: check target's references for TypeDefinition (static and dynamic) */
            }
        }
#endif

        if (too_many_refs) {
            res->status_code = MU_STATUS_BAD_NOCONTINUATIONPOINTS; /* Or Bad_ResponseTooLarge */
            continue;
        }

        res->references = &ref_pool[node_refs_start];
        res->num_references = match_count;
        refs_used = node_refs_start + match_count;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_browse_process(const mu_address_space_t *address_space, const mu_address_space_t *dynamic,
                                     const mu_browse_request_t *req, mu_browse_result_t *results, size_t max_results,
                                     mu_reference_description_t *ref_pool, size_t max_total_refs) {
    return mu_browse_process_with_user_index(address_space, NULL, dynamic,
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
                                             NULL, 0,
#endif
                                             req, results, max_results, ref_pool, max_total_refs);
}
