/* src/encoding/binary_variant.c */
#include "muc_opcua/config.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/services/diagnostics.h"
#include "muc_opcua/services/method.h"
#include <stdlib.h>

/* In-memory element stride for a built-in type, for indexing array values. */
static size_t element_size(mu_builtin_type_t type) {
    static const size_t sizes[MU_TYPE_LOCALIZEDTEXT + 1] = {
        [MU_TYPE_BOOLEAN] = sizeof(opcua_boolean_t),
        [MU_TYPE_SBYTE] = sizeof(opcua_sbyte_t),
        [MU_TYPE_BYTE] = sizeof(opcua_byte_t),
        [MU_TYPE_INT16] = sizeof(opcua_int16_t),
        [MU_TYPE_UINT16] = sizeof(opcua_uint16_t),
        [MU_TYPE_INT32] = sizeof(opcua_int32_t),
        [MU_TYPE_UINT32] = sizeof(opcua_uint32_t),
        [MU_TYPE_INT64] = sizeof(opcua_int64_t),
        [MU_TYPE_UINT64] = sizeof(opcua_uint64_t),
        [MU_TYPE_FLOAT] = sizeof(opcua_float_t),
        [MU_TYPE_DOUBLE] = sizeof(opcua_double_t),
        [MU_TYPE_STRING] = sizeof(mu_string_t),
        [MU_TYPE_DATETIME] = sizeof(opcua_datetime_t),
        [MU_TYPE_BYTESTRING] = sizeof(mu_bytestring_t),
        [MU_TYPE_NODEID] = sizeof(mu_nodeid_t),
        [MU_TYPE_STATUSCODE] = sizeof(opcua_statuscode_t),
        [MU_TYPE_QUALIFIEDNAME] = sizeof(mu_qualified_name_t),
        [MU_TYPE_LOCALIZEDTEXT] = sizeof(mu_localized_text_t),
    };
    int type_id = (int)type;
    if (type_id < 0 || (size_t)type_id >= sizeof(sizes) / sizeof(sizes[0])) {
        return 0;
    }
    return sizes[type_id];
}

/* Read one built-in value into the slot pointed to by p (used for both
   scalars and array elements). For scalars p is &variant->value (all union
   members alias offset 0); for arrays it is the element address. */
static opcua_statuscode_t read_scalar_value(mu_binary_reader_t *reader, mu_builtin_type_t type, void *p) {
    switch (type) {
    case MU_TYPE_BOOLEAN:
        return mu_binary_read_boolean(reader, (opcua_boolean_t *)p);
    case MU_TYPE_SBYTE:
        return mu_binary_read_sbyte(reader, (opcua_sbyte_t *)p);
    case MU_TYPE_BYTE:
        return mu_binary_read_byte(reader, (opcua_byte_t *)p);
    case MU_TYPE_INT16:
        return mu_binary_read_int16(reader, (opcua_int16_t *)p);
    case MU_TYPE_UINT16:
        return mu_binary_read_uint16(reader, (opcua_uint16_t *)p);
    case MU_TYPE_INT32:
        return mu_binary_read_int32(reader, (opcua_int32_t *)p);
    case MU_TYPE_UINT32:
        return mu_binary_read_uint32(reader, (opcua_uint32_t *)p);
    case MU_TYPE_INT64:
        return mu_binary_read_int64(reader, (opcua_int64_t *)p);
    case MU_TYPE_UINT64:
        return mu_binary_read_uint64(reader, (opcua_uint64_t *)p);
    case MU_TYPE_FLOAT:
        return mu_binary_read_float(reader, (opcua_float_t *)p);
    case MU_TYPE_DOUBLE:
        return mu_binary_read_double(reader, (opcua_double_t *)p);
    case MU_TYPE_STRING:
        return mu_binary_read_string(reader, (mu_string_t *)p);
    case MU_TYPE_BYTESTRING:
        return mu_binary_read_bytestring(reader, (mu_bytestring_t *)p);
    case MU_TYPE_DATETIME:
        return mu_binary_read_int64(reader, (opcua_datetime_t *)p);
    case MU_TYPE_STATUSCODE:
        return mu_binary_read_statuscode(reader, (opcua_statuscode_t *)p);
    case MU_TYPE_NODEID:
        return mu_binary_read_nodeid(reader, (mu_nodeid_t *)p);
    case MU_TYPE_QUALIFIEDNAME:
        return mu_binary_read_qualified_name(reader, (mu_qualified_name_t *)p);
    case MU_TYPE_LOCALIZEDTEXT: {
        mu_localized_text_t *lt = (mu_localized_text_t *)p;
        opcua_byte_t lt_mask;
        opcua_statuscode_t s = mu_binary_read_byte(reader, &lt_mask);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        lt->locale = (mu_string_t){-1, NULL};
        lt->text = (mu_string_t){-1, NULL};
        if (lt_mask & 0x01) {
            s = mu_binary_read_string(reader, &lt->locale);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
        if (lt_mask & 0x02) {
            s = mu_binary_read_string(reader, &lt->text);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
        return MU_STATUS_GOOD;
    }
    default:
        return MU_STATUS_BAD_DECODINGERROR;
    }
}

opcua_statuscode_t mu_binary_read_variant(mu_binary_reader_t *reader, mu_variant_t *variant) {
    opcua_byte_t encoding_mask;
    opcua_statuscode_t status = mu_binary_read_byte(reader, &encoding_mask);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    opcua_byte_t type = encoding_mask & 0x3F;
    opcua_boolean_t is_array = (encoding_mask & 0x80) != 0;
    opcua_boolean_t has_dimensions = (encoding_mask & 0x40) != 0;

    /* OPC-10000-6 section 5.2.2.16: the dimensions bit is only meaningful with
       the array bit; a dimensions flag without an array is malformed. */
    if (has_dimensions && !is_array) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    variant->type = (mu_builtin_type_t)type;
    variant->is_array = false;
    variant->array_length = 0;
    variant->value.array = NULL;

    if (type == MU_TYPE_NULL) {
        return MU_STATUS_GOOD;
    }

    if (is_array) {
        variant->is_array = true;
        opcua_int32_t length = 0;
        status = mu_binary_read_int32(reader, &length);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        /* OPC-10000-6 section 5.2.5: Int32 length encodes element count, with
           -1 meaning a null array. Any other negative value is malformed. */
        if (length < -1) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        /* Spec 057: OperationLimits MaxArrayLength. Reject a value array whose
           element count exceeds the compiled ceiling, independent of buffer size
           (a DoS guard the buffer-size check below does not by itself provide).
           length == -1 (null) is exempt. Advertised via base_nodes.c. */
        if (length > (opcua_int32_t)MU_INTERN_MAX_ARRAY_LENGTH) {
            return MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED;
        }
        variant->array_length = length;

        if (length > 0) {
            size_t stride = element_size((mu_builtin_type_t)type);
            if (stride == 0) {
                return MU_STATUS_BAD_DECODINGERROR;
            }
            /* Each element occupies at least one byte on the wire, so a length
               greater than the remaining buffer cannot decode. This also bounds
               the allocation below to the remaining input size. */
            if (reader->position > reader->length || (size_t)length > reader->length - reader->position) {
                return MU_STATUS_BAD_DECODINGERROR;
            }
            if ((size_t)length > SIZE_MAX / stride) {
                return MU_STATUS_BAD_DECODINGERROR;
            }

            /* Minimum viable array read: heap-allocate the decoded element
               buffer. The variant owns this allocation; callers that decode a
               variant array must release variant.value.array (e.g. with
               free((void *)variant.value.array)) once no longer needed. A
               future mu_variant_destroy helper should centralize this. */
#if MUC_OPCUA_ALLOW_HEAP
            {
                void *elements = calloc((size_t)length, stride);
                if (elements == NULL) {
                    return MU_STATUS_BAD_OUTOFMEMORY;
                }
                for (opcua_int32_t i = 0; i < length; ++i) {
                    status = read_scalar_value(reader, (mu_builtin_type_t)type,
                                               (opcua_byte_t *)elements + (size_t)i * stride);
                    if (status != MU_STATUS_GOOD) {
                        free(elements);
                        return status;
                    }
                }
                variant->value.array = elements;
            }
#else
            /* Heap allocation disabled -- skip the array elements on the wire
               but report failure since we cannot store them. */
            return MU_STATUS_BAD_OUTOFMEMORY;
#endif
        }

        if (has_dimensions) {
            /* The variant has no field to carry array dimensions today, but the
               dimensions Int32 array (Int32 count followed by that many Int32s)
               must still be consumed to keep the reader position consistent. */
            opcua_int32_t dim_count = 0;
            status = mu_binary_read_int32(reader, &dim_count);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
            if (dim_count < -1) {
                return MU_STATUS_BAD_DECODINGERROR;
            }
            for (opcua_int32_t i = 0; i < dim_count; ++i) {
                opcua_int32_t dim;
                status = mu_binary_read_int32(reader, &dim);
                if (status != MU_STATUS_GOOD) {
                    return status;
                }
            }
        }
        return MU_STATUS_GOOD;
    }

    return read_scalar_value(reader, (mu_builtin_type_t)type, &variant->value);
}

/* Write one built-in value from a pointer to it (used for both scalars and array
   elements). For scalars the pointer is &variant->value (all union members alias
   offset 0); for arrays it is the element address. */
static opcua_statuscode_t write_scalar_value(mu_binary_writer_t *w, mu_builtin_type_t type, const void *p) {
    switch (type) {
    case MU_TYPE_BOOLEAN:
        return mu_binary_write_boolean(w, *(const opcua_boolean_t *)p);
    case MU_TYPE_SBYTE:
        return mu_binary_write_sbyte(w, *(const opcua_sbyte_t *)p);
    case MU_TYPE_BYTE:
        return mu_binary_write_byte(w, *(const opcua_byte_t *)p);
    case MU_TYPE_INT16:
        return mu_binary_write_int16(w, *(const opcua_int16_t *)p);
    case MU_TYPE_UINT16:
        return mu_binary_write_uint16(w, *(const opcua_uint16_t *)p);
    case MU_TYPE_INT32:
        return mu_binary_write_int32(w, *(const opcua_int32_t *)p);
    case MU_TYPE_UINT32:
        return mu_binary_write_uint32(w, *(const opcua_uint32_t *)p);
    case MU_TYPE_INT64:
        return mu_binary_write_int64(w, *(const opcua_int64_t *)p);
    case MU_TYPE_UINT64:
        return mu_binary_write_uint64(w, *(const opcua_uint64_t *)p);
    case MU_TYPE_FLOAT:
        return mu_binary_write_float(w, *(const opcua_float_t *)p);
    case MU_TYPE_DOUBLE:
        return mu_binary_write_double(w, *(const opcua_double_t *)p);
    case MU_TYPE_STRING:
        return mu_binary_write_string(w, (const mu_string_t *)p);
    case MU_TYPE_BYTESTRING:
        return mu_binary_write_bytestring(w, (const mu_bytestring_t *)p);
    case MU_TYPE_DATETIME:
        return mu_binary_write_int64(w, *(const opcua_datetime_t *)p);
    case MU_TYPE_STATUSCODE:
        return mu_binary_write_statuscode(w, *(const opcua_statuscode_t *)p);
    case MU_TYPE_NODEID:
        return mu_binary_write_nodeid(w, (const mu_nodeid_t *)p);
    case MU_TYPE_QUALIFIEDNAME: {
        const mu_qualified_name_t *q = (const mu_qualified_name_t *)p;
        opcua_statuscode_t s = mu_binary_write_uint16(w, q->namespace_index);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        return mu_binary_write_string(w, &q->name);
    }
    case MU_TYPE_LOCALIZEDTEXT: {
        const mu_localized_text_t *lt = (const mu_localized_text_t *)p;
        opcua_byte_t lt_mask = 0;
        if (lt->locale.length >= 0) {
            lt_mask |= 0x01;
        }
        if (lt->text.length >= 0) {
            lt_mask |= 0x02;
        }
        opcua_statuscode_t s = mu_binary_write_byte(w, lt_mask);
        if (s != MU_STATUS_GOOD) {
            return s;
        }
        if (lt_mask & 0x01) {
            s = mu_binary_write_string(w, &lt->locale);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
        if (lt_mask & 0x02) {
            s = mu_binary_write_string(w, &lt->text);
            if (s != MU_STATUS_GOOD) {
                return s;
            }
        }
        return MU_STATUS_GOOD;
    }
    default:
        return MU_STATUS_BAD_ENCODINGERROR;
    }
}

opcua_statuscode_t mu_binary_write_variant(mu_binary_writer_t *writer, const mu_variant_t *variant) {
    if (!variant) {
        return MU_STATUS_BAD_ENCODINGERROR;
    }

    opcua_byte_t encoding_mask = (opcua_byte_t)variant->type;
    if (variant->is_array) {
        encoding_mask |= 0x80; /* OPC 10000-6 5.2.2.16 array bit */
    }
    opcua_statuscode_t status = mu_binary_write_byte(writer, encoding_mask);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    if (variant->type == MU_TYPE_NULL) {
        return MU_STATUS_GOOD;
    }

    if (variant->is_array) {
        status = mu_binary_write_int32(writer, variant->array_length);
        if (status != MU_STATUS_GOOD) {
            return status;
        }
        if (variant->array_length < 0 || variant->value.array == NULL) {
            return MU_STATUS_GOOD; /* null array */
        }
#if MUC_OPCUA_CU_METHOD_SERVER
        if (variant->type == MU_TYPE_EXTENSIONOBJECT) {
            /* The only ExtensionObject-array Value this server encodes is the
               Method Server Facet's InputArguments/OutputArguments Argument[]
               (spec 062); value.array is a mu_argument_t[]. */
            const mu_argument_t *args = (const mu_argument_t *)variant->value.array;
            for (opcua_int32_t i = 0; i < variant->array_length; ++i) {
                status = mu_binary_write_argument(writer, &args[i]);
                if (status != MU_STATUS_GOOD) {
                    return status;
                }
            }
            return MU_STATUS_GOOD;
        }
#endif
        size_t stride = element_size(variant->type);
        if (stride == 0) {
            return MU_STATUS_BAD_ENCODINGERROR;
        }
        const opcua_byte_t *base = (const opcua_byte_t *)variant->value.array;
        for (opcua_int32_t i = 0; i < variant->array_length; ++i) {
            status = write_scalar_value(writer, variant->type, base + (size_t)i * stride);
            if (status != MU_STATUS_GOOD) {
                return status;
            }
        }
        return MU_STATUS_GOOD;
    }

#if MUC_OPCUA_CU_DIAGNOSTICS
    if (variant->type == MU_TYPE_EXTENSIONOBJECT) {
        /* The only scalar ExtensionObject Value this server encodes is the Base Server
           Behaviour facet's ServerDiagnosticsSummary (spec 064); value.array holds a
           const mu_diagnostics_summary_t* (OPC-10000-5 §12.9). */
        return mu_binary_write_server_diagnostics_summary(writer,
                                                          (const mu_diagnostics_summary_t *)variant->value.array);
    }
#endif
    return write_scalar_value(writer, variant->type, &variant->value);
}
