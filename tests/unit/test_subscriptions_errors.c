/* tests/unit/test_subscriptions_errors.c
 *
 * Feature 005 US1 malformed request checks for Standard DataChange
 * Subscription 2017 additions. These dispatch-level tests are active only
 * when MICRO_OPCUA_SUBSCRIPTIONS_STANDARD is enabled.
 *
 * OPC-10000-4 5.13.5: SetTriggering.
 * OPC-10000-4 7.22.2: DataChangeFilter / DeadbandType.
 * OPC-10000-6 binary decoding: malformed lengths return Bad_DecodingError.
 */
#include "unity.h"

#include "../../src/core/server_internal.h"
#include "micro_opcua/micro_opcua.h"

#include <string.h>

#define AUTH_TOKEN 12345u
#define ID_DATACHANGEFILTER_ENC_BINARY 724u

void setUp(void) {}
void tearDown(void) {}

#if MICRO_OPCUA_SUBSCRIPTIONS && MICRO_OPCUA_SUBSCRIPTIONS_STANDARD

static opcua_statuscode_t test_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer == NULL && length != 0u) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (buffer != NULL) {
        memset(buffer, 0x5a, length);
    }
    return MU_STATUS_GOOD;
}

static opcua_uint64_t test_tick_ms(void *context) {
    (void)context;
    return 0u;
}

static opcua_datetime_t test_time(void *context) {
    (void)context;
    return 0;
}

static void prepare_dispatch_server(mu_server_t *server, opcua_uint32_t *subscription_id) {
    opcua_uint32_t revised_lifetime = 0u;
    opcua_uint64_t revised_timeout = 0u;
    opcua_uint32_t session_id = 0u;
    opcua_uint32_t auth_token = 0u;
    mu_subscription_t *sub = NULL;

    memset(server, 0, sizeof(*server));
    server->config.entropy_adapter.generate_random = test_entropy;
    server->config.time_adapter.get_tick_ms = test_tick_ms;
    server->config.time_adapter.get_time = test_time;

    mu_secure_channel_init(&server->secure_channel);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_secure_channel_open(&server->secure_channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE,
                                                   3600000u, &revised_lifetime));
    for (size_t i = 0u; i < MU_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_session_create(&server->sessions[0], 0u, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL_UINT32(AUTH_TOKEN, auth_token);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token, 321u));
    server->sessions[0].secure_channel_id = server->secure_channel.channel_id;

    mu_subscriptions_init(&server->subs);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_subscription_create(&server->subs, session_id, 100u, 30u, 10u, 0u, 0u, true, 0u, &sub));
    TEST_ASSERT_NOT_NULL(sub);
    *subscription_id = sub->subscription_id;
}

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t auth = {0u, MU_NODEID_NUMERIC, {AUTH_TOKEN}};
    mu_nodeid_t null_id = {0u, MU_NODEID_NUMERIC, {0u}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(w, &auth);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &null_string);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &null_id, 0u);
}

static opcua_statuscode_t dispatch_body(mu_server_t *server, opcua_uint32_t request_id, const opcua_byte_t *body,
                                        size_t body_len) {
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    return mu_service_dispatch(server, request_id, body, body_len, response, &response_len);
}

void test_malformed_datachange_filter_length_returns_decoding_error(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[256];
    mu_binary_writer_t w;

    prepare_dispatch_server(&server, &subscription_id);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 1u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t node = {0u, MU_NODEID_NUMERIC, {2258u}};
        mu_string_t null_string = {-1, NULL};
        mu_nodeid_t filter_type = {0u, MU_NODEID_NUMERIC, {ID_DATACHANGEFILTER_ENC_BINARY}};

        mu_binary_write_nodeid(&w, &node);
        mu_binary_write_uint32(&w, 13u);
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_uint16(&w, 0u);
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_uint32(&w, 2u);
        mu_binary_write_uint32(&w, 7u);
        mu_binary_write_double(&w, 100.0);
        mu_binary_write_extension_object_header(&w, &filter_type, 4u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            dispatch_body(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, body, w.position));
}

void test_truncated_set_triggering_link_array_returns_decoding_error(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[128];
    mu_binary_writer_t w;

    prepare_dispatch_server(&server, &subscription_id);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 2u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 1u);
    mu_binary_write_int32(&w, 1);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            dispatch_body(&server, MU_ID_SETTRIGGERINGREQUEST, body, w.position));
}

void test_oversized_set_triggering_link_array_returns_too_many_operations(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[128];
    mu_binary_writer_t w;

    prepare_dispatch_server(&server, &subscription_id);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 3u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 1u);
    mu_binary_write_int32(&w, 33);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS,
                            dispatch_body(&server, MU_ID_SETTRIGGERINGREQUEST, body, w.position));
}

#else

void test_standard_error_tests_require_standard_subscription_build(void) {
    TEST_PASS_MESSAGE("MICRO_OPCUA_SUBSCRIPTIONS_STANDARD is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MICRO_OPCUA_SUBSCRIPTIONS && MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
    RUN_TEST(test_malformed_datachange_filter_length_returns_decoding_error);
    RUN_TEST(test_truncated_set_triggering_link_array_returns_decoding_error);
    RUN_TEST(test_oversized_set_triggering_link_array_returns_too_many_operations);
#else
    RUN_TEST(test_standard_error_tests_require_standard_subscription_build);
#endif
    return UNITY_END();
}
