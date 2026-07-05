/* src/core/dispatch_attribute.c
 * Read and Write attribute service handlers extracted from service_dispatch.c
 * (T010). Implements Read (OPC-10000-4 5.11.2) and Write (5.11.4). Read
 * compiles under MUC_OPCUA_SERVICE_READ; Write under MUC_OPCUA_SERVICE_WRITE. */
#include "../address_space/base_nodes.h"
#include "../services/service_header.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "server_internal.h"
#include "service_dispatch.h"
#ifdef MUC_OPCUA_SERVICE_READ
#include "../services/read.h"
#endif
#ifdef MUC_OPCUA_SERVICE_WRITE
#include "../services/write.h"
#endif
#include <stddef.h>
#include <string.h>

/* Per-request operation bounds (bounded, stack-allocated). Sized so a standards
   client's connect-time batch reads (e.g. the .NET stack reading the ~12
   ServerCapabilities/OperationLimits properties at once) are accepted rather than
   rejected with Bad_TooManyOperations; missing nodes return per-node
   Bad_NodeIdUnknown, which clients tolerate. */
#define MU_DISPATCH_MAX_READ_NODES 32

#ifdef MUC_OPCUA_SERVICE_READ
/* mu_read_process_with_user_index is defined in src/services/read.c. It is not
   declared in read.h (only mu_read_process is) so forward-declare it here for
   the dispatch handler. OPC-10000-4 5.11.2. */
extern opcua_statuscode_t mu_read_process_with_user_index(const mu_address_space_t *address_space,
                                                          mu_address_space_index_t *user_index,
                                                          const mu_address_space_t *dynamic,
                                                          const mu_read_request_t *req, mu_read_response_t *resp,
                                                          mu_datavalue_t *results_array, size_t max_results);
#endif

#ifdef MUC_OPCUA_SERVICE_READ
/* Read (OPC 10000-4 5.11.2): decode the request after the RequestHeader, read each
   attribute from the address space, and encode the ReadResponse. */
opcua_statuscode_t handle_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                               size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_read_request_t rreq;
    mu_read_value_id_t nodes[MU_DISPATCH_MAX_READ_NODES];
    s = mu_read_request_decode(r, &rreq, nodes, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_read_response_t rresp;
    mu_datavalue_t results[MU_DISPATCH_MAX_READ_NODES];
    s = mu_read_process_with_user_index(server->config.address_space, &server->user_address_space_index,
                                        &server->runtime_base.space, &rreq, &rresp, results,
                                        MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_READRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_read_response_encode(w, &rresp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SERVICE_READ */

#ifdef MUC_OPCUA_SERVICE_WRITE
opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_write_request_t wreq;
    mu_write_value_t nodes[MU_DISPATCH_MAX_READ_NODES];
    s = mu_write_request_decode(r, &wreq, nodes, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (wreq.num_nodes_to_write == 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    mu_write_response_t wresp;
    opcua_statuscode_t results[MU_DISPATCH_MAX_READ_NODES];
    wresp.num_results = wreq.num_nodes_to_write;
    wresp.results = results;

    for (size_t i = 0; i < wreq.num_nodes_to_write; ++i) {
        const mu_write_value_t *write_val = &wreq.nodes_to_write[i];
        opcua_statuscode_t result = MU_STATUS_GOOD;

        const mu_node_t *node = mu_resolve_node(server->config.address_space, &server->user_address_space_index,
                                                &server->runtime_base.space, &write_val->node_id);
        if (!node) {
            result = MU_STATUS_BAD_NODEIDUNKNOWN;
        } else if (write_val->attribute_id != MU_ATTRIBUTEID_VALUE) {
            result = MU_STATUS_BAD_NOTWRITABLE;
        } else if (!write_val->value.has_value) {
            /* OPC-10000-4 section 5.11.4.2: Value Attribute writes require DataValue.value. */
            result = MU_STATUS_BAD_TYPEMISMATCH;
        } else if (write_val->index_range.length > 0) {
            result = MU_STATUS_BAD_WRITENOTSUPPORTED;
        } else if (node->node_class != MU_NODECLASS_VARIABLE) {
            result = MU_STATUS_BAD_NOTWRITABLE;
        } else if (node->value) {
            /* Check value type matches if the variable has a current value */
            mu_variant_t current_val;
            s = mu_value_source_read(node->value, &node->node_id, &current_val);
            if (s == MU_STATUS_GOOD && current_val.type != write_val->value.value.type) {
                result = MU_STATUS_BAD_TYPEMISMATCH;
            }
        }

        if (result == MU_STATUS_GOOD) {
            /* Apply write callback if configured */
            mu_write_handler_t write_handler = server->config.write_handler;
            void *write_handler_handle = server->config.write_handler_handle;
            if (write_handler) {
                result = write_handler(write_handler_handle, &write_val->node_id,
                                       (opcua_uint32_t)write_val->attribute_id, &write_val->value.value);
            } else {
                result = MU_STATUS_BAD_WRITENOTSUPPORTED;
            }
        }

        results[i] = result;
    }

    s = write_response_prefix(w, MU_ID_WRITERESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_write_response_encode(w, &wresp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SERVICE_WRITE */
