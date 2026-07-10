/* src/services/event_filter.c
 *
 * EventFilter WhereClause (ContentFilter) evaluator.
 * OPC-10000-4 §7.7 ContentFilter, §7.7.3 FilterOperator, §7.4 EventFilter.
 *
 * The MonitoredItem owns a compact mu_where_clause_t (see event_filter.h). This
 * file evaluates that operand tree against a triggered event's fields: element[0]
 * is the root and its boolean result is the filter verdict. Every operand is
 * resolved to a uniform typed value (a Literal, an event-field SimpleAttribute,
 * or a nested boolean Element) so a single comparison routine serves all
 * operators.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_EVENT_FILTER_WHERE

#include "event_filter.h"
#include "muc_opcua/address_space.h" /* mu_nodeid_equal */
#include <stdbool.h>
#include <string.h>

/* FilterOperator wire codes (OPC-10000-4 §7.7.3). Kept local to avoid a hard
   dependency on query.h (the Query service may be gated off while events are on);
   these are stable protocol constants and match mu_filter_operator_t. */
#define MU_OP_EQUALS 0u
#define MU_OP_ISNULL 1u
#define MU_OP_GREATERTHAN 2u
#define MU_OP_LESSTHAN 3u
#define MU_OP_GREATERTHANOREQUAL 4u
#define MU_OP_LESSTHANOREQUAL 5u
#define MU_OP_LIKE 6u
#define MU_OP_NOT 7u
#define MU_OP_BETWEEN 8u
#define MU_OP_INLIST 9u
#define MU_OP_AND 10u
#define MU_OP_OR 11u
#define MU_OP_OFTYPE 14u

#define MU_WHERE_MAX_DEPTH 8

/* ---- typed value model -------------------------------------------------- */

typedef enum {
    VK_NULL = 0,
    VK_INT,    /* signed/unsigned integer, boolean, datetime — held in .i */
    VK_DOUBLE, /* float/double — held in .d */
    VK_STRING, /* string / bytestring — held in .s */
    VK_NODEID  /* NodeId — held in .nid */
} value_kind_t;

typedef struct {
    value_kind_t kind;
    opcua_int64_t i;
    opcua_double_t d;
    mu_string_t s;
    mu_nodeid_t nid;
} eval_value_t;

static eval_value_t value_null(void) {
    eval_value_t v;
    memset(&v, 0, sizeof(v));
    v.kind = VK_NULL;
    return v;
}

static eval_value_t value_int(opcua_int64_t x) {
    eval_value_t v = value_null();
    v.kind = VK_INT;
    v.i = x;
    return v;
}

/* ---- event-field access ------------------------------------------------- */

/* A string/bytestring field with no backing data is an absent field → Null. */
static eval_value_t value_string_or_null(mu_string_t s) {
    eval_value_t v = value_null();
    if (s.data != NULL && s.length > 0) {
        v.kind = VK_STRING;
        v.s = s;
    }
    return v;
}

/* The null NodeId (ns=0, i=0) marks an absent field → Null. */
static eval_value_t value_nodeid_or_null(mu_nodeid_t nid) {
    eval_value_t v = value_null();
    bool is_null = nid.namespace_index == 0 && nid.identifier_type == MU_NODEID_NUMERIC && nid.identifier.numeric == 0;
    if (!is_null) {
        v.kind = VK_NODEID;
        v.nid = nid;
    }
    return v;
}

static eval_value_t read_event_field(const mu_event_fields_t *f, opcua_byte_t field) {
    eval_value_t v = value_null();
    if (f == NULL) {
        return v;
    }
    switch (field) {
    case MU_EVENT_FIELD_EVENTID: {
        mu_string_t id = {f->event_id.length, f->event_id.data};
        v = value_string_or_null(id);
        break;
    }
    case MU_EVENT_FIELD_EVENTTYPE:
        v = value_nodeid_or_null(f->event_type);
        break;
    case MU_EVENT_FIELD_SOURCENODE:
        v = value_nodeid_or_null(f->source_node);
        break;
    case MU_EVENT_FIELD_SOURCENAME:
        v = value_string_or_null(f->source_name);
        break;
    case MU_EVENT_FIELD_TIME:
        v = value_int((opcua_int64_t)f->time);
        break;
    case MU_EVENT_FIELD_RECEIVETIME:
        v = value_int((opcua_int64_t)f->receive_time);
        break;
    case MU_EVENT_FIELD_MESSAGE:
        v = value_string_or_null(f->message);
        break;
    case MU_EVENT_FIELD_SEVERITY:
        v = value_int((opcua_int64_t)f->severity);
        break;
    default:
        /* LocalTime and any field this server does not emit → Null. */
        break;
    }
    return v;
}

/* ---- literal reconstruction --------------------------------------------- */

static eval_value_t read_literal(const mu_where_clause_t *filter, const mu_where_operand_t *op) {
    eval_value_t v = value_null();
    switch (op->lit_type) {
    case MU_TYPE_BOOLEAN:
    case MU_TYPE_SBYTE:
    case MU_TYPE_BYTE:
    case MU_TYPE_INT16:
    case MU_TYPE_UINT16:
    case MU_TYPE_INT32:
    case MU_TYPE_UINT32:
    case MU_TYPE_INT64:
    case MU_TYPE_UINT64:
    case MU_TYPE_DATETIME:
    case MU_TYPE_STATUSCODE:
        v = value_int(op->num.i);
        break;
    case MU_TYPE_FLOAT:
    case MU_TYPE_DOUBLE:
        v.kind = VK_DOUBLE;
        v.d = op->num.d;
        break;
    case MU_TYPE_STRING:
    case MU_TYPE_BYTESTRING:
        v.kind = VK_STRING;
        if (op->blob_len > 0 && (size_t)(op->blob_off + op->blob_len) <= filter->blob_len) {
            v.s.length = (opcua_int32_t)op->blob_len;
            v.s.data = &filter->blob[op->blob_off];
        } else {
            v.s.length = 0;
            v.s.data = NULL;
        }
        break;
    case MU_TYPE_NODEID:
        v.kind = VK_NODEID;
        v.nid.namespace_index = op->nodeid_ns;
        if (op->blob_len > 0 && (size_t)(op->blob_off + op->blob_len) <= filter->blob_len) {
            v.nid.identifier_type = MU_NODEID_STRING;
            v.nid.identifier.string.length = (opcua_int32_t)op->blob_len;
            v.nid.identifier.string.data = &filter->blob[op->blob_off];
        } else {
            v.nid.identifier_type = MU_NODEID_NUMERIC;
            v.nid.identifier.numeric = op->num.nodeid_numeric;
        }
        break;
    default:
        break;
    }
    return v;
}

/* ---- comparison --------------------------------------------------------- */

static bool value_is_numeric(const eval_value_t *v) {
    return v->kind == VK_INT || v->kind == VK_DOUBLE;
}

static opcua_double_t value_as_double(const eval_value_t *v) {
    return v->kind == VK_DOUBLE ? v->d : (opcua_double_t)v->i;
}

/* Order comparison. Returns -1/0/1, or -2 when the values are not comparable
   (per OPC-10000-4 §7.7.3 a filter element that cannot be evaluated is treated
   as returning false by the caller). */
static int value_compare(const eval_value_t *a, const eval_value_t *b) {
    if (a->kind == VK_NULL || b->kind == VK_NULL) {
        return -2;
    }
    if (value_is_numeric(a) && value_is_numeric(b)) {
        if (a->kind == VK_INT && b->kind == VK_INT) {
            if (a->i < b->i) {
                return -1;
            }
            return a->i > b->i ? 1 : 0;
        }
        opcua_double_t da = value_as_double(a);
        opcua_double_t db = value_as_double(b);
        if (da < db) {
            return -1;
        }
        return da > db ? 1 : 0;
    }
    if (a->kind == VK_STRING && b->kind == VK_STRING) {
        opcua_int32_t n = a->s.length < b->s.length ? a->s.length : b->s.length;
        int c = (n > 0) ? memcmp(a->s.data, b->s.data, (size_t)n) : 0;
        if (c != 0) {
            return c < 0 ? -1 : 1;
        }
        if (a->s.length == b->s.length) {
            return 0;
        }
        return a->s.length < b->s.length ? -1 : 1;
    }
    if (a->kind == VK_NODEID && b->kind == VK_NODEID) {
        return mu_nodeid_equal(&a->nid, &b->nid) ? 0 : -2;
    }
    return -2;
}

/* ---- OPC UA Like pattern match (OPC-10000-4 §7.7.3 Like) ---------------- */
/* Wildcards: '%' any run, '_' any single, '[set]' / '[^set]' with a-b ranges,
   '\' escapes the next char. */
static bool like_match(const opcua_byte_t *p, opcua_int32_t plen, const opcua_byte_t *s, opcua_int32_t slen) {
    opcua_int32_t pi = 0, si = 0;
    while (pi < plen) {
        opcua_byte_t pc = p[pi];
        if (pc == '%') {
            /* collapse consecutive '%' then try to match the remainder */
            while (pi < plen && p[pi] == '%') {
                pi++;
            }
            if (pi == plen) {
                return true; /* trailing % matches the rest */
            }
            for (opcua_int32_t k = si; k <= slen; ++k) {
                if (like_match(&p[pi], plen - pi, &s[k], slen - k)) {
                    return true;
                }
            }
            return false;
        }
        if (si >= slen) {
            return false;
        }
        if (pc == '_') {
            pi++;
            si++;
            continue;
        }
        if (pc == '[') {
            pi++;
            bool negate = false;
            if (pi < plen && p[pi] == '^') {
                negate = true;
                pi++;
            }
            bool matched = false;
            while (pi < plen && p[pi] != ']') {
                opcua_byte_t lo = p[pi];
                if (pi + 2 < plen && p[pi + 1] == '-' && p[pi + 2] != ']') {
                    opcua_byte_t hi = p[pi + 2];
                    if (s[si] >= lo && s[si] <= hi) {
                        matched = true;
                    }
                    pi += 3;
                } else {
                    if (s[si] == lo) {
                        matched = true;
                    }
                    pi++;
                }
            }
            if (pi < plen && p[pi] == ']') {
                pi++;
            }
            if (matched == negate) {
                return false;
            }
            si++;
            continue;
        }
        if (pc == '\\' && pi + 1 < plen) {
            pi++;
            pc = p[pi];
        }
        if (s[si] != pc) {
            return false;
        }
        pi++;
        si++;
    }
    return si == slen;
}

/* ---- element evaluation ------------------------------------------------- */

static bool eval_element(const mu_where_clause_t *filter, const mu_event_fields_t *fields, size_t element_index,
                         int depth);

/* Resolve an operand to a typed value. Element operands are evaluated as a
   boolean sub-expression (VK_INT 0/1). */
static eval_value_t resolve_operand(const mu_where_clause_t *filter, const mu_event_fields_t *fields,
                                    const mu_where_operand_t *op, int depth) {
    switch (op->kind) {
    case MU_WHERE_OPERAND_ATTRIBUTE:
        return read_event_field(fields, op->field);
    case MU_WHERE_OPERAND_LITERAL:
        return read_literal(filter, op);
    case MU_WHERE_OPERAND_ELEMENT:
        return value_int(eval_element(filter, fields, op->element_index, depth + 1) ? 1 : 0);
    default:
        return value_null();
    }
}

static bool operand_truthy(const mu_where_clause_t *filter, const mu_event_fields_t *fields,
                           const mu_where_operand_t *op, int depth) {
    if (op->kind == MU_WHERE_OPERAND_ELEMENT) {
        return eval_element(filter, fields, op->element_index, depth + 1);
    }
    eval_value_t v = resolve_operand(filter, fields, op, depth);
    return v.kind == VK_INT ? (v.i != 0) : false;
}

