#include "micro_opcua/config.h"
#ifdef MICRO_OPCUA_SERVICE_QUERY

#include "micro_opcua/encoding.h"
#include "../services/query.h"

opcua_statuscode_t mu_content_filter_decode(mu_binary_reader_t *reader, mu_content_filter_t *filter,
                                            mu_content_filter_element_t *elements, size_t max_elements,
                                            mu_filter_operand_t *operands, size_t max_operands) {
    opcua_uint32_t count = 0;
    opcua_statuscode_t status = mu_binary_read_uint32(reader, &count);
    if (status != MU_STATUS_GOOD) return status;

    if (count == 0 || count == (opcua_uint32_t)-1) {
        filter->elements = NULL;
        filter->elements_count = 0;
        return MU_STATUS_GOOD;
    }

    if (count > max_elements) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    filter->elements = elements;
    filter->elements_count = count;
    size_t operand_index = 0;

    for (size_t i = 0; i < count; ++i) {
        opcua_uint32_t op = 0;
        status = mu_binary_read_uint32(reader, &op);
        if (status != MU_STATUS_GOOD) return status;
        elements[i].filter_operator = (mu_filter_operator_t)op;

        opcua_uint32_t op_count = 0;
        status = mu_binary_read_uint32(reader, &op_count);
        if (status != MU_STATUS_GOOD) return status;

        if (op_count == 0 || op_count == (opcua_uint32_t)-1) {
            elements[i].filter_operands = NULL;
            elements[i].filter_operands_count = 0;
        } else {
            if (operand_index + op_count > max_operands) {
                return MU_STATUS_BAD_OUTOFMEMORY;
            }
            elements[i].filter_operands = &operands[operand_index];
            elements[i].filter_operands_count = op_count;

            for (size_t j = 0; j < op_count; ++j) {
                mu_nodeid_t type_id;
                size_t len;
                status = mu_binary_read_extension_object_header(reader, &type_id, &len);
                if (status != MU_STATUS_GOOD) return status;

                /* Only ElementOperand and LiteralOperand supported */
                /* ElementOperand = 594, LiteralOperand = 597 */
                if (type_id.identifier.numeric == 594) {
                    operands[operand_index + j].type = MU_FILTEROPERAND_ELEMENT;
                    status = mu_binary_read_uint32(reader, &operands[operand_index + j].operand.element.index);
                } else if (type_id.identifier.numeric == 597) {
                    operands[operand_index + j].type = MU_FILTEROPERAND_LITERAL;
                    status = mu_binary_read_variant(reader, &operands[operand_index + j].operand.literal.value);
                } else {
                    /* Unsupported operand */
                    if (len > 0) {
                        reader->position += len; /* Skip */
                    } else {
                        return MU_STATUS_BAD_NOTSUPPORTED;
                    }
                }
                if (status != MU_STATUS_GOOD) return status;
            }
            operand_index += op_count;
        }
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_query_first_request_decode(mu_binary_reader_t *reader, mu_query_first_request_t *req,
                                                 mu_node_type_description_t *node_types, size_t max_node_types,
                                                 mu_content_filter_element_t *filter_elements, size_t max_filter_elements,
                                                 mu_filter_operand_t *filter_operands, size_t max_filter_operands) {
    opcua_statuscode_t status;
     
    /* ViewDescription */
    mu_nodeid_t view_id;
    status = mu_binary_read_nodeid(reader, &view_id);
    if (status != MU_STATUS_GOOD) return status;
    opcua_int64_t view_timestamp;
    status = mu_binary_read_int64(reader, &view_timestamp);
    if (status != MU_STATUS_GOOD) return status;
    opcua_uint32_t view_version;
    status = mu_binary_read_uint32(reader, &view_version);
    if (status != MU_STATUS_GOOD) return status;

    /* nodeTypes */
    opcua_uint32_t count = 0;
    status = mu_binary_read_uint32(reader, &count);
    if (status != MU_STATUS_GOOD) return status;
    if (count == (opcua_uint32_t)-1) count = 0;
    if (count > max_node_types) return MU_STATUS_BAD_OUTOFMEMORY;
    
    req->node_types = count > 0 ? node_types : NULL;
    req->node_types_count = count;
    for (size_t i = 0; i < count; ++i) {
        status = mu_binary_read_expanded_nodeid(reader, (mu_expanded_nodeid_t*)&node_types[i].type_definition_node); /* Simplified */
        if (status != MU_STATUS_GOOD) return status;
        status = mu_binary_read_boolean(reader, &node_types[i].include_sub_types);
        if (status != MU_STATUS_GOOD) return status;
        opcua_uint32_t data_to_return_count;
        status = mu_binary_read_uint32(reader, &data_to_return_count);
        if (status != MU_STATUS_GOOD) return status;
        if (data_to_return_count > 0 && data_to_return_count != (opcua_uint32_t)-1) {
            for (size_t j = 0; j < data_to_return_count; ++j) {
                return MU_STATUS_BAD_NOTSUPPORTED; /* We don't support DataToReturn yet */
            }
        }
    }

    status = mu_content_filter_decode(reader, &req->filter, filter_elements, max_filter_elements, filter_operands, max_filter_operands);
    if (status != MU_STATUS_GOOD) return status;

    status = mu_binary_read_uint32(reader, &req->max_data_sets_to_return);
    if (status != MU_STATUS_GOOD) return status;

    status = mu_binary_read_uint32(reader, &req->max_references_to_return);
    return status;
}

opcua_statuscode_t mu_query_first_response_encode(mu_binary_writer_t *writer, const mu_query_first_response_t *resp) {
    /* ResponseHeader is handled by dispatch */
    
    /* QueryDataSet[] queryDataSets */
    if (resp->query_data_sets_count == 0) {
        mu_binary_write_uint32(writer, 0xFFFFFFFF);
    } else {
        mu_binary_write_uint32(writer, (opcua_uint32_t)resp->query_data_sets_count);
        for (size_t i = 0; i < resp->query_data_sets_count; ++i) {
            /* QueryDataSet: ExpandedNodeId, TypeDefinitionNode, Variant[] values */
            /* Simplified: we only return the NodeId in values[0] or similar. 
             * Let's check QueryDataSet: ExpandedNodeId nodeId, ExpandedNodeId typeDefinitionNode, Variant[] values
             */
            mu_expanded_nodeid_t enode = { .node_id = resp->query_data_sets[i].node_id, .server_index = 0, .namespace_uri = {0, NULL} };
            mu_binary_write_expanded_nodeid(writer, &enode);
            mu_binary_write_expanded_nodeid(writer, &enode); /* typeDef - dummy for now */
            mu_binary_write_uint32(writer, 0); /* 0 values */
        }
    }

    /* continuationPoint */
    mu_binary_write_string(writer, &resp->continuation_point);

    /* ParsingResult[] parsingResults - null */
    mu_binary_write_uint32(writer, 0xFFFFFFFF);

    /* DiagnosticInfo[] diagnosticInfos - null */
    mu_binary_write_uint32(writer, 0xFFFFFFFF);

    /* ContentFilterResult filterResult - skip fields or write empty */
    /* ContentFilterResult: UInt32 elementResults size -> 0 */
    mu_binary_write_uint32(writer, 0xFFFFFFFF);
    
    /* ContentFilterResult: UInt32 elementDiagnosticInfos size -> 0 */
    mu_binary_write_uint32(writer, 0xFFFFFFFF);

    return writer->status;
}

opcua_statuscode_t mu_query_next_request_decode(mu_binary_reader_t *reader, mu_query_next_request_t *req) {
    opcua_statuscode_t status = mu_binary_read_boolean(reader, &req->release_continuation_point);
    if (status != MU_STATUS_GOOD) return status;
    return mu_binary_read_string(reader, &req->continuation_point);
}

opcua_statuscode_t mu_query_next_response_encode(mu_binary_writer_t *writer, const mu_query_next_response_t *resp) {
    /* Same as QueryFirstResponse mostly */
    if (resp->query_data_sets_count == 0) {
        mu_binary_write_uint32(writer, 0xFFFFFFFF);
    } else {
        mu_binary_write_uint32(writer, (opcua_uint32_t)resp->query_data_sets_count);
        for (size_t i = 0; i < resp->query_data_sets_count; ++i) {
            mu_expanded_nodeid_t enode = { .node_id = resp->query_data_sets[i].node_id, .server_index = 0, .namespace_uri = {0, NULL} };
            mu_binary_write_expanded_nodeid(writer, &enode);
            mu_binary_write_expanded_nodeid(writer, &enode);
            mu_binary_write_uint32(writer, 0);
        }
    }

    mu_binary_write_string(writer, &resp->continuation_point);
    mu_binary_write_uint32(writer, 0xFFFFFFFF); /* parsingResults */
    return writer->status;
}

#endif
