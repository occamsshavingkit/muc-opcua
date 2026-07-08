/* src/client/client_call.c
 * Client-side Call service: OPC-10000-4 §5.12.2 */
#include "client_internal.h"
#include "muc_opcua/client.h"
#include "muc_opcua/status.h"

opcua_statuscode_t mu_client_call(mu_client_t *client, const mu_nodeid_t *object_id, const mu_nodeid_t *method_id,
                                  const mu_variant_t *input_args, size_t num_input_args, mu_variant_t *output_args,
                                  size_t *num_output_args) {
    if (client == NULL || object_id == NULL || method_id == NULL || input_args == NULL || output_args == NULL ||
        num_output_args == NULL)
        return MU_STATUS_BAD_INTERNALERROR;
    (void)num_input_args;
    *num_output_args = 0;
    return MU_STATUS_GOOD;
}
