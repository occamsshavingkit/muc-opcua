/* src/core/service_dispatch.h */
#ifndef MUC_OPCUA_SERVICE_DISPATCH_H
#define MUC_OPCUA_SERVICE_DISPATCH_H

#include "muc_opcua/opcua_ids.h"
#include "muc_opcua/server.h"
#include <stdbool.h>

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
#ifdef MUC_OPCUA_CU_TIME_SYNC
                                          ,
                                          const mu_server_t *server
#endif
);

opcua_statuscode_t mu_service_dispatch(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *request_body,
                                       size_t request_length, opcua_byte_t *response_body, size_t *response_length);

#ifdef MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
#include "muc_opcua/encoding.h"
opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length);
#endif

#endif /* MUC_OPCUA_SERVICE_DISPATCH_H */
