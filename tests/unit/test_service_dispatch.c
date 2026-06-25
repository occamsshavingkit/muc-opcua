/* tests/unit/test_service_dispatch.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/service_dispatch.h"

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
        MU_ID_WRITEREQUEST, 
        MU_ID_CREATESUBSCRIPTIONREQUEST, 
        MU_ID_PUBLISHREQUEST, 
        MU_ID_CALLREQUEST, 
        MU_ID_HISTORYREADREQUEST, 
        MU_ID_BROWSENEXTREQUEST, 
        MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, 
        MU_ID_REGISTERNODESREQUEST
    };

    for (size_t i = 0; i < sizeof(unsupported)/sizeof(unsupported[0]); i++) {
        TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED, 
                          mu_service_dispatch(&server, unsupported[i], req_body, 1, resp_body, &resp_len));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_service_dispatch_known_requests);
    RUN_TEST(test_service_dispatch_unknown_request);
    RUN_TEST(test_service_dispatch_unsupported_services);
    return UNITY_END();
}
