#ifndef MUC_OPCUA_NODE_MANAGEMENT_COMMON_H
#define MUC_OPCUA_NODE_MANAGEMENT_COMMON_H

#include "../node_management.h"
#include <string.h>

#ifdef MUC_OPCUA_CU_NODEMANAGEMENT

#define MU_OBJECTATTRIBUTES_ENCODING_DEFAULTBINARY_ID 354u
#define MU_NODEATTRIBUTES_DISPLAYNAME_BIT (1u << 6)

opcua_boolean_t mu_dynamic_string_fits(const mu_string_t *name, size_t max_length);
void mu_dynamic_string_copy(mu_string_t *target, opcua_byte_t *dest, const mu_string_t *source);
void mu_dynamic_nodeid_copy(mu_nodeid_t *target, opcua_byte_t *dest, const mu_nodeid_t *source);

#endif /* MUC_OPCUA_CU_NODEMANAGEMENT */
#endif /* MUC_OPCUA_NODE_MANAGEMENT_COMMON_H */
