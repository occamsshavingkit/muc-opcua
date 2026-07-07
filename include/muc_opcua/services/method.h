/* include/muc_opcua/services/method.h
 *
 * Method Server Facet: arbitrary method callback registration API.
 * OPC-10000-4 §5.12.2.2 (Call service).
 */
#ifndef MUC_OPCUA_SERVICES_METHOD_H
#define MUC_OPCUA_SERVICES_METHOD_H

#include "muc_opcua/address_space.h"
#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MUC_OPCUA_METHOD_SERVER

struct mu_server;

typedef struct {
    mu_nodeid_t object_id;
    mu_nodeid_t method_id;
    mu_variant_t *input_arguments;
    size_t input_count;
    mu_variant_t *output_arguments;
    size_t output_count;
    /* Per-input-argument status codes returned to the client */
    opcua_statuscode_t *input_argument_results;
} mu_method_call_t;

#define MU_MAX_METHOD_REGISTRATIONS 16

opcua_statuscode_t mu_method_server_register(struct mu_server *server, const mu_nodeid_t *method_id,
                                             mu_method_callback_t callback);

opcua_statuscode_t mu_method_server_dispatch(struct mu_server *server, const mu_nodeid_t *object_id,
                                             const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                             size_t input_count, mu_variant_t *output_args, size_t *output_count,
                                             opcua_statuscode_t *input_results);

#endif /* MUC_OPCUA_METHOD_SERVER */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_METHOD_H */
