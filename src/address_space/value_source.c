/* src/address_space/value_source.c */
#include "micro_opcua/address_space.h"
#include <string.h>

opcua_statuscode_t mu_value_source_read(const mu_value_source_t *source, const mu_nodeid_t *node_id, mu_variant_t *value) {
    if (!source || !value) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    
    if (source->type == MU_VALUESOURCE_STATIC) {
        switch (source->data.static_value.type) {
            case MU_TYPE_BOOLEAN:
            case MU_TYPE_INT32:
            case MU_TYPE_UINT32:
            case MU_TYPE_FLOAT:
                *value = source->data.static_value;
                return MU_STATUS_GOOD;
            case MU_TYPE_STRING:
                if (source->data.static_value.value.str.length > MU_MAX_STRING_VALUE_LENGTH) {
                    return MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED;
                }
                *value = source->data.static_value;
                return MU_STATUS_GOOD;
            default:
                return MU_STATUS_BAD_NOTREADABLE;
        }
    } else if (source->type == MU_VALUESOURCE_CALLBACK) {
        if (!source->data.callback.read) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        return source->data.callback.read(source->data.callback.context, node_id, value);
    }
    
    return MU_STATUS_BAD_INTERNALERROR;
}
