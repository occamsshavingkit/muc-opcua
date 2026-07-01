#include "micro_opcua/encoding.h"
#include "micro_opcua/pubsub.h"
#include "micro_opcua/status.h"

enum {
    MU_UADP_FLAGS_VERSION1 = 0x01,
    MU_UADP_FLAGS_PUBLISHER_ID = 0x10,
    MU_UADP_FLAGS_PAYLOAD_HEADER = 0x40,
    MU_UADP_FLAGS_EXTENDED_FLAGS1 = 0x80,
    MU_UADP_PUBLISHER_ID_UINT32 = 0x02,
    MU_UADP_EXTENDED_FLAGS1_SECURITY_HEADER = 0x10,
    MU_UADP_DATASET_FLAGS_VALID_VARIANT = 0x01,
};

static const opcua_byte_t MU_UADP_SUPPORTED_NETWORK_FLAGS =
    (opcua_byte_t)(MU_UADP_FLAGS_VERSION1 | MU_UADP_FLAGS_PUBLISHER_ID | MU_UADP_FLAGS_PAYLOAD_HEADER |
                   MU_UADP_FLAGS_EXTENDED_FLAGS1);

static opcua_boolean_t mu_pubsub_variant_is_supported_scalar(const mu_variant_t *value) {
    return value != NULL && !value->is_array;
}

static opcua_boolean_t mu_uadp_variant_encoding_is_unsupported(opcua_byte_t encoding_mask) {
    opcua_byte_t type = (opcua_byte_t)(encoding_mask & 0x3fu);

    if ((encoding_mask & 0xc0u) != 0u) {
        return true;
    }

    /* Variable-length scalar Variants are intentionally accepted in this
     * caller-storage subset. The public mu_pubsub_received_message_t contract
     * requires the input buffer to outlive decoded field values because the
     * shared binary Variant decoder may borrow payload bytes. */
    switch (type) {
    case MU_TYPE_NULL:
    case MU_TYPE_BOOLEAN:
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
    case MU_TYPE_STRING:
    case MU_TYPE_DATETIME:
    case MU_TYPE_BYTESTRING:
    case MU_TYPE_NODEID:
    case MU_TYPE_STATUSCODE:
    case MU_TYPE_QUALIFIEDNAME:
    case MU_TYPE_LOCALIZEDTEXT:
        return false;
    case MU_TYPE_GUID:
    case MU_TYPE_XMLELEMENT:
    case MU_TYPE_EXPANDEDNODEID:
    case MU_TYPE_EXTENSIONOBJECT:
    case MU_TYPE_DATAVALUE:
    case MU_TYPE_VARIANT:
    case MU_TYPE_DIAGNOSTICINFO:
        return true;
    default:
        return false;
    }
}

static void mu_pubsub_patch_u16(opcua_byte_t *buffer, size_t offset, opcua_uint16_t value) {
    buffer[offset] = (opcua_byte_t)(value & 0xffu);
    buffer[offset + 1u] = (opcua_byte_t)((value >> 8u) & 0xffu);
}

/* Encode one scoped UADP NetworkMessage:
 * - UInt32 PublisherId
 * - one PayloadHeader entry with DataSetWriterId
 * - one Data Key Frame DataSetMessage
 * - Variant field encoding for caller-provided scalar fields
 *
 * Normative surface: OPC-10000-14 sections 7.2.4.4.2, 7.2.4.5.2,
 * 7.2.4.5.4, and 7.2.4.5.5. Variant values use OPC-10000-6 section
 * 5.2.2.16 through the shared binary Variant encoder.
 */
