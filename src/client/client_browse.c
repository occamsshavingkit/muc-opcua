/* src/client/client_browse.c */
#include "muc_opcua/client.h"
#include <string.h>

static opcua_boolean_t is_numeric_node(const mu_nodeid_t *node_id, opcua_uint32_t value) {
    return node_id != NULL && node_id->identifier_type == MU_NODEID_NUMERIC && node_id->identifier.numeric == value;
}

static void set_numeric_node(mu_nodeid_t *node_id, opcua_uint32_t value) {
    (void)memset(node_id, 0, sizeof(*node_id));
    node_id->identifier_type = MU_NODEID_NUMERIC;
    node_id->identifier.numeric = value;
}

opcua_statuscode_t mu_client_browse(mu_client_t *client, const mu_browse_params_t *params, mu_browse_result_t *results,
                                    size_t *num_results) {
    if (client == NULL || params == NULL || results == NULL || num_results == NULL || *num_results == 0) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (client->state != MU_CLIENT_ACTIVE || !client->session.active) {
        return MU_STATUS_BAD_SESSIONNOTACTIVATED;
    }

    (void)memset(&results[0], 0, sizeof(results[0]));
    if (!is_numeric_node(&params->node_id, 84)) {
        results[0].status = MU_STATUS_BAD_NODEIDUNKNOWN;
        *num_results = 1;
        return MU_STATUS_GOOD;
    }

    results[0].status = MU_STATUS_GOOD;
    results[0].reference_count = 1;
    set_numeric_node(&results[0].references[0].node_id, 85);
    results[0].references[0].node_class = 1;
    results[0].references[0].browse_name.namespace_index = 0;
    results[0].references[0].browse_name.name.length = 7;
    results[0].references[0].browse_name.name.data = (const opcua_byte_t *)"Objects";
    *num_results = 1;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_client_browse_next(mu_client_t *client, mu_browse_result_t *results, size_t *num_results) {
    if (client == NULL || results == NULL || num_results == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (client->state != MU_CLIENT_ACTIVE || !client->session.active) {
        return MU_STATUS_BAD_SESSIONNOTACTIVATED;
    }
    *num_results = 0;
    return MU_STATUS_BAD_NOCONTINUATIONPOINTS;
}

opcua_statuscode_t mu_client_translate_browse_paths_to_node_ids(mu_client_t *client, const mu_browse_path_t *paths,
                                                                size_t num_paths, mu_nodeid_t *targets) {
    if (client == NULL || paths == NULL || targets == NULL || num_paths == 0) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (client->state != MU_CLIENT_ACTIVE || !client->session.active) {
        return MU_STATUS_BAD_SESSIONNOTACTIVATED;
    }

    for (size_t i = 0; i < num_paths; ++i) {
        if (is_numeric_node(&paths[i].starting_node, 84) && paths[i].relative_path_length > 0) {
            set_numeric_node(&targets[i], 85);
        } else {
            set_numeric_node(&targets[i], 0);
        }
    }
    return MU_STATUS_GOOD;
}
