/* tests/unit/test_method_call.c
 *
 * Feature 005 US3: Server Call method support for OPC-10000-5 Base Info methods.
 * Active coverage is gated to the Embedded/Standard slice:
 * MUC_OPCUA_SUBSCRIPTIONS_STANDARD + MUC_OPCUA_BASE_TYPE_SYSTEM.
 *
 * OPC-10000-4 5.12.2.2: Call Service request/response wire shape.
 * OPC-10000-5 9.1: Server/GetMonitoredItems.
 * OPC-10000-5 9.2: Server/ResendData.
 */
#include "unity.h"

#include "../../src/address_space/base_nodes.h"
#include "../../src/core/server_internal.h"
#include "../../src/core/uasc.h"
#include "../../src/services/secure_channel.h"
#include "../../src/services/session.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/muc_opcua.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM

#define AUTH_TOKEN 12345u
#define ID_SERVER_OBJECT 2253u
#define ID_SERVER_GETMONITOREDITEMS 11492u
#define ID_SERVER_RESENDDATA 12873u
#define ID_SERVER_GETMONITOREDITEMS_INPUTARGUMENTS 11493u
#define ID_SERVER_GETMONITOREDITEMS_OUTPUTARGUMENTS 11494u
#define ID_SERVER_RESENDDATA_INPUTARGUMENTS 12874u
#define ID_DATACHANGENOTIFICATION 811u
#define SAMPLE_NODE_ID 5000u

typedef struct {
    opcua_byte_t send_buffer[8192];
    opcua_byte_t last_write[8192];
    size_t last_write_len;
} test_io_t;

static opcua_int32_t s_value;

static opcua_statuscode_t sample_value_read(void *context, const mu_nodeid_t *node_id, mu_variant_t *value) {
    (void)context;
    TEST_ASSERT_EQUAL_UINT16(1u, node_id->namespace_index);
    TEST_ASSERT_EQUAL_UINT32(SAMPLE_NODE_ID, node_id->identifier.numeric);

    memset(value, 0, sizeof(*value));
    value->type = MU_TYPE_INT32;
    value->value.i32 = s_value;
    return MU_STATUS_GOOD;
}

static mu_value_source_t s_sample_value = {MU_VALUESOURCE_CALLBACK, {.callback = {sample_value_read, NULL}}};

static const mu_node_t s_user_nodes[] = {{{1u, MU_NODEID_NUMERIC, {SAMPLE_NODE_ID}},
                                          MU_NODECLASS_VARIABLE,
                                          {6, (const opcua_byte_t *)"Sample"},
                                          {6, (const opcua_byte_t *)"Sample"},
                                          NULL,
                                          0,
                                          &s_sample_value}};

static const mu_address_space_t s_user_space = {s_user_nodes, sizeof(s_user_nodes) / sizeof(s_user_nodes[0])};

static opcua_statuscode_t fake_write(void *context, void *handle, const opcua_byte_t *buffer, size_t length,
                                     size_t *written) {
    test_io_t *io = (test_io_t *)context;
    (void)handle;
    TEST_ASSERT_TRUE(length <= sizeof(io->last_write));
    memcpy(io->last_write, buffer, length);
    io->last_write_len = length;
    *written = length;
    return MU_STATUS_GOOD;
}

static void fake_close(void *context, void *handle) {
    (void)context;
    (void)handle;
}

static opcua_statuscode_t test_method_call_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer != NULL) {
        (void)memset(buffer, 0x5a, length);
    }
    return MU_STATUS_GOOD;
}

