/* tests/unit/test_service_dispatch.c */
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/server_internal.h" // IWYU pragma: keep
#include "../../src/core/service_dispatch.h"
#include "../../src/services/session.h"

static opcua_statuscode_t fake_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer != NULL) {
        (void)memset(buffer, 0x42, length);
    }
    return MU_STATUS_GOOD;
}

static void skip_response_header(mu_binary_reader_t *reader, opcua_uint32_t *request_handle,
                                 opcua_statuscode_t *service_result) {
    opcua_int64_t timestamp;
    opcua_byte_t diagnostics_mask;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(reader, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(reader, request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &diagnostics_mask));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(reader, &string_table_count));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(reader, &additional_header_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &additional_header_encoding));
}

void test_service_dispatch_known_requests(void) {
    const mu_service_handler_t *handler;

    handler = mu_get_service_handler(MU_ID_FINDSERVERSREQUEST);
    TEST_ASSERT_NOT_NULL(handler);
    TEST_ASSERT_EQUAL(MU_ID_FINDSERVERSRESPONSE, handler->response_id);
    TEST_ASSERT_FALSE(handler->requires_session);

    handler = mu_get_service_handler(MU_ID_READREQUEST);
    TEST_ASSERT_NOT_NULL(handler);
    TEST_ASSERT_EQUAL(MU_ID_READRESPONSE, handler->response_id);
    TEST_ASSERT_TRUE(handler->requires_session);
}

void test_set_monitoring_mode_uses_binary_encoding_node_ids(void) {
    /* OPC-10000-6 defines service dispatch by the DefaultBinary encoding NodeId,
       not the service structure DataType NodeId. */
    TEST_ASSERT_EQUAL_UINT32(769u, MU_ID_SETMONITORINGMODEREQUEST);
    TEST_ASSERT_EQUAL_UINT32(772u, MU_ID_SETMONITORINGMODERESPONSE);
}

/* The dispatch table keys on each service's _Encoding_DefaultBinary NodeId --
 * the TypeId a client puts on the wire -- not the request structure's DataType
 * NodeId. Reachability tests cannot catch a wrong value here because the table
 * and the client-facing constant are the same macro, so this pins every
 * subscription/MonitoredItem service encoding id against the OPC UA NodeSet
 * (NodeIds.csv). Grounded via python-opcua / node-opcua / UnifiedAutomation .NET
 * SDK references. Regression for issue #302: SetMonitoringMode and
 * SetTriggering previously used their DataType NodeIds (767/770 and 773/776),
 * which made both services unreachable to spec-compliant clients. */
