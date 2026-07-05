/* src/core/server/dispatch.c */
#include "common.h"

#ifdef MUC_OPCUA_CUSTOM_METHODS
opcua_statuscode_t mu_server_register_method_callback(mu_server_t *server, const mu_nodeid_t *method_id,
                                                      mu_method_callback_t callback, void *context) {
    if (server == NULL || method_id == NULL || callback == NULL) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }

    if (server->registered_method_count >= MU_MAX_REGISTERED_METHODS) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    /* Check if already registered */
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (mu_nodeid_equal(&server->registered_methods[i].method_id, method_id)) {
            server->registered_methods[i].callback = callback;
            server->registered_methods[i].context = context;
            return MU_STATUS_GOOD;
        }
    }

    size_t idx = server->registered_method_count++;
    server->registered_methods[idx].method_id = *method_id;
    server->registered_methods[idx].callback = callback;
    server->registered_methods[idx].context = context;

    return MU_STATUS_GOOD;
}
#endif
