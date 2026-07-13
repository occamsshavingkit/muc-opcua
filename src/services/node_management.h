/* src/services/node_management.h */
#ifndef MUC_OPCUA_NODE_MANAGEMENT_H
#define MUC_OPCUA_NODE_MANAGEMENT_H

#include "../core/server_internal.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/server.h"

#ifdef MUC_OPCUA_CU_NODEMANAGEMENT

/* AddNodesItem */
typedef struct {
    mu_nodeid_t parent_node_id;
    mu_nodeid_t reference_type_id;
    mu_expanded_nodeid_t requested_new_node_id;
    mu_qualified_name_t browse_name;
    opcua_uint32_t node_class; /* mu_nodeclass_t enum encoded as UInt32 */

    /* ExtensionObject for nodeAttributes. We store the payload reference. */
    mu_nodeid_t node_attributes_type_id;
    size_t node_attributes_len;
    const opcua_byte_t *node_attributes;

    mu_expanded_nodeid_t type_definition;
} mu_add_nodes_item_t;

typedef struct {
    mu_add_nodes_item_t *items;
    size_t count;
} mu_add_nodes_request_t;

typedef struct {
    opcua_statuscode_t status_code;
    mu_nodeid_t added_node_id;
} mu_add_nodes_result_t;

typedef struct {
    mu_add_nodes_result_t *results;
    size_t count;
} mu_add_nodes_response_t;

/* AddReferencesItem */
typedef struct {
    mu_nodeid_t source_node_id;
    mu_nodeid_t reference_type_id;
    opcua_boolean_t is_forward;
    mu_string_t target_server_uri;
    mu_expanded_nodeid_t target_node_id;
    opcua_uint32_t target_node_class;
} mu_add_references_item_t;

typedef struct {
    mu_add_references_item_t *items;
    size_t count;
} mu_add_references_request_t;

typedef struct {
    opcua_statuscode_t status_code;
} mu_add_references_result_t;

typedef struct {
    mu_add_references_result_t *results;
    size_t count;
} mu_add_references_response_t;

/* DeleteNodesItem */
typedef struct {
    mu_nodeid_t node_id;
    opcua_boolean_t delete_target_references;
} mu_delete_nodes_item_t;

typedef struct {
    mu_delete_nodes_item_t *items;
    size_t count;
} mu_delete_nodes_request_t;

typedef struct {
    opcua_statuscode_t *results;
    size_t count;
} mu_delete_nodes_response_t;

/* DeleteReferencesItem */
typedef struct {
    mu_nodeid_t source_node_id;
    mu_nodeid_t reference_type_id;
    opcua_boolean_t is_forward;
    mu_expanded_nodeid_t target_node_id;
    opcua_boolean_t delete_bidirectional;
} mu_delete_references_item_t;

typedef struct {
    mu_delete_references_item_t *items;
    size_t count;
} mu_delete_references_request_t;

typedef struct {
    opcua_statuscode_t *results;
    size_t count;
} mu_delete_references_response_t;

/* Decoder / Encoder prototypes */
opcua_statuscode_t mu_add_nodes_request_decode(mu_binary_reader_t *r, mu_add_nodes_item_t *items, size_t max_items,
                                               size_t *out_count);
opcua_statuscode_t mu_add_nodes_response_encode(mu_binary_writer_t *w, const mu_add_nodes_result_t *results,
                                                size_t count);

opcua_statuscode_t mu_add_references_request_decode(mu_binary_reader_t *r, mu_add_references_item_t *items,
                                                    size_t max_items, size_t *out_count);
opcua_statuscode_t mu_add_references_response_encode(mu_binary_writer_t *w, const mu_add_references_result_t *results,
                                                     size_t count);

opcua_statuscode_t mu_delete_nodes_request_decode(mu_binary_reader_t *r, mu_delete_nodes_item_t *items,
                                                  size_t max_items, size_t *out_count);
opcua_statuscode_t mu_delete_nodes_response_encode(mu_binary_writer_t *w, const opcua_statuscode_t *results,
                                                   size_t count);

opcua_statuscode_t mu_delete_references_request_decode(mu_binary_reader_t *r, mu_delete_references_item_t *items,
                                                       size_t max_items, size_t *out_count);
opcua_statuscode_t mu_delete_references_response_encode(mu_binary_writer_t *w, const opcua_statuscode_t *results,
                                                        size_t count);

/* Process functions used by dispatch */
opcua_statuscode_t mu_add_nodes_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w);
opcua_statuscode_t mu_add_references_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w);
opcua_statuscode_t mu_delete_nodes_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w);
opcua_statuscode_t mu_delete_references_process(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w);

#endif /* MUC_OPCUA_CU_NODEMANAGEMENT */
#endif /* MUC_OPCUA_NODE_MANAGEMENT_H */
