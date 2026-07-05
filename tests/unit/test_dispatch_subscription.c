/* tests/unit/test_dispatch_subscription.c
 *
 * T032a [P] [US5]: Isolated unit tests for dispatch_subscription.c
 * subscription dispatch handlers — CreateSubscription, ModifySubscription,
 * and DeleteSubscriptions dispatch paths.
 *
 * Grounding:
 *   OPC-10000-4 §5.14.2 CreateSubscription
 *   OPC-10000-4 §5.14.3 ModifySubscription
 *   OPC-10000-4 §5.14.8 DeleteSubscriptions
 */
#include "unity.h"

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/muc_opcua.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS

static opcua_statuscode_t test_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer != NULL) {
        (void)memset(buffer, 0x5A, length);
    }
    return MU_STATUS_GOOD;
}

static opcua_uint64_t test_tick_ms(void *context) {
    (void)context;
    return 1000u;
}

static opcua_datetime_t test_time(void *context) {
    (void)context;
    return 0;
}

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t auth_token = {0u, MU_NODEID_NUMERIC, {12345u}};
    mu_nodeid_t null_id = {0u, MU_NODEID_NUMERIC, {0u}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(w, &auth_token);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &null_string);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &null_id, 0u);
}

static void read_response_header(mu_binary_reader_t *r, opcua_uint32_t *handle, opcua_statuscode_t *result) {
    opcua_int64_t timestamp = 0;
    opcua_byte_t diagnostics_mask = 0u;
    opcua_int32_t string_table_count = 0;
    mu_nodeid_t additional_header_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_byte_t additional_header_encoding = 0u;

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int64(r, &timestamp));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(r, handle));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(r, result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(r, &diagnostics_mask));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(r, &string_table_count));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(r, &additional_header_type));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(r, &additional_header_encoding));
}

static opcua_uint32_t count_subscriptions(const mu_subscriptions_t *subs) {
    opcua_uint32_t count = 0u;
    for (opcua_uint32_t i = 0u; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        if (subs->subscriptions[i].in_use) {
            ++count;
        }
    }
    return count;
}

static void prepare_server(mu_server_t *server) {
    opcua_uint64_t revised_timeout = 0u;
    opcua_uint32_t session_id = 0u;
    opcua_uint32_t auth_token = 0u;
    opcua_uint32_t sc_lifetime = 0u;

    (void)memset(server, 0, sizeof(*server));
    server->config.entropy_adapter.generate_random = test_entropy;
    server->config.time_adapter.get_tick_ms = test_tick_ms;
    server->config.time_adapter.get_time = test_time;

    mu_secure_channel_init(&server->secure_channel);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_secure_channel_open(&server->secure_channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE,
                                                   3600000u, &server->config.entropy_adapter, &sc_lifetime));
    for (size_t i = 0u; i < MU_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_session_create(&server->sessions[0], 0u, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token, 321u));
    server->sessions[0].secure_channel_id = server->secure_channel.channel_id;
    server->active_session = &server->sessions[0];

    mu_subscriptions_init(&server->subs);
}

/* ------------------------------------------------------------------ */
/*  CreateSubscription dispatch path                                   */
/* ------------------------------------------------------------------ */

void test_create_subscription_returns_valid_subscription_id(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_uint32_t subscription_id = 0u;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 1u);
    mu_binary_write_double(&w, 500.0);
    mu_binary_write_uint32(&w, 30u);
    mu_binary_write_uint32(&w, 10u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CREATESUBSCRIPTIONRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(1u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &subscription_id));
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, subscription_id);
}

void test_create_subscription_returns_revised_parameters(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_uint32_t subscription_id = 0u;
    opcua_uint64_t revised_interval_bits = 0u;
    opcua_uint32_t revised_lifetime_count = 0u;
    opcua_uint32_t revised_keep_alive_count = 0u;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 2u);
    mu_binary_write_double(&w, 100.0);
    mu_binary_write_uint32(&w, 60u);
    mu_binary_write_uint32(&w, 20u);
    mu_binary_write_uint32(&w, 5u);
    mu_binary_write_boolean(&w, false);
    mu_binary_write_byte(&w, 127u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CREATESUBSCRIPTIONRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(2u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &subscription_id));
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint64(&r, &revised_interval_bits));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, revised_interval_bits);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &revised_lifetime_count));
    TEST_ASSERT_EQUAL_UINT32(60u, revised_lifetime_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &revised_keep_alive_count));
    TEST_ASSERT_EQUAL_UINT32(20u, revised_keep_alive_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

void test_create_subscription_creates_subscription_in_server_state(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 3u);
    mu_binary_write_double(&w, 250.0);
    mu_binary_write_uint32(&w, 50u);
    mu_binary_write_uint32(&w, 15u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 4u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    opcua_uint32_t count = 0u;
    for (size_t i = 0u; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        if (server.subs.subscriptions[i].in_use) {
            ++count;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, count);
}

void test_create_subscription_duplicate_keeps_first_and_returns_another_id(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_uint32_t first_id = 0u;
    opcua_uint32_t second_id = 0u;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 4u);
    mu_binary_write_double(&w, 500.0);
    mu_binary_write_uint32(&w, 30u);
    mu_binary_write_uint32(&w, 10u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &first_id));

    /* Second create should succeed with a different subscription id. */
    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 5u);
    mu_binary_write_double(&w, 500.0);
    mu_binary_write_uint32(&w, 30u);
    mu_binary_write_uint32(&w, 10u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    response_len = sizeof(response);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &second_id));

    TEST_ASSERT_NOT_EQUAL_UINT32(first_id, second_id);

    opcua_uint32_t count = 0u;
    for (size_t i = 0u; i < MU_MAX_SUBSCRIPTIONS; ++i) {
        if (server.subs.subscriptions[i].in_use) {
            ++count;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(2u, count);
}

/* ------------------------------------------------------------------ */
/*  ModifySubscription dispatch path                                   */
/* ------------------------------------------------------------------ */

void test_modify_subscription_updates_and_returns_revised_parameters(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_uint32_t subscription_id = 0u;
    opcua_uint64_t revised_interval_bits = 0u;
    opcua_uint32_t revised_lifetime_count = 0u;
    opcua_uint32_t revised_keep_alive_count = 0u;

    prepare_server(&server);

    /* Create a subscription to modify. */
    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 10u);
    mu_binary_write_double(&w, 500.0);
    mu_binary_write_uint32(&w, 30u);
    mu_binary_write_uint32(&w, 10u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &subscription_id));
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, subscription_id);

    /* Modify the subscription. */
    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 11u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_double(&w, 200.0);
    mu_binary_write_uint32(&w, 45u);
    mu_binary_write_uint32(&w, 15u);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_byte(&w, 5u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    response_len = sizeof(response);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_MODIFYSUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_MODIFYSUBSCRIPTIONRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(11u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint64(&r, &revised_interval_bits));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, revised_interval_bits);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &revised_lifetime_count));
    TEST_ASSERT_EQUAL_UINT32(45u, revised_lifetime_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &revised_keep_alive_count));
    TEST_ASSERT_EQUAL_UINT32(15u, revised_keep_alive_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

void test_modify_subscription_unknown_id_returns_subscription_id_invalid(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    const opcua_uint32_t unknown_subscription_id = 0xDEADBEEFu;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 12u);
    mu_binary_write_uint32(&w, unknown_subscription_id);
    mu_binary_write_double(&w, 200.0);
    mu_binary_write_uint32(&w, 45u);
    mu_binary_write_uint32(&w, 15u);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_byte(&w, 5u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID,
                            mu_service_dispatch(&server, MU_ID_MODIFYSUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));
}

