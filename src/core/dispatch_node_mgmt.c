/* src/core/dispatch_node_mgmt.c
 * NodeManagement service-set handlers extracted from service_dispatch.c (T013).
 * Implements AddNodes (OPC-10000-4 5.8.2), AddReferences (5.8.3),
 * DeleteNodes (5.8.4), and DeleteReferences (5.8.5). These handlers compile
 * under MUC_OPCUA_SERVICE_NODEMANAGEMENT; the dispatch table in
 * service_dispatch.c references them by name. */
#include "../services/node_management.h"
#include "../services/service_header.h"
#include "muc_opcua/encoding.h"
#include "server_internal.h"
#include "service_dispatch.h"
#include <stddef.h>
#include <string.h>

#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
/* AddNodes (OPC 10000-4 5.8.2): decode the request header, write the response
   prefix, and forward to the shared AddNodes processor in node_management.c. */
opcua_statuscode_t handle_add_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                    size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_ADDNODESRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_add_nodes_process(server, r, w);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* AddReferences (OPC 10000-4 5.8.3). */
opcua_statuscode_t handle_add_references(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                         size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_ADDREFERENCESRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_add_references_process(server, r, w);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* DeleteNodes (OPC 10000-4 5.8.4). */
opcua_statuscode_t handle_delete_nodes(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                       size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_DELETENODESRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_delete_nodes_process(server, r, w);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

/* DeleteReferences (OPC 10000-4 5.8.5). */
opcua_statuscode_t handle_delete_references(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                            size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_DELETEREFERENCESRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_delete_references_process(server, r, w);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SERVICE_NODEMANAGEMENT */
