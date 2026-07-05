/* src/services/read.c */
#include "read.h"
#include "../address_space/base_nodes.h"
#include <stddef.h>
#include <string.h>

/* maxAge read cache (OPC-10000-4 section 5.11.2). Single-entry static cache.
 * maxAge=0 bypasses the cache (mandatory). Larger maxAge values allow
 * returning a cached value when (now - read_time) <= max_age_ms * 10000. */
/* single-entry cache — one stored Variant */

#define MU_READ_CACHE_SLOTS 1UL
#define MU_MAXAGE_ALWAYS_CACHE_THRESHOLD_MS 1e12 /* ~31 years; treat as "uncapped maxAge" */

typedef struct {
    mu_nodeid_t node_id;
    mu_variant_t variant;
    opcua_datetime_t read_time;
    opcua_boolean_t valid;
} mu_read_cache_entry_t;

static mu_read_cache_entry_t mu_read_cache[MU_READ_CACHE_SLOTS];
static size_t mu_read_cache_hits = 0;
static size_t mu_read_cache_misses = 0;

static opcua_boolean_t mu_read_cache_lookup(const mu_nodeid_t *node_id, opcua_double_t max_age_ms, opcua_datetime_t now,
                                            mu_variant_t *out) {
    (void)node_id;

    /* OPC-10000-4 §5.11.2: maxAge=0 means always read from source */
    if (max_age_ms == 0.0) {
        mu_read_cache_misses++;
        return false;
    }

    for (size_t i = 0; i < MU_READ_CACHE_SLOTS; i++) {
        if (!mu_read_cache[i].valid)
            continue;

        opcua_datetime_t age_ticks = now - mu_read_cache[i].read_time;

        /* Clock skew: cached read_time is in the future — still the freshest data we have */
        if (age_ticks < 0) {
            memcpy(out, &mu_read_cache[i].variant, sizeof(mu_variant_t));
            mu_read_cache_hits++;
            return true;
        }

        /* maxAge >= threshold: treat as "use cached unconditionally" (spec "max Double") */
        if (max_age_ms >= MU_MAXAGE_ALWAYS_CACHE_THRESHOLD_MS) {
            memcpy(out, &mu_read_cache[i].variant, sizeof(mu_variant_t));
            mu_read_cache_hits++;
            return true;
        }

        opcua_datetime_t max_age_ticks = (opcua_datetime_t)(max_age_ms * 10000.0);
        if (age_ticks <= max_age_ticks) {
            memcpy(out, &mu_read_cache[i].variant, sizeof(mu_variant_t));
            mu_read_cache_hits++;
            return true;
        }
    }

    mu_read_cache_misses++;
    return false;
}

static void mu_read_cache_store(const mu_nodeid_t *node_id, const mu_variant_t *val, opcua_datetime_t read_time) {
    (void)node_id;
    memcpy(&mu_read_cache[0].node_id, node_id, sizeof(mu_nodeid_t));
    memcpy(&mu_read_cache[0].variant, val, sizeof(mu_variant_t));
    mu_read_cache[0].read_time = read_time;
    mu_read_cache[0].valid = true;
}

void mu_read_cache_get_stats(mu_read_cache_stats_t *out) {
    if (out) {
        out->hits = mu_read_cache_hits;
        out->misses = mu_read_cache_misses;
    }
}

#ifdef MUC_OPCUA_MULTI_CHUNK
static size_t variant_elem_size(mu_builtin_type_t type) {
    switch (type) {
    case MU_TYPE_BOOLEAN:
        return sizeof(opcua_boolean_t);
    case MU_TYPE_SBYTE:
        return sizeof(opcua_sbyte_t);
    case MU_TYPE_BYTE:
        return sizeof(opcua_byte_t);
    case MU_TYPE_INT16:
        return sizeof(opcua_int16_t);
    case MU_TYPE_UINT16:
        return sizeof(opcua_uint16_t);
    case MU_TYPE_INT32:
        return sizeof(opcua_int32_t);
    case MU_TYPE_UINT32:
        return sizeof(opcua_uint32_t);
    case MU_TYPE_INT64:
        return sizeof(opcua_int64_t);
    case MU_TYPE_UINT64:
        return sizeof(opcua_uint64_t);
    case MU_TYPE_FLOAT:
        return sizeof(opcua_float_t);
    case MU_TYPE_DOUBLE:
        return sizeof(opcua_double_t);
    case MU_TYPE_STRING:
        return sizeof(mu_string_t);
    case MU_TYPE_DATETIME:
        return sizeof(opcua_datetime_t);
    case MU_TYPE_BYTESTRING:
        return sizeof(mu_bytestring_t);
    case MU_TYPE_NODEID:
        return sizeof(mu_nodeid_t);
    case MU_TYPE_EXPANDEDNODEID:
        return sizeof(mu_expanded_nodeid_t);
    case MU_TYPE_STATUSCODE:
        return sizeof(opcua_statuscode_t);
    case MU_TYPE_QUALIFIEDNAME:
        return sizeof(mu_qualified_name_t);
    case MU_TYPE_LOCALIZEDTEXT:
        return sizeof(mu_localized_text_t);
    default:
        return 0;
    }
}
#endif

#ifdef MUC_OPCUA_MULTI_CHUNK
static opcua_statuscode_t parse_numeric_range(const mu_string_t *range_str, opcua_int32_t *start, opcua_int32_t *end) {
    const char *p = (const char *)range_str->data;
    opcua_int32_t len = range_str->length;
    opcua_int32_t pos = 0;

    *start = 0;
    *end = -1;

    if (pos >= len || p[pos] < '0' || p[pos] > '9') {
        return MU_STATUS_BAD_INDEXRANGEINVALID;
    }

    while (pos < len && p[pos] >= '0' && p[pos] <= '9') {
        *start = *start * 10 + (p[pos] - '0');
        pos++;
    }

    if (pos < len && p[pos] == ':') {
        pos++;
        if (pos >= len || p[pos] < '0' || p[pos] > '9') {
            return MU_STATUS_BAD_INDEXRANGEINVALID;
        }

        *end = 0;
        while (pos < len && p[pos] >= '0' && p[pos] <= '9') {
            *end = *end * 10 + (p[pos] - '0');
            pos++;
        }
    }

    if (pos != len) {
        return MU_STATUS_BAD_INDEXRANGEINVALID;
    }

    if (*end != -1 && *start >= *end) {
        return MU_STATUS_BAD_INDEXRANGEINVALID;
    }

    return MU_STATUS_GOOD;
}

/* Apply a NumericRange to an array value (OPC-10000-4 §7.27).
   Modifies `value` in place. Returns GOOD, Bad_IndexRangeInvalid, or Bad_IndexRangeNoData. */
static opcua_statuscode_t apply_index_range(const mu_string_t *index_range, mu_variant_t *value) {
    if (!index_range || index_range->length <= 0 || !index_range->data) {
        return MU_STATUS_GOOD;
    }

    if (!value->is_array) {
        return MU_STATUS_GOOD;
    }

    if (value->array_length <= 0) {
        return MU_STATUS_GOOD;
    }

    opcua_int32_t start, end;
    opcua_statuscode_t s = parse_numeric_range(index_range, &start, &end);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (start < 0 || start >= value->array_length) {
        return MU_STATUS_BAD_INDEXRANGENODATA;
    }

    size_t esize = variant_elem_size(value->type);
    if (esize == 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    const char *arr = (const char *)value->value.array;

    if (end == -1) {
        memcpy(&value->value, arr + (size_t)start * esize, esize);
        value->is_array = false;
        value->array_length = 0;
    } else {
        if (end >= value->array_length) {
            end = value->array_length - 1;
        }
        value->value.array = arr + (size_t)start * esize;
        value->array_length = end - start + 1;
    }

    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_MULTI_CHUNK */

opcua_statuscode_t mu_read_request_decode(mu_binary_reader_t *reader, mu_read_request_t *req,
                                          mu_read_value_id_t *nodes_array, size_t max_nodes) {
    if (!reader || !req || !nodes_array) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;

    status = mu_binary_read_double(reader, &req->max_age);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    status = mu_binary_read_uint32(reader, &req->timestamps_to_return);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    opcua_int32_t no_of_nodes;
    status = mu_binary_read_int32(reader, &no_of_nodes);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (no_of_nodes < 0) {
        req->num_nodes_to_read = 0;
        return MU_STATUS_GOOD;
    }
    if ((size_t)no_of_nodes > max_nodes) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    req->num_nodes_to_read = (size_t)no_of_nodes;
    req->nodes_to_read = nodes_array;

    for (size_t i = 0; i < req->num_nodes_to_read; ++i) {
        mu_read_value_id_t *node = &req->nodes_to_read[i];

        status = mu_binary_read_nodeid(reader, &node->node_id);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_uint32(reader, &node->attribute_id);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_string(reader, &node->index_range);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_uint16(reader, &node->data_encoding_namespace_index);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        status = mu_binary_read_string(reader, &node->data_encoding_name);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_read_response_encode(mu_binary_writer_t *writer, const mu_read_response_t *resp) {
    if (!writer || !resp) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;

    /* Results array */
    if (resp->num_results == 0 || resp->results == NULL) {
        status = mu_binary_write_int32(writer, 0);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
    } else {
        status = mu_binary_write_int32(writer, (opcua_int32_t)resp->num_results);
        if (status != MU_STATUS_GOOD) {
            return status;
        }

        for (size_t i = 0; i < resp->num_results; ++i) {
            status = mu_binary_write_datavalue(writer, &resp->results[i]);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
        }
    }

    /* DiagnosticInfos array */
    status = mu_binary_write_int32(writer, 0); /* empty */
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    return MU_STATUS_GOOD;
}

static opcua_statuscode_t read_attribute(const mu_address_space_t *address_space, const mu_node_t *node,
                                         opcua_uint32_t attribute_id, mu_variant_t *value) {
    (void)address_space;
    /* Default to a scalar; array attributes (e.g. a String[] Value) set these explicitly. */
    value->is_array = false;
    value->array_length = 0;
    switch (attribute_id) {
    case MU_ATTRIBUTEID_NODEID:
        value->type = MU_TYPE_NODEID;
        value->value.nodeid = node->node_id;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_NODECLASS:
        value->type = MU_TYPE_INT32;
        value->value.i32 = (opcua_int32_t)node->node_class;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_BROWSENAME:
        /* BrowseName is a mandatory, readable Attribute of every Node
           (OPC 10000-3 5.2.4), encoded as a QualifiedName. */
        value->type = MU_TYPE_QUALIFIEDNAME;
        value->value.qualified_name.namespace_index = node->node_id.namespace_index;
        value->value.qualified_name.name = node->browse_name;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_DISPLAYNAME:
        /* DisplayName is a mandatory, readable Attribute of every Node
           (OPC 10000-3 5.2.5), encoded as a LocalizedText (null locale). */
        value->type = MU_TYPE_LOCALIZEDTEXT;
        value->value.localized_text.locale = (mu_string_t){-1, NULL};
        value->value.localized_text.text = node->display_name;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_VALUE:
        /* The Value Attribute only exists for Variable (and VariableType) Nodes;
           reading it on another NodeClass is Bad_AttributeIdInvalid (OPC 10000-3 5.6). */
        if (node->node_class != MU_NODECLASS_VARIABLE) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        if (node->value) {
            return mu_value_source_read(node->value, &node->node_id, value);
        }
        return MU_STATUS_BAD_NOTREADABLE;

#ifdef MUC_OPCUA_MULTI_CHUNK
    case MU_ATTRIBUTEID_DESCRIPTION:
        value->type = MU_TYPE_LOCALIZEDTEXT;
        value->value.localized_text.locale = (mu_string_t){-1, NULL};
        value->value.localized_text.text = (mu_string_t){-1, NULL};
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_WRITEMASK:
        value->type = MU_TYPE_UINT32;
        value->value.ui32 = 0;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_USERWRITEMASK:
        value->type = MU_TYPE_UINT32;
        value->value.ui32 = 0;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_DATATYPE:
        if (node->node_class != MU_NODECLASS_VARIABLE && node->node_class != MU_NODECLASS_VARIABLETYPE) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        value->type = MU_TYPE_NODEID;
        if (node->type_definition.namespace_index != 0 || node->type_definition.identifier_type != 0 ||
            node->type_definition.identifier.numeric != 0) {
            value->value.nodeid = node->type_definition;
        } else {
            value->value.nodeid = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {.numeric = 24}};
        }
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_VALUERANK:
        if (node->node_class != MU_NODECLASS_VARIABLE && node->node_class != MU_NODECLASS_VARIABLETYPE) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        value->type = MU_TYPE_INT32;
        value->value.i32 = -1;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_ACCESSLEVEL:
        if (node->node_class != MU_NODECLASS_VARIABLE) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        value->type = MU_TYPE_UINT32;
        value->value.ui32 = (node->value != NULL) ? 0x01 : 0;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_HISTORIZING:
        if (node->node_class != MU_NODECLASS_VARIABLE) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        value->type = MU_TYPE_BOOLEAN;
        value->value.b = false;
        return MU_STATUS_GOOD;
#endif /* MUC_OPCUA_MULTI_CHUNK */

    default:
        return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
    }
}

opcua_statuscode_t mu_read_process_with_user_index(const mu_address_space_t *address_space,
                                                   mu_address_space_index_t *user_index,
                                                   const mu_address_space_t *dynamic, const mu_read_request_t *req,
                                                   opcua_datetime_t now, mu_read_response_t *resp,
                                                   mu_datavalue_t *results_array, size_t max_results) {
    if (!req || !resp || !results_array) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    /* OPC-10000-4 section 5.11.2.3 uses TimestampsToReturn from section 7.39. */
    if (req->timestamps_to_return > MU_TIMESTAMPS_TO_RETURN_NEITHER) {
        return MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID;
    }
    if (req->num_nodes_to_read > max_results) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    resp->results = results_array;
    resp->num_results = req->num_nodes_to_read;

    for (size_t i = 0; i < req->num_nodes_to_read; ++i) {
        const mu_read_value_id_t *read_val = &req->nodes_to_read[i];
        mu_datavalue_t *dv = &resp->results[i];

        dv->has_value = false;
        dv->has_status = false;
        dv->has_source_timestamp = false;
        dv->has_source_picoseconds = false;
        dv->has_server_timestamp = false;
        dv->has_server_picoseconds = false;

        const mu_node_t *node = mu_resolve_node(address_space, user_index, dynamic, &read_val->node_id);
        if (!node) {
            dv->has_status = true;
            dv->status = MU_STATUS_BAD_NODEIDUNKNOWN;
            continue;
        }

        opcua_statuscode_t status;
        if (read_val->attribute_id == MU_ATTRIBUTEID_VALUE) {
            dv->value.is_array = false;
            dv->value.array_length = 0;
            if (node->node_class != MU_NODECLASS_VARIABLE) {
                status = MU_STATUS_BAD_ATTRIBUTEIDINVALID;
            } else if (node->value) {
                if (mu_read_cache_lookup(&read_val->node_id, req->max_age, now, &dv->value)) {
                    status = MU_STATUS_GOOD;
                } else {
                    status = mu_value_source_read(node->value, &node->node_id, &dv->value);
                    if (status == MU_STATUS_GOOD) {
                        mu_read_cache_store(&read_val->node_id, &dv->value, now);
                    }
                }
            } else {
                status = MU_STATUS_BAD_NOTREADABLE;
            }
        } else {
            status = read_attribute(address_space, node, read_val->attribute_id, &dv->value);
        }
        if (status == MU_STATUS_GOOD) {
#ifdef MUC_OPCUA_MULTI_CHUNK
            status = apply_index_range(&read_val->index_range, &dv->value);
#endif
        }
        if (status == MU_STATUS_GOOD) {
            dv->has_value = true;
            /* Populate sourceTimestamp / serverTimestamp per TimestampsToReturn.
             * OPC-10000-4 5.11.2.2 Table 47 references the TimestampsToReturn
             * enumeration (7.39 Table 180): SOURCE(0), SERVER(1), BOTH(2),
             * NEITHER(3). Without per-value capture the server's current UTC
             * time stands in for both, matching the MonitoredItem behaviour
             * in subscription_publish.c. */
            opcua_uint32_t ttr = req->timestamps_to_return;
            if (ttr == MU_TIMESTAMPS_TO_RETURN_SOURCE || ttr == MU_TIMESTAMPS_TO_RETURN_BOTH) {
                dv->has_source_timestamp = true;
                dv->source_timestamp = now;
            }
            if (ttr == MU_TIMESTAMPS_TO_RETURN_SERVER || ttr == MU_TIMESTAMPS_TO_RETURN_BOTH) {
                dv->has_server_timestamp = true;
                dv->server_timestamp = now;
            }
        } else {
            dv->has_status = true;
            dv->status = status;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_read_process(const mu_address_space_t *address_space, const mu_address_space_t *dynamic,
                                   const mu_read_request_t *req, opcua_datetime_t now, mu_read_response_t *resp,
                                   mu_datavalue_t *results_array, size_t max_results) {
    return mu_read_process_with_user_index(address_space, NULL, dynamic, req, now, resp, results_array, max_results);
}
