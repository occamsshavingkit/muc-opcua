#include "muc_opcua/config.h"

#ifdef MUC_OPCUA_SERVICE_HISTORY

#include "../core/server_internal.h"
#include "../core/service_message.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/services/history.h"

#include "history.h"
#include "muc_opcua/opcua_ids.h"
#include <string.h>

static opcua_statuscode_t mu_history_read_store_continuation_point(mu_history_read_value_id_t *node,
                                                                   const mu_bytestring_t *continuation_point) {
    if (continuation_point->length < 0) {
        node->continuation_point.length = -1;
        node->continuation_point.data = NULL;
        return MU_STATUS_GOOD;
    }

    if ((size_t)continuation_point->length > sizeof(node->continuation_point_storage)) {
        return MU_STATUS_BAD_CONTINUATIONPOINTINVALID;
    }

    node->continuation_point.length = continuation_point->length;
    if (continuation_point->length == 0) {
        node->continuation_point.data = NULL;
        return MU_STATUS_GOOD;
    }

    /* OPC-10000-4 5.11.3.2 and 7.9: continuation points may be reused by a
       later HistoryRead, so decoded bytes must be owned beyond request decode. */
    memcpy(node->continuation_point_storage, continuation_point->data, (size_t)continuation_point->length);
    node->continuation_point.data = node->continuation_point_storage;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_history_read_request_decode(mu_binary_reader_t *reader, mu_history_read_request_t *req,
                                                  mu_history_read_value_id_t *nodes_array, size_t max_nodes) {
    if (!reader || !req || !nodes_array)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t s;
    mu_nodeid_t ext_type;

    // Read historyReadDetails (ExtensionObject)
    s = mu_binary_read_nodeid(reader, &ext_type);
    if (s != MU_STATUS_GOOD)
        return s;

    // We only support ReadRawModifiedDetails for now
    if (ext_type.identifier.numeric != MU_ID_READRAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY) {
        return MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
    }

    opcua_byte_t encoding_mask;
    s = mu_binary_read_byte(reader, &encoding_mask);
    if (s != MU_STATUS_GOOD)
        return s;

    // Only ByteString encoding is supported
    if (encoding_mask != 0x01)
        return MU_STATUS_BAD_NOTSUPPORTED;

    opcua_int32_t length;
    s = mu_binary_read_int32(reader, &length);
    if (s != MU_STATUS_GOOD)
        return s;
    if (length < 0) {
        reader->status = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (reader->position > reader->length || (size_t)length > reader->length - reader->position) {
        reader->status = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }

    /* OPC-10000-4 section 5.11.3.2 and OPC-10000-6 section 5.2.2.15:
       historyReadDetails must decode strictly inside the ExtensionObject body. */
    size_t body_start = reader->position;
    mu_binary_reader_t body_reader;
    mu_binary_reader_init(&body_reader, reader->buffer + body_start, (size_t)length);

    // Decode ReadRawModifiedDetails
    s = mu_binary_read_boolean(&body_reader, &req->details.is_read_modified);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_read_int64(&body_reader, &req->details.start_time);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_read_int64(&body_reader, &req->details.end_time);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_read_uint32(&body_reader, &req->details.num_values_per_node);
    if (s != MU_STATUS_GOOD)
        return s;

    s = mu_binary_read_boolean(&body_reader, &req->details.return_bounds);
    if (s != MU_STATUS_GOOD)
        return s;
    if (body_reader.position != (size_t)length) {
        reader->status = MU_STATUS_BAD_DECODINGERROR;
        return MU_STATUS_BAD_DECODINGERROR;
    }
    reader->position = body_start + (size_t)length;

    // Decode timestampsToReturn
    s = mu_binary_read_uint32(reader, &req->timestamps_to_return);
    if (s != MU_STATUS_GOOD)
        return s;

    // Decode releaseContinuationPoints
    s = mu_binary_read_boolean(reader, &req->release_continuation_points);
    if (s != MU_STATUS_GOOD)
        return s;

    // Decode nodesToRead array
    opcua_int32_t no_of_nodes;
    s = mu_binary_read_int32(reader, &no_of_nodes);
    if (s != MU_STATUS_GOOD)
        return s;

    if (no_of_nodes < 0) {
        req->num_nodes_to_read = 0;
        req->nodes_to_read = NULL;
        return MU_STATUS_GOOD;
    }

    if ((size_t)no_of_nodes > max_nodes) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    req->num_nodes_to_read = (size_t)no_of_nodes;
    req->nodes_to_read = nodes_array;

    for (size_t i = 0; i < req->num_nodes_to_read; ++i) {
        mu_history_read_value_id_t *node = &req->nodes_to_read[i];

        s = mu_binary_read_nodeid(reader, &node->node_id);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_string(reader, &node->index_range);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_uint16(reader, &node->data_encoding.namespace_index);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_read_string(reader, &node->data_encoding.name);
        if (s != MU_STATUS_GOOD)
            return s;

        mu_bytestring_t continuation_point;
        s = mu_binary_read_bytestring(reader, &continuation_point);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_history_read_store_continuation_point(node, &continuation_point);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_history_read_response_encode(mu_binary_writer_t *writer, const mu_history_read_response_t *resp) {
    if (!writer || !resp)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t s;
    s = mu_binary_write_int32(writer, (opcua_int32_t)resp->num_results);
    if (s != MU_STATUS_GOOD)
        return s;

    for (size_t i = 0; i < resp->num_results; ++i) {
        const mu_history_read_result_t *res = &resp->results[i];

        s = mu_binary_write_statuscode(writer, res->status_code);
        if (s != MU_STATUS_GOOD)
            return s;

        s = mu_binary_write_bytestring(writer, &res->continuation_point);
        if (s != MU_STATUS_GOOD)
            return s;

        // historyData (ExtensionObject)
        if (res->history_data.num_data_values > 0) {
            mu_nodeid_t ext_id = {.identifier_type = MU_NODEID_NUMERIC,
                                  .namespace_index = 0,
                                  .identifier.numeric = MU_ID_HISTORYDATA_ENCODING_DEFAULTBINARY};
            s = mu_binary_write_nodeid(writer, &ext_id);
            if (s != MU_STATUS_GOOD)
                return s;

            s = mu_binary_write_byte(writer, 0x01); // ByteString encoding
            if (s != MU_STATUS_GOOD)
                return s;

            // Compute length of HistoryData
            // Length = num_data_values (4 bytes) + sum of encoded DataValues
            // To do this right, we need a sub-writer or a length calculation loop.
            // Since we need to calculate the length, let's do a dummy run or use a sub-writer.
            // A simpler way: remember position, write 0, write array, update length.
            size_t length_pos = writer->position;
            s = mu_binary_write_int32(writer, 0); // placeholder
            if (s != MU_STATUS_GOOD)
                return s;

            size_t start_pos = writer->position;

            s = mu_binary_write_int32(writer, (opcua_int32_t)res->history_data.num_data_values);
            if (s != MU_STATUS_GOOD)
                return s;

            for (size_t j = 0; j < res->history_data.num_data_values; ++j) {
                s = mu_binary_write_datavalue(writer, &res->history_data.data_values[j]);
                if (s != MU_STATUS_GOOD)
                    return s;
            }

            size_t end_pos = writer->position;
            // Write length back
            opcua_int32_t length = (opcua_int32_t)(end_pos - start_pos);
            opcua_byte_t *len_ptr = writer->buffer + length_pos;
            len_ptr[0] = (opcua_byte_t)(length & 0xFF);
            len_ptr[1] = (opcua_byte_t)((length >> 8) & 0xFF);
            len_ptr[2] = (opcua_byte_t)((length >> 16) & 0xFF);
            len_ptr[3] = (opcua_byte_t)((length >> 24) & 0xFF);
        } else {
            // Null ExtensionObject
            mu_nodeid_t null_id = {.identifier_type = MU_NODEID_NUMERIC, .namespace_index = 0, .identifier.numeric = 0};
            s = mu_binary_write_nodeid(writer, &null_id);
            if (s != MU_STATUS_GOOD)
                return s;

            s = mu_binary_write_byte(writer, 0x00); // None encoding
            if (s != MU_STATUS_GOOD)
                return s;
        }
    }

    // DiagnosticInfos array (null)
    s = mu_binary_write_int32(writer, -1);
    if (s != MU_STATUS_GOOD)
        return s;

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_history_update_request_decode(mu_binary_reader_t *reader, mu_history_update_request_t *req,
                                                    mu_history_update_item_t *items_array, size_t max_items) {
    if (!reader || !req || !items_array)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t s;
    opcua_int32_t num_details;
    s = mu_binary_read_int32(reader, &num_details);
    if (s != MU_STATUS_GOOD)
        return s;

    if (num_details < 0) {
        req->num_items = 0;
        req->items = NULL;
        return MU_STATUS_GOOD;
    }

    if ((size_t)num_details > max_items) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }

    req->num_items = (size_t)num_details;
    req->items = items_array;

    for (size_t i = 0; i < req->num_items; ++i) {
        mu_history_update_item_t *item = &req->items[i];
        item->type = MU_HISTORY_UPDATE_TYPE_INVALID;

        mu_nodeid_t type_id;
        s = mu_binary_read_nodeid(reader, &type_id);
        if (s != MU_STATUS_GOOD)
            return s;

        opcua_byte_t encoding_mask;
        s = mu_binary_read_byte(reader, &encoding_mask);
        if (s != MU_STATUS_GOOD)
            return s;

        if ((encoding_mask & 0x01) == 0) {
            return MU_STATUS_BAD_NOTSUPPORTED; // ByteString body is required
        }

        opcua_int32_t length;
        s = mu_binary_read_int32(reader, &length);
        if (s != MU_STATUS_GOOD)
            return s;
        /* OPC-10000-6 5.2.2.15: an ExtensionObject body length is a non-negative
           Int32. Reject negatives so the later (size_t)length skip math cannot
           wrap (matches binary_extension_object.c's convention). */
        if (length < 0)
            return MU_STATUS_BAD_DECODINGERROR;

        size_t start_pos = reader->position;

        if (type_id.identifier.numeric == MU_ID_UPDATEDATADETAILS_ENCODING_DEFAULTBINARY) {
            item->type = MU_HISTORY_UPDATE_TYPE_DATA;
            s = mu_binary_read_nodeid(reader, &item->body.data.node_id);
            if (s != MU_STATUS_GOOD)
                return s;

            s = mu_binary_read_uint32(reader, &item->body.data.perform_insert_replace);
            if (s != MU_STATUS_GOOD)
                return s;

            opcua_int32_t num_vals;
            s = mu_binary_read_int32(reader, &num_vals);
            if (s != MU_STATUS_GOOD)
                return s;

            if (num_vals < 0) {
                item->body.data.num_values = 0;
            } else {
                if ((size_t)num_vals > MU_MAX_HISTORY_UPDATE_VALUES_PER_DETAIL) {
                    return MU_STATUS_BAD_TOOMANYOPERATIONS;
                }
                item->body.data.num_values = (size_t)num_vals;

                for (size_t j = 0; j < item->body.data.num_values; ++j) {
                    mu_historical_data_point_t *dp = &item->body.data.values[j];
                    mu_datavalue_t dv = {0};
                    s = mu_binary_read_datavalue(reader, &dv);
                    if (s != MU_STATUS_GOOD)
                        return s;

                    dp->value = dv.value;
                    dp->status = dv.status;
                    dp->source_timestamp = dv.source_timestamp;
                    dp->server_timestamp = dv.server_timestamp;
                }
            }
        } else if (type_id.identifier.numeric == MU_ID_DELETERAWMODIFIEDDETAILS_ENCODING_DEFAULTBINARY) {
            item->type = MU_HISTORY_UPDATE_TYPE_DELETE;
            s = mu_binary_read_nodeid(reader, &item->body.delete_raw.node_id);
            if (s != MU_STATUS_GOOD)
                return s;

            s = mu_binary_read_boolean(reader, &item->body.delete_raw.is_delete_modified);
            if (s != MU_STATUS_GOOD)
                return s;

            s = mu_binary_read_int64(reader, &item->body.delete_raw.start_time);
            if (s != MU_STATUS_GOOD)
                return s;

            s = mu_binary_read_int64(reader, &item->body.delete_raw.end_time);
            if (s != MU_STATUS_GOOD)
                return s;
        } else {
            return MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED;
        }

        // Verify/skip remaining bytes in this ExtensionObject
        size_t bytes_read = reader->position - start_pos;
        if (bytes_read < (size_t)length) {
            size_t diff = (size_t)length - bytes_read;
            if (reader->position + diff > reader->length) {
                return MU_STATUS_BAD_DECODINGERROR;
            }
            reader->position += diff;
        }
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_history_update_response_encode(mu_binary_writer_t *writer,
                                                     const mu_history_update_response_t *resp) {
    if (!writer || !resp)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_statuscode_t s;
    s = mu_binary_write_int32(writer, (opcua_int32_t)resp->num_results);
    if (s != MU_STATUS_GOOD)
        return s;

    for (size_t i = 0; i < resp->num_results; ++i) {
        const mu_history_update_result_t *res = &resp->results[i];

        s = mu_binary_write_statuscode(writer, res->status_code);
        if (s != MU_STATUS_GOOD)
            return s;

        // operationResults (array of StatusCode)
        if (res->num_operation_results == 0) {
            s = mu_binary_write_int32(writer, -1);
            if (s != MU_STATUS_GOOD)
                return s;
        } else {
            s = mu_binary_write_int32(writer, (opcua_int32_t)res->num_operation_results);
            if (s != MU_STATUS_GOOD)
                return s;

            for (size_t j = 0; j < res->num_operation_results; ++j) {
                s = mu_binary_write_statuscode(writer, res->operation_results[j]);
                if (s != MU_STATUS_GOOD)
                    return s;
            }
        }

        // diagnosticInfos (DiagnosticInfo array - null/empty)
        s = mu_binary_write_int32(writer, -1);
        if (s != MU_STATUS_GOOD)
            return s;
    }

    // diagnosticInfos (DiagnosticInfo array for the whole service - null/empty)
    s = mu_binary_write_int32(writer, -1);
    if (s != MU_STATUS_GOOD)
        return s;

    return MU_STATUS_GOOD;
}

#endif // MUC_OPCUA_SERVICE_HISTORY
