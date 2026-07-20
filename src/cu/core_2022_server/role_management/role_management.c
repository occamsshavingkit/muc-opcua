/* src/cu/core_2022_server/role_management/role_management.c
 *
 * User Role Management Server Facet (spec 095, OPC-10000-7 CU 2080,
 * OPC-10000-12 §9.5-9.6).
 *
 * Implements the four Role management method handlers and registers them on
 * the server via the Method Server Facet (spec 062). Each handler extracts
 * the Call input arguments, dispatches to the integrator-provided
 * mu_role_management_adapter_t, and encodes the Call response per
 * OPC-10000-4 §5.11. With no adapter configured every method returns
 * Bad_NotSupported.
 */
#include "muc_opcua/services/role_management.h"
#include "core/server_internal.h"
#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/services/method.h"
#include <stddef.h>
#include <string.h>

#if MUC_OPCUA_CU_USER_ROLE_MANAGEMENT

static opcua_statuscode_t validate_role_object(const mu_server_t *server, const mu_nodeid_t *object_id) {
    if (server == NULL || object_id == NULL) {
        return MU_STATUS_BAD_METHODINVALID;
    }
    const mu_node_t *node =
        mu_address_space_find_node(server->config.address_space, (mu_address_space_index_t *)0, object_id);
    if (node == NULL) {
        return MU_STATUS_GOOD;
    }
    if (node->type_definition.namespace_index != 0 || node->type_definition.identifier_type != MU_NODEID_NUMERIC) {
        return MU_STATUS_BAD_METHODINVALID;
    }
    opcua_uint32_t td = node->type_definition.identifier.numeric;
    if (td != MU_ID_ROLE_SET_TYPE && td != MU_ID_ROLE_TYPE) {
        return MU_STATUS_BAD_METHODINVALID;
    }
    return MU_STATUS_GOOD;
}

static const mu_role_management_adapter_t *adapter_of(const mu_server_t *server) {
    if (server == NULL) {
        return NULL;
    }
    return server->config.role_management_adapter;
}

static mu_variant_t nodeid_variant(mu_nodeid_t id) {
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    v.type = MU_TYPE_NODEID;
    v.value.nodeid = id;
    return v;
}

static opcua_statuscode_t read_string_arg(const mu_variant_t *args, size_t count, size_t index, mu_string_t *out) {
    if (index >= count) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }
    if (args[index].is_array || args[index].type != MU_TYPE_STRING) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    *out = args[index].value.str;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_nodeid_arg(const mu_variant_t *args, size_t count, size_t index, mu_nodeid_t *out) {
    if (index >= count) {
        return MU_STATUS_BAD_ARGUMENTSMISSING;
    }
    if (args[index].is_array || args[index].type != MU_TYPE_NODEID) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    *out = args[index].value.nodeid;
    return MU_STATUS_GOOD;
}

/* AddRole (OPC-10000-12 §9.5).
 * Input:  RoleName:String, NamespaceUri:String
 * Output: RoleId:NodeId */
static opcua_statuscode_t handle_add_role(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                          const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                          size_t input_args_count, mu_variant_t *output_args,
                                          size_t *output_args_count) {
    (void)context;
    (void)method_id;

    opcua_statuscode_t s = validate_role_object(server, object_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (output_args_count == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    *output_args_count = 0u;

    const mu_role_management_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->add_role == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_string_t role_name;
    s = read_string_arg(input_args, input_args_count, 0u, &role_name);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_string_t namespace_uri;
    s = read_string_arg(input_args, input_args_count, 1u, &namespace_uri);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_nodeid_t role_id;
    (void)memset(&role_id, 0, sizeof(role_id));
    s = adapter->add_role(adapter->context, &role_id, &role_name);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (output_args != NULL) {
        output_args[0] = nodeid_variant(role_id);
        *output_args_count = 1u;
    }
    return MU_STATUS_GOOD;
}

/* RemoveRole (OPC-10000-12 §9.5).
 * Input:  RoleId:NodeId
 * Output: none */
static opcua_statuscode_t handle_remove_role(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                             const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                             size_t input_args_count, mu_variant_t *output_args,
                                             size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)output_args;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    opcua_statuscode_t s = validate_role_object(server, object_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    const mu_role_management_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->remove_role == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_nodeid_t role_id;
    s = read_nodeid_arg(input_args, input_args_count, 0u, &role_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return adapter->remove_role(adapter->context, &role_id);
}

/* AddIdentity (OPC-10000-12 §9.6).
 * Input:  Identity:String
 * Output: none */
static opcua_statuscode_t handle_add_identity(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                              const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                              size_t input_args_count, mu_variant_t *output_args,
                                              size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)output_args;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    opcua_statuscode_t s = validate_role_object(server, object_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    const mu_role_management_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->add_identity == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_string_t identity;
    s = read_string_arg(input_args, input_args_count, 0u, &identity);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return adapter->add_identity(adapter->context, object_id, &identity);
}

/* RemoveIdentity (OPC-10000-12 §9.6).
 * Input:  Identity:String
 * Output: none */
static opcua_statuscode_t handle_remove_identity(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                                 const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                 size_t input_args_count, mu_variant_t *output_args,
                                                 size_t *output_args_count) {
    (void)context;
    (void)method_id;
    (void)output_args;

    if (output_args_count != NULL) {
        *output_args_count = 0u;
    }

    opcua_statuscode_t s = validate_role_object(server, object_id);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    const mu_role_management_adapter_t *adapter = adapter_of(server);
    if (adapter == NULL || adapter->remove_identity == NULL) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    mu_string_t identity;
    s = read_string_arg(input_args, input_args_count, 0u, &identity);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    return adapter->remove_identity(adapter->context, object_id, &identity);
}

opcua_statuscode_t mu_role_management_register(mu_server_t *server) {
    if (server == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    static const mu_nodeid_t add_role_id = {0u, MU_NODEID_NUMERIC, {MU_ID_ROLE_ADD_ROLE}};
    static const mu_nodeid_t remove_role_id = {0u, MU_NODEID_NUMERIC, {MU_ID_ROLE_REMOVE_ROLE}};
    static const mu_nodeid_t add_identity_id = {0u, MU_NODEID_NUMERIC, {MU_ID_ROLE_ADD_IDENTITY}};
    static const mu_nodeid_t remove_identity_id = {0u, MU_NODEID_NUMERIC, {MU_ID_ROLE_REMOVE_IDENTITY}};

    opcua_statuscode_t s;
    s = mu_server_register_method_callback(server, &add_role_id, handle_add_role, NULL, NULL, 0, NULL, 0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &remove_role_id, handle_remove_role, NULL, NULL, 0, NULL, 0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &add_identity_id, handle_add_identity, NULL, NULL, 0, NULL, 0, true);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_server_register_method_callback(server, &remove_identity_id, handle_remove_identity, NULL, NULL, 0, NULL, 0,
                                           true);
    return s;
}

#endif /* MUC_OPCUA_CU_USER_ROLE_MANAGEMENT */
