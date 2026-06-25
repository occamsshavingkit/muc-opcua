/* src/core/service_dispatch.c */
#include "service_dispatch.h"
#include "server_internal.h"
#include "micro_opcua/encoding.h"
#include "../services/discovery.h"
#include "../services/session.h"
#include "../services/secure_channel.h"
#include "../services/browse.h"
#include "../services/read.h"
#include <stddef.h>

static const mu_service_handler_t g_supported_services[] = {
    { MU_ID_FINDSERVERSREQUEST,        MU_ID_FINDSERVERSRESPONSE,        false },
    { MU_ID_GETENDPOINTSREQUEST,       MU_ID_GETENDPOINTSRESPONSE,       false },
    { MU_ID_OPENSECURECHANNELREQUEST,  MU_ID_OPENSECURECHANNELRESPONSE,  false },
    { MU_ID_CLOSESECURECHANNELREQUEST, MU_ID_CLOSESECURECHANNELRESPONSE, false },
    { MU_ID_CREATESESSIONREQUEST,      MU_ID_CREATESESSIONRESPONSE,      false }, /* Does not require an activated session */
    { MU_ID_ACTIVATESESSIONREQUEST,    MU_ID_ACTIVATESESSIONRESPONSE,    false }, /* Does not require an activated session to activate */
    { MU_ID_CLOSESESSIONREQUEST,       MU_ID_CLOSESESSIONRESPONSE,       true  },
    { MU_ID_BROWSEREQUEST,             MU_ID_BROWSERESPONSE,             true  },
    { MU_ID_READREQUEST,               MU_ID_READRESPONSE,               true  }
};

static const size_t g_num_supported_services = sizeof(g_supported_services) / sizeof(g_supported_services[0]);

const mu_service_handler_t* mu_get_service_handler(opcua_uint32_t request_id) {
    for (size_t i = 0; i < g_num_supported_services; ++i) {
        if (g_supported_services[i].request_id == request_id) {
            return &g_supported_services[i];
        }
    }
    return NULL;
}

opcua_statuscode_t mu_service_dispatch(
    mu_server_t *server, 
    opcua_uint32_t request_id, 
    const opcua_byte_t *request_body, 
    size_t request_length, 
    opcua_byte_t *response_body, 
    size_t *response_length) 
{
    if (!server || !request_body || !response_body || !response_length) return MU_STATUS_BAD_INTERNALERROR;
    (void)request_length;

    const mu_service_handler_t *handler = mu_get_service_handler(request_id);
    if (handler == NULL) {
        return MU_STATUS_BAD_SERVICEUNSUPPORTED;
    }

    /* Handlers to be integrated later */
    
    return MU_STATUS_GOOD;
}
