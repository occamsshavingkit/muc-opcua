/* src/address_space/complex_types.c
 *
 * ComplexType Server Facet: Structure/Enum registration.
 * Storage lives in caller-provided mu_server_t.complex_types — zero BSS.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_CU_COMPLEX_TYPES

#include "core/server_internal.h"
#include "muc_opcua/address_space/complex_types.h"
#include "muc_opcua/server.h"

#define MU_MAX_COMPLEX_TYPES 8

opcua_statuscode_t mu_register_structure_type(struct mu_server *server, const mu_nodeid_t *type_id,
                                              const mu_structure_definition_t *def) {
    if (server->complex_types.structure_count >= MU_MAX_COMPLEX_TYPES)
        return MU_STATUS_BAD_OUTOFMEMORY;
    server->complex_types.structures[server->complex_types.structure_count] = def;
    server->complex_types.structure_ids[server->complex_types.structure_count] = *type_id;
    server->complex_types.structure_count++;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_register_enumeration_type(struct mu_server *server, const mu_nodeid_t *type_id,
                                                const mu_enum_definition_t *def) {
    if (server->complex_types.enum_count >= MU_MAX_COMPLEX_TYPES)
        return MU_STATUS_BAD_OUTOFMEMORY;
    server->complex_types.enums[server->complex_types.enum_count] = def;
    server->complex_types.enum_ids[server->complex_types.enum_count] = *type_id;
    server->complex_types.enum_count++;
    return MU_STATUS_GOOD;
}

const mu_structure_definition_t *mu_find_structure_definition(const struct mu_server *server,
                                                              const mu_nodeid_t *type_id) {
    if (server == NULL || type_id == NULL)
        return NULL;
    opcua_uint16_t i;
    for (i = 0; i < server->complex_types.structure_count; i++) {
        const mu_nodeid_t *stored = &server->complex_types.structure_ids[i];
        if (stored->namespace_index == type_id->namespace_index &&
            stored->identifier_type == type_id->identifier_type &&
            stored->identifier.numeric == type_id->identifier.numeric)
            return server->complex_types.structures[i];
    }
    return NULL;
}

const mu_enum_definition_t *mu_find_enum_definition(const struct mu_server *server, const mu_nodeid_t *type_id) {
    if (server == NULL || type_id == NULL)
        return NULL;
    opcua_uint16_t i;
    for (i = 0; i < server->complex_types.enum_count; i++) {
        const mu_nodeid_t *stored = &server->complex_types.enum_ids[i];
        if (stored->namespace_index == type_id->namespace_index &&
            stored->identifier_type == type_id->identifier_type &&
            stored->identifier.numeric == type_id->identifier.numeric)
            return server->complex_types.enums[i];
    }
    return NULL;
}

#endif /* MUC_OPCUA_CU_COMPLEX_TYPES */
