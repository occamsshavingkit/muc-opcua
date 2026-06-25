/* src/services/browse.h */
#ifndef MICRO_OPCUA_SERVICES_BROWSE_H
#define MICRO_OPCUA_SERVICES_BROWSE_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include "micro_opcua/address_space.h"
#include "micro_opcua/encoding.h"

typedef enum {
    MU_BROWSE_DIRECTION_FORWARD = 0,
    MU_BROWSE_DIRECTION_INVERSE = 1,
    MU_BROWSE_DIRECTION_BOTH = 2
} mu_browse_direction_t;

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t browse_direction;
    mu_nodeid_t reference_type_id;
    opcua_boolean_t include_subtypes;
    opcua_uint32_t node_class_mask;
    opcua_uint32_t result_mask;
} mu_browse_description_t;

typedef struct {
    mu_nodeid_t view_id;
    opcua_datetime_t timestamp;
    opcua_uint32_t view_version;
    opcua_uint32_t requested_max_references_per_node;
    mu_browse_description_t *nodes_to_browse;
    size_t num_nodes_to_browse;
} mu_browse_request_t;

typedef struct {
    mu_nodeid_t reference_type_id;
    opcua_boolean_t is_forward;
    mu_nodeid_t node_id;
    opcua_uint16_t browse_name_namespace_index;
    const char *browse_name;
    const char *display_name;
    opcua_uint32_t node_class;
    mu_nodeid_t type_definition;
} mu_reference_description_t;

typedef struct {
    opcua_statuscode_t status_code;
    mu_reference_description_t *references;
    size_t num_references;
} mu_browse_result_t;

typedef struct {
    mu_browse_result_t *results;
    size_t num_results;
} mu_browse_response_t;

/* Decodes the body of a BrowseRequest (after the RequestHeader) */
opcua_statuscode_t mu_browse_request_decode(mu_binary_reader_t *reader, 
                                            mu_browse_request_t *req,
                                            mu_browse_description_t *desc_array,
                                            size_t max_desc);

/* Encodes the body of a BrowseResponse (after the ResponseHeader) */
opcua_statuscode_t mu_browse_response_encode(mu_binary_writer_t *writer, 
                                             const mu_browse_response_t *resp);

#endif /* MICRO_OPCUA_SERVICES_BROWSE_H */
