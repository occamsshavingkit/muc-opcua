/* src/services/service_header.h */
#ifndef MUC_OPCUA_SERVICES_SERVICE_HEADER_H
#define MUC_OPCUA_SERVICES_SERVICE_HEADER_H

#include "muc_opcua/encoding.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

/* RequestHeader (OPC 10000-4 7.32) - common to every service request. */
typedef struct {
    mu_nodeid_t authentication_token;
    opcua_datetime_t timestamp;
    opcua_uint32_t request_handle;
    opcua_uint32_t return_diagnostics;
    mu_string_t audit_entry_id;
    opcua_uint32_t timeout_hint;
    /* additionalHeader (ExtensionObject) is decoded and discarded */
} mu_request_header_t;

/* ResponseHeader (OPC 10000-4 7.33) - common to every service response. */
typedef struct {
    opcua_datetime_t timestamp;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_uint32_t return_diagnostics;
    /* stringTable and additionalHeader are emitted null. serviceDiagnostics is
       emitted only for the Base Services Diagnostics CU. */
} mu_response_header_t;

opcua_statuscode_t mu_request_header_decode(mu_binary_reader_t *reader, mu_request_header_t *header);
opcua_statuscode_t mu_response_header_encode(mu_binary_writer_t *writer, const mu_response_header_t *header);

#endif /* MUC_OPCUA_SERVICES_SERVICE_HEADER_H */