opcua_statuscode_t mu_encode_uadp_network_message(const mu_pubsub_connection_t *conn,
                                                  const mu_pubsub_writer_group_t *wg, opcua_byte_t *buffer,
                                                  size_t buffer_size, size_t *bytes_written) {
    const mu_pubsub_dataset_writer_t *writer = NULL;
    mu_binary_writer_t binary;
    size_t message_size_offset = 0;
    size_t message_start = 0;
    size_t message_size = 0;

    if (bytes_written) {
        *bytes_written = 0;
    }
    if (!conn || !wg || !buffer || !bytes_written) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    writer = &wg->dataset_writer;
    if (writer->field_count > MU_PUBSUB_MAX_FIELDS) {
        return MU_STATUS_BAD_TOOMANYOPERATIONS;
    }
    if (writer->field_count > 0u && writer->fields == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    for (size_t i = 0; i < writer->field_count; ++i) {
        if (!mu_pubsub_variant_is_supported_scalar(&writer->fields[i].value)) {
            return MU_STATUS_BAD_NOTSUPPORTED;
        }
    }

    mu_binary_writer_init(&binary, buffer, buffer_size);

    mu_binary_write_byte(&binary, (opcua_byte_t)(MU_UADP_FLAGS_VERSION1 | MU_UADP_FLAGS_PUBLISHER_ID |
                                                 MU_UADP_FLAGS_PAYLOAD_HEADER | MU_UADP_FLAGS_EXTENDED_FLAGS1));
    mu_binary_write_byte(&binary, MU_UADP_PUBLISHER_ID_UINT32);
    mu_binary_write_uint32(&binary, conn->publisher_id);

    mu_binary_write_byte(&binary, 1u); /* PayloadHeader Count */
    mu_binary_write_uint16(&binary, writer->data_set_writer_id);

    message_size_offset = binary.position;
    mu_binary_write_uint16(&binary, 0u); /* patched after DataSetMessage body */
    message_start = binary.position;

    mu_binary_write_byte(&binary, MU_UADP_DATASET_FLAGS_VALID_VARIANT);
    for (size_t i = 0; i < writer->field_count; ++i) {
        mu_binary_write_variant(&binary, &writer->fields[i].value);
    }

    if (binary.status != MU_STATUS_GOOD) {
        return binary.status;
    }
    message_size = binary.position - message_start;
    if (message_size > 0xffffu) {
        return MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED;
    }

    mu_pubsub_patch_u16(buffer, message_size_offset, (opcua_uint16_t)message_size);
    *bytes_written = binary.position;
    return binary.status;
}

/* Decode the scoped UADP NetworkMessage emitted by mu_encode_uadp_network_message().
 * Normative surface: OPC-10000-14 sections 7.2.4.4.2, 7.2.4.5.2,
 * 7.2.4.5.3, 7.2.4.5.4, and 7.2.4.5.5. Variant fields use
 * OPC-10000-6 section 5.2.2.16 through the shared Variant decoder.
 */
opcua_statuscode_t mu_decode_uadp_network_message(const opcua_byte_t *buffer, size_t buffer_size,
                                                  mu_pubsub_received_message_t *message) {
    mu_binary_reader_t binary;
    mu_binary_reader_t data_set_reader;
    opcua_byte_t flags = 0;
    opcua_byte_t extended_flags1 = 0;
    opcua_uint32_t publisher_id = 0;
    opcua_byte_t payload_count = 0;
    opcua_uint16_t data_set_writer_id = 0;
    opcua_uint16_t data_set_message_size = 0;
    opcua_byte_t data_set_flags1 = 0;
    size_t decoded_fields = 0;
    opcua_statuscode_t status;

    if (message) {
        message->field_count = 0;
    }
    if (!buffer || !message) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }
    if (message->field_capacity > 0u && message->fields == NULL) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    mu_binary_reader_init(&binary, buffer, buffer_size);

    status = mu_binary_read_byte(&binary, &flags);
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (flags != MU_UADP_SUPPORTED_NETWORK_FLAGS) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    status = mu_binary_read_byte(&binary, &extended_flags1);
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if ((extended_flags1 & MU_UADP_EXTENDED_FLAGS1_SECURITY_HEADER) != 0u) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }
    if (extended_flags1 != MU_UADP_PUBLISHER_ID_UINT32) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    status = mu_binary_read_uint32(&binary, &publisher_id);
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    status = mu_binary_read_byte(&binary, &payload_count);
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (payload_count != 1u) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    status = mu_binary_read_uint16(&binary, &data_set_writer_id);
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    status = mu_binary_read_uint16(&binary, &data_set_message_size);
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    /* This subset carries one sized DataSetMessage; the size entry must
       account for the complete remaining DataSet payload before header decode. */
    if ((size_t)data_set_message_size != binary.length - binary.position) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    mu_binary_reader_init(&data_set_reader, buffer + binary.position, (size_t)data_set_message_size);
    binary.position += (size_t)data_set_message_size;

    status = mu_binary_read_byte(&data_set_reader, &data_set_flags1);
    if (status != MU_STATUS_GOOD) {
        return MU_STATUS_BAD_DECODINGERROR;
    }
    if (data_set_flags1 != MU_UADP_DATASET_FLAGS_VALID_VARIANT) {
        return MU_STATUS_BAD_NOTSUPPORTED;
    }

    while (data_set_reader.position < data_set_reader.length) {
        if (decoded_fields >= message->field_capacity || decoded_fields >= MU_PUBSUB_MAX_FIELDS) {
            return MU_STATUS_BAD_TOOMANYOPERATIONS;
        }
        if (mu_uadp_variant_encoding_is_unsupported(data_set_reader.buffer[data_set_reader.position])) {
            return MU_STATUS_BAD_NOTSUPPORTED;
        }

        status = mu_binary_read_variant(&data_set_reader, &message->fields[decoded_fields]);
        if (status != MU_STATUS_GOOD) {
            return MU_STATUS_BAD_DECODINGERROR;
        }
        message->fields[decoded_fields].is_array = false;
        message->fields[decoded_fields].array_length = 0;
        ++decoded_fields;
    }

    if (data_set_reader.position != data_set_reader.length || binary.position != binary.length) {
        return MU_STATUS_BAD_DECODINGERROR;
    }

    message->publisher_id = publisher_id;
    message->data_set_writer_id = data_set_writer_id;
    message->field_count = decoded_fields;
    return MU_STATUS_GOOD;
}
