/* src/services/browse.c */
#include "browse.h"
#include <stddef.h>

opcua_statuscode_t mu_browse_request_decode(mu_binary_reader_t *reader, 
                                            mu_browse_request_t *req,
                                            mu_browse_description_t *desc_array,
                                            size_t max_desc)
{
    if (!reader || !req || !desc_array) return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t status;
    
    /* ViewDescription */
    status = mu_binary_read_nodeid(reader, &req->view_id);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_read_int64(reader, &req->timestamp);
    if (status != MU_STATUS_GOOD) return status;
    status = mu_binary_read_uint32(reader, &req->view_version);
    if (status != MU_STATUS_GOOD) return status;
    
    /* RequestedMaxReferencesPerNode */
    status = mu_binary_read_uint32(reader, &req->requested_max_references_per_node);
    if (status != MU_STATUS_GOOD) return status;
    
    /* NodesToBrowse Array Length */
    opcua_int32_t no_of_nodes;
    status = mu_binary_read_int32(reader, &no_of_nodes);
    if (status != MU_STATUS_GOOD) return status;
    
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
        if (status != MU_STATUS_GOOD) return status;
        
        status = mu_binary_read_uint32(reader, &desc->browse_direction);
        if (status != MU_STATUS_GOOD) return status;
        
        status = mu_binary_read_nodeid(reader, &desc->reference_type_id);
        if (status != MU_STATUS_GOOD) return status;
        
        opcua_boolean_t include_subtypes;
        status = mu_binary_read_boolean(reader, &include_subtypes);
        desc->include_subtypes = include_subtypes;
        if (status != MU_STATUS_GOOD) return status;
        
        status = mu_binary_read_uint32(reader, &desc->node_class_mask);
        if (status != MU_STATUS_GOOD) return status;
        
        status = mu_binary_read_uint32(reader, &desc->result_mask);
        if (status != MU_STATUS_GOOD) return status;
    }
    
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_browse_response_encode(mu_binary_writer_t *writer, 
                                             const mu_browse_response_t *resp)
{
    if (!writer || !resp) return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t status;
    
    /* Results array */
    if (resp->num_results == 0 || resp->results == NULL) {
        status = mu_binary_write_int32(writer, 0);
        if (status != MU_STATUS_GOOD) return status;
    } else {
        status = mu_binary_write_int32(writer, (opcua_int32_t)resp->num_results);
        if (status != MU_STATUS_GOOD) return status;
        
        for (size_t i = 0; i < resp->num_results; ++i) {
            mu_browse_result_t *res = &resp->results[i];
            
            status = mu_binary_write_statuscode(writer, res->status_code);
            if (status != MU_STATUS_GOOD) return status;
            
            /* ContinuationPoint: ByteString (null/empty = -1 length) */
            status = mu_binary_write_int32(writer, -1);
            if (status != MU_STATUS_GOOD) return status;
            
            /* References array */
            if (res->num_references == 0 || res->references == NULL) {
                status = mu_binary_write_int32(writer, 0);
                if (status != MU_STATUS_GOOD) return status;
            } else {
                status = mu_binary_write_int32(writer, (opcua_int32_t)res->num_references);
                if (status != MU_STATUS_GOOD) return status;
                
                for (size_t j = 0; j < res->num_references; ++j) {
                    mu_reference_description_t *ref = &res->references[j];
                    
                    status = mu_binary_write_nodeid(writer, &ref->reference_type_id);
                    if (status != MU_STATUS_GOOD) return status;
                    
                    status = mu_binary_write_boolean(writer, ref->is_forward);
                    if (status != MU_STATUS_GOOD) return status;
                    
                    /* NodeId (ExpandedNodeId) */
                    status = mu_binary_write_nodeid(writer, &ref->node_id);
                    if (status != MU_STATUS_GOOD) return status;
                    
                    /* BrowseName (QualifiedName) */
                    status = mu_binary_write_uint16(writer, ref->browse_name_namespace_index);
                    if (status != MU_STATUS_GOOD) return status;
                    mu_string_t browse_name_str = { .length = 0, .data = (opcua_byte_t *)ref->browse_name };
                    if (ref->browse_name) {
                        for (size_t k = 0; ref->browse_name[k] != '\0'; ++k) browse_name_str.length++;
                    }
                    status = mu_binary_write_string(writer, &browse_name_str);
                    if (status != MU_STATUS_GOOD) return status;
                    
                    /* DisplayName (LocalizedText) */
                    opcua_byte_t lt_mask = 2; /* Text present */
                    status = mu_binary_write_byte(writer, lt_mask);
                    if (status != MU_STATUS_GOOD) return status;
                    mu_string_t display_name_str = { .length = 0, .data = (opcua_byte_t *)ref->display_name };
                    if (ref->display_name) {
                        for (size_t k = 0; ref->display_name[k] != '\0'; ++k) display_name_str.length++;
                    }
                    status = mu_binary_write_string(writer, &display_name_str);
                    if (status != MU_STATUS_GOOD) return status;
                    
                    /* NodeClass */
                    status = mu_binary_write_uint32(writer, ref->node_class);
                    if (status != MU_STATUS_GOOD) return status;
                    
                    /* TypeDefinition (ExpandedNodeId) */
                    status = mu_binary_write_nodeid(writer, &ref->type_definition);
                    if (status != MU_STATUS_GOOD) return status;
                }
            }
        }
    }
    
    /* DiagnosticInfos array */
    status = mu_binary_write_int32(writer, 0); /* 0 or -1 means empty */
    if (status != MU_STATUS_GOOD) return status;
    
    return MU_STATUS_GOOD;
}
