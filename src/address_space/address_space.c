/* src/address_space/address_space.c */
#include "micro_opcua/address_space.h"
#include <stddef.h>

opcua_statuscode_t mu_address_space_validate(const mu_address_space_t *address_space) {
    size_t i, j, r;
    
    if (!address_space) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    
    if (!address_space->nodes && address_space->node_count > 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    
    /* Check for duplicates */
    for (i = 0; i < address_space->node_count; i++) {
        for (j = i + 1; j < address_space->node_count; j++) {
            if (mu_nodeid_equal(&address_space->nodes[i].node_id, &address_space->nodes[j].node_id)) {
                return MU_STATUS_BAD_INTERNALERROR;
            }
        }
    }
    
    /* Check for unresolved references */
    for (i = 0; i < address_space->node_count; i++) {
        const mu_node_t *node = &address_space->nodes[i];
        if (node->reference_count > 0 && !node->references) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        
        for (r = 0; r < node->reference_count; r++) {
            const mu_reference_t *ref = &node->references[r];
            if (!mu_address_space_find_node(address_space, &ref->target_id)) {
                return MU_STATUS_BAD_INTERNALERROR;
            }
        }
    }
    
    return MU_STATUS_GOOD;
}
