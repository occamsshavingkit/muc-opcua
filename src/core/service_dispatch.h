/* src/core/service_dispatch.h */
#ifndef MUC_OPCUA_SERVICE_DISPATCH_H
#define MUC_OPCUA_SERVICE_DISPATCH_H

#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/server.h"
#include <stdbool.h>

/* write_response_prefix()/mu_write_service_fault() take the server instance when
 * either the response timestamp (TIME_SYNC) or the returnDiagnostics echo
 * (BASE_SERVICES_DIAGNOSTICS) is compiled in. Both read per-request state that
 * lives on the server object (server->return_diagnostics), never file-static, to
 * satisfy the zero-mutable-static / 0-B-.bss constitution. */
#if defined(MUC_OPCUA_CU_TIME_SYNC) ||                                                                                 \
    (defined(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS) && MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS)
#define MU_RESPONSE_PREFIX_WANTS_SERVER 1
#endif

typedef struct {
    opcua_uint32_t request_id;
    opcua_uint32_t response_id;
    bool requires_session;
} mu_service_handler_t;

const mu_service_handler_t *mu_get_service_handler(opcua_uint32_t request_id);

/* Encode a ServiceFault response body (response-type NodeId + ResponseHeader with
   the given serviceResult) so the server can always reply on a dispatch failure
   instead of leaving the client to time out. *length is in/out. */
opcua_statuscode_t mu_write_service_fault(opcua_byte_t *buffer, size_t *length, opcua_uint32_t request_handle,
                                          opcua_statuscode_t service_result
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                          ,
                                          const mu_server_t *server
#endif
);

opcua_statuscode_t mu_service_dispatch(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *request_body,
                                       size_t request_length, opcua_byte_t *response_body, size_t *response_length);

#ifdef MUC_OPCUA_SERVICE_WRITE
#include "muc_opcua/encoding.h"
opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length);
#endif

#endif /* MUC_OPCUA_SERVICE_DISPATCH_H */
