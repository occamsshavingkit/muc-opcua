/* src/address_space/base_nodes.c */
#include "base_nodes.h"
#include <stddef.h>

static const mu_reference_t s_root_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 0, MU_NODEID_NUMERIC, { 85 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 0, MU_NODEID_NUMERIC, { 86 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 0, MU_NODEID_NUMERIC, { 87 } }, true }
};

static const mu_reference_t s_objects_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 0, MU_NODEID_NUMERIC, { 2253 } }, true }
};

static const mu_reference_t s_server_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 47 } }, { 0, MU_NODEID_NUMERIC, { 2254 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 47 } }, { 0, MU_NODEID_NUMERIC, { 2255 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 47 } }, { 0, MU_NODEID_NUMERIC, { 2256 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 47 } }, { 0, MU_NODEID_NUMERIC, { 2268 } }, true }
};

static const mu_reference_t s_server_status_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 47 } }, { 0, MU_NODEID_NUMERIC, { 2259 } }, true }
};

static const mu_reference_t s_server_capabilities_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 46 } }, { 0, MU_NODEID_NUMERIC, { 2269 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 46 } }, { 0, MU_NODEID_NUMERIC, { 2271 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 47 } }, { 0, MU_NODEID_NUMERIC, { 11704 } }, true }
};

static const mu_reference_t s_operation_limits_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 46 } }, { 0, MU_NODEID_NUMERIC, { 11705 } }, true },
    { { 0, MU_NODEID_NUMERIC, { 46 } }, { 0, MU_NODEID_NUMERIC, { 11710 } }, true }
};

static const mu_string_t s_server_array[] = {
    { 22, (const opcua_byte_t *)"urn:micro-opcua:server" }
};

static const mu_value_source_t s_server_array_value = {
    MU_VALUESOURCE_STATIC,
    { .static_value = { .type = MU_TYPE_STRING, .value.array = s_server_array, .is_array = true, .array_length = 1 } }
};

static const mu_string_t s_namespace_array[] = {
    { 28, (const opcua_byte_t *)"http://opcfoundation.org/UA/" }
};

static const mu_value_source_t s_namespace_array_value = {
    MU_VALUESOURCE_STATIC,
    { .static_value = { .type = MU_TYPE_STRING, .value.array = s_namespace_array, .is_array = true, .array_length = 1 } }
};

static const mu_value_source_t s_server_status_state_value = {
    MU_VALUESOURCE_STATIC,
    { .static_value = { .type = MU_TYPE_INT32, .value.i32 = 0 } }
};

static const mu_string_t s_server_profile_array[] = {
    { 65, (const opcua_byte_t *)"http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017" }
};

static const mu_value_source_t s_server_profile_array_value = {
    MU_VALUESOURCE_STATIC,
    { .static_value = { .type = MU_TYPE_STRING, .value.array = s_server_profile_array, .is_array = true, .array_length = 1 } }
};

static const mu_string_t s_locale_id_array[] = {
    { 2, (const opcua_byte_t *)"en" }
};

static const mu_value_source_t s_locale_id_array_value = {
    MU_VALUESOURCE_STATIC,
    { .static_value = { .type = MU_TYPE_STRING, .value.array = s_locale_id_array, .is_array = true, .array_length = 1 } }
};

static const mu_value_source_t s_max_nodes_per_read_value = {
    MU_VALUESOURCE_STATIC,
    { .static_value = { .type = MU_TYPE_UINT32, .value.ui32 = 32 } }
};

static const mu_value_source_t s_max_nodes_per_browse_value = {
    MU_VALUESOURCE_STATIC,
    { .static_value = { .type = MU_TYPE_UINT32, .value.ui32 = 8 } }
};

