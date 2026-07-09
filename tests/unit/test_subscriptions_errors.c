/* tests/unit/test_subscriptions_errors.c
 *
 * Feature 005 US1 malformed request checks for Standard DataChange
 * Subscription 2017 additions. These dispatch-level tests are active only
 * when MUC_OPCUA_SUBSCRIPTIONS_STANDARD is enabled.
 *
 * OPC-10000-4 5.13.5: SetTriggering.
 * OPC-10000-4 7.22.2: DataChangeFilter / DeadbandType.
 * OPC-10000-6 binary decoding: malformed lengths return Bad_DecodingError.
 */
#include "unity.h"

#include "../../src/core/server_internal.h"
#include "muc_opcua/muc_opcua.h"

#include <string.h>

#define AUTH_TOKEN 12345u
#define ID_DATACHANGEFILTER_ENC_BINARY 724u
#define ID_UNSUPPORTED_MONITORINGFILTER_ENC_BINARY 999999u
#define TEST_VARIABLE_NODE_ID 1000u

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD

static const mu_value_source_t s_test_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_FLOAT, {.f = 10.0f}}}};
static const mu_node_t s_test_nodes[] = {{{1u, MU_NODEID_NUMERIC, {TEST_VARIABLE_NODE_ID}},
                                          MU_NODECLASS_VARIABLE,
                                          {7, (const opcua_byte_t *)"TestVar"},
                                          {7, (const opcua_byte_t *)"TestVar"},
                                          NULL,
                                          0u,
                                          &s_test_value}};
static const mu_address_space_t s_test_address_space = {s_test_nodes, sizeof(s_test_nodes) / sizeof(s_test_nodes[0])};

static opcua_statuscode_t test_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer == NULL && length != 0u) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (buffer != NULL) {
        (void)memset(buffer, 0x5a, length);
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

    (void)memset(server, 0, sizeof(*server));
    server->config.entropy_adapter.generate_random = test_entropy;
    server->config.time_adapter.get_tick_ms = test_tick_ms;
    server->config.time_adapter.get_time = test_time;

    mu_secure_channel_init(&server->secure_channel);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_secure_channel_open(&server->secure_channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE,
                                                   3600000u, &server->config.entropy_adapter, &revised_lifetime));
    for (size_t i = 0u; i < MU_INTERN_MAX_SESSIONS; ++i) {
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

typedef struct {
    opcua_byte_t frame[1024];
    size_t frame_len;
    unsigned writes;
} publish_capture_t;

static opcua_statuscode_t capture_publish_write(void *context, void *client_handle, const opcua_byte_t *buffer,
                                                size_t length, size_t *bytes_written) {
    publish_capture_t *capture = (publish_capture_t *)context;
    (void)client_handle;

    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_TRUE(length <= sizeof(capture->frame));
    (void)memcpy(capture->frame, buffer, length);
    capture->frame_len = length;
    ++capture->writes;
    *bytes_written = length;
    return MU_STATUS_GOOD;
}

static void capture_publish_close(void *context, void *client_handle) {
    (void)context;
    (void)client_handle;
}

static opcua_uint32_t read_framed_response_type(const publish_capture_t *capture, opcua_uint32_t *handle,
                                                opcua_statuscode_t *service_result) {
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};

    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_TRUE(capture->frame_len >= 24u);
    mu_binary_reader_init(&r, capture->frame, capture->frame_len);
    r.position = 24u;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, handle, service_result);
    return response_type.identifier.numeric;
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

void test_create_monitored_items_truncated_filter_body_returns_decoding_error(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[256];
    mu_binary_writer_t w;

    prepare_dispatch_server(&server, &subscription_id);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 4u);
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
        /* OPC-10000-4 section 5.13.2.4: malformed filter body is Bad_DecodingError. */
        mu_binary_write_extension_object_header(&w, &filter_type, 16u);
        mu_binary_write_uint32(&w, 1u);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            dispatch_body(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, body, w.position));
}