static void prepare_server(mu_server_t *server, test_io_t *io) {
    opcua_uint32_t revised_lifetime = 0u;
    opcua_uint64_t revised_timeout = 0u;
    opcua_uint32_t session_id = 0u;
    opcua_uint32_t auth_token = 0u;

    memset(server, 0, sizeof(*server));
    memset(io, 0, sizeof(*io));

    server->config.address_space = &s_user_space;
    server->config.send_buffer = io->send_buffer;
    server->config.send_buffer_size = sizeof(io->send_buffer);
    server->config.entropy_adapter.generate_random = test_method_call_entropy;
    server->config.tcp_adapter.context = io;
    server->config.tcp_adapter.write = fake_write;
    server->config.tcp_adapter.close_connection = fake_close;
    server->client_handle = (void *)1;

    mu_secure_channel_init(&server->secure_channel);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_secure_channel_open(&server->secure_channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE,
                                                   3600000u, &server->config.entropy_adapter, &revised_lifetime));
    for (size_t i = 0u; i < MU_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_session_create(&server->sessions[0], 0u, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL_UINT32(AUTH_TOKEN, auth_token);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token,
                                                                MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY));
    server->sessions[0].secure_channel_id = server->secure_channel.channel_id;
    server->conns[0].client_handle = (void *)1;
    server->conns[0].secure_channel = server->secure_channel;
    mu_subscriptions_init(&server->subs);
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t auth_token, opcua_uint32_t request_handle) {
    mu_nodeid_t token = {0u, MU_NODEID_NUMERIC, {auth_token}};
    mu_nodeid_t null_node = {0u, MU_NODEID_NUMERIC, {0u}};
    mu_string_t null_string = {-1, NULL};

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_nodeid(writer, &token));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int64(writer, 0));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_uint32(writer, request_handle));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_uint32(writer, 0));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_string(writer, &null_string));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_uint32(writer, 0));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_extension_object_header(writer, &null_node, 0u));
}

static void write_uint32_arg(mu_binary_writer_t *writer, opcua_uint32_t value) {
    mu_variant_t variant;
    memset(&variant, 0, sizeof(variant));
    variant.type = MU_TYPE_UINT32;
    variant.value.ui32 = value;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_variant(writer, &variant));
}

static size_t write_call_request(opcua_byte_t *buffer, size_t capacity, opcua_uint32_t request_handle,
                                 opcua_uint32_t object_id, opcua_uint32_t method_id, opcua_uint32_t subscription_id) {
    mu_binary_writer_t writer;
    mu_nodeid_t object = {0u, MU_NODEID_NUMERIC, {object_id}};
    mu_nodeid_t method = {0u, MU_NODEID_NUMERIC, {method_id}};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, AUTH_TOKEN, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 1));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &object));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &method));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 1));
    write_uint32_arg(&writer, subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, writer.status);
    return writer.position;
}

static void skip_response_header(mu_binary_reader_t *reader, opcua_uint32_t *request_handle,
                                 opcua_statuscode_t *service_result) {
    opcua_int64_t timestamp;
    opcua_byte_t diagnostics_mask;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int64(reader, &timestamp));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(reader, request_handle));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, service_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(reader, &diagnostics_mask));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(reader, &string_table_count));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(reader, &additional_header_type));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(reader, &additional_header_encoding));

    TEST_ASSERT_EQUAL_INT64(0, timestamp);
    TEST_ASSERT_EQUAL_UINT8(0u, diagnostics_mask);
    TEST_ASSERT_TRUE(string_table_count == 0 || string_table_count == -1);
    TEST_ASSERT_EQUAL_UINT32(0u, additional_header_type.identifier.numeric);
    TEST_ASSERT_EQUAL_UINT8(0u, additional_header_encoding);
}

static opcua_statuscode_t read_call_result_prefix(mu_binary_reader_t *reader, opcua_uint32_t expected_request_handle,
                                                  opcua_int32_t *input_arg_result_count,
                                                  opcua_int32_t *output_arg_count) {
    mu_nodeid_t response_type;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_int32_t result_count;
    opcua_statuscode_t method_status;
    opcua_int32_t input_diag_count;

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(reader, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CALLRESPONSE, response_type.identifier.numeric);
    skip_response_header(reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(expected_request_handle, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(reader, &result_count));
    TEST_ASSERT_EQUAL_INT32(1, result_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, &method_status));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(reader, input_arg_result_count));
    for (opcua_int32_t i = 0; i < *input_arg_result_count; ++i) {
        opcua_statuscode_t ignored;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, &ignored));
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(reader, &input_diag_count));
    TEST_ASSERT_EQUAL_INT32(0, input_diag_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(reader, output_arg_count));

    return method_status;
}