void test_subscription_service_ids_are_binary_encoding_node_ids(void) {
    TEST_ASSERT_EQUAL_UINT32(751u, MU_ID_CREATEMONITOREDITEMSREQUEST);
    TEST_ASSERT_EQUAL_UINT32(754u, MU_ID_CREATEMONITOREDITEMSRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(763u, MU_ID_MODIFYMONITOREDITEMSREQUEST);
    TEST_ASSERT_EQUAL_UINT32(766u, MU_ID_MODIFYMONITOREDITEMSRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(769u, MU_ID_SETMONITORINGMODEREQUEST);
    TEST_ASSERT_EQUAL_UINT32(772u, MU_ID_SETMONITORINGMODERESPONSE);
    TEST_ASSERT_EQUAL_UINT32(775u, MU_ID_SETTRIGGERINGREQUEST);
    TEST_ASSERT_EQUAL_UINT32(778u, MU_ID_SETTRIGGERINGRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(781u, MU_ID_DELETEMONITOREDITEMSREQUEST);
    TEST_ASSERT_EQUAL_UINT32(784u, MU_ID_DELETEMONITOREDITEMSRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(787u, MU_ID_CREATESUBSCRIPTIONREQUEST);
    TEST_ASSERT_EQUAL_UINT32(790u, MU_ID_CREATESUBSCRIPTIONRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(793u, MU_ID_MODIFYSUBSCRIPTIONREQUEST);
    TEST_ASSERT_EQUAL_UINT32(796u, MU_ID_MODIFYSUBSCRIPTIONRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(799u, MU_ID_SETPUBLISHINGMODEREQUEST);
    TEST_ASSERT_EQUAL_UINT32(802u, MU_ID_SETPUBLISHINGMODERESPONSE);
    TEST_ASSERT_EQUAL_UINT32(826u, MU_ID_PUBLISHREQUEST);
    TEST_ASSERT_EQUAL_UINT32(829u, MU_ID_PUBLISHRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(832u, MU_ID_REPUBLISHREQUEST);
    TEST_ASSERT_EQUAL_UINT32(835u, MU_ID_REPUBLISHRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(841u, MU_ID_TRANSFERSUBSCRIPTIONSREQUEST);
    TEST_ASSERT_EQUAL_UINT32(844u, MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE);
    TEST_ASSERT_EQUAL_UINT32(847u, MU_ID_DELETESUBSCRIPTIONSREQUEST);
    TEST_ASSERT_EQUAL_UINT32(850u, MU_ID_DELETESUBSCRIPTIONSRESPONSE);
}

void test_service_dispatch_unknown_request(void) {
    const mu_service_handler_t *handler = mu_get_service_handler(99999);
    TEST_ASSERT_NULL(handler);
}

void test_service_dispatch_unsupported_services(void) {
    opcua_byte_t req_body[1] = {0};
    opcua_byte_t resp_body[8] = {0};
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;

    const opcua_uint32_t unsupported[] = {
#ifndef MUC_OPCUA_SERVICE_WRITE
        MU_ID_WRITEREQUEST,
#endif
#if !(MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM)
        MU_ID_CALLREQUEST,
#endif
#ifndef MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
        MU_ID_HISTORYREADREQUEST,
#endif
#if !MUC_OPCUA_REDUNDANCY
        841u /* TransferSubscriptionsRequest_Encoding_DefaultBinary */
#endif
    };

    /* OPC-10000-4 section 7.38.2 defines Bad_ServiceUnsupported for a requested
       service the Server does not support. OPC-10000-7 section 4.2 treats
       ConformanceUnits as specific feature sets, so these disabled or out-of-
       profile services must remain absent from the dispatch table rather than
       becoming implied profile/conformance support. */
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++) {
        size_t resp_len = sizeof(resp_body);
        TEST_ASSERT_NULL(mu_get_service_handler(unsupported[i]));
        TEST_ASSERT_EQUAL_HEX32(
            MU_STATUS_BAD_SERVICEUNSUPPORTED,
            mu_service_dispatch(&server, unsupported[i], req_body, sizeof(req_body), resp_body, &resp_len));
    }
}

void test_activate_session_consumes_nonempty_certificates_and_identity_body(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.entropy_adapter.generate_random = fake_entropy;
    mu_session_init(&server.sessions[0]);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id;
    opcua_uint32_t auth_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_session_create(&server.sessions[0], 0, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL(12345, auth_token);

    /* OPC-10000-4 §5.7.3.2: ActivateSession includes ClientSoftwareCertificates[],
       LocaleIds[], UserIdentityToken, and UserTokenSignature in that order. */
    static const opcua_byte_t request_body[] = {
        /* 0x00 RequestHeader, authenticationToken i=12345 */
        0x01, 0x00, 0x39, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 0x1f ClientSignature: null algorithm, null signature */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        /* 0x27 ClientSoftwareCertificates[1]: certificateData "ABC", signature "XY" */
        0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x41, 0x42, 0x43, 0x02, 0x00, 0x00, 0x00, 0x58, 0x59,
        /* 0x38 LocaleIds[1]: "en-US" */
        0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x65, 0x6e, 0x2d, 0x55, 0x53,
        /* 0x45 UserIdentityToken: AnonymousIdentityToken body policyId "anonymous" */
        0x01, 0x00, 0x41, 0x01, 0x01, 0x0d, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x61, 0x6e, 0x6f, 0x6e, 0x79,
        0x6d, 0x6f, 0x75, 0x73,
        /* 0x5b UserTokenSignature: algorithm "sigalg", signature 0xab */
        0x06, 0x00, 0x00, 0x00, 0x73, 0x69, 0x67, 0x61, 0x6c, 0x67, 0x01, 0x00, 0x00, 0x00, 0xab};
    TEST_ASSERT_EQUAL_size_t(106, sizeof(request_body));

    opcua_byte_t response_body[256];
    size_t response_len = sizeof(response_body);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, request_body,
                                                          sizeof(request_body), response_body, &response_len));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, response_body, response_len);
    mu_nodeid_t response_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, response_type.identifier.numeric);

    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL(0, request_handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);

    mu_bytestring_t server_nonce;
    opcua_int32_t results_count;
    opcua_int32_t diagnostics_count;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&reader, &server_nonce));
    TEST_ASSERT_EQUAL(32, server_nonce.length);
    TEST_ASSERT_EQUAL_HEX8(0x42, server_nonce.data[0]);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &results_count));
    TEST_ASSERT_EQUAL(0, results_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &diagnostics_count));
    TEST_ASSERT_EQUAL(0, diagnostics_count);
    TEST_ASSERT_EQUAL_size_t(response_len, reader.position);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_service_dispatch_known_requests);
    RUN_TEST(test_set_monitoring_mode_uses_binary_encoding_node_ids);
    RUN_TEST(test_subscription_service_ids_are_binary_encoding_node_ids);
    RUN_TEST(test_service_dispatch_unknown_request);
    RUN_TEST(test_service_dispatch_unsupported_services);
    RUN_TEST(test_activate_session_consumes_nonempty_certificates_and_identity_body);
    return UNITY_END();
}
