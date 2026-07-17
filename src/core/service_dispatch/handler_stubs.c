#include "common.h"

#ifdef MUC_OPCUA_CU_QUERY
opcua_statuscode_t handle_query_first(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                      size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t status = mu_request_header_decode(r, &req_header);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    mu_query_first_request_t req;
    mu_query_first_response_t resp;

    mu_node_type_description_t node_types[4];
    mu_content_filter_element_t filter_elements[4];
    mu_filter_operand_t filter_operands[8];
    mu_query_data_set_t data_sets[16];

    status = mu_query_first_request_decode(r, &req, node_types, 4, filter_elements, 4, filter_operands, 8);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_query_first_process(server, &req, &resp, data_sets, 16);

    opcua_statuscode_t wstatus = write_response_prefix(w, MU_ID_QUERYFIRSTRESPONSE, req_header.request_handle, status
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                                       ,
                                                       server
#endif
    );
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }
    wstatus = mu_query_first_response_encode(w, &resp);
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }

    *response_length = w->position;
    return status;
}

opcua_statuscode_t handle_query_next(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                     size_t *response_length) {
    mu_request_header_t req_header;
    opcua_statuscode_t status = mu_request_header_decode(r, &req_header);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    mu_query_next_request_t req;
    mu_query_next_response_t resp;
    mu_query_data_set_t data_sets[16];

    status = mu_query_next_request_decode(r, &req);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_query_next_process(server, &req, &resp, data_sets, 16);

    opcua_statuscode_t wstatus = write_response_prefix(w, MU_ID_QUERYNEXTRESPONSE, req_header.request_handle, status
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                                       ,
                                                       server
#endif
    );
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }
    wstatus = mu_query_next_response_encode(w, &resp);
    if (wstatus != MU_STATUS_GOOD) {
        return wstatus;
    }

    *response_length = w->position;
    return status;
}
#endif

#ifdef MUC_OPCUA_CU_VIEW_REGISTERNODES
opcua_statuscode_t handle_register_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > MU_DISPATCH_MAX_REGISTER_NODES) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    mu_nodeid_t nodes[MU_DISPATCH_MAX_REGISTER_NODES];
    size_t node_count = (size_t)count;
    for (size_t i = 0; i < node_count; ++i) {
        s = mu_binary_read_nodeid(r, &nodes[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = write_response_prefix(w, MU_ID_REGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD
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
    for (size_t i = 0; i < node_count; ++i) {
        s = mu_binary_write_nodeid(w, &nodes[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t handle_unregister_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                           size_t *response_length) {
    (void)server;

    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t count;
    s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (count < 0) {
        count = 0;
    }
    if ((size_t)count > MU_DISPATCH_MAX_REGISTER_NODES) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    size_t node_count = (size_t)count;
    for (size_t i = 0; i < node_count; ++i) {
        mu_nodeid_t ignored;
        s = mu_binary_read_nodeid(r, &ignored);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = write_response_prefix(w, MU_ID_UNREGISTERNODESRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif
