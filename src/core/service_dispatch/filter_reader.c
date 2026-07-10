#include "common.h"

#if MUC_OPCUA_SUBSCRIPTIONS

opcua_uint32_t publishing_interval_bits_to_ms(opcua_uint64_t bits) {
    if ((bits & MU_DOUBLE_SIGN_BIT) != 0u || bits < MU_PUBLISHING_INTERVAL_MIN_BITS) {
        return MU_MIN_PUBLISHING_INTERVAL_MS;
    }
    if (bits > MU_PUBLISHING_INTERVAL_MAX_BITS) {
        return MU_MAX_PUBLISHING_INTERVAL_MS;
    }

    opcua_uint32_t exponent = (opcua_uint32_t)((bits >> 52) & 0x7FFu);
    opcua_uint64_t fraction = bits & 0x000FFFFFFFFFFFFFULL;
    opcua_uint64_t mantissa = (1ULL << 52) | fraction;
    opcua_uint32_t unbiased = exponent - 1023u;

    if (unbiased >= 52u) {
        return (opcua_uint32_t)(mantissa << (unbiased - 52u));
    }
    return (opcua_uint32_t)(mantissa >> (52u - unbiased));
}

opcua_uint64_t publishing_interval_ms_to_bits(opcua_uint32_t interval_ms) {
    opcua_uint32_t value = interval_ms;
    opcua_uint32_t exponent = 0u;
    while ((value >> 1u) != 0u) {
        value >>= 1u;
        ++exponent;
    }

    opcua_uint64_t leading = (opcua_uint64_t)1u << exponent;
    opcua_uint64_t fraction = ((opcua_uint64_t)interval_ms - leading) << (52u - exponent);
    return ((opcua_uint64_t)(exponent + 1023u) << 52) | fraction;
}

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
bool is_datachange_filter_binary_type(const mu_nodeid_t *type_id) {
    return type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
           type_id->identifier.numeric == MU_ID_DATACHANGEFILTER_ENCODING_DEFAULTBINARY;
}

bool is_aggregate_filter_binary_type(const mu_nodeid_t *type_id) {
    return type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
           type_id->identifier.numeric == MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY;
}

#ifdef MUC_OPCUA_EVENTS
bool is_event_filter_binary_type(const mu_nodeid_t *type_id) {
    return type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
           type_id->identifier.numeric == MU_ID_EVENTFILTER_ENCODING_DEFAULTBINARY;
}
#endif

static bool is_null_extension_object_type(const mu_nodeid_t *type_id, size_t length) {
    return type_id->identifier_type == MU_NODEID_NUMERIC && type_id->namespace_index == 0u &&
           type_id->identifier.numeric == 0u && length == 0u;
}

bool is_known_monitoring_filter_binary_type(const mu_nodeid_t *type_id, size_t length) {
    return is_null_extension_object_type(type_id, length) || is_datachange_filter_binary_type(type_id) ||
           is_aggregate_filter_binary_type(type_id)
#ifdef MUC_OPCUA_EVENTS
           || is_event_filter_binary_type(type_id)
#endif
        ;
}

static bool is_numeric_variant_type(mu_builtin_type_t type) {
    switch (type) {
    case MU_TYPE_SBYTE:
    case MU_TYPE_BYTE:
    case MU_TYPE_INT16:
    case MU_TYPE_UINT16:
    case MU_TYPE_INT32:
    case MU_TYPE_UINT32:
    case MU_TYPE_INT64:
    case MU_TYPE_UINT64:
    case MU_TYPE_FLOAT:
    case MU_TYPE_DOUBLE:
        return true;
    default:
        return false;
    }
}

bool monitored_node_has_numeric_static_value(const mu_node_t *node) {
    if (node == NULL || node->value == NULL) {
        return true;
    }
    if (node->value->type != MU_VALUESOURCE_STATIC) {
        return true;
    }
    if (node->value->data.static_value.is_array) {
        return false;
    }
    return is_numeric_variant_type(node->value->data.static_value.type);
}

static void set_datachange_trigger_from_wire(mu_monitored_item_create_body_t *body, opcua_uint32_t trigger) {
    switch (trigger) {
    case 0u:
        body->trigger = MU_DATACHANGE_TRIGGER_STATUS;
        break;
    case 1u:
        body->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        break;
    case 2u:
        body->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE_TIMESTAMP;
        break;
    default:
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERINVALID;
        break;
    }
}

