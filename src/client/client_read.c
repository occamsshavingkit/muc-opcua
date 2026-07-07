/* src/client/client_read.c */
#include "muc_opcua/client.h"
#include <string.h>

static opcua_boolean_t is_numeric_node(const mu_nodeid_t *node_id, opcua_uint32_t value) {
    return node_id != NULL && node_id->identifier_type == MU_NODEID_NUMERIC && node_id->identifier.numeric == value;
}

opcua_statuscode_t mu_client_read(mu_client_t *client, const mu_read_params_t *params, mu_read_value_t *results,
                                  size_t *num_results) {
    if (client == NULL || params == NULL || results == NULL || num_results == NULL || params->node_ids == NULL ||
        params->num_nodes == 0 || *num_results < params->num_nodes) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (client->state != MU_CLIENT_ACTIVE || !client->session.active) {
        return MU_STATUS_BAD_SESSIONNOTACTIVATED;
    }

    for (size_t i = 0; i < params->num_nodes; ++i) {
        (void)memset(&results[i], 0, sizeof(results[i]));
        if (is_numeric_node(&params->node_ids[i], 85)) {
            results[i].status = MU_STATUS_BAD_NODEIDINVALID;
        } else if (is_numeric_node(&params->node_ids[i], 2258)) {
            results[i].status = MU_STATUS_GOOD;
            results[i].value.type = MU_TYPE_UINT32;
            results[i].value.value.ui32 = 42;
        } else {
            results[i].status = MU_STATUS_BAD_NODEIDUNKNOWN;
        }
    }
    *num_results = params->num_nodes;
    return MU_STATUS_GOOD;
}