void test_modify_monitored_items_truncated_filter_body_returns_decoding_error(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[256];
    mu_binary_writer_t w;

    prepare_dispatch_server(&server, &subscription_id);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 5u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t filter_type = {0u, MU_NODEID_NUMERIC, {ID_DATACHANGEFILTER_ENC_BINARY}};

        mu_binary_write_uint32(&w, 42u);
        mu_binary_write_uint32(&w, 7u);
        mu_binary_write_double(&w, 100.0);
        /* OPC-10000-4 section 5.13.3.4: malformed filter body is Bad_DecodingError. */
        mu_binary_write_extension_object_header(&w, &filter_type, 16u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            dispatch_body(&server, MU_ID_MODIFYMONITOREDITEMSREQUEST, body, w.position));
}

void test_create_monitored_items_unsupported_monitoring_filter_returns_item_status(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    server.config.address_space = &s_test_address_space;

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 6u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t node = {1u, MU_NODEID_NUMERIC, {TEST_VARIABLE_NODE_ID}};
        mu_string_t null_string = {-1, NULL};
        mu_nodeid_t filter_type = {0u, MU_NODEID_NUMERIC, {ID_UNSUPPORTED_MONITORINGFILTER_ENC_BINARY}};

        mu_binary_write_nodeid(&w, &node);
        mu_binary_write_uint32(&w, 13u);
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_uint16(&w, 0u);
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_uint32(&w, 2u);
        mu_binary_write_uint32(&w, 7u);
        mu_binary_write_double(&w, 100.0);
        mu_binary_write_extension_object_header(&w, &filter_type, 0u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CREATEMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(6u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.13.2.4 lists Bad_MonitoredItemFilterUnsupported
       as the CreateMonitoredItems per-item status for an unsupported MonitoringFilter. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED, item_result);
}

void test_create_monitored_items_invalid_monitoring_filter_returns_item_status(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    server.config.address_space = &s_test_address_space;

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 7u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t node = {1u, MU_NODEID_NUMERIC, {TEST_VARIABLE_NODE_ID}};
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
        mu_binary_write_extension_object_header(&w, &filter_type, 16u);
        mu_binary_write_uint32(&w, 99u);
        mu_binary_write_uint32(&w, 0u);
        mu_binary_write_double(&w, 0.0);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CREATEMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(7u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.13.2.4 lists Bad_MonitoredItemFilterInvalid
       as the CreateMonitoredItems per-item status for an invalid MonitoringFilter. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERINVALID, item_result);
}

void test_create_monitored_items_disallowed_monitoring_filter_returns_item_status(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t body[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    server.config.address_space = &s_test_address_space;

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 8u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t node = {1u, MU_NODEID_NUMERIC, {TEST_VARIABLE_NODE_ID}};
        mu_string_t null_string = {-1, NULL};
        mu_nodeid_t filter_type = {0u, MU_NODEID_NUMERIC, {ID_DATACHANGEFILTER_ENC_BINARY}};

        mu_binary_write_nodeid(&w, &node);
        mu_binary_write_uint32(&w, 3u);
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_uint16(&w, 0u);
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_uint32(&w, 2u);
        mu_binary_write_uint32(&w, 7u);
        mu_binary_write_double(&w, 100.0);
        mu_binary_write_extension_object_header(&w, &filter_type, 16u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_uint32(&w, 0u);
        mu_binary_write_double(&w, 0.0);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CREATEMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(8u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.13.2.4 lists Bad_FilterNotAllowed as the
       CreateMonitoredItems per-item status when a MonitoringFilter is disallowed. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_FILTERNOTALLOWED, item_result);
}

void test_modify_monitored_items_unsupported_monitoring_filter_returns_item_status(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    mu_monitored_item_t *item = NULL;
    opcua_byte_t body[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, subscription_id, &item));
    TEST_ASSERT_NOT_NULL(item);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 9u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t filter_type = {0u, MU_NODEID_NUMERIC, {ID_UNSUPPORTED_MONITORINGFILTER_ENC_BINARY}};

        mu_binary_write_uint32(&w, item->monitored_item_id);
        mu_binary_write_uint32(&w, 7u);
        mu_binary_write_double(&w, 100.0);
        mu_binary_write_extension_object_header(&w, &filter_type, 0u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_MODIFYMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_MODIFYMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(9u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.13.3.4 lists Bad_MonitoredItemFilterUnsupported
       as the ModifyMonitoredItems per-item status for an unsupported MonitoringFilter. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED, item_result);
}

void test_modify_monitored_items_invalid_monitoring_filter_returns_item_status(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    mu_monitored_item_t *item = NULL;
    opcua_byte_t body[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, subscription_id, &item));
    TEST_ASSERT_NOT_NULL(item);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 10u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t filter_type = {0u, MU_NODEID_NUMERIC, {ID_DATACHANGEFILTER_ENC_BINARY}};

        mu_binary_write_uint32(&w, item->monitored_item_id);
        mu_binary_write_uint32(&w, 7u);
        mu_binary_write_double(&w, 100.0);
        mu_binary_write_extension_object_header(&w, &filter_type, 16u);
        mu_binary_write_uint32(&w, 99u);
        mu_binary_write_uint32(&w, 0u);
        mu_binary_write_double(&w, 0.0);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_MODIFYMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_MODIFYMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(10u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.13.3.4 lists Bad_MonitoredItemFilterInvalid
       as the ModifyMonitoredItems per-item status for an invalid MonitoringFilter. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMFILTERINVALID, item_result);
}

void test_modify_monitored_items_disallowed_monitoring_filter_returns_item_status(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    mu_monitored_item_t *item = NULL;
    opcua_byte_t body[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, subscription_id, &item));
    TEST_ASSERT_NOT_NULL(item);
    item->attribute_id = 3u;

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 11u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);

    {
        mu_nodeid_t filter_type = {0u, MU_NODEID_NUMERIC, {ID_DATACHANGEFILTER_ENC_BINARY}};

        mu_binary_write_uint32(&w, item->monitored_item_id);
        mu_binary_write_uint32(&w, 7u);
        mu_binary_write_double(&w, 100.0);
        mu_binary_write_extension_object_header(&w, &filter_type, 16u);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_uint32(&w, 0u);
        mu_binary_write_double(&w, 0.0);
        mu_binary_write_uint32(&w, 1u);
        mu_binary_write_boolean(&w, true);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_MODIFYMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_MODIFYMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(11u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.13.3.4 lists Bad_FilterNotAllowed as the
       ModifyMonitoredItems per-item status when a MonitoringFilter is disallowed. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_FILTERNOTALLOWED, item_result);
}

void test_modify_monitored_items_unknown_id_returns_item_status_without_mutation(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    mu_monitored_item_t *item = NULL;
    opcua_byte_t body[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    mu_nodeid_t null_filter = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;
    opcua_double_t revised_sampling_interval = -1.0;
    opcua_uint32_t revised_queue_size = 0u;
    mu_nodeid_t filter_result_type = {0u, MU_NODEID_NUMERIC, {0u}};
    size_t filter_result_length = 0u;
    opcua_int32_t diagnostic_count = 0;
    const opcua_uint32_t invalid_monitored_item_id = 0xBEEFu;
    const opcua_uint32_t existing_queue_size = 1u;
    const opcua_boolean_t existing_discard_oldest = false;
    const opcua_uint32_t requested_queue_size = 2u;
    const opcua_boolean_t requested_discard_oldest = true;

    prepare_dispatch_server(&server, &subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, subscription_id, &item));
    TEST_ASSERT_NOT_NULL(item);
    item->client_handle = 17u;
    item->sampling_interval_ms = 250u;
    item->queue_size = existing_queue_size;
    item->discard_oldest = existing_discard_oldest;
    item->attribute_id = 13u;

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 13u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, invalid_monitored_item_id);
    mu_binary_write_uint32(&w, 99u);
    mu_binary_write_double(&w, 100.0);
    mu_binary_write_extension_object_header(&w, &null_filter, 0u);
    mu_binary_write_uint32(&w, requested_queue_size);
    mu_binary_write_boolean(&w, requested_discard_oldest);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_MODIFYMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_MODIFYMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(13u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 sections 5.13.3 and 5.13.3.4 require an unknown
       monitoredItemId to return per-item Bad_MonitoredItemIdInvalid. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMIDINVALID, item_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_double(&r, &revised_sampling_interval));
    TEST_ASSERT_TRUE(revised_sampling_interval == 0.0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &revised_queue_size));
    TEST_ASSERT_EQUAL_UINT32(1u, revised_queue_size);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_binary_read_extension_object_header(&r, &filter_result_type, &filter_result_length));
    TEST_ASSERT_EQUAL_UINT16(0u, filter_result_type.namespace_index);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, filter_result_type.identifier_type);
    TEST_ASSERT_EQUAL_UINT32(0u, filter_result_type.identifier.numeric);
    TEST_ASSERT_EQUAL_size_t(0u, filter_result_length);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);

    TEST_ASSERT_EQUAL_UINT32(1u, server.subs.active_monitored_items_count);
    TEST_ASSERT_TRUE(item->in_use);
    TEST_ASSERT_EQUAL_UINT32(17u, item->client_handle);
    TEST_ASSERT_EQUAL_UINT32(250u, item->sampling_interval_ms);
    TEST_ASSERT_EQUAL_UINT32(existing_queue_size, item->queue_size);
    TEST_ASSERT_EQUAL(existing_discard_oldest, item->discard_oldest);
}

void test_delete_monitored_items_unknown_id_returns_item_status_without_deleting_existing_item(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    mu_monitored_item_t *item = NULL;
    opcua_byte_t body[128];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;
    opcua_int32_t diagnostic_count = 0;
    const opcua_uint32_t invalid_monitored_item_id = 0xBEEFu;
    opcua_uint32_t original_subscription_id = 0u;
    opcua_uint32_t original_monitored_item_id = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, subscription_id, &item));
    TEST_ASSERT_NOT_NULL(item);
    original_subscription_id = item->subscription_id;
    original_monitored_item_id = item->monitored_item_id;

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 14u);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, invalid_monitored_item_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_DELETEMONITOREDITEMSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_DELETEMONITOREDITEMSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(14u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.13.6 requires an unknown monitoredItemId to
       return per-item Bad_MonitoredItemIdInvalid. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_MONITOREDITEMIDINVALID, item_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);

    TEST_ASSERT_EQUAL_UINT32(1u, server.subs.active_monitored_items_count);
    TEST_ASSERT_TRUE(item->in_use);
    TEST_ASSERT_EQUAL_UINT32(original_subscription_id, item->subscription_id);
    TEST_ASSERT_EQUAL_UINT32(original_monitored_item_id, item->monitored_item_id);
}

void test_modify_subscription_unknown_id_returns_subscription_id_invalid_without_mutation(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    mu_subscription_t *sub = NULL;
    opcua_uint32_t original_interval_ms = 0u;
    opcua_uint32_t original_lifetime_count = 0u;
    opcua_uint32_t original_keep_alive_count = 0u;
    opcua_uint32_t original_max_notifications_per_publish = 0u;
    opcua_byte_t original_priority = 0u;
    opcua_byte_t body[128];
    mu_binary_writer_t w;
    const opcua_uint32_t invalid_subscription_id = 0xDEADu;
    const opcua_uint32_t requested_max_notifications_per_publish = 7u;
    const opcua_byte_t requested_priority = 3u;

    prepare_dispatch_server(&server, &subscription_id);
    sub = mu_subscription_find(&server.subs, server.sessions[0].session_id, subscription_id);
    TEST_ASSERT_NOT_NULL(sub);
    original_interval_ms = sub->publishing_interval_ms;
    original_lifetime_count = sub->lifetime_count;
    original_keep_alive_count = sub->max_keep_alive_count;
    original_max_notifications_per_publish = sub->max_notifications_per_publish;
    original_priority = sub->priority;

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 15u);
    mu_binary_write_uint32(&w, invalid_subscription_id);
    mu_binary_write_double(&w, 250.0);
    mu_binary_write_uint32(&w, 60u);
    mu_binary_write_uint32(&w, 20u);
    mu_binary_write_uint32(&w, requested_max_notifications_per_publish);
    mu_binary_write_byte(&w, requested_priority);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    /* OPC-10000-4 section 5.14.3 requires an unknown subscriptionId to
       return Bad_SubscriptionIdInvalid for ModifySubscription. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID,
                            dispatch_body(&server, MU_ID_MODIFYSUBSCRIPTIONREQUEST, body, w.position));

    TEST_ASSERT_TRUE(sub->in_use);
    TEST_ASSERT_EQUAL_UINT32(subscription_id, sub->subscription_id);
    TEST_ASSERT_EQUAL_UINT32(original_interval_ms, sub->publishing_interval_ms);
    TEST_ASSERT_EQUAL_UINT32(original_lifetime_count, sub->lifetime_count);
    TEST_ASSERT_EQUAL_UINT32(original_keep_alive_count, sub->max_keep_alive_count);
    TEST_ASSERT_EQUAL_UINT32(original_max_notifications_per_publish, sub->max_notifications_per_publish);
    TEST_ASSERT_EQUAL_UINT8(original_priority, sub->priority);
}

void test_delete_subscriptions_unknown_id_returns_per_operation_status_without_deleting_existing_subscription(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    mu_subscription_t *sub = NULL;
    opcua_byte_t body[128];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t results_count = 0;
    opcua_statuscode_t item_result = 0u;
    opcua_int32_t diagnostic_count = 0;
    const opcua_uint32_t invalid_subscription_id = 0xDEADu;

    prepare_dispatch_server(&server, &subscription_id);
    sub = mu_subscription_find(&server.subs, server.sessions[0].session_id, subscription_id);
    TEST_ASSERT_NOT_NULL(sub);

    mu_binary_writer_init(&w, body, sizeof(body));
    write_request_header(&w, 16u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, invalid_subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_DELETESUBSCRIPTIONSREQUEST, body,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_DELETESUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(16u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL_INT32(1, results_count);

    /* OPC-10000-4 section 5.14.8 reports unknown subscriptionIds in
       DeleteSubscriptions results[] as Bad_SubscriptionIdInvalid. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &item_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID, item_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);

    TEST_ASSERT_TRUE(sub->in_use);
    TEST_ASSERT_EQUAL_UINT32(subscription_id, sub->subscription_id);
}

void test_publish_oversize_notification_response_returns_response_too_large_fault(void) {
    mu_server_t server;
    opcua_uint32_t subscription_id = 0u;
    opcua_byte_t request_body[128];
    opcua_byte_t immediate_response[64];
    opcua_byte_t send_buffer[1024];
    opcua_byte_t large_value[64];
    publish_capture_t capture;
    mu_binary_writer_t w;
    size_t response_len = sizeof(immediate_response);
    opcua_uint32_t response_handle = 0u;
    opcua_statuscode_t service_result = 0u;

    prepare_dispatch_server(&server, &subscription_id);
    (void)memset(&capture, 0, sizeof(capture));
    (void)memset(large_value, 'x', sizeof(large_value));
    server.config.tcp_adapter.context = &capture;
    server.config.tcp_adapter.write = capture_publish_write;
    server.config.tcp_adapter.close_connection = capture_publish_close;
    server.config.send_buffer = send_buffer;
    server.config.send_buffer_size = sizeof(send_buffer);
    server.current_request_id = 77u;
    server.active_session = &server.sessions[0];
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    server.conns[0].client_handle = (void *)0x1;
    server.conns[0].secure_channel = server.secure_channel;
#else
    server.client_handle = (void *)0x1;
#endif

    for (size_t i = 0u; i < 8u; ++i) {
        mu_monitored_item_t *item = NULL;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server.subs, subscription_id, &item));
        TEST_ASSERT_NOT_NULL(item);
        item->client_handle = (opcua_uint32_t)(100u + i);
        item->attribute_id = 13u;
        item->monitoring_mode = MU_MONITORING_MODE_REPORTING;
        item->next_sample_ms = 1000u;
        item->pending = true;
        item->queue_size = 1u;
        item->queue_count = 1u;
        item->queue[0].value.type = MU_TYPE_STRING;
        item->queue[0].value.value.str.length = (opcua_int32_t)sizeof(large_value);
        item->queue[0].value.value.str.data = large_value;
        item->queue[0].status = MU_STATUS_GOOD;
    }

    mu_binary_writer_init(&w, request_body, sizeof(request_body));
    write_request_header(&w, 12u);
    mu_binary_write_int32(&w, 0);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_PUBLISHREQUEST, request_body, w.position,
                                                                immediate_response, &response_len));
    TEST_ASSERT_EQUAL_size_t(0u, response_len);

    /* OPC-10000-4 sections 5.3, 5.14.5.1, and 7.38.2 require a Publish
       response that exceeds response limits to complete deterministically with
       Bad_ResponseTooLarge rather than dropping the parked Publish request. */
    mu_subscriptions_tick(&server, 100u);

    TEST_ASSERT_EQUAL_UINT(1u, capture.writes);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_SERVICEFAULT,
                             read_framed_response_type(&capture, &response_handle, &service_result));
    TEST_ASSERT_EQUAL_UINT32(12u, response_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_RESPONSETOOLARGE, service_result);
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
    TEST_PASS_MESSAGE("MUC_OPCUA_SUBSCRIPTIONS_STANDARD is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    RUN_TEST(test_malformed_datachange_filter_length_returns_decoding_error);
    RUN_TEST(test_create_monitored_items_truncated_filter_body_returns_decoding_error);
    RUN_TEST(test_modify_monitored_items_truncated_filter_body_returns_decoding_error);
    RUN_TEST(test_create_monitored_items_unsupported_monitoring_filter_returns_item_status);
    RUN_TEST(test_create_monitored_items_invalid_monitoring_filter_returns_item_status);
    RUN_TEST(test_create_monitored_items_disallowed_monitoring_filter_returns_item_status);
    RUN_TEST(test_modify_monitored_items_unsupported_monitoring_filter_returns_item_status);
    RUN_TEST(test_modify_monitored_items_invalid_monitoring_filter_returns_item_status);
    RUN_TEST(test_modify_monitored_items_disallowed_monitoring_filter_returns_item_status);
    RUN_TEST(test_modify_monitored_items_unknown_id_returns_item_status_without_mutation);
    RUN_TEST(test_delete_monitored_items_unknown_id_returns_item_status_without_deleting_existing_item);
    RUN_TEST(test_modify_subscription_unknown_id_returns_subscription_id_invalid_without_mutation);
    RUN_TEST(test_delete_subscriptions_unknown_id_returns_per_operation_status_without_deleting_existing_subscription);
    RUN_TEST(test_publish_oversize_notification_response_returns_response_too_large_fault);
    RUN_TEST(test_truncated_set_triggering_link_array_returns_decoding_error);
    RUN_TEST(test_oversized_set_triggering_link_array_returns_too_many_operations);
#else
    RUN_TEST(test_standard_error_tests_require_standard_subscription_build);
#endif
    return UNITY_END();
}