static const mu_node_t s_base_nodes[] = {
    {
        { 0, MU_NODEID_NUMERIC, { 84 } },
        MU_NODECLASS_OBJECT,
        { 4, (const opcua_byte_t *)"Root" },
        { 4, (const opcua_byte_t *)"Root" },
        s_root_refs,
        3,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 85 } },
        MU_NODECLASS_OBJECT,
        { 7, (const opcua_byte_t *)"Objects" },
        { 7, (const opcua_byte_t *)"Objects" },
        s_objects_refs,
        1,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 86 } },
        MU_NODECLASS_OBJECT,
        { 5, (const opcua_byte_t *)"Types" },
        { 5, (const opcua_byte_t *)"Types" },
        NULL,
        0,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 87 } },
        MU_NODECLASS_OBJECT,
        { 5, (const opcua_byte_t *)"Views" },
        { 5, (const opcua_byte_t *)"Views" },
        NULL,
        0,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2253 } },
        MU_NODECLASS_OBJECT,
        { 6, (const opcua_byte_t *)"Server" },
        { 6, (const opcua_byte_t *)"Server" },
        s_server_refs,
        4,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2254 } },
        MU_NODECLASS_VARIABLE,
        { 11, (const opcua_byte_t *)"ServerArray" },
        { 11, (const opcua_byte_t *)"ServerArray" },
        NULL,
        0,
        &s_server_array_value
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2255 } },
        MU_NODECLASS_VARIABLE,
        { 14, (const opcua_byte_t *)"NamespaceArray" },
        { 14, (const opcua_byte_t *)"NamespaceArray" },
        NULL,
        0,
        &s_namespace_array_value
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2256 } },
        MU_NODECLASS_VARIABLE,
        { 12, (const opcua_byte_t *)"ServerStatus" },
        { 12, (const opcua_byte_t *)"ServerStatus" },
        s_server_status_refs,
        1,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2259 } },
        MU_NODECLASS_VARIABLE,
        { 5, (const opcua_byte_t *)"State" },
        { 5, (const opcua_byte_t *)"State" },
        NULL,
        0,
        &s_server_status_state_value
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2268 } },
        MU_NODECLASS_OBJECT,
        { 18, (const opcua_byte_t *)"ServerCapabilities" },
        { 18, (const opcua_byte_t *)"ServerCapabilities" },
        s_server_capabilities_refs,
        3,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2269 } },
        MU_NODECLASS_VARIABLE,
        { 18, (const opcua_byte_t *)"ServerProfileArray" },
        { 18, (const opcua_byte_t *)"ServerProfileArray" },
        NULL,
        0,
        &s_server_profile_array_value
    },
    {
        { 0, MU_NODEID_NUMERIC, { 2271 } },
        MU_NODECLASS_VARIABLE,
        { 13, (const opcua_byte_t *)"LocaleIdArray" },
        { 13, (const opcua_byte_t *)"LocaleIdArray" },
        NULL,
        0,
        &s_locale_id_array_value
    },
    {
        { 0, MU_NODEID_NUMERIC, { 11704 } },
        MU_NODECLASS_OBJECT,
        { 15, (const opcua_byte_t *)"OperationLimits" },
        { 15, (const opcua_byte_t *)"OperationLimits" },
        s_operation_limits_refs,
        2,
        NULL
    },
    {
        { 0, MU_NODEID_NUMERIC, { 11705 } },
        MU_NODECLASS_VARIABLE,
        { 15, (const opcua_byte_t *)"MaxNodesPerRead" },
        { 15, (const opcua_byte_t *)"MaxNodesPerRead" },
        NULL,
        0,
        &s_max_nodes_per_read_value
    },
    {
        { 0, MU_NODEID_NUMERIC, { 11710 } },
        MU_NODECLASS_VARIABLE,
        { 17, (const opcua_byte_t *)"MaxNodesPerBrowse" },
        { 17, (const opcua_byte_t *)"MaxNodesPerBrowse" },
        NULL,
        0,
        &s_max_nodes_per_browse_value
    }
};

static const mu_address_space_t s_base_space = {
    s_base_nodes,
    sizeof(s_base_nodes) / sizeof(s_base_nodes[0])
};

const mu_address_space_t *mu_base_address_space(void) {
    return &s_base_space;
}

const mu_node_t *mu_resolve_node(const mu_address_space_t *user, const mu_nodeid_t *node_id) {
    if (user) {
        const mu_node_t *n = mu_address_space_find_node(user, node_id);
        if (n) return n;
    }
    return mu_address_space_find_node(mu_base_address_space(), node_id);
}
