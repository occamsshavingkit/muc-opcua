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
static opcua_byte_t resolve_event_field_type_by_name(const mu_string_t *name) {
    static const struct {
        const char *str;
        opcua_int32_t len;
        opcua_byte_t field_type;
    } field_table[] = {
        {"EventId", 7, 1}, {"EventType", 9, 2}, {"Time", 4, 3}, {"Message", 7, 4}, {"Severity", 8, 5},
    };
    static const size_t table_size = sizeof(field_table) / sizeof(field_table[0]);

    for (size_t i = 0; i < table_size; i++) {
        if (name->length == field_table[i].len && memcmp(name->data, field_table[i].str, (size_t)name->length) == 0) {
            return field_table[i].field_type;
        }
    }
    return 0;
}

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
        mu_nodeid_t type_id;
        s = mu_binary_read_nodeid(r, &type_id);
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

        opcua_byte_t field_type = 0;
        if (last_name.length > 0 && last_name.data != NULL) {
            field_type = resolve_event_field_type_by_name(&last_name);
        }

        if (i < 8) {
            body->select_clauses[i] = field_type;
            body->select_clauses_count++;
        }
    }

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
}
#endif

#endif