static void read_uint32_array_variant(mu_binary_reader_t *reader, opcua_uint32_t *values,
                                      opcua_int32_t expected_count) {
    opcua_byte_t mask;
    opcua_int32_t count;

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(reader, &mask));
    TEST_ASSERT_EQUAL_UINT8(0x80u | MU_TYPE_UINT32, mask);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(reader, &count));
    TEST_ASSERT_EQUAL_INT32(expected_count, count);
    for (opcua_int32_t i = 0; i < count; ++i) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(reader, &values[i]));
    }
}

static void dispatch_call(mu_server_t *server, const opcua_byte_t *request_body, size_t request_length,
                          opcua_byte_t *response_body, size_t *response_length) {
    *response_length = 1024u;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_CALLREQUEST, request_body, request_length,
                                                                response_body, response_length));
}

static mu_subscription_t *create_subscription_with_items(mu_server_t *server, opcua_uint32_t client_handle_1,
                                                         opcua_uint32_t client_handle_2) {
    mu_subscription_t *sub = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_subscription_create(&server->subs, server->sessions[0].session_id, 100u,
                                                                   30u, 10u, 0u, 0u, true, 0u, &sub));
    TEST_ASSERT_NOT_NULL(sub);

    const opcua_uint32_t client_handles[] = {client_handle_1, client_handle_2};
    size_t item_count = client_handle_2 == 0u ? 1u : 2u;
    for (size_t i = 0u; i < item_count; ++i) {
        mu_monitored_item_t *item = NULL;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_monitored_item_alloc(&server->subs, sub->subscription_id, &item));
        TEST_ASSERT_NOT_NULL(item);
        item->client_handle = client_handles[i];
        item->node_id = (mu_nodeid_t){1u, MU_NODEID_NUMERIC, {SAMPLE_NODE_ID}};
        item->resolved_node = &s_user_nodes[0];
        item->attribute_id = 13u;
        item->sampling_interval_ms = 100u;
        item->monitoring_mode = MU_MONITORING_MODE_REPORTING;
        item->trigger = MU_DATACHANGE_TRIGGER_STATUS_VALUE;
        item->last_value.type = MU_TYPE_INT32;
        item->last_value.value.i32 = s_value;
        item->last_status = MU_STATUS_GOOD;
        item->has_value = true;
        item->pending = true;
        item->next_sample_ms = 1000u;
        item->queue_size = 1u;
        item->discard_oldest = true;
    }

    return sub;
}

