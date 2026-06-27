/* tests/unit/test_service_dispatch.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/service_dispatch.h"
#include "../../src/core/server_internal.h"
#include "../../src/services/session.h"

static opcua_statuscode_t fake_entropy(void *context, opcua_byte_t *buffer, size_t length)
{
    (void)context;
    if (buffer != NULL) {
        memset(buffer, 0x42, length);
    }
    return MU_STATUS_GOOD;
}

static void skip_response_header(mu_binary_reader_t *reader,
                                 opcua_uint32_t *request_handle,
                                 opcua_statuscode_t *service_result)
{
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

void test_service_dispatch_unknown_request(void) {
    const mu_service_handler_t *handler = mu_get_service_handler(99999);
    TEST_ASSERT_NULL(handler);
}

void test_service_dispatch_unsupported_services(void) {
    opcua_byte_t req_body[1];
    opcua_byte_t resp_body[1];
    size_t resp_len = 1;
    mu_server_t server;

    opcua_uint32_t unsupported[] = {
        673, /* WriteRequest (not in Nano) */
        711, /* CallRequest (Method) */
        643  /* HistoryReadRequest */
    };

    for (size_t i = 0; i < sizeof(unsupported)/sizeof(unsupported[0]); i++) {
        TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED, 
                          mu_service_dispatch(&server, unsupported[i], req_body, 1, resp_body, &resp_len));
    }
}

void test_activate_session_consumes_nonempty_certificates_and_identity_body(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.entropy_adapter.generate_random = fake_entropy;
    mu_session_init(&server.sessions[0]);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id;
    opcua_uint32_t auth_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_session_create(&server.sessions[0], 0, &revised_timeout,
                                        &session_id, &auth_token));
    TEST_ASSERT_EQUAL(12345, auth_token);

    /* OPC-10000-4 §5.7.3.2: ActivateSession includes ClientSoftwareCertificates[],
       LocaleIds[], UserIdentityToken, and UserTokenSignature in that order. */
    static const opcua_byte_t request_body[] = {
        /* 0x00 RequestHeader, authenticationToken i=12345 */
        0x01, 0x00, 0x39, 0x30, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 0x1f ClientSignature: null algorithm, null signature */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        /* 0x27 ClientSoftwareCertificates[1]: certificateData "ABC", signature "XY" */
        0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x41, 0x42, 0x43, 0x02, 0x00, 0x00, 0x00, 0x58,
        0x59,
        /* 0x38 LocaleIds[1]: "en-US" */
        0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
        0x65, 0x6e, 0x2d, 0x55, 0x53,
        /* 0x45 UserIdentityToken: AnonymousIdentityToken body policyId "anonymous" */
        0x01, 0x00, 0x41, 0x01, 0x01, 0x0d, 0x00, 0x00,
        0x00, 0x09, 0x00, 0x00, 0x00, 0x61, 0x6e, 0x6f,
        0x6e, 0x79, 0x6d, 0x6f, 0x75, 0x73,
        /* 0x5b UserTokenSignature: algorithm "sigalg", signature 0xab */
        0x06, 0x00, 0x00, 0x00, 0x73, 0x69, 0x67, 0x61,
        0x6c, 0x67, 0x01, 0x00, 0x00, 0x00, 0xab
    };
    TEST_ASSERT_EQUAL_size_t(106, sizeof(request_body));

    opcua_byte_t response_body[256];
    size_t response_len = sizeof(response_body);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST,
                                          request_body, sizeof(request_body),
                                          response_body, &response_len));
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
    RUN_TEST(test_service_dispatch_unknown_request);
    RUN_TEST(test_service_dispatch_unsupported_services);
    RUN_TEST(test_activate_session_consumes_nonempty_certificates_and_identity_body);
    return UNITY_END();
}
