/* src/core/dispatch_method.c
 * Call service handler extracted from service_dispatch.c (T014).
 * Implements Call (OPC-10000-4 §5.12.2.2), limited to the OPC-10000-5
 * Base Info methods GetMonitoredItems (§9.1) and ResendData (§9.2) on the
 * Server object. The dispatch table in service_dispatch.c references this
 * handler by name; its extern prototype lives in server_internal.h.
 *
 * Guard note: the original guard in service_dispatch.c was the local macro
 * MU_DISPATCH_CALL_ENABLED (== MUC_OPCUA_SUBSCRIPTIONS &&
 * MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM). That
 * underlying condition is reproduced verbatim here; the tasks.md token
 * "MUC_OPCUA_METHOD_CALL" is not defined anywhere in the codebase, so using
 * it would silently exclude the handler and break the dispatch table. */
#include "../services/service_header.h"
#include "../services/subscription.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "server_internal.h"
#include "service_dispatch.h"
#include <stddef.h>
#include <string.h>

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM
#define MU_DISPATCH_CALL_ENABLED 1

#define MU_DISPATCH_MAX_CALL_METHODS 8
#define MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS 4

#define MU_ID_SERVER_OBJECT 2253u
#define MU_ID_SERVER_GETMONITOREDITEMS 11492u
#define MU_ID_SERVER_RESENDDATA 12873u

static bool nodeid_is_ns0_numeric(const mu_nodeid_t *node_id, opcua_uint32_t numeric_id) {
    return node_id->identifier_type == MU_NODEID_NUMERIC && node_id->namespace_index == 0u &&
           node_id->identifier.numeric == numeric_id;
}