static void parse_publish_value(const opcua_byte_t *chunk, size_t chunk_len, opcua_uint32_t expected_request_handle,
                                opcua_uint32_t expected_subscription_id, opcua_uint32_t expected_client_handle,
                                opcua_int32_t expected_value) {
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_uint32_t subscription_id;
    opcua_int32_t available_count;
    opcua_boolean_t more_notifications;
    opcua_uint32_t sequence_number;
    opcua_int64_t publish_time;
    opcua_int32_t notification_data_count;
    mu_nodeid_t data_type;
    size_t data_length;
    opcua_int32_t item_count;
    opcua_uint32_t client_handle;
    mu_datavalue_t data_value;
    opcua_int32_t data_diagnostics_count;
    opcua_int32_t ack_results_count;
    opcua_int32_t response_diagnostics_count;

    TEST_ASSERT_TRUE(chunk_len > MU_UASC_SYMMETRIC_HEADER_SIZE);
    mu_binary_reader_init(&reader, chunk + MU_UASC_SYMMETRIC_HEADER_SIZE, chunk_len - MU_UASC_SYMMETRIC_HEADER_SIZE);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_PUBLISHRESPONSE, response_type.identifier.numeric);
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(expected_request_handle, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &subscription_id));
    TEST_ASSERT_EQUAL_UINT32(expected_subscription_id, subscription_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &available_count));
    TEST_ASSERT_EQUAL_INT32(0, available_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_boolean(&reader, &more_notifications));
    TEST_ASSERT_FALSE(more_notifications);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &sequence_number));
    TEST_ASSERT_TRUE(sequence_number > 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int64(&reader, &publish_time));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &notification_data_count));
    TEST_ASSERT_EQUAL_INT32(1, notification_data_count);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&reader, &data_type, &data_length));
    TEST_ASSERT_EQUAL_UINT32(ID_DATACHANGENOTIFICATION, data_type.identifier.numeric);
    TEST_ASSERT_TRUE(data_length > 0u);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &item_count));
    TEST_ASSERT_EQUAL_INT32(1, item_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &client_handle));
    TEST_ASSERT_EQUAL_UINT32(expected_client_handle, client_handle);
    memset(&data_value, 0, sizeof(data_value));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_datavalue(&reader, &data_value));
    TEST_ASSERT_TRUE(data_value.has_value);
    TEST_ASSERT_EQUAL_INT(MU_TYPE_INT32, data_value.value.type);
    TEST_ASSERT_EQUAL_INT32(expected_value, data_value.value.value.i32);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &data_diagnostics_count));
    TEST_ASSERT_EQUAL_INT32(0, data_diagnostics_count);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &ack_results_count));
    TEST_ASSERT_EQUAL_INT32(0, ack_results_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &response_diagnostics_count));
    TEST_ASSERT_EQUAL_INT32(0, response_diagnostics_count);
    TEST_ASSERT_EQUAL_size_t(reader.length, reader.position);
}

static const mu_node_t *base_node(opcua_uint32_t id) {
    mu_nodeid_t node_id = {0u, MU_NODEID_NUMERIC, {id}};
    return mu_address_space_find_node(mu_base_address_space(), &node_id);
}

static bool has_forward_ref(opcua_uint32_t source_id, opcua_uint32_t ref_type_id, opcua_uint32_t target_id) {
    const mu_node_t *source = base_node(source_id);
    TEST_ASSERT_NOT_NULL(source);

    for (size_t i = 0u; i < source->reference_count; ++i) {
        const mu_reference_t *ref = &source->references[i];
        if (ref->is_forward && ref->reference_type_id.namespace_index == 0u &&
            ref->reference_type_id.identifier.numeric == ref_type_id && ref->target_id.namespace_index == 0u &&
            ref->target_id.identifier.numeric == target_id) {
            return true;
        }
    }
    return false;
}

void test_server_call_method_nodes_are_browsable(void) {
    const mu_node_t *get_items = base_node(ID_SERVER_GETMONITOREDITEMS);
    const mu_node_t *resend = base_node(ID_SERVER_RESENDDATA);

    TEST_ASSERT_NOT_NULL(get_items);
    TEST_ASSERT_EQUAL_INT(MU_NODECLASS_METHOD, get_items->node_class);
    TEST_ASSERT_NOT_NULL(resend);
    TEST_ASSERT_EQUAL_INT(MU_NODECLASS_METHOD, resend->node_class);

    TEST_ASSERT_TRUE(has_forward_ref(ID_SERVER_OBJECT, 47u, ID_SERVER_GETMONITOREDITEMS));
    TEST_ASSERT_TRUE(has_forward_ref(ID_SERVER_OBJECT, 47u, ID_SERVER_RESENDDATA));
    TEST_ASSERT_TRUE(has_forward_ref(ID_SERVER_GETMONITOREDITEMS, 46u, ID_SERVER_GETMONITOREDITEMS_INPUTARGUMENTS));
    TEST_ASSERT_TRUE(has_forward_ref(ID_SERVER_GETMONITOREDITEMS, 46u, ID_SERVER_GETMONITOREDITEMS_OUTPUTARGUMENTS));
    TEST_ASSERT_TRUE(has_forward_ref(ID_SERVER_RESENDDATA, 46u, ID_SERVER_RESENDDATA_INPUTARGUMENTS));
}