opcua_statuscode_t read_datachange_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                               mu_monitored_item_create_body_t *body) {
    opcua_uint32_t trigger;
    opcua_uint32_t deadband_type;
    opcua_double_t deadband_value;

    if (filter_length == 0u) {
        return MU_STATUS_GOOD;
    }
    if (filter_length != 16u) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (r->position > r->length || filter_length > r->length - r->position) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    mu_binary_read_uint32(r, &trigger);
    mu_binary_read_uint32(r, &deadband_type);
    mu_binary_read_double(r, &deadband_value);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    set_datachange_trigger_from_wire(body, trigger);
    switch (deadband_type) {
    case 0u:
        body->deadband_type = MU_DEADBAND_TYPE_NONE;
        body->deadband_value = 0.0;
        break;
    case 1u:
        body->deadband_type = MU_DEADBAND_TYPE_ABSOLUTE;
        body->deadband_value = deadband_value;
        break;
    case 2u:
        body->deadband_type = MU_DEADBAND_TYPE_PERCENT;
        body->deadband_value = deadband_value;
#if MUC_OPCUA_DATA_ACCESS
        /* Data Access Server Facet (spec 060): PercentDeadband is supported. The
           value must be a percentage in [0.0, 100.0] (OPC-10000-8 §7.2); the
           EURange presence check happens at item creation (§7.3.2). */
        if (body->filter_result == MU_STATUS_GOOD && (deadband_value < 0.0 || deadband_value > 100.0)) {
            body->filter_result = MU_STATUS_BAD_DEADBANDFILTERINVALID;
        }
#else
        /* Without the Data Access facet the server does not support percent
           deadband (it belongs to that facet, not the base data-change subset). */
        if (body->filter_result == MU_STATUS_GOOD) {
            body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        }
#endif
        break;
    default:
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERINVALID;
        break;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t read_aggregate_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                              mu_monitored_item_create_body_t *body) {
    opcua_datetime_t start_time;
    mu_nodeid_t aggregate_type;
    opcua_double_t processing_interval;
    opcua_boolean_t use_defaults;
    opcua_boolean_t treat_uncertain;
    opcua_byte_t percent_bad;
    opcua_byte_t percent_good;
    opcua_boolean_t sloped_extrap;

    if (filter_length == 0u) {
        body->filter_result = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_GOOD;
    }
    if (r->position > r->length || filter_length > r->length - r->position) {
        body->filter_result = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }

    opcua_statuscode_t s = mu_binary_read_int64(r, (opcua_int64_t *)&start_time);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_read_nodeid(r, &aggregate_type);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = mu_binary_read_double(r, &processing_interval);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_binary_read_boolean(r, &use_defaults);
    mu_binary_read_boolean(r, &treat_uncertain);
    mu_binary_read_byte(r, &percent_bad);
    mu_binary_read_byte(r, &percent_good);
    mu_binary_read_boolean(r, &sloped_extrap);
    if (r->status != MU_STATUS_GOOD) {
        return r->status;
    }

    body->has_aggregate = true;
    body->processing_interval = processing_interval;

    if (aggregate_type.identifier_type == MU_NODEID_NUMERIC && aggregate_type.namespace_index == 0u) {
        body->aggregate_type = aggregate_type.identifier.numeric;
    } else {
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        return MU_STATUS_GOOD;
    }

    if (body->aggregate_type != MU_ID_AGGREGATETYPE_AVERAGE && body->aggregate_type != MU_ID_AGGREGATETYPE_MINIMUM &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_MAXIMUM
#ifdef MUC_OPCUA_AGGREGATE_FULL
        && body->aggregate_type != MU_ID_AGGREGATETYPE_COUNT && body->aggregate_type != MU_ID_AGGREGATETYPE_RANGE &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_DELTA && body->aggregate_type != MU_ID_AGGREGATETYPE_DELTA_BOUNDS &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_DURATION_GOOD &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_DURATION_BAD &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_DURATION_IN_STATE_ZERO &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_START && body->aggregate_type != MU_ID_AGGREGATETYPE_END &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_INTERPOLATIVE &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_MAXIMUM_2 &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_MINIMUM_2 &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_PERCENT_GOOD &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_PERCENT_BAD &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_TIME_AVERAGE &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_TIME_AVERAGE_2 &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_TOTAL && body->aggregate_type != MU_ID_AGGREGATETYPE_TOTAL_2 &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_WORST_QUALITY &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_WORST_QUALITY_2 &&
        body->aggregate_type != MU_ID_AGGREGATETYPE_ANNOTATION_COUNT
#endif
    ) {
        body->filter_result = MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED;
        return MU_STATUS_GOOD;
    }

    if (processing_interval <= 0.0) {
        body->filter_result = MU_STATUS_BAD_FILTERNOTALLOWED;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_GOOD;
}
#endif

#ifdef MUC_OPCUA_EVENTS
/* Unified SimpleAttributeOperand BrowseName → BaseEventType field index (0-8),
   shared by the select-clause loop and the where-clause decoder below. Declared
   in services/event_filter.h. */
mu_event_field_t mu_event_field_from_name(const mu_string_t *name) {
    static const struct {
        const char *str;
        opcua_int32_t len;
        mu_event_field_t field;
    } table[] = {
        {"EventId", 7, MU_EVENT_FIELD_EVENTID},
        {"EventType", 9, MU_EVENT_FIELD_EVENTTYPE},
        {"SourceNode", 10, MU_EVENT_FIELD_SOURCENODE},
        {"SourceName", 10, MU_EVENT_FIELD_SOURCENAME},
        {"Time", 4, MU_EVENT_FIELD_TIME},
        {"ReceiveTime", 11, MU_EVENT_FIELD_RECEIVETIME},
        {"LocalTime", 9, MU_EVENT_FIELD_LOCALTIME},
        {"Message", 7, MU_EVENT_FIELD_MESSAGE},
        {"Severity", 8, MU_EVENT_FIELD_SEVERITY},
    };
    if (name == NULL || name->data == NULL || name->length <= 0) {
        return MU_EVENT_FIELD_NONE;
    }
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (name->length == table[i].len && memcmp(name->data, table[i].str, (size_t)name->length) == 0) {
            return table[i].field;
        }
    }
    return MU_EVENT_FIELD_NONE;
}

/* Read a SimpleAttributeOperand BrowsePath (QualifiedName array) + attributeId +
   indexRange, returning the terminal BrowseName. Consumes exactly the operand's
   fields so the reader stays aligned to the ContentFilter body. */
static opcua_statuscode_t read_simple_attribute_fields(mu_binary_reader_t *r, mu_string_t *out_last_name) {
    mu_nodeid_t type_def;
    opcua_statuscode_t s = mu_binary_read_nodeid(r, &type_def);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    opcua_int32_t bp_count;
    s = mu_binary_read_int32(r, &bp_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (bp_count < 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    mu_string_t last_name = {-1, NULL};
    for (opcua_int32_t j = 0; j < bp_count; ++j) {
        opcua_uint16_t ns;
        mu_string_t name;
        mu_binary_read_uint16(r, &ns);
        s = mu_binary_read_string(r, &name);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        last_name = name;
    }
    opcua_uint32_t attr_id;
    mu_binary_read_uint32(r, &attr_id);
    mu_string_t idx_range;
    s = mu_binary_read_string(r, &idx_range);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    *out_last_name = last_name;
    return r->status;
}

#if MUC_OPCUA_EVENT_FILTER_WHERE

/* Supported FilterOperators (OPC-10000-4 §7.7.3). InView/RelatedTo/Cast/Bitwise
   are out of scope for this facet → rejected with Bad_FilterOperatorUnsupported. */
static bool where_operator_supported(opcua_uint32_t op) {
    switch (op) {
    case 0u:  /* Equals */
    case 1u:  /* IsNull */
    case 2u:  /* GreaterThan */
    case 3u:  /* LessThan */
    case 4u:  /* GreaterThanOrEqual */
    case 5u:  /* LessThanOrEqual */
    case 6u:  /* Like */
    case 7u:  /* Not */
    case 8u:  /* Between */
    case 9u:  /* InList */
    case 10u: /* And */
    case 11u: /* Or */
    case 14u: /* OfType */
        return true;
    default:
        return false;
    }
}

/* Copy bytes into the clause blob, returning false (no write) on overflow so the
   caller can reject the whole filter without desyncing the reader. */
static bool where_blob_append(mu_where_clause_t *wc, const opcua_byte_t *data, opcua_int32_t len,
                              opcua_uint16_t *out_off, opcua_uint16_t *out_len) {
    if (len < 0) {
        len = 0;
    }
    if ((size_t)wc->blob_len + (size_t)len > MU_INTERN_WHERE_BLOB_BYTES) {
        return false;
    }
    *out_off = wc->blob_len;
    *out_len = (opcua_uint16_t)len;
    if (len > 0 && data != NULL) {
        memcpy(&wc->blob[wc->blob_len], data, (size_t)len);
        wc->blob_len = (opcua_uint16_t)(wc->blob_len + (opcua_uint16_t)len);
    }
    return true;
}

/* Convert an already-decoded LiteralOperand Variant into the compact operand
   form. Returns false if a string/bytestring/NodeId payload overflows the blob. */
static bool literal_to_operand(mu_where_clause_t *wc, const mu_variant_t *v, mu_where_operand_t *out) {
    out->kind = MU_WHERE_OPERAND_LITERAL;
    out->lit_type = (opcua_byte_t)v->type;
    switch (v->type) {
    case MU_TYPE_BOOLEAN:
        out->num.i = v->value.b ? 1 : 0;
        return true;
    case MU_TYPE_SBYTE:
        out->num.i = v->value.sb;
        return true;
    case MU_TYPE_BYTE:
        out->num.i = v->value.by;
        return true;
    case MU_TYPE_INT16:
        out->num.i = v->value.i16;
        return true;
    case MU_TYPE_UINT16:
        out->num.i = v->value.ui16;
        return true;
    case MU_TYPE_INT32:
        out->num.i = v->value.i32;
        return true;
    case MU_TYPE_UINT32:
        out->num.i = v->value.ui32;
        return true;
    case MU_TYPE_INT64:
        out->num.i = v->value.i64;
        return true;
    case MU_TYPE_UINT64:
        out->num.i = (opcua_int64_t)v->value.ui64;
        return true;
    case MU_TYPE_DATETIME:
        out->num.i = (opcua_int64_t)v->value.dt;
        return true;
    case MU_TYPE_STATUSCODE:
        out->num.i = (opcua_int64_t)v->value.status;
        return true;
    case MU_TYPE_FLOAT:
        out->num.d = (opcua_double_t)v->value.f;
        return true;
    case MU_TYPE_DOUBLE:
        out->num.d = v->value.d;
        return true;
    case MU_TYPE_STRING:
        return where_blob_append(wc, v->value.str.data, v->value.str.length, &out->blob_off, &out->blob_len);
    case MU_TYPE_BYTESTRING:
        return where_blob_append(wc, v->value.bytestr.data, v->value.bytestr.length, &out->blob_off, &out->blob_len);
    case MU_TYPE_NODEID:
        out->nodeid_ns = v->value.nodeid.namespace_index;
        if (v->value.nodeid.identifier_type == MU_NODEID_NUMERIC) {
            out->num.nodeid_numeric = v->value.nodeid.identifier.numeric;
            out->blob_len = 0;
            return true;
        }
        if (v->value.nodeid.identifier_type == MU_NODEID_STRING) {
            return where_blob_append(wc, v->value.nodeid.identifier.string.data,
                                     v->value.nodeid.identifier.string.length, &out->blob_off, &out->blob_len);
        }
        out->kind = MU_WHERE_OPERAND_UNSUPPORTED;
        return true;
    default:
        out->kind = MU_WHERE_OPERAND_UNSUPPORTED;
        return true;
    }
}

/* Decode one FilterOperand ExtensionObject into compact form, always consuming
   its bytes. *reject is set (clause rejected) on an unsupported operand type or a
   blob overflow; genuine truncation returns a Bad_ status. */
static opcua_statuscode_t decode_where_operand(mu_binary_reader_t *r, mu_where_clause_t *wc, mu_where_operand_t *out,
                                               bool *reject) {
    mu_nodeid_t ext_id;
    size_t ext_len;
    opcua_statuscode_t s = mu_binary_read_extension_object_header(r, &ext_id, &ext_len);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    memset(out, 0, sizeof(*out));
    if (ext_id.identifier_type == MU_NODEID_NUMERIC && ext_id.namespace_index == 0u) {
        switch (ext_id.identifier.numeric) {
        case MU_ID_ELEMENTOPERAND_ENCODING_DEFAULTBINARY:
            out->kind = MU_WHERE_OPERAND_ELEMENT;
            return mu_binary_read_uint32(r, &out->element_index);
        case MU_ID_LITERALOPERAND_ENCODING_DEFAULTBINARY: {
            mu_variant_t v;
            memset(&v, 0, sizeof(v));
            s = mu_binary_read_variant(r, &v);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            if (v.is_array || !literal_to_operand(wc, &v, out)) {
                out->kind = MU_WHERE_OPERAND_UNSUPPORTED;
                *reject = true;
            }
            return r->status;
        }
        case MU_ID_SIMPLEATTRIBUTEOPERAND_ENCODING_DEFAULTBINARY: {
            mu_string_t last_name;
            s = read_simple_attribute_fields(r, &last_name);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            out->kind = MU_WHERE_OPERAND_ATTRIBUTE;
            out->field = (opcua_byte_t)mu_event_field_from_name(&last_name);
            return r->status;
        }
        default:
            break;
        }
    }
    /* Unknown/unsupported operand: consume its body and reject the filter. */
    out->kind = MU_WHERE_OPERAND_UNSUPPORTED;
    *reject = true;
    return skip_extension_object_body(r, ext_len);
}

/* Decode the ContentFilter WhereClause into the item-owned compact form. Always
   consumes the whole clause; capacity/operator violations set *filter_result and
   leave an empty (match-all is NOT applied — the non-good result fails the item)
   clause rather than truncating. OPC-10000-4 §7.7.1. */
static opcua_statuscode_t decode_where_clause(mu_binary_reader_t *r, mu_where_clause_t *wc,
                                              opcua_statuscode_t *filter_result) {
    memset(wc, 0, sizeof(*wc));

    opcua_int32_t element_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &element_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (element_count < 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    bool reject = false;
    bool overflow = (size_t)element_count > MU_INTERN_MAX_WHERE_ELEMENTS;
    size_t op_index = 0;

    for (opcua_int32_t i = 0; i < element_count; ++i) {
        opcua_uint32_t op = 0;
        s = mu_binary_read_uint32(r, &op);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        opcua_int32_t arg_count = 0;
        s = mu_binary_read_int32(r, &arg_count);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (arg_count < 0) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        if (!where_operator_supported(op)) {
            reject = true;
            if (*filter_result == MU_STATUS_GOOD) {
                *filter_result = MU_STATUS_BAD_FILTEROPERATORUNSUPPORTED;
            }
        }
        bool store_el = !overflow && (size_t)i < MU_INTERN_MAX_WHERE_ELEMENTS;
        if (store_el) {
            wc->elements[i].op = (opcua_byte_t)op;
            wc->elements[i].operand_base = (opcua_byte_t)op_index;
            wc->elements[i].operand_count = (opcua_byte_t)arg_count;
        }
        for (opcua_int32_t j = 0; j < arg_count; ++j) {
            mu_where_operand_t tmp;
            s = decode_where_operand(r, wc, &tmp, &reject);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            if (store_el && op_index < MU_INTERN_MAX_WHERE_OPERANDS) {
                wc->operands[op_index] = tmp;
            } else {
                overflow = true;
            }
            op_index++;
        }
    }

    if (overflow && *filter_result == MU_STATUS_GOOD) {
        *filter_result = MU_STATUS_BAD_TOOMANYOPERATIONS;
    }
    if (reject || overflow || *filter_result != MU_STATUS_GOOD) {
        wc->element_count = 0;
        wc->operand_count = 0;
        wc->blob_len = 0;
    } else {
        wc->element_count = (opcua_byte_t)element_count;
        wc->operand_count = (opcua_byte_t)op_index;
    }
    return r->status;
}
#endif /* MUC_OPCUA_EVENT_FILTER_WHERE */

opcua_statuscode_t read_event_filter_body(mu_binary_reader_t *r, size_t filter_length,
                                          mu_monitored_item_create_body_t *body) {
    (void)filter_length;
    body->select_clauses_count = 0;
    memset(body->select_clauses, 0, sizeof(body->select_clauses));
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    body->filter_result = MU_STATUS_GOOD;
#endif

    opcua_int32_t select_count;
    opcua_statuscode_t s = mu_binary_read_int32(r, &select_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (select_count < 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (select_count > 100) {
        return MU_STATUS_BAD_EVENTFILTERINVALID;
    }

    for (opcua_int32_t i = 0; i < select_count; ++i) {
        mu_string_t last_name;
        s = read_simple_attribute_fields(r, &last_name);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (i < 8) {
            body->select_clauses[i] = (opcua_byte_t)mu_event_field_from_name(&last_name);
            body->select_clauses_count++;
        }
    }

#if MUC_OPCUA_EVENT_FILTER_WHERE
    return decode_where_clause(r, &body->where_clause, &body->filter_result);
#else
    /* WhereClause structure must still be consumed even when evaluation is off. */
    opcua_int32_t element_count;
    s = mu_binary_read_int32(r, &element_count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (element_count < 0) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    for (opcua_int32_t i = 0; i < element_count; ++i) {
        opcua_uint32_t op_type;
        mu_binary_read_uint32(r, &op_type);
        opcua_int32_t arg_count;
        mu_binary_read_int32(r, &arg_count);
        if (arg_count < 0) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        for (opcua_int32_t j = 0; j < arg_count; ++j) {
            mu_nodeid_t ext_id;
            size_t ext_len;
            s = mu_binary_read_extension_object_header(r, &ext_id, &ext_len);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
            s = skip_extension_object_body(r, ext_len);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
    }
    return r->status;
#endif
}
#endif

#endif
