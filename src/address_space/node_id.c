/* src/address_space/node_id.c */
#include "micro_opcua/address_space.h"
#include <string.h>

opcua_boolean_t mu_nodeid_equal(const mu_nodeid_t *n1, const mu_nodeid_t *n2) {
    if (!n1 || !n2) {
        return false;
    }
    
    if (n1->namespace_index != n2->namespace_index) {
        return false;
    }
    
    if (n1->identifier_type != n2->identifier_type) {
        return false;
    }
    
    switch (n1->identifier_type) {
        case MU_NODEID_NUMERIC:
            return n1->identifier.numeric == n2->identifier.numeric;
            
        case MU_NODEID_STRING:
            if (n1->identifier.string.length != n2->identifier.string.length) {
                return false;
            }
            if (n1->identifier.string.length <= 0) {
                return true;
            }
            if (!n1->identifier.string.data || !n2->identifier.string.data) {
                return n1->identifier.string.data == n2->identifier.string.data;
            }
            return memcmp(n1->identifier.string.data, n2->identifier.string.data, (size_t)n1->identifier.string.length) == 0;
            
        default:
            /* GUID and Opaque not supported in minimal profile */
            return false;
    }
}

opcua_boolean_t mu_nodeid_in_namespace(const mu_nodeid_t *node_id, opcua_uint16_t namespace_index) {
    if (!node_id) {
        return false;
    }
    return node_id->namespace_index == namespace_index;
}

const mu_node_t *mu_address_space_find_node(const mu_address_space_t *address_space, const mu_nodeid_t *node_id) {
    size_t i;
    
    if (!address_space || !address_space->nodes || !node_id) {
        return NULL;
    }
    
    for (i = 0; i < address_space->node_count; i++) {
        if (mu_nodeid_equal(&address_space->nodes[i].node_id, node_id)) {
            return &address_space->nodes[i];
        }
    }
    
    return NULL;
}
