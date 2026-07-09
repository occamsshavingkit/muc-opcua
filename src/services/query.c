#include "muc_opcua/config.h"
#ifdef MUC_OPCUA_SERVICE_QUERY

#include "../core/server_internal.h"
#include "muc_opcua/address_space.h"
#include "query.h"
#include <string.h>

static opcua_boolean_t mu_evaluate_content_filter(mu_server_t *server, const mu_node_t *node,
                                                  const mu_content_filter_t *filter) {
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

static void mu_query_next_set_empty_response(mu_query_next_response_t *resp) {
    resp->query_data_sets = NULL;
    resp->query_data_sets_count = 0;
    resp->continuation_point.length = -1;
    resp->continuation_point.data = NULL;
}

static opcua_boolean_t mu_query_continuation_point_supplied(const mu_string_t *continuation_point) {
    return continuation_point != NULL && continuation_point->length > 0;
}

static opcua_boolean_t mu_query_continuation_point_matches(const mu_string_t *left, const mu_string_t *right) {
    if (!mu_query_continuation_point_supplied(left) || !mu_query_continuation_point_supplied(right)) {
        return false;
    }
    if (left->data == NULL || right->data == NULL || left->length != right->length) {
        return false;
    }

    return memcmp(left->data, right->data, (size_t)left->length) == 0;
}

static opcua_boolean_t mu_query_find_continuation_point(mu_server_t *server, const mu_string_t *continuation_point,
                                                        size_t *slot_index) {
    for (size_t i = 0; i < MU_INTERN_MAX_QUERY_CONTINUATION_POINTS; ++i) {
        if (mu_query_continuation_point_matches(&server->query_context.continuation_points[i].id, continuation_point)) {
            if (slot_index != NULL) {
                *slot_index = i;
            }
            return true;
        }
    }

    return false;
}

static void mu_query_release_continuation_point(mu_server_t *server, size_t slot_index) {
    server->query_context.continuation_points[slot_index].id.length = -1;
    server->query_context.continuation_points[slot_index].id.data = NULL;
    server->query_context.continuation_points[slot_index].session_id = 0;
    server->query_context.continuation_points[slot_index].next_index = 0;
    server->query_context.continuation_points[slot_index].timestamp_ms = 0;
}

opcua_statuscode_t mu_query_first_process(mu_server_t *server, const mu_query_first_request_t *req,
                                          mu_query_first_response_t *resp, mu_query_data_set_t *data_sets,
                                          size_t max_data_sets) {
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
#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
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
        for (size_t i = 0; i < MU_INTERN_MAX_QUERY_CONTINUATION_POINTS; ++i) {
            if (server->query_context.continuation_points[i].id.data == NULL) {
                /* Let's skip CP string formatting and just return the index encoded in binary! */
                server->query_context.continuation_points[i].next_index = next_index;
                server->query_context.continuation_points[i].id.data =
                    server->query_context.continuation_points[i].id_buf;
                server->query_context.continuation_points[i].id.length =
                    sizeof(server->query_context.continuation_points[i].id_buf);
                (void)memset(server->query_context.continuation_points[i].id_buf, 0, 8);
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
                                         mu_query_next_response_t *resp, mu_query_data_set_t *data_sets,
                                         size_t max_data_sets) {
    (void)data_sets;
    (void)max_data_sets;

    mu_query_next_set_empty_response(resp);

    if (!mu_query_continuation_point_supplied(&req->continuation_point)) {
        return MU_STATUS_GOOD;
    }

    size_t slot_index = 0;
    if (!mu_query_find_continuation_point(server, &req->continuation_point, &slot_index)) {
        /* OPC-10000-4 sections B.2.4 and 7.9: unknown QueryNext handles are invalid. */
        return MU_STATUS_BAD_CONTINUATIONPOINTINVALID;
    }

    if (req->release_continuation_point) {
        mu_query_release_continuation_point(server, slot_index);
    }

    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_SERVICE_QUERY */