static bool eval_element(const mu_where_clause_t *filter, const mu_event_fields_t *fields, size_t element_index,
                         int depth) {
    if (depth > MU_WHERE_MAX_DEPTH || element_index >= filter->element_count) {
        return false;
    }
    const mu_where_element_t *el = &filter->elements[element_index];
    const mu_where_operand_t *ops = &filter->operands[el->operand_base];
    opcua_byte_t n = el->operand_count;

    switch (el->op) {
    case MU_OP_AND:
        return n >= 2 && operand_truthy(filter, fields, &ops[0], depth) &&
               operand_truthy(filter, fields, &ops[1], depth);
    case MU_OP_OR:
        return n >= 2 &&
               (operand_truthy(filter, fields, &ops[0], depth) || operand_truthy(filter, fields, &ops[1], depth));
    case MU_OP_NOT:
        return n >= 1 && !operand_truthy(filter, fields, &ops[0], depth);
    case MU_OP_ISNULL: {
        if (n < 1) {
            return false;
        }
        eval_value_t a = resolve_operand(filter, fields, &ops[0], depth);
        return a.kind == VK_NULL;
    }
    case MU_OP_EQUALS: {
        if (n < 2) {
            return false;
        }
        eval_value_t a = resolve_operand(filter, fields, &ops[0], depth);
        eval_value_t b = resolve_operand(filter, fields, &ops[1], depth);
        return value_compare(&a, &b) == 0;
    }
    case MU_OP_GREATERTHAN:
    case MU_OP_LESSTHAN:
    case MU_OP_GREATERTHANOREQUAL:
    case MU_OP_LESSTHANOREQUAL: {
        if (n < 2) {
            return false;
        }
        eval_value_t a = resolve_operand(filter, fields, &ops[0], depth);
        eval_value_t b = resolve_operand(filter, fields, &ops[1], depth);
        int c = value_compare(&a, &b);
        if (c == -2) {
            return false;
        }
        switch (el->op) {
        case MU_OP_GREATERTHAN:
            return c > 0;
        case MU_OP_LESSTHAN:
            return c < 0;
        case MU_OP_GREATERTHANOREQUAL:
            return c >= 0;
        default:
            return c <= 0;
        }
    }
    case MU_OP_BETWEEN: {
        if (n < 3) {
            return false;
        }
        eval_value_t x = resolve_operand(filter, fields, &ops[0], depth);
        eval_value_t lo = resolve_operand(filter, fields, &ops[1], depth);
        eval_value_t hi = resolve_operand(filter, fields, &ops[2], depth);
        int cl = value_compare(&x, &lo);
        int ch = value_compare(&x, &hi);
        return cl != -2 && ch != -2 && cl >= 0 && ch <= 0;
    }
    case MU_OP_INLIST: {
        if (n < 2) {
            return false;
        }
        eval_value_t x = resolve_operand(filter, fields, &ops[0], depth);
        for (opcua_byte_t j = 1; j < n; ++j) {
            eval_value_t y = resolve_operand(filter, fields, &ops[j], depth);
            if (value_compare(&x, &y) == 0) {
                return true;
            }
        }
        return false;
    }
    case MU_OP_LIKE: {
        if (n < 2) {
            return false;
        }
        eval_value_t a = resolve_operand(filter, fields, &ops[0], depth);
        eval_value_t b = resolve_operand(filter, fields, &ops[1], depth);
        if (a.kind != VK_STRING || b.kind != VK_STRING) {
            return false;
        }
        return like_match(b.s.data, b.s.length, a.s.data, a.s.length);
    }
    case MU_OP_OFTYPE: {
        /* Exact EventType match. Subtype-of resolution requires an event-type
           hierarchy this server profile does not maintain; documented as a
           boundary (conformance doc). */
        if (n < 1) {
            return false;
        }
        eval_value_t a = read_event_field(fields, MU_EVENT_FIELD_EVENTTYPE);
        eval_value_t b = resolve_operand(filter, fields, &ops[0], depth);
        return a.kind == VK_NODEID && b.kind == VK_NODEID && mu_nodeid_equal(&a.nid, &b.nid);
    }
    default:
        /* Unsupported operators are rejected at CreateMonitoredItems; if one
           reaches here, fail closed. */
        return false;
    }
}

bool mu_where_clause_eval(const mu_where_clause_t *filter, const mu_event_fields_t *fields) {
    if (filter == NULL || filter->element_count == 0) {
        return true; /* empty WhereClause matches every event */
    }
    return eval_element(filter, fields, 0, 0);
}

#endif /* MUC_OPCUA_EVENT_FILTER_WHERE */