void test_get_monitored_items_returns_server_and_client_handles(void) {
    mu_server_t server;
    test_io_t io;
    opcua_byte_t request_body[256];
    opcua_byte_t response_body[1024];
    size_t response_len = sizeof(response_body);
    opcua_int32_t input_result_count;
    opcua_int32_t output_arg_count;
    opcua_uint32_t server_handles[2] = {0u, 0u};
    opcua_uint32_t client_handles[2] = {0u, 0u};

    s_value = 42;
    prepare_server(&server, &io);
    mu_subscription_t *sub = create_subscription_with_items(&server, 7001u, 7002u);

    size_t request_len = write_call_request(request_body, sizeof(request_body), 10u, ID_SERVER_OBJECT,
                                            ID_SERVER_GETMONITOREDITEMS, sub->subscription_id);
    dispatch_call(&server, request_body, request_len, response_body, &response_len);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, response_body, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            read_call_result_prefix(&reader, 10u, &input_result_count, &output_arg_count));
    TEST_ASSERT_EQUAL_INT32(0, input_result_count);
    TEST_ASSERT_EQUAL_INT32(2, output_arg_count);
    read_uint32_array_variant(&reader, server_handles, 2);
    read_uint32_array_variant(&reader, client_handles, 2);

    TEST_ASSERT_EQUAL_UINT32(server.subs.monitored_items[0].monitored_item_id, server_handles[0]);
    TEST_ASSERT_EQUAL_UINT32(server.subs.monitored_items[1].monitored_item_id, server_handles[1]);
    TEST_ASSERT_EQUAL_UINT32(7001u, client_handles[0]);
    TEST_ASSERT_EQUAL_UINT32(7002u, client_handles[1]);

    opcua_int32_t diagnostic_count;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);
    TEST_ASSERT_EQUAL_size_t(response_len, reader.position);
}

void test_resend_data_reissues_current_values_on_next_publish(void) {
    mu_server_t server;
    test_io_t io;
    opcua_byte_t request_body[256];
    opcua_byte_t response_body[1024];
    size_t response_len = sizeof(response_body);
    opcua_int32_t input_result_count;
    opcua_int32_t output_arg_count;

    s_value = 10;
    prepare_server(&server, &io);
    mu_subscription_t *sub = create_subscription_with_items(&server, 9001u, 0u);

    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_GOOD, mu_publish_request_enqueue(&server.subs, server.sessions[0].session_id, 77u, 70u, 0u, NULL));
    mu_subscriptions_tick(&server, 100u);
    TEST_ASSERT_TRUE(io.last_write_len > 0u);
    parse_publish_value(io.last_write, io.last_write_len, 70u, sub->subscription_id, 9001u, 10);

    io.last_write_len = 0u;
    size_t request_len = write_call_request(request_body, sizeof(request_body), 11u, ID_SERVER_OBJECT,
                                            ID_SERVER_RESENDDATA, sub->subscription_id);
    dispatch_call(&server, request_body, request_len, response_body, &response_len);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, response_body, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            read_call_result_prefix(&reader, 11u, &input_result_count, &output_arg_count));
    TEST_ASSERT_EQUAL_INT32(0, input_result_count);
    TEST_ASSERT_EQUAL_INT32(0, output_arg_count);
    opcua_int32_t diagnostic_count;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &diagnostic_count));
    TEST_ASSERT_EQUAL_INT32(0, diagnostic_count);

    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_GOOD, mu_publish_request_enqueue(&server.subs, server.sessions[0].session_id, 78u, 71u, 100u, NULL));
    mu_subscriptions_tick(&server, 200u);
    TEST_ASSERT_TRUE(io.last_write_len > 0u);
    parse_publish_value(io.last_write, io.last_write_len, 71u, sub->subscription_id, 9001u, 10);
}

#ifdef MUC_OPCUA_CUSTOM_METHODS
static size_t write_custom_call_request(opcua_byte_t *buffer, size_t capacity, opcua_uint32_t request_handle,
                                        const mu_nodeid_t *object_id, const mu_nodeid_t *method_id,
                                        const mu_variant_t *args, opcua_int32_t arg_count) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, AUTH_TOKEN, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 1));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, object_id));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, method_id));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int32(&writer, arg_count));
    for (opcua_int32_t i = 0; i < arg_count; ++i) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_variant(&writer, &args[i]));
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, writer.status);
    return writer.position;
}

static opcua_statuscode_t custom_test_method_cb(struct mu_server *server, const mu_nodeid_t *object_id,
                                                const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                                size_t input_args_count, mu_variant_t *output_args,
                                                size_t *output_args_count) {
    (void)server;
    (void)object_id;
    (void)method_id;

    TEST_ASSERT_EQUAL(1, input_args_count);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, input_args[0].type);
    TEST_ASSERT_EQUAL(42, input_args[0].value.i32);

    output_args[0].type = MU_TYPE_INT32;
    output_args[0].is_array = false;
    output_args[0].value.i32 = 999;
    *output_args_count = 1;

    return MU_STATUS_GOOD;
}

void test_custom_method_callback_execution(void) {
    mu_server_t server;
    test_io_t io;
    opcua_byte_t request_body[256];
    opcua_byte_t response_body[1024];
    size_t response_len = sizeof(response_body);
    opcua_int32_t input_result_count;
    opcua_int32_t output_arg_count;

    prepare_server(&server, &io);

    static const mu_node_t custom_nodes[] = {{{1u, MU_NODEID_NUMERIC, {8888u}},
                                              MU_NODECLASS_OBJECT,
                                              {10, (const opcua_byte_t *)"TestObject"},
                                              {10, (const opcua_byte_t *)"TestObject"},
                                              NULL,
                                              0,
                                              NULL},
                                             {{1u, MU_NODEID_NUMERIC, {9999u}},
                                              MU_NODECLASS_METHOD,
                                              {10, (const opcua_byte_t *)"TestMethod"},
                                              {10, (const opcua_byte_t *)"TestMethod"},
                                              NULL,
                                              0,
                                              NULL}};
    mu_address_space_t custom_as = {custom_nodes, sizeof(custom_nodes) / sizeof(custom_nodes[0])};
    server.config.address_space = &custom_as;

    mu_nodeid_t method_id = {1u, MU_NODEID_NUMERIC, {9999u}};
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_server_register_method_callback(&server, &method_id, custom_test_method_cb, NULL));

    mu_nodeid_t object_id = {1u, MU_NODEID_NUMERIC, {8888u}};
    mu_variant_t input_arg;
    memset(&input_arg, 0, sizeof(input_arg));
    input_arg.type = MU_TYPE_INT32;
    input_arg.value.i32 = 42;

    size_t request_len =
        write_custom_call_request(request_body, sizeof(request_body), 12u, &object_id, &method_id, &input_arg, 1);
    dispatch_call(&server, request_body, request_len, response_body, &response_len);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, response_body, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            read_call_result_prefix(&reader, 12u, &input_result_count, &output_arg_count));
    TEST_ASSERT_EQUAL_INT32(0, input_result_count);
    TEST_ASSERT_EQUAL_INT32(1, output_arg_count);

    opcua_byte_t mask;
    opcua_int32_t val;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &mask));
    TEST_ASSERT_EQUAL_UINT8(MU_TYPE_INT32, mask);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &val));
    TEST_ASSERT_EQUAL_INT32(999, val);
}
#endif

#else

void test_method_call_us3_is_gated_to_embedded_standard_builds(void) {
    TEST_PASS();
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SUBSCRIPTIONS && MUC_OPCUA_SUBSCRIPTIONS_STANDARD && MUC_OPCUA_BASE_TYPE_SYSTEM
    RUN_TEST(test_server_call_method_nodes_are_browsable);
    RUN_TEST(test_get_monitored_items_returns_server_and_client_handles);
    RUN_TEST(test_resend_data_reissues_current_values_on_next_publish);
#ifdef MUC_OPCUA_CUSTOM_METHODS
    RUN_TEST(test_custom_method_callback_execution);
#endif
#else
    RUN_TEST(test_method_call_us3_is_gated_to_embedded_standard_builds);
#endif
    return UNITY_END();
}
