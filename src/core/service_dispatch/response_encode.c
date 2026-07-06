#include "common.h"

static opcua_datetime_t resolve_timestamp(const mu_server_t *server) {
#ifdef MUC_OPCUA_TIME_SYNC
    if (server && server->config.time_adapter.get_time) {
        return server->config.time_adapter.get_time(server->config.time_adapter.context);
    }
#endif
    (void)server;
    return 0;
}

opcua_statuscode_t write_response_prefix(mu_binary_writer_t *w, opcua_uint32_t response_type_id,
                                         opcua_uint32_t request_handle, opcua_statuscode_t service_result,
                                         const mu_server_t *server) {
    mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {response_type_id}};
    opcua_statuscode_t s = mu_binary_write_nodeid(w, &type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_response_header_t rh;
    rh.timestamp = resolve_timestamp(server);
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    return mu_response_header_encode(w, &rh);
}

opcua_statuscode_t mu_write_service_fault(opcua_byte_t *buffer, size_t *length, opcua_uint32_t request_handle,
                                          opcua_statuscode_t service_result, const mu_server_t *server) {
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
    rh.timestamp = resolve_timestamp(server);
    rh.request_handle = request_handle;
    rh.service_result = service_result;
    s = mu_response_header_encode(&w, &rh);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *length = w.position;
    return MU_STATUS_GOOD;
}
