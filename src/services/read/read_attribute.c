/* src/services/read/read_attribute.c */
#include "../../address_space/base_nodes.h"
#include "common.h"
#include <stddef.h>
#include <string.h>

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

opcua_statuscode_t apply_index_range(const mu_string_t *index_range, mu_variant_t *value) {
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
#endif

#ifdef MUC_OPCUA_MULTI_CHUNK
static opcua_statuscode_t read_multichunk_attribute(const mu_node_t *node, opcua_uint32_t attribute_id,
                                                    mu_variant_t *value) {
    switch (attribute_id) {
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
    default:
        return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
    }
}
#endif

opcua_statuscode_t read_attribute(const mu_address_space_t *address_space, const mu_node_t *node,
                                  opcua_uint32_t attribute_id, mu_variant_t *value) {
    (void)address_space;
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
        value->type = MU_TYPE_QUALIFIEDNAME;
        value->value.qualified_name.namespace_index = node->node_id.namespace_index;
        value->value.qualified_name.name = node->browse_name;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_DISPLAYNAME:
        value->type = MU_TYPE_LOCALIZEDTEXT;
        value->value.localized_text.locale = (mu_string_t){-1, NULL};
        value->value.localized_text.text = node->display_name;
        return MU_STATUS_GOOD;

    case MU_ATTRIBUTEID_VALUE:
        if (node->node_class != MU_NODECLASS_VARIABLE) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        if (node->value) {
            return mu_value_source_read(node->value, &node->node_id, value);
        }
        return MU_STATUS_BAD_NOTREADABLE;

#if MUC_OPCUA_METHOD_SERVER
    case MU_ATTRIBUTEID_EXECUTABLE:
    case MU_ATTRIBUTEID_USEREXECUTABLE:
        /* Executable/UserExecutable apply only to Method nodes (OPC-10000-3 §5.7).
           A served Method is executable; per-registration non-executability is
           enforced at Call time (Bad_NotExecutable). Method Server Facet only. */
        if (node->node_class != MU_NODECLASS_METHOD) {
            return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        }
        value->type = MU_TYPE_BOOLEAN;
        value->value.b = true;
        return MU_STATUS_GOOD;
#endif
#ifdef MUC_OPCUA_MULTI_CHUNK
    default:
        return read_multichunk_attribute(node, attribute_id, value);
#else
    default:
        return MU_STATUS_BAD_ATTRIBUTEIDINVALID;
#endif
    }
}
