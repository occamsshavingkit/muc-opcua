/* src/address_space/value_source.c */
#include "micro_opcua/address_space.h"
#include <string.h>

opcua_statuscode_t mu_value_source_read(const mu_value_source_t *source, const mu_nodeid_t *node_id, mu_variant_t *value) {
    if (!source || !value) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    
    if (source->type == MU_VALUESOURCE_STATIC) {
        switch (source->data.static_value.type) {
            case MU_TYPE_BOOLEAN:
            case MU_TYPE_INT32:
            case MU_TYPE_UINT32:
            case MU_TYPE_FLOAT:
            case MU_TYPE_STRING:
                /* In T060 we'll add 64-byte limit check */
                *value = source->data.static_value;
                return MU_STATUS_GOOD;
            default:
                return MU_STATUS_BAD_NOTREADABLE;
        }
    }
    
    return MU_STATUS_BAD_INTERNALERROR; /* Callback handled in T059 */
}
