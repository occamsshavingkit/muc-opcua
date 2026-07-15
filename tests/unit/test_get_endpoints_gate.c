/* tests/unit/test_get_endpoints_gate.c
 * T050 / opc_cu_2328 and T051 / opc_cu_2352: independent Discovery service
 * dispatch gating for GetEndpoints and FindServers. Grounding: OPC-10000-4
 * sections 5.5.1, 5.5.2, and 5.5.4 define Discovery service behavior;
 * disabled CUs must not expose their service. */
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void discovery_server(mu_server_t *server) {
    (void)memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    server->config.endpoint_url = "opc.tcp://localhost:4840";
    server->config.application_uri = "urn:test:app";
    server->config.product_uri = "urn:test:product";
    server->config.application_name = "Test Server";
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};

    mu_binary_write_nodeid(writer, &null_id);                     /* authenticationToken */
    mu_binary_write_int64(writer, 0);                             /* timestamp */
    mu_binary_write_uint32(writer, request_handle);               /* requestHandle */
    mu_binary_write_uint32(writer, 0);                            /* returnDiagnostics */
    mu_binary_write_string(writer, &null_str);                    /* auditEntryId */
    mu_binary_write_uint32(writer, 0);                            /* timeoutHint */
    mu_binary_write_extension_object_header(writer, &null_id, 0); /* additionalHeader */
}

static size_t build_findservers_self_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    write_request_header(&writer, 50);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);          /* localeIds[] */
    mu_binary_write_int32(&writer, 0);          /* serverUris[] */
    mu_binary_write_int32(&writer, 0);          /* serverTypes[] */

    return writer.position;
}

static size_t build_getendpoints_self_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    write_request_header(&writer, 51);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);          /* localeIds[] */
    mu_binary_write_int32(&writer, 0);          /* profileUris[] */

    return writer.position;
}

static opcua_uint32_t response_type(const opcua_byte_t *response, size_t response_len) {
    mu_binary_reader_t reader;
    mu_nodeid_t type;

    mu_binary_reader_init(&reader, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &type));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, type.identifier_type);
    return type.identifier.numeric;
}

void test_t050_getendpoints_gate_matches_opc_cu_2328_selection(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_getendpoints_self_request(request, sizeof(request));
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);
    opcua_statuscode_t status =
        mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, request, request_len, response, &response_len);

#ifdef MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_GETENDPOINTSRESPONSE, response_type(response, response_len));
#else
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SERVICEUNSUPPORTED, status);
#endif
}

void test_t051_findservers_gate_matches_opc_cu_2352_selection(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_findservers_self_request(request, sizeof(request));
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);
    opcua_statuscode_t status =
        mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len, response, &response_len);

#if defined(MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF) && MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_FINDSERVERSRESPONSE, response_type(response, response_len));
#else
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SERVICEUNSUPPORTED, status);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_t050_getendpoints_gate_matches_opc_cu_2328_selection);
    RUN_TEST(test_t051_findservers_gate_matches_opc_cu_2352_selection);
    return UNITY_END();
}
