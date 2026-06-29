#include "micro_opcua/config.h"
#ifdef MICRO_OPCUA_SERVICE_QUERY

#include "query.h"
#include "micro_opcua/address_space.h"
#include "../core/server_internal.h"
#include <string.h>

static opcua_boolean_t mu_evaluate_content_filter(mu_server_t *server, const mu_node_t *node, const mu_content_filter_t *filter) {
    (void)server;
    (void)node;
    if (!filter || filter->elements_count == 0) {
        return true; /* Empty filter matches everything */
    }

    /* We only support a very simplified filter matching for now */
    for (size_t i = 0; i < filter->elements_count; ++i) {
        const mu_content_filter_element_t *el = &filter->elements[i];
        if (el->filter_operator == MU_FILTEROPERATOR_OFTYPE) {
            /* If we had full OFTYPE implementation */
        }
    }

    return true; /* Fallback: return everything */
}

opcua_statuscode_t mu_query_first_process(mu_server_t *server, const mu_query_first_request_t *req,
                                          mu_query_first_response_t *resp, mu_query_data_set_t *data_sets, size_t max_data_sets) {
    size_t returned = 0;
    size_t next_index = 0;

    /* Scan address space */
    for (size_t i = 0; i < server->user_address_space_index.count; ++i) {
        opcua_uint16_t idx = server->user_address_space_index.order[i];
        const mu_node_t *node = &server->config.address_space->nodes[idx];
        
        if (mu_evaluate_content_filter(server, node, &req->filter)) {
            if (returned < max_data_sets) {
                if (req->max_data_sets_to_return > 0 && returned >= req->max_data_sets_to_return) {
                    next_index = i;
                    break;
                }
                data_sets[returned].node_id = node->node_id;
                returned++;
            } else {
                /* Exceeded response size limits without max_data_sets_to_return being hit */
                next_index = i;
                break;
            }
        }
    }
#ifdef MICRO_OPCUA_SERVICE_NODEMANAGEMENT
    /* Also scan dynamic address space if we haven't hit the limit */
    if (next_index == 0) {
        for (size_t i = 0; i < server->dynamic_address_space.nodes_count; ++i) {
            const mu_node_t *node = &server->dynamic_address_space.nodes[i];
            if (mu_evaluate_content_filter(server, node, &req->filter)) {
                if (returned < max_data_sets) {
                    if (req->max_data_sets_to_return > 0 && returned >= req->max_data_sets_to_return) {
                        next_index = server->user_address_space_index.count + i;
                        break;
                    }
                    data_sets[returned].node_id = node->node_id;
                    returned++;
                } else {
                    next_index = server->user_address_space_index.count + i;
                    break;
                }
            }
        }
    }
#endif

    resp->query_data_sets = returned > 0 ? data_sets : NULL;
    resp->query_data_sets_count = returned;

    if (next_index > 0) {
        /* Generate continuation point */
        for (size_t i = 0; i < MU_MAX_QUERY_CONTINUATION_POINTS; ++i) {
            if (server->query_context.continuation_points[i].id.data == NULL) {
                /* Let's skip CP string formatting and just return the index encoded in binary! */
                server->query_context.continuation_points[i].next_index = next_index;
                server->query_context.continuation_points[i].id.data = server->query_context.continuation_points[i].id_buf;
                server->query_context.continuation_points[i].id.length = sizeof(server->query_context.continuation_points[i].id_buf);
                memset(server->query_context.continuation_points[i].id_buf, 0, 8);
                server->query_context.continuation_points[i].id_buf[0] = next_index & 0xFF;
                server->query_context.continuation_points[i].id_buf[1] = (next_index >> 8) & 0xFF;
                
                resp->continuation_point = server->query_context.continuation_points[i].id;
                break;
            }
        }
    } else {
        resp->continuation_point.length = -1;
        resp->continuation_point.data = NULL;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_query_next_process(mu_server_t *server, const mu_query_next_request_t *req,
                                         mu_query_next_response_t *resp, mu_query_data_set_t *data_sets, size_t max_data_sets) {
    (void)server;
    (void)data_sets;
    (void)max_data_sets;
    if (req->release_continuation_point) {
        return MU_STATUS_GOOD;
    }

    /* Dummy response */
    resp->query_data_sets = NULL;
    resp->query_data_sets_count = 0;
    resp->continuation_point.length = -1;
    resp->continuation_point.data = NULL;

    return MU_STATUS_GOOD;
}

#endif /* MICRO_OPCUA_SERVICE_QUERY */
