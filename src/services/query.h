/**
 * @file query.h
 * @brief Query Services (OPC 10000-4, 5.9) and ContentFilter (OPC 10000-4, 7.7)
 */

#ifndef MU_SERVICES_QUERY_H
#define MU_SERVICES_QUERY_H

#include "micro_opcua/types.h"
#include "service_header.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FilterOperator (OPC 10000-4, 7.7.3)
 * Only a subset is supported in this implementation.
 */
typedef enum {
    MU_FILTEROPERATOR_EQUALS = 0,
    MU_FILTEROPERATOR_ISNULL = 1,
    MU_FILTEROPERATOR_GREATERTHAN = 2,
    MU_FILTEROPERATOR_LESSTHAN = 3,
    MU_FILTEROPERATOR_GREATERTHANOREQUAL = 4,
    MU_FILTEROPERATOR_LESSTHANOREQUAL = 5,
    MU_FILTEROPERATOR_LIKE = 6,
    MU_FILTEROPERATOR_NOT = 7,
    MU_FILTEROPERATOR_BETWEEN = 8,
    MU_FILTEROPERATOR_INLIST = 9,
    MU_FILTEROPERATOR_AND = 10,
    MU_FILTEROPERATOR_OR = 11,
    MU_FILTEROPERATOR_CAST = 12,
    MU_FILTEROPERATOR_INVIEW = 13,
    MU_FILTEROPERATOR_OFTYPE = 14,
    MU_FILTEROPERATOR_RELATEDTO = 15,
    MU_FILTEROPERATOR_BITWISEAND = 16,
    MU_FILTEROPERATOR_BITWISEOR = 17
} mu_filter_operator_t;

/**
 * @brief FilterOperand (OPC 10000-4, 7.7.4)
 * For simplicity, we only support ElementOperand and LiteralOperand.
 */
typedef enum {
    MU_FILTEROPERAND_ELEMENT = 0,
    MU_FILTEROPERAND_LITERAL = 1
} mu_filter_operand_type_t;

typedef struct {
    opcua_uint32_t index;
} mu_element_operand_t;

typedef struct {
    mu_variant_t value;
} mu_literal_operand_t;

typedef struct {
    mu_filter_operand_type_t type;
    union {
        mu_element_operand_t element;
        mu_literal_operand_t literal;
    } operand;
} mu_filter_operand_t;

/**
 * @brief ContentFilterElement (OPC 10000-4, 7.7.2)
 */
typedef struct {
    mu_filter_operator_t filter_operator;
    const mu_filter_operand_t *filter_operands;
    size_t filter_operands_count;
} mu_content_filter_element_t;

/**
 * @brief ContentFilter (OPC 10000-4, 7.7.1)
 */
typedef struct {
    const mu_content_filter_element_t *elements;
    size_t elements_count;
} mu_content_filter_t;

/**
 * @brief NodeTypeDescription (OPC 10000-4, 7.21)
 */
typedef struct {
    mu_nodeid_t type_definition_node;
    opcua_boolean_t include_sub_types;
    size_t data_to_return_count; /* Not fully supported */
} mu_node_type_description_t;

/**
 * @brief QueryFirstRequest (OPC 10000-4, 5.9.3)
 */
typedef struct {
    mu_request_header_t header;
    mu_node_type_description_t *node_types;
    size_t node_types_count;
    mu_content_filter_t filter;
    opcua_uint32_t max_data_sets_to_return;
    opcua_uint32_t max_references_to_return;
} mu_query_first_request_t;

/**
 * @brief QueryDataDescription (OPC 10000-4, 7.23)
 */
typedef struct {
    mu_nodeid_t node_id;
} mu_query_data_set_t;

/**
 * @brief QueryFirstResponse (OPC 10000-4, 5.9.3)
 */
typedef struct {
    mu_response_header_t header;
    const mu_query_data_set_t *query_data_sets;
    size_t query_data_sets_count;
    mu_string_t continuation_point;
} mu_query_first_response_t;

/**
 * @brief QueryNextRequest (OPC 10000-4, 5.9.4)
 */
typedef struct {
    mu_request_header_t header;
    opcua_boolean_t release_continuation_point;
    mu_string_t continuation_point;
} mu_query_next_request_t;

/**
 * @brief QueryNextResponse (OPC 10000-4, 5.9.4)
 */
typedef struct {
    mu_response_header_t header;
    const mu_query_data_set_t *query_data_sets;
    size_t query_data_sets_count;
    mu_string_t continuation_point;
} mu_query_next_response_t;

/* Encoding and decoding functions */
#include "micro_opcua/encoding.h"

opcua_statuscode_t mu_content_filter_decode(mu_binary_reader_t *reader, mu_content_filter_t *filter,
                                            mu_content_filter_element_t *elements, size_t max_elements,
                                            mu_filter_operand_t *operands, size_t max_operands);

opcua_statuscode_t mu_query_first_request_decode(mu_binary_reader_t *reader, mu_query_first_request_t *req,
                                                 mu_node_type_description_t *node_types, size_t max_node_types,
                                                 mu_content_filter_element_t *filter_elements, size_t max_filter_elements,
                                                 mu_filter_operand_t *filter_operands, size_t max_filter_operands);

opcua_statuscode_t mu_query_first_response_encode(mu_binary_writer_t *writer, const mu_query_first_response_t *resp);

opcua_statuscode_t mu_query_next_request_decode(mu_binary_reader_t *reader, mu_query_next_request_t *req);

opcua_statuscode_t mu_query_next_response_encode(mu_binary_writer_t *writer, const mu_query_next_response_t *resp);

/* Service logic */
struct mu_server;
opcua_statuscode_t mu_query_first_process(struct mu_server *server, const mu_query_first_request_t *req,
                                          mu_query_first_response_t *resp, mu_query_data_set_t *data_sets, size_t max_data_sets);

opcua_statuscode_t mu_query_next_process(struct mu_server *server, const mu_query_next_request_t *req,
                                         mu_query_next_response_t *resp, mu_query_data_set_t *data_sets, size_t max_data_sets);

#ifdef __cplusplus
}
#endif

#endif /* MU_SERVICES_QUERY_H */