static opcua_statuscode_t write_call_method_result(mu_binary_writer_t *w, opcua_statuscode_t status,
                                                   opcua_int32_t input_argument_result_count,
                                                   const opcua_statuscode_t *input_argument_results,
                                                   opcua_int32_t output_argument_count,
                                                   const mu_variant_t *output_arguments) {
    opcua_statuscode_t s = mu_binary_write_statuscode(w, status);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, input_argument_result_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < input_argument_result_count; ++i) {
        s = mu_binary_write_statuscode(w, input_argument_results[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_write_int32(w, output_argument_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    for (opcua_int32_t i = 0; i < output_argument_count; ++i) {
        s = mu_binary_write_variant(w, &output_arguments[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_call_input_arguments(mu_binary_reader_t *r, mu_variant_t *args,
                                                    opcua_int32_t *arg_count) {
    opcua_int32_t count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (count > MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    if (count <= 0) {
        *arg_count = 0;
        return MU_STATUS_GOOD;
    }

    for (opcua_int32_t i = 0; i < count; ++i) {
        memset(&args[i], 0, sizeof(args[i]));
        s = mu_binary_read_variant(r, &args[i]);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    *arg_count = count;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t write_get_monitored_items_result(mu_server_t *server, mu_binary_writer_t *w,
                                                           opcua_uint32_t subscription_id) {
    opcua_uint32_t server_handles[MU_MAX_MONITORED_ITEMS];
    opcua_uint32_t client_handles[MU_MAX_MONITORED_ITEMS];
    size_t handle_count = 0u;
    opcua_statuscode_t result =
        mu_subscription_get_monitored_items(&server->subs, server->active_session->session_id, subscription_id,
                                            server_handles, client_handles, MU_MAX_MONITORED_ITEMS, &handle_count);
    if (result != MU_STATUS_GOOD) {
        return write_call_method_result(w, result, 0, NULL, 0, NULL);
    }

    mu_variant_t outputs[2];
    memset(outputs, 0, sizeof(outputs));
    outputs[0].type = MU_TYPE_UINT32;
    outputs[0].is_array = true;
    outputs[0].array_length = (opcua_int32_t)handle_count;
    outputs[0].value.array = server_handles;
    outputs[1].type = MU_TYPE_UINT32;
    outputs[1].is_array = true;
    outputs[1].array_length = (opcua_int32_t)handle_count;
    outputs[1].value.array = client_handles;

    return write_call_method_result(w, MU_STATUS_GOOD, 0, NULL, 2, outputs);
}

static opcua_statuscode_t write_resend_data_result(mu_server_t *server, mu_binary_writer_t *w,
                                                   opcua_uint32_t subscription_id) {
    opcua_statuscode_t result =
        mu_subscription_request_resend_data(&server->subs, server->active_session->session_id, subscription_id);
    return write_call_method_result(w, result, 0, NULL, 0, NULL);
}

static opcua_statuscode_t write_single_call_method_result(mu_server_t *server, mu_binary_writer_t *w,
                                                          const mu_nodeid_t *object_id, const mu_nodeid_t *method_id,
                                                          const mu_variant_t *args, opcua_int32_t arg_count) {
    opcua_statuscode_t input_result = MU_STATUS_BAD_INVALIDARGUMENT;

    if (nodeid_is_ns0_numeric(method_id, MU_ID_SERVER_GETMONITOREDITEMS) ||
        nodeid_is_ns0_numeric(method_id, MU_ID_SERVER_RESENDDATA)) {
        if (!nodeid_is_ns0_numeric(object_id, MU_ID_SERVER_OBJECT)) {
            return write_call_method_result(w, MU_STATUS_BAD_NODEIDINVALID, 0, NULL, 0, NULL);
        }
        if (arg_count <= 0) {
            return write_call_method_result(w, MU_STATUS_BAD_ARGUMENTSMISSING, 0, NULL, 0, NULL);
        }
        if (arg_count > 1) {
            return write_call_method_result(w, MU_STATUS_BAD_TOOMANYARGUMENTS, 0, NULL, 0, NULL);
        }
        if (args[0].is_array || args[0].type != MU_TYPE_UINT32) {
            return write_call_method_result(w, MU_STATUS_BAD_INVALIDARGUMENT, 1, &input_result, 0, NULL);
        }

        if (nodeid_is_ns0_numeric(method_id, MU_ID_SERVER_GETMONITOREDITEMS)) {
            return write_get_monitored_items_result(server, w, args[0].value.ui32);
        }

        return write_resend_data_result(server, w, args[0].value.ui32);
    }

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
    if (nodeid_is_ns0_numeric(method_id, 9111) || nodeid_is_ns0_numeric(method_id, 9113) ||
        nodeid_is_ns0_numeric(method_id, 9069)) {
        mu_variant_t output_args[2];
        size_t output_args_count = 0;
        opcua_statuscode_t alarms_status = mu_alarms_conditions_method_dispatch(server, method_id, object_id, arg_count,
                                                                                args, &output_args_count, output_args);
        return write_call_method_result(w, alarms_status, 0, NULL, output_args_count, output_args);
    }
#endif

#ifdef MUC_OPCUA_CUSTOM_METHODS
    /* Verify object exists in address space */
    const mu_node_t *obj_node =
        mu_address_space_find_node(server->config.address_space, &server->user_address_space_index, object_id);
    if (obj_node == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_NODEIDINVALID, 0, NULL, 0, NULL);
    }

    /* Verify method node exists in address space */
    const mu_node_t *method_node =
        mu_address_space_find_node(server->config.address_space, &server->user_address_space_index, method_id);
    if (method_node == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
    }

    /* Lookup registered callback */
    mu_method_callback_t callback = NULL;
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (mu_nodeid_equal(&server->registered_methods[i].method_id, method_id)) {
            callback = server->registered_methods[i].callback;
            break;
        }
    }

    if (callback == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
    }

    /* Invoke the custom callback */
    mu_variant_t output_args[8];
    size_t output_args_count = 8;
    memset(output_args, 0, sizeof(output_args));
    opcua_statuscode_t handler_status =
        callback(server, object_id, method_id, args, (size_t)arg_count, output_args, &output_args_count);
    if (handler_status != MU_STATUS_GOOD) {
        return write_call_method_result(w, handler_status, 0, NULL, 0, NULL);
    }

    return write_call_method_result(w, MU_STATUS_GOOD, 0, NULL, (opcua_int32_t)output_args_count, output_args);
#else
    return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
#endif
}

/* Call (OPC-10000-4 §5.12.2.2), limited to OPC-10000-5 Base Info methods
   GetMonitoredItems (§9.1) and ResendData (§9.2) on the Server object. */
opcua_statuscode_t handle_call(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                               size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    opcua_int32_t method_count;
    s = mu_binary_read_int32(r, &method_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (method_count < -1) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (method_count <= 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }
    if ((size_t)method_count > MU_DISPATCH_MAX_CALL_METHODS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    s = write_response_prefix(w, MU_ID_CALLRESPONSE, req.request_handle, MU_STATUS_GOOD);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_binary_write_int32(w, method_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    for (opcua_int32_t i = 0; i < method_count; ++i) {
        mu_nodeid_t object_id;
        mu_nodeid_t method_id;
        mu_variant_t args[MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS];
        opcua_int32_t arg_count = 0;

        s = mu_binary_read_nodeid(r, &object_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = mu_binary_read_nodeid(r, &method_id);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        s = read_call_input_arguments(r, args, &arg_count);
        if (s != MU_STATUS_GOOD) {
            return s;
        }

        s = write_single_call_method_result(server, w, &object_id, &method_id, args, arg_count);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
    }

    s = mu_binary_write_int32(w, 0);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM */
