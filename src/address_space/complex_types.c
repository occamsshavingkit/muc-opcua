/* src/address_space/complex_types.c
 *
 * ComplexType Server Facet: Structure/Enum registration.
 * Storage lives in caller-provided mu_server_t.complex_types — zero BSS.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_COMPLEX_TYPES

#include "../../src/core/server_internal.h"
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

#endif /* MUC_OPCUA_COMPLEX_TYPES */
