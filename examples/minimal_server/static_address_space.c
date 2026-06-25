/* examples/minimal_server/static_address_space.c */
#include "static_address_space.h"
#include <stddef.h>

/* A minimal address space with one int32 variable */
static const mu_reference_t s_node1_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 85 } }, { 0, MU_NODEID_NUMERIC, { 62 } }, false } /* HasTypeDefinition -> BaseVariableType */
};

static const mu_value_source_t s_node1_value = { MU_VALUESOURCE_STATIC, { .static_value = { MU_TYPE_INT32, { .i32 = 42 } } } };

static const mu_node_t s_nodes[] = {
    {
        { 1, MU_NODEID_NUMERIC, { 1000 } },
        MU_NODECLASS_VARIABLE,
        { 6, (const opcua_byte_t *)"MyVar1" },
        { 6, (const opcua_byte_t *)"MyVar1" },
        s_node1_refs,
        1,
        &s_node1_value
    }
};

const mu_address_space_t g_minimal_address_space = {
    s_nodes,
    1
};
