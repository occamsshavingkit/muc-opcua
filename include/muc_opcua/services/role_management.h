/* include/muc_opcua/services/role_management.h
 *
 * User Role Management Server Facet (spec 095, PG19, CU 2080).
 * CRUD for OPC UA Roles and identity-to-role mappings.
 * OPC-10000-12 §9.5-9.6, OPC-10000-7 v1.05.02.
 *
 * The facet exposes four Method nodes: AddRole(15997) and RemoveRole(16000)
 * on the RoleSetType(15607) ObjectType, and AddIdentity(15612) and
 * RemoveIdentity(15614) on the RoleType(15620) RoleName_Placeholder(15608)
 * InstanceDeclaration. Method dispatch rides the existing Method Server
 * Facet (spec 062): mu_server_init() calls mu_role_management_register()
 * automatically when MUC_OPCUA_CU_USER_ROLE_MANAGEMENT is on, registering
 * the four method callbacks. The callbacks route to the integrator-provided
 * adapter; with no adapter each method returns Bad_NotSupported.
 */
#ifndef MUC_OPCUA_SERVICES_ROLE_MANAGEMENT_H
#define MUC_OPCUA_SERVICES_ROLE_MANAGEMENT_H

#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Method NodeIds in namespace 0 (OPC-10000-12 §9.5-9.6, NodeIds.csv). */
#define MU_ID_ROLE_ADD_ROLE 15997u
#define MU_ID_ROLE_REMOVE_ROLE 16000u
#define MU_ID_ROLE_ADD_IDENTITY 15612u
#define MU_ID_ROLE_REMOVE_IDENTITY 15614u

/* ObjectType NodeIds (OPC-10000-12 §9.5-9.6). */
#define MU_ID_ROLE_SET_TYPE 15607u
#define MU_ID_ROLE_NAME_PLACEHOLDER 15608u
#define MU_ID_ROLE_TYPE 15620u

#ifdef MUC_OPCUA_CU_USER_ROLE_MANAGEMENT

struct mu_server;

typedef struct {
    opcua_statuscode_t (*add_role)(void *ctx, const mu_nodeid_t *role_id, const mu_string_t *role_name);
    opcua_statuscode_t (*remove_role)(void *ctx, const mu_nodeid_t *role_id);
    opcua_statuscode_t (*add_identity)(void *ctx, const mu_nodeid_t *role_id, const mu_string_t *identity);
    opcua_statuscode_t (*remove_identity)(void *ctx, const mu_nodeid_t *role_id, const mu_string_t *identity);

    void *context;
} mu_role_management_adapter_t;

opcua_statuscode_t mu_role_management_register(struct mu_server *server);

#endif /* MUC_OPCUA_CU_USER_ROLE_MANAGEMENT */

#ifdef MUC_OPCUA_CU_SECURITY_ROLE_SERVER_AUTHORIZATION

typedef opcua_statuscode_t (*mu_role_permission_check_t)(void *context, const mu_nodeid_t *session_id,
                                                          opcua_uint32_t service_type, const mu_nodeid_t *target_node_id);

#endif /* MUC_OPCUA_CU_SECURITY_ROLE_SERVER_AUTHORIZATION */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_ROLE_MANAGEMENT_H */
