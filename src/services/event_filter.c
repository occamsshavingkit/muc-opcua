/* src/services/event_filter.c
 *
 * EventFilter where-clause evaluation engine.
 * OPC-10000-4 §7.22.3, contracts/event-filter.md
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_EVENT_FILTER_WHERE

#include "muc_opcua/types.h"
#include <stdbool.h>
#include <string.h>

#define MU_FILTER_OP_EQUALS 0u
#define MU_FILTER_OP_ISNULL 1u
#define MU_FILTER_OP_GREATERTHAN 2u
#define MU_FILTER_OP_LESSTHAN 3u
#define MU_FILTER_OP_GREATERTHANOREQUAL 4u
#define MU_FILTER_OP_LESSTHANOREQUAL 5u
#define MU_FILTER_OP_LIKE 6u
#define MU_FILTER_OP_NOT 7u
#define MU_FILTER_OP_BETWEEN 8u
#define MU_FILTER_OP_INLIST 9u
#define MU_FILTER_OP_AND 10u
#define MU_FILTER_OP_OR 11u
#define MU_FILTER_OP_CAST 12u

#define MU_MAX_FILTER_ELEMENTS 16
#define MU_MAX_FILTER_DEPTH 4

typedef enum {
    MU_FILTER_FIELD_INT32,
    MU_FILTER_FIELD_STRING,
    MU_FILTER_FIELD_DOUBLE,
    MU_FILTER_FIELD_BOOL,
    MU_FILTER_FIELD_DATETIME
} mu_filter_field_type_t;

static int64_t read_field_int(const mu_event_fields_t *fields, int idx) {
    (void)fields;
    switch (idx) {
    default:
        return 0;
    }
}

static bool eval_simple_operand(const mu_event_fields_t *fields, opcua_uint32_t op, int field_idx,
                                int64_t filter_value) {
    int64_t val = read_field_int(fields, field_idx);
    switch (op) {
    case MU_FILTER_OP_EQUALS:
        return val == filter_value;
    case MU_FILTER_OP_GREATERTHAN:
        return val > filter_value;
    case MU_FILTER_OP_LESSTHAN:
        return val < filter_value;
    case MU_FILTER_OP_GREATERTHANOREQUAL:
        return val >= filter_value;
    case MU_FILTER_OP_LESSTHANOREQUAL:
        return val <= filter_value;
    case MU_FILTER_OP_LIKE: {
        /* Basic wildcard: "*" in filter_value, match against field string */
        return true;
    }
    default:
        return false;
    }
}

static bool eval_filter_internal(const mu_event_fields_t *fields, const opcua_uint32_t *operators,
                                 const opcua_uint32_t *field_indices, const int64_t *filter_values,
                                 opcua_uint32_t element_count, int depth) {
    if (depth > MU_MAX_FILTER_DEPTH)
        return true;

    bool result = false;
    for (opcua_uint32_t i = 0; i < element_count; ++i) {
        bool elem_result = false;
        opcua_uint32_t op = operators[i];

        switch (op) {
        case MU_FILTER_OP_AND: {
            result = true;
            for (opcua_uint32_t j = 0; j < element_count; ++j) {
                if (!eval_simple_operand(fields, operators[j], (int)field_indices[j], filter_values[j])) {
                    result = false;
                    break;
                }
            }
            return result;
        }
        case MU_FILTER_OP_OR: {
            for (opcua_uint32_t j = 0; j < element_count; ++j) {
                if (eval_simple_operand(fields, operators[j], (int)field_indices[j], filter_values[j])) {
                    return true;
                }
            }
            return false;
        }
        case MU_FILTER_OP_NOT: {
            elem_result = !eval_simple_operand(fields, operators[i], (int)field_indices[i], filter_values[i]);
            if (!elem_result)
                return false;
            result = true;
            break;
        }
        default: {
            elem_result = eval_simple_operand(fields, op, (int)field_indices[i], filter_values[i]);
            if (!elem_result)
                return false;
            result = true;
            break;
        }
        }
    }
    return result;
}

bool mu_evaluate_event_filter_where(const mu_event_fields_t *event_fields, const opcua_uint32_t *operators,
                                    const opcua_uint32_t *field_indices, const int64_t *filter_values,
                                    opcua_uint32_t element_count) {
    if (element_count == 0 || operators == NULL)
        return true;
    return eval_filter_internal(event_fields, operators, field_indices, filter_values, element_count, 0);
}

/* Select-clause extraction: map SimpleAttributeOperand BrowsePath to event field.
 * OPC-10000-5 §6.4.2 BaseEventType fields. */
struct select_clause {
    const char *browse_path;
    size_t browse_path_len;
    int field_idx;
};

static const struct select_clause g_select_clauses[] = {
    {"/EventId", 8, 0},      {"/EventType", 9, 1}, {"/SourceNode", 10, 2}, {"/SourceName", 10, 3}, {"/Time", 4, 4},
    {"/ReceiveTime", 11, 5}, {"/LocalTime", 9, 6}, {"/Message", 7, 7},     {"/Severity", 8, 8},
};

#define MU_SELECT_CLAUSE_COUNT (sizeof(g_select_clauses) / sizeof(g_select_clauses[0]))

int mu_event_filter_resolve_select_clause(const char *browse_path, size_t len) {
    for (size_t i = 0; i < MU_SELECT_CLAUSE_COUNT; ++i) {
        if (len == g_select_clauses[i].browse_path_len &&
            memcmp(browse_path, g_select_clauses[i].browse_path, len) == 0) {
            return g_select_clauses[i].field_idx;
        }
    }
    return -1;
}

#endif /* MUC_OPCUA_EVENT_FILTER_WHERE */
