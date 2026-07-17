#include "common.h"

#ifdef MUC_OPCUA_CU_TIME_SYNC
static opcua_datetime_t resolve_timestamp(const mu_server_t *server) {
    if (server && server->config.time_adapter.get_time) {
        return server->config.time_adapter.get_time(server->config.time_adapter.context);
    }
    return 0;
}
#endif

opcua_statuscode_t write_response_prefix(mu_binary_writer_t *w, opcua_uint32_t response_type_id,
                                         opcua_uint32_t request_handle, opcua_statuscode_t service_result
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                         ,
                                         const mu_server_t *server
#endif
) {
    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {response_type_id}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_response_header_t rh;
#ifdef MUC_OPCUA_CU_TIME_SYNC
    rh.timestamp = resolve_timestamp(server);
#else
    rh.timestamp = 0;
#endif
    rh.request_handle = request_handle;
    rh.service_result = service_result;
#if defined(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS) && MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS
    /* NULL-tolerant like resolve_timestamp(): some internal/test callers encode a
       response prefix without an owning server. */
    rh.return_diagnostics = server ? server->return_diagnostics : 0u;
#else
    rh.return_diagnostics = 0u;
#endif
    return mu_response_header_encode(w, &rh);
}

opcua_statuscode_t mu_write_service_fault(opcua_byte_t *buffer, size_t *length, opcua_uint32_t request_handle,
                                          opcua_statuscode_t service_result
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                                          ,
                                          const mu_server_t *server
#endif
) {
    if (!buffer || !length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buffer, *length);

    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_SERVICEFAULT}};
    opcua_statuscode_t s = mu_binary_write_nodeid(&w, &type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_response_header_t rh;
#ifdef MUC_OPCUA_CU_TIME_SYNC
    rh.timestamp = resolve_timestamp(server);
#else
    rh.timestamp = 0;
#endif
    rh.request_handle = request_handle;
    rh.service_result = service_result;
#if defined(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS) && MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS
    /* NULL-tolerant like resolve_timestamp(): some internal/test callers encode a
       response prefix without an owning server. */
    rh.return_diagnostics = server ? server->return_diagnostics : 0u;
#else
    rh.return_diagnostics = 0u;
#endif
    s = mu_response_header_encode(&w, &rh);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *length = w.position;
    return MU_STATUS_GOOD;
}
