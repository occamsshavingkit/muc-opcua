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
    {{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true} /* Organizes -> MyVar1 */
};

static const mu_reference_t s_myvar1_refs[] = {
    {{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false} /* inverse Organizes -> Objects */
};

static const mu_value_source_t s_myvar1_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};

/*
 * Server Base Info: a client (e.g. the OPC Foundation .NET reference stack) reads
 * the standard NamespaceArray (ns=0;i=2255) and ServerArray (ns=0;i=2254) by NodeId
 * during session setup to build its namespace table. Both are String[] values.
 */
static const mu_string_t s_namespace_array[] = {{28, (const opcua_byte_t *)"http://opcfoundation.org/UA/"},
                                                {38, (const opcua_byte_t *)"urn:localhost:muc_opcua:minimal_server"}};
static const mu_value_source_t s_namespacearray_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {.type = MU_TYPE_STRING, .value.array = s_namespace_array, .is_array = true, .array_length = 2}}};

static const mu_string_t s_server_array[] = {{38, (const opcua_byte_t *)"urn:localhost:muc_opcua:minimal_server"}};
static const mu_value_source_t s_serverarray_value = {
    MU_VALUESOURCE_STATIC,
    {.static_value = {.type = MU_TYPE_STRING, .value.array = s_server_array, .is_array = true, .array_length = 1}}};

/*
 * OperationLimits (OPC 10000-5): advertise the server's per-request maxima so a
 * client self-limits its batch Read/Browse instead of overflowing the dispatch
 * caps. Values match MU_DISPATCH_MAX_{READ_NODES,BROWSE_NODES} in service_dispatch.c.
 *   MaxNodesPerRead   = ns=0;i=11705
 *   MaxNodesPerBrowse = ns=0;i=11710
 */
static const mu_value_source_t s_max_nodes_per_read_value = {MU_VALUESOURCE_STATIC,
                                                             {.static_value = {MU_TYPE_UINT32, {.ui32 = 32}}}};
static const mu_value_source_t s_max_nodes_per_browse_value = {MU_VALUESOURCE_STATIC,
                                                               {.static_value = {MU_TYPE_UINT32, {.ui32 = 8}}}};

static const mu_node_t s_nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                     MU_NODECLASS_OBJECT,
                                     {7, (const opcua_byte_t *)"Objects"},
                                     {7, (const opcua_byte_t *)"Objects"},
                                     s_objects_refs,
                                     1,
                                     NULL,
                                     .type_definition = {0}},
                                    {{1, MU_NODEID_NUMERIC, {1000}},
                                     MU_NODECLASS_VARIABLE,
                                     {6, (const opcua_byte_t *)"MyVar1"},
                                     {6, (const opcua_byte_t *)"MyVar1"},
                                     s_myvar1_refs,
                                     1,
                                     &s_myvar1_value,
                                     .type_definition = {0}},
                                    {{0, MU_NODEID_NUMERIC, {2255}},
                                     MU_NODECLASS_VARIABLE,
                                     {14, (const opcua_byte_t *)"NamespaceArray"},
                                     {14, (const opcua_byte_t *)"NamespaceArray"},
                                     NULL,
                                     0,
                                     &s_namespacearray_value,
                                     .type_definition = {0}},
                                    {{0, MU_NODEID_NUMERIC, {2254}},
                                     MU_NODECLASS_VARIABLE,
                                     {11, (const opcua_byte_t *)"ServerArray"},
                                     {11, (const opcua_byte_t *)"ServerArray"},
                                     NULL,
                                     0,
                                     &s_serverarray_value,
                                     .type_definition = {0}},
                                    {{0, MU_NODEID_NUMERIC, {11705}},
                                     MU_NODECLASS_VARIABLE,
                                     {15, (const opcua_byte_t *)"MaxNodesPerRead"},
                                     {15, (const opcua_byte_t *)"MaxNodesPerRead"},
                                     NULL,
                                     0,
                                     &s_max_nodes_per_read_value,
                                     .type_definition = {0}},
                                    {{0, MU_NODEID_NUMERIC, {11710}},
                                     MU_NODECLASS_VARIABLE,
                                     {17, (const opcua_byte_t *)"MaxNodesPerBrowse"},
                                     {17, (const opcua_byte_t *)"MaxNodesPerBrowse"},
                                     NULL,
                                     0,
                                     &s_max_nodes_per_browse_value,
                                     .type_definition = {0}}};

const mu_address_space_t g_minimal_address_space = {s_nodes, 6};
