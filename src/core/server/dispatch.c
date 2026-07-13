/* src/core/server/dispatch.c */
#include "common.h"

#if MUC_OPCUA_CU_METHOD_SERVER
static void store_method_registration(mu_server_t *server, size_t idx, const mu_nodeid_t *method_id,
                                      mu_method_callback_t callback, void *context, const mu_argument_t *input_args,
                                      size_t input_count, const mu_argument_t *output_args, size_t output_count,
                                      opcua_boolean_t executable) {
    server->registered_methods[idx].method_id = *method_id;
    server->registered_methods[idx].callback = callback;
    server->registered_methods[idx].context = context;
    server->registered_methods[idx].input_args = input_args;
    server->registered_methods[idx].input_count = input_count;
    server->registered_methods[idx].output_args = output_args;
    server->registered_methods[idx].output_count = output_count;
    server->registered_methods[idx].executable = executable;
}

opcua_statuscode_t mu_server_register_method_callback(mu_server_t *server, const mu_nodeid_t *method_id,
                                                      mu_method_callback_t callback, void *context,
                                                      const mu_argument_t *input_args, size_t input_count,
                                                      const mu_argument_t *output_args, size_t output_count,
                                                      opcua_boolean_t executable) {
    if (server == NULL || method_id == NULL || callback == NULL) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }

    /* Re-registration of the same method_id overwrites in place. */
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (mu_nodeid_equal(&server->registered_methods[i].method_id, method_id)) {
            store_method_registration(server, i, method_id, callback, context, input_args, input_count, output_args,
                                      output_count, executable);
            return MU_STATUS_GOOD;
        }
    }

    if (server->registered_method_count >= MU_MAX_REGISTERED_METHODS) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    size_t idx = server->registered_method_count++;
    store_method_registration(server, idx, method_id, callback, context, input_args, input_count, output_args,
                              output_count, executable);
    return MU_STATUS_GOOD;
}
#endif
