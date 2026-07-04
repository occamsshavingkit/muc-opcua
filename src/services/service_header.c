/* src/services/service_header.c */
#include "service_header.h"

opcua_statuscode_t mu_request_header_decode(mu_binary_reader_t *reader, mu_request_header_t *header) {
    if (!reader || !header) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;

    status = mu_binary_read_nodeid(reader, &header->authentication_token);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_read_int64(reader, &header->timestamp);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_read_uint32(reader, &header->request_handle);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_read_uint32(reader, &header->return_diagnostics);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_read_string(reader, &header->audit_entry_id);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_read_uint32(reader, &header->timeout_hint);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    /* additionalHeader: ExtensionObject - decoded and discarded */
    return mu_binary_skip_extension_object(reader);
}

opcua_statuscode_t mu_response_header_encode(mu_binary_writer_t *writer, const mu_response_header_t *header) {
    if (!writer || !header) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_statuscode_t status;

    status = mu_binary_write_int64(writer, header->timestamp);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_write_uint32(writer, header->request_handle);
    if (status != MU_STATUS_GOOD) {
        return status;
    }
    status = mu_binary_write_statuscode(writer, header->service_result);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    /* serviceDiagnostics: empty DiagnosticInfo (encoding mask byte 0x00) */
    status = mu_binary_write_byte(writer, 0x00);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    /* stringTable: null String array (length -1) */
    status = mu_binary_write_int32(writer, -1);
    if (status != MU_STATUS_GOOD) {
        return status;
    }

    /* additionalHeader: null ExtensionObject (NodeId i=0 + encoding 0x00) */
    {
        mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
        return mu_binary_write_extension_object_header(writer, &null_id, 0);
    }
}
