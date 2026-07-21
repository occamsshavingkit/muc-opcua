/* src/core/dispatch_method.c
 * Call service handler extracted from service_dispatch.c (T014).
 * Implements Call (OPC-10000-4 §5.12.2.2), limited to the OPC-10000-5
 * Base Info methods GetMonitoredItems (§9.1) and ResendData (§9.2) on the
 * Server object. The dispatch table in service_dispatch.c references this
 * handler by name; its extern prototype lives in server_internal.h.
 *
 * Guard note: the original guard in service_dispatch.c was the local macro
 * MU_DISPATCH_CALL_ENABLED (== MUC_OPCUA_CU_SUBSCRIPTION_BASIC &&
 * MUC_OPCUA_CU_SUBSCRIPTION_STANDARD && MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER). That
 * underlying condition is reproduced verbatim here; the tasks.md token
 * "MUC_OPCUA_METHOD_CALL" is not defined anywhere in the codebase, so using
 * it would silently exclude the handler and break the dispatch table. */
#include "core/server_internal.h"
#include "core/service_dispatch.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/services/audit.h"
#include "muc_opcua/services/method.h"
#include "services/service_header.h"
#include "services/subscription.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if MUC_OPCUA_CU_SUBSCRIPTION_BASIC && MUC_OPCUA_CU_SUBSCRIPTION_STANDARD && MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
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

#if MUC_OPCUA_CU_METHOD_SERVER
/* Validate one input argument against its declared Argument (OPC-10000-4 §5.11.2).
   Builtin DataType NodeIds (ns0 numeric) equal the mu_builtin_type_t enum values,
   so a scalar type-check is a numeric compare; non-ns0/complex declared types are
   not type-checked here (documented subset). value_rank: -1 scalar, >=1 array. */
static opcua_statuscode_t validate_call_argument(const mu_variant_t *arg, const mu_argument_t *decl) {
    bool wants_array = decl->value_rank >= 1;
    if ((bool)arg->is_array != wants_array) {
        return MU_STATUS_BAD_TYPEMISMATCH;
    }
    if (decl->data_type.identifier_type == MU_NODEID_NUMERIC && decl->data_type.namespace_index == 0u &&
        decl->data_type.identifier.numeric != 0u) {
        if ((opcua_uint32_t)arg->type != decl->data_type.identifier.numeric) {
            return MU_STATUS_BAD_TYPEMISMATCH;
        }
    }
    return MU_STATUS_GOOD;
}
#endif

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
    opcua_uint32_t server_handles[MU_INTERN_MAX_MONITORED_ITEMS];
    opcua_uint32_t client_handles[MU_INTERN_MAX_MONITORED_ITEMS];
    size_t handle_count = 0u;
    opcua_statuscode_t result = mu_subscription_get_monitored_items(&server->subs, server->active_session->session_id,
                                                                    subscription_id, server_handles, client_handles,
                                                                    MU_INTERN_MAX_MONITORED_ITEMS, &handle_count);
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

#if MUC_OPCUA_CU_METHOD_SERVER
    /* Object must exist. §5.11: a NodeId that is well-formed but not in the address
       space is Bad_NodeIdUnknown (Bad_NodeIdInvalid is for a malformed NodeId). */
    const mu_node_t *obj_node =
        mu_address_space_find_node(server->config.address_space, &server->user_address_space_index, object_id);
    if (obj_node == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_NODEIDUNKNOWN, 0, NULL, 0, NULL);
    }

    /* Method node must exist. */
    const mu_node_t *method_node =
        mu_address_space_find_node(server->config.address_space, &server->user_address_space_index, method_id);
    if (method_node == NULL) {
        return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
    }

    /* Locate the registration for this method. */
    size_t reg_index = server->registered_method_count;
    for (size_t i = 0; i < server->registered_method_count; ++i) {
        if (mu_nodeid_equal(&server->registered_methods[i].method_id, method_id)) {
            reg_index = i;
            break;
        }
    }
    if (reg_index == server->registered_method_count) {
        return write_call_method_result(w, MU_STATUS_BAD_METHODINVALID, 0, NULL, 0, NULL);
    }

    /* Executable attribute (OPC-10000-4 §5.11): a non-executable method is rejected. */
    if (!server->registered_methods[reg_index].executable) {
        return write_call_method_result(w, MU_STATUS_BAD_NOTEXECUTABLE, 0, NULL, 0, NULL);
    }

    /* Argument validation against the declared signature (§5.11.2). Skipped when
       the method registered no input signature (input_args == NULL). */
    const mu_argument_t *in_sig = server->registered_methods[reg_index].input_args;
    size_t in_declared = server->registered_methods[reg_index].input_count;
    if (in_sig != NULL) {
        if ((size_t)arg_count < in_declared) {
            return write_call_method_result(w, MU_STATUS_BAD_ARGUMENTSMISSING, 0, NULL, 0, NULL);
        }
        if ((size_t)arg_count > in_declared) {
            return write_call_method_result(w, MU_STATUS_BAD_TOOMANYARGUMENTS, 0, NULL, 0, NULL);
        }
        opcua_statuscode_t in_results[MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS];
        bool any_bad = false;
        for (opcua_int32_t i = 0; i < arg_count; ++i) {
            in_results[i] = validate_call_argument(&args[i], &in_sig[i]);
            if (in_results[i] != MU_STATUS_GOOD) {
                any_bad = true;
            }
        }
        if (any_bad) {
            return write_call_method_result(w, MU_STATUS_BAD_INVALIDARGUMENT, arg_count, in_results, 0, NULL);
        }
    }

    /* Invoke the callback, delivering its registered context. */
    mu_variant_t output_args[8];
    size_t output_args_count = 8;
    memset(output_args, 0, sizeof(output_args));
    opcua_statuscode_t handler_status = server->registered_methods[reg_index].callback(
        server, server->registered_methods[reg_index].context, object_id, method_id, args, (size_t)arg_count,
        output_args, &output_args_count);
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

    s = write_response_prefix(w, MU_ID_CALLRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MU_RESPONSE_PREFIX_WANTS_SERVER
                              ,
                              server
#endif
    );
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
        memset(args, 0, sizeof(args));

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
#if MUC_OPCUA_ALLOW_HEAP
            for (opcua_int32_t ai = 0; ai < MU_DISPATCH_MAX_CALL_INPUT_ARGUMENTS; ++ai) {
                if (args[ai].is_array && args[ai].value.array != NULL) {
                    free((void *)args[ai].value.array);
                }
            }
#endif
            return s;
        }

        s = write_single_call_method_result(server, w, &object_id, &method_id, args, arg_count);
#if MUC_OPCUA_CU_AUDITING
        {
            mu_audit_event_t audit_ev;
            (void)memset(&audit_ev, 0, sizeof(audit_ev));
            audit_ev.event_type = MU_AUDIT_EVENT_METHOD;
            audit_ev.status = (s == MU_STATUS_GOOD);
            audit_ev.specific.method.object_id = object_id;
            audit_ev.specific.method.method_id = method_id;
            mu_raise_audit_event(server, &audit_ev);
        }
#endif
#if MUC_OPCUA_ALLOW_HEAP
        /* Release heap-decoded array arguments (only allocated when heap is on). */
        for (opcua_int32_t ai = 0; ai < arg_count; ++ai) {
            if (args[ai].is_array && args[ai].value.array != NULL) {
                free((void *)args[ai].value.array);
            }
        }
#endif
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

#endif /* MUC_OPCUA_CU_SUBSCRIPTION_BASIC && MUC_OPCUA_CU_SUBSCRIPTION_STANDARD &&                                     \
          MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER */
