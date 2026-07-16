/* service_dispatch/attribute_handler.c - Read/Write attribute handlers */
#include "common.h"

#include <stdlib.h>
#include <string.h>

/* Per-request operation bounds (bounded, stack-allocated). Sized so a standards
   client's connect-time batch reads (e.g. the .NET stack reading the ~12
   ServerCapabilities/OperationLimits properties at once) are accepted rather than
   rejected with Bad_TooManyOperations; missing nodes return per-node
   Bad_NodeIdUnknown, which clients tolerate. */
#define MU_DISPATCH_MAX_READ_NODES 32

/* Spec 057: the advertised MaxNodesPerRead/MaxNodesPerWrite (base_nodes.c) must
 * equal the value actually enforced here (Read and Write share this bound). */
_Static_assert(MU_DISPATCH_MAX_READ_NODES == MU_MAX_NODES_PER_READ &&
                   MU_DISPATCH_MAX_READ_NODES == MU_MAX_NODES_PER_WRITE,
               "advertised MaxNodesPerRead/Write must match the enforced dispatch bound");

#ifdef MUC_OPCUA_CU_ATTRIBUTE_READ
/* mu_read_process_with_user_index is defined in src/services/read.c. It is not
   declared in read.h (only mu_read_process is) so forward-declare it here for
   the dispatch handler. OPC-10000-4 5.11.2. */
extern opcua_statuscode_t mu_read_process_with_user_index(const mu_address_space_t *address_space,
                                                          mu_address_space_index_t *user_index,
                                                          const mu_address_space_t *dynamic,
                                                          const mu_read_request_t *req, opcua_datetime_t now,
                                                          mu_read_response_t *resp, mu_datavalue_t *results_array,
                                                          size_t max_results, mu_read_cache_t *cache);
#endif

#ifdef MUC_OPCUA_CU_ATTRIBUTE_READ
/* Read (OPC 10000-4 5.11.2): decode the request after the RequestHeader, read each
   attribute from the address space, and encode the ReadResponse. */
opcua_statuscode_t handle_read(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                               size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_read_request_t rreq;
    mu_read_value_id_t nodes[MU_DISPATCH_MAX_READ_NODES];
    s = mu_read_request_decode(r, &rreq, nodes, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_read_response_t rresp;
    mu_datavalue_t results[MU_DISPATCH_MAX_READ_NODES];
    /* OPC-10000-4 5.11.2.2 Table 47: timestampsToReturn controls which
     * timestamp Attributes accompany each Value. Source the current UTC time
     * from the server's time adapter (mirrors subscription_publish.c). */
    opcua_datetime_t now = server->config.time_adapter.get_time
                               ? server->config.time_adapter.get_time(server->config.time_adapter.context)
                               : 0;
#if MUC_OPCUA_READ_CACHE
    mu_read_cache_t *read_cache = &server->read_cache;
#else
    mu_read_cache_t *read_cache = NULL;
#endif
    s = mu_read_process_with_user_index(server->config.address_space, &server->user_address_space_index,
                                        &server->runtime_base.space, &rreq, now, &rresp, results,
                                        MU_DISPATCH_MAX_READ_NODES, read_cache);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    s = write_response_prefix(w, MU_ID_READRESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_CU_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    s = mu_read_response_encode(w, &rresp);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_CU_ATTRIBUTE_READ */

#ifdef MUC_OPCUA_SERVICE_WRITE
#ifdef MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE
/* OPC-10000-4 5.11.4.2 requires an IndexRange write to carry an array Value.
 * Array Values are only decodable when heap is available (see binary_variant.c:
 * a no-heap build returns Bad_OutOfMemory for any non-empty array). IndexRange
 * therefore cannot function on the strictly no-heap MCU tiers (nano/micro/
 * embedded), so it must never be enabled there. The manifest keeps its
 * profile_defaults off for those tiers; this assert makes the invariant a
 * build error for any other (e.g. custom) misconfiguration. */
_Static_assert(MUC_OPCUA_ALLOW_HEAP, "MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE requires MUC_OPCUA_ALLOW_HEAP "
                                     "(IndexRange writes carry an array Value, which needs heap to decode)");

/* Byte size of one array element for the partial-update merge below.
 * OPC-10000-4 5.11.4: IndexRange applies to the built-in element type carried
 * by the array Variant, so this mirrors the full set of MU_TYPE_* Variant
 * members rather than only the numeric ones exercised by current tests. */
static size_t write_index_range_elem_size(mu_builtin_type_t type) {
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

/* Strict one-dimensional IndexRange parser for Write (OPC-10000-4 5.11.4).
 * Unlike the Read-side range narrowing (read_attribute.c), an out-of-bounds
 * end index is rejected rather than clamped: a partial-update target must be
 * fully inside the current array. On success *start is the first affected
 * element and *count is the number of elements the range selects. */
static opcua_statuscode_t parse_write_index_range(const mu_string_t *range_str, opcua_int32_t array_length,
                                                  opcua_int32_t *start, opcua_int32_t *count) {
    const char *p = (const char *)range_str->data;
    opcua_int32_t len = range_str->length;
    opcua_int32_t pos = 0;
    opcua_int32_t range_start = 0;
    opcua_int32_t range_end = -1;

    if (len <= 0 || !p || p[pos] < '0' || p[pos] > '9') {
        return MU_STATUS_BAD_INDEXRANGEINVALID;
    }

    while (pos < len && p[pos] >= '0' && p[pos] <= '9') {
        range_start = range_start * 10 + (p[pos] - '0');
        pos++;
    }

    if (pos < len && p[pos] == ':') {
        pos++;
        if (pos >= len || p[pos] < '0' || p[pos] > '9') {
            return MU_STATUS_BAD_INDEXRANGEINVALID;
        }
        range_end = 0;
        while (pos < len && p[pos] >= '0' && p[pos] <= '9') {
            range_end = range_end * 10 + (p[pos] - '0');
            pos++;
        }
    }

    if (pos != len || (range_end != -1 && range_start >= range_end) || range_start >= array_length) {
        return MU_STATUS_BAD_INDEXRANGEINVALID;
    }

    if (range_end == -1) {
        *start = range_start;
        *count = 1;
    } else {
        if (range_end >= array_length) {
            return MU_STATUS_BAD_INDEXRANGEINVALID;
        }
        *start = range_start;
        *count = range_end - range_start + 1;
    }

    return MU_STATUS_GOOD;
}

/* Apply a Write IndexRange partial array update: read the node's current
 * array, merge the WriteValue's array into the selected slice, and dispatch
 * the merged full array to the configured write callback. OPC-10000-4
 * 5.11.4.2: "A Server shall return a Bad_WriteNotSupported error if an
 * indexRange is provided and writing of indexRange is not possible for the
 * Node" (e.g. the Node's current value is not an array). */
static opcua_statuscode_t write_value_index_range(mu_server_t *server, const mu_node_t *node,
                                                  const mu_write_value_t *write_val) {
    if (!node->value) {
        return MU_STATUS_BAD_WRITENOTSUPPORTED;
    }

    mu_variant_t current;
    opcua_statuscode_t s = mu_value_source_read(node->value, &node->node_id, &current);
    if (s != MU_STATUS_GOOD) {
        return s;
    }
    if (!current.is_array || current.array_length <= 0) {
        return MU_STATUS_BAD_WRITENOTSUPPORTED;
    }

    /* OPC-10000-4 5.11.4.2: "If the indexRange parameter is specified then
     * the Value shall be an array even if only one element is being
     * written." */
    if (!write_val->value.value.is_array) {
        return MU_STATUS_BAD_TYPEMISMATCH;
    }

    opcua_int32_t start, count;
    s = parse_write_index_range(&write_val->index_range, current.array_length, &start, &count);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (!mu_variant_type_is_assignable(current.type, write_val->value.value.type)) {
        return MU_STATUS_BAD_TYPEMISMATCH;
    }

    const mu_variant_t *patch = &write_val->value.value;
    if (patch->array_length != count) {
        return MU_STATUS_BAD_INDEXRANGEDATAMISMATCH;
    }

    size_t esize = write_index_range_elem_size(current.type);
    if (esize == 0) {
        return MU_STATUS_BAD_WRITENOTSUPPORTED;
    }

    /* Bounded by the address space's own configured array length (fixed at
     * integration time), not by attacker-controlled input. */
    opcua_byte_t merged[(size_t)current.array_length * esize];
    (void)memcpy(merged, current.value.array, (size_t)current.array_length * esize);
    (void)memcpy(merged + (size_t)start * esize, patch->value.array, (size_t)count * esize);

    mu_datavalue_t effective_value = write_val->value;
    effective_value.value = current;
    effective_value.value.value.array = merged;
    effective_value.value.array_length = current.array_length;

    mu_write_handler_t write_handler = server->config.write_handler;
    if (!write_handler) {
        return MU_STATUS_BAD_WRITENOTSUPPORTED;
    }
    return write_handler(server->config.write_handler_handle, &write_val->node_id,
                         (opcua_uint32_t)write_val->attribute_id, &effective_value);
}
#endif /* MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE */

static bool write_attribute_is_valid_for_node(const mu_node_t *node, opcua_uint32_t attribute_id) {
    switch (attribute_id) {
    case MU_ATTRIBUTEID_NODEID:
    case MU_ATTRIBUTEID_NODECLASS:
    case MU_ATTRIBUTEID_BROWSENAME:
    case MU_ATTRIBUTEID_DISPLAYNAME:
    case MU_ATTRIBUTEID_DESCRIPTION:
    case MU_ATTRIBUTEID_WRITEMASK:
    case MU_ATTRIBUTEID_USERWRITEMASK:
        return true;
    case MU_ATTRIBUTEID_VALUE:
        return true;
    case MU_ATTRIBUTEID_DATATYPE:
    case MU_ATTRIBUTEID_VALUERANK:
        return node->node_class == MU_NODECLASS_VARIABLE || node->node_class == MU_NODECLASS_VARIABLETYPE;
    case MU_ATTRIBUTEID_ACCESSLEVEL:
    case MU_ATTRIBUTEID_USERACCESSLEVEL:
    case MU_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL:
    case MU_ATTRIBUTEID_HISTORIZING:
        return node->node_class == MU_NODECLASS_VARIABLE;
    case MU_ATTRIBUTEID_EXECUTABLE:
    case MU_ATTRIBUTEID_USEREXECUTABLE:
        return node->node_class == MU_NODECLASS_METHOD;
    case MU_ATTRIBUTEID_EVENTNOTIFIER:
        return node->node_class == MU_NODECLASS_OBJECT || node->node_class == MU_NODECLASS_VIEW;
    case MU_ATTRIBUTEID_ISABSTRACT:
        return node->node_class == MU_NODECLASS_OBJECTTYPE || node->node_class == MU_NODECLASS_VARIABLETYPE ||
               node->node_class == MU_NODECLASS_REFERENCETYPE || node->node_class == MU_NODECLASS_DATATYPE;
    case MU_ATTRIBUTEID_SYMMETRIC:
    case MU_ATTRIBUTEID_INVERSENAME:
        return node->node_class == MU_NODECLASS_REFERENCETYPE;
    case MU_ATTRIBUTEID_CONTAINSNOLOOPS:
        return node->node_class == MU_NODECLASS_VIEW;
    default:
        return false;
    }
}

opcua_statuscode_t handle_write(mu_server_t *server, mu_binary_reader_t *r, mu_binary_writer_t *w,
                                size_t *response_length) {
    mu_request_header_t req;
    opcua_statuscode_t s = mu_request_header_decode(r, &req);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    mu_write_request_t wreq;
    mu_write_value_t nodes[MU_DISPATCH_MAX_READ_NODES];
    s = mu_write_request_decode(r, &wreq, nodes, MU_DISPATCH_MAX_READ_NODES);
    if (s != MU_STATUS_GOOD) {
        return s;
    }

    if (wreq.num_nodes_to_write == 0) {
        return MU_STATUS_BAD_NOTHINGTODO;
    }

    mu_write_response_t wresp;
    opcua_statuscode_t results[MU_DISPATCH_MAX_READ_NODES];
    wresp.num_results = wreq.num_nodes_to_write;
    wresp.results = results;

    for (size_t i = 0; i < wreq.num_nodes_to_write; ++i) {
        const mu_write_value_t *write_val = &wreq.nodes_to_write[i];
        opcua_statuscode_t result = MU_STATUS_GOOD;
        bool handled = false;

        const mu_node_t *node = mu_resolve_node(server->config.address_space, &server->user_address_space_index,
                                                &server->runtime_base.space, &write_val->node_id);
        if (!node) {
            result = MU_STATUS_BAD_NODEIDUNKNOWN;
        } else if (!write_attribute_is_valid_for_node(node, write_val->attribute_id)) {
            result = MU_STATUS_BAD_ATTRIBUTEIDINVALID;
        } else if (write_val->attribute_id != MU_ATTRIBUTEID_VALUE) {
            result = MU_STATUS_BAD_NOTWRITABLE;
#ifndef MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP
        } else if (write_val->value.has_status || write_val->value.has_source_timestamp ||
                   write_val->value.has_source_picoseconds || write_val->value.has_server_timestamp ||
                   write_val->value.has_server_picoseconds) {
            result = MU_STATUS_BAD_WRITENOTSUPPORTED;
#endif
        } else if (!write_val->value.has_value) {
            /* OPC-10000-4 section 5.11.4.2: Value Attribute writes require DataValue.value. */
            result = MU_STATUS_BAD_TYPEMISMATCH;
        } else if (node->node_class != MU_NODECLASS_VARIABLE) {
            result = MU_STATUS_BAD_NOTWRITABLE;
        } else if (write_val->index_range.length > 0) {
#ifdef MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE
            /* opc_cu_3147: bounded partial array update, OPC-10000-4 5.11.4. */
            result = write_value_index_range(server, node, write_val);
#else
            result = MU_STATUS_BAD_WRITENOTSUPPORTED;
#endif
            handled = true;
        } else if (node->value) {
            /* Check value type is assignable if the variable has a current value.
             * OPC-10000-4 5.11.4.2 Table 53: subtypes of the Attribute DataType
             * shall be accepted by the Server. mu_variant_type_is_assignable
             * encodes that rule (currently exact-match for built-in types,
             * with infrastructure for TypeDef-based subtypes). */
            mu_variant_t current_val;
            s = mu_value_source_read(node->value, &node->node_id, &current_val);
            if (s == MU_STATUS_GOOD && !mu_variant_type_is_assignable(current_val.type, write_val->value.value.type)) {
                result = MU_STATUS_BAD_TYPEMISMATCH;
            }
        }

        if (!handled && result == MU_STATUS_GOOD) {
            /* Apply write callback if configured */
            mu_write_handler_t write_handler = server->config.write_handler;
            void *write_handler_handle = server->config.write_handler_handle;
            if (write_handler) {
                result = write_handler(write_handler_handle, &write_val->node_id,
                                       (opcua_uint32_t)write_val->attribute_id, &write_val->value);
            } else {
                result = MU_STATUS_BAD_WRITENOTSUPPORTED;
            }
        }

        results[i] = result;
#if MUC_OPCUA_CU_AUDITING
        if (result == MU_STATUS_GOOD) {
            /* spec 074: emit an AuditWriteUpdateEvent (i=2100) per successful
               attribute write (OPC-10000-5 §6.4.25). NewValue = the written
               scalar (the adapter drops non-scalar values to Null); OldValue
               capture (pre-write read) is a documented follow-up. No-op unless
               auditing is enabled. */
            mu_audit_event_t audit_ev;
            (void)memset(&audit_ev, 0, sizeof(audit_ev));
            audit_ev.event_type = MU_AUDIT_EVENT_WRITE_UPDATE;
            audit_ev.status = true;
            audit_ev.specific.write_update.node_id = write_val->node_id;
            audit_ev.specific.write_update.new_value = write_val->value.value;
            audit_ev.specific.write_update.old_value.type = MU_TYPE_NULL;
            mu_raise_audit_event(server, &audit_ev);
        }
#endif
    }

    s = write_response_prefix(w, MU_ID_WRITERESPONSE, req.request_handle, MU_STATUS_GOOD
#ifdef MUC_OPCUA_CU_TIME_SYNC
                              ,
                              server
#endif
    );
    if (s == MU_STATUS_GOOD) {
        s = mu_write_response_encode(w, &wresp);
    }

#if MUC_OPCUA_ALLOW_HEAP
    /* Release any heap-decoded array values (only ever allocated when
       MUC_OPCUA_ALLOW_HEAP is on; a no-heap build rejects array writes upstream). */
    for (size_t i = 0; i < wreq.num_nodes_to_write; ++i) {
        if (wreq.nodes_to_write[i].value.has_value && wreq.nodes_to_write[i].value.value.is_array &&
            wreq.nodes_to_write[i].value.value.value.array != NULL) {
            free((void *)wreq.nodes_to_write[i].value.value.value.array);
        }
    }
#endif

    if (s != MU_STATUS_GOOD) {
        return s;
    }

    *response_length = w->position;
    return MU_STATUS_GOOD;
}
#endif /* MUC_OPCUA_SERVICE_WRITE */
