/* examples/minimal_server/static_address_space.c */
#include "static_address_space.h"
#include <stddef.h>

/*
 * A minimal, self-consistent address space: the standard Objects folder
 * (ns=0;i=85, OPC 10000-5) organizes a single read-only Int32 variable
 * MyVar1 (ns=1;i=1000). Every reference target resolves to a node in the
 * space, so mu_address_space_validate() passes. The node model only
 * represents Object/Variable NodeClasses, so the Variable's HasTypeDefinition
 * to a VariableType is intentionally omitted from this tiny example.
 */

/* Organizes (ns=0;i=35) */
static const mu_reference_t s_objects_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 1, MU_NODEID_NUMERIC, { 1000 } }, true }  /* Organizes -> MyVar1 */
};

static const mu_reference_t s_myvar1_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 0, MU_NODEID_NUMERIC, { 85 } }, false }   /* inverse Organizes -> Objects */
};

static const mu_value_source_t s_myvar1_value = {
    MU_VALUESOURCE_STATIC, { .static_value = { MU_TYPE_INT32, { .i32 = 42 } } }
};

static const mu_node_t s_nodes[] = {
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
        { 1, MU_NODEID_NUMERIC, { 1000 } },
        MU_NODECLASS_VARIABLE,
        { 6, (const opcua_byte_t *)"MyVar1" },
        { 6, (const opcua_byte_t *)"MyVar1" },
        s_myvar1_refs,
        1,
        &s_myvar1_value
    }
};

const mu_address_space_t g_minimal_address_space = {
    s_nodes,
    2
};