/* ------------------------------------------------------------------ */
/*  DeleteSubscriptions dispatch path                                  */
/* ------------------------------------------------------------------ */

void test_delete_subscriptions_removes_existing_subscription(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_uint32_t subscription_id = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t per_id_status = 0u;
    opcua_int32_t diagnostic_count = 0;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 20u);
    mu_binary_write_double(&w, 500.0);
    mu_binary_write_uint32(&w, 30u);
    mu_binary_write_uint32(&w, 10u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &subscription_id));
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, subscription_id);
    TEST_ASSERT_EQUAL_UINT32(1u, count_subscriptions(&server.subs));

    /* Delete the subscription. */
    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 21u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    response_len = sizeof(response);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_DELETESUBSCRIPTIONSREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_DELETESUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(21u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &per_id_status));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, per_id_status);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);

    TEST_ASSERT_EQUAL_UINT32(0u, count_subscriptions(&server.subs));
}

void test_delete_subscriptions_unknown_id_returns_per_operation_status(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t per_id_status = 0u;
    opcua_int32_t diagnostic_count = 0;
    const opcua_uint32_t unknown_subscription_id = 0xF00Du;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 22u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, unknown_subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_DELETESUBSCRIPTIONSREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_DELETESUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(22u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 §5.14.8 reports unknown subscriptionIds as
       Bad_SubscriptionIdInvalid in the per-id results[]. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &per_id_status));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID, per_id_status);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

void test_delete_subscriptions_multiple_ids_returns_corresponding_statuses(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_uint32_t subscription_id = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t per_id_status = 0u;
    opcua_int32_t diagnostic_count = 0;
    const opcua_uint32_t unknown_subscription_id = 0xC0FFEEu;

    prepare_server(&server);

    /* Create a subscription first. */
    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 30u);
    mu_binary_write_double(&w, 500.0);
    mu_binary_write_uint32(&w, 30u);
    mu_binary_write_uint32(&w, 10u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_CREATESUBSCRIPTIONREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &subscription_id));
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, subscription_id);

    /* Delete two ids: one valid, one unknown. */
    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 31u);
    mu_binary_write_int32(&w, 2);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, unknown_subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    response_len = sizeof(response);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_service_dispatch(&server, MU_ID_DELETESUBSCRIPTIONSREQUEST, request, w.position,
                                                response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_DELETESUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(31u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(2, results_count);

    /* First status: valid subscription should be deleted → GOOD. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &per_id_status));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, per_id_status);

    /* Second status: unknown subscription → Bad_SubscriptionIdInvalid. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &per_id_status));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID, per_id_status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

#else

void test_subscription_dispatch_tests_require_subscription_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS
    RUN_TEST(test_create_subscription_returns_valid_subscription_id);
    RUN_TEST(test_create_subscription_returns_revised_parameters);
    RUN_TEST(test_create_subscription_creates_subscription_in_server_state);
    RUN_TEST(test_create_subscription_duplicate_keeps_first_and_returns_another_id);
    RUN_TEST(test_modify_subscription_updates_and_returns_revised_parameters);
    RUN_TEST(test_modify_subscription_unknown_id_returns_subscription_id_invalid);
    RUN_TEST(test_delete_subscriptions_removes_existing_subscription);
    RUN_TEST(test_delete_subscriptions_unknown_id_returns_per_operation_status);
    RUN_TEST(test_delete_subscriptions_multiple_ids_returns_corresponding_statuses);
#else
    RUN_TEST(test_subscription_dispatch_tests_require_subscription_build);
#endif
    return UNITY_END();
}
