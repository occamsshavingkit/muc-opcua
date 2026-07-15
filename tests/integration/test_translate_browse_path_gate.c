/* tests/integration/test_translate_browse_path_gate.c
 * T052 / opc_cu_2317: TranslateBrowsePathsToNodeIds has its own CU gate.
 * T053 / opc_cu_3530: Browse and BrowseNext have their own CU gate.
 * Grounding: OPC-10000-4 sections 5.9.2, 5.9.3, and 5.9.4.
 */
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/session.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

#ifndef MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH
#define MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH 0
#endif

#ifndef MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH
#define MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH 0
#endif

#ifndef MUC_OPCUA_CU_VIEW_BASIC_2
#define MUC_OPCUA_CU_VIEW_BASIC_2 0
#endif

void setUp(void) {}
void tearDown(void) {}

static opcua_statuscode_t gate_listen(void *context, const char *endpoint_url) {
    (void)context;
    (void)endpoint_url;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t gate_accept(void *context, void **client_handle) {
    (void)context;
    *client_handle = NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t gate_read(void *context, void *client_handle, opcua_byte_t *buffer, size_t capacity,
                                    size_t *bytes_read) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    (void)capacity;
    *bytes_read = 0;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t gate_write(void *context, void *client_handle, const opcua_byte_t *buffer, size_t length,
                                     size_t *bytes_written) {
    (void)context;
    (void)client_handle;
    (void)buffer;
    *bytes_written = length;
    return MU_STATUS_GOOD;
}

static void gate_close(void *context, void *client_handle) {
    (void)context;
    (void)client_handle;
}

static void gate_shutdown(void *context) {
    (void)context;
}

static opcua_datetime_t gate_time(void *context) {
    (void)context;
    return 0;
}

static opcua_uint64_t gate_tick_ms(void *context) {
    (void)context;
    return 0;
}

static opcua_statuscode_t gate_random(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    (void)memset(buffer, 0, length);
    return MU_STATUS_GOOD;
}

/* Objects(85) -Organizes(35)-> MyVar1(ns=1;i=1000). */
static const mu_reference_t gate_obj_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true}};
static const mu_reference_t gate_var_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
static const mu_value_source_t gate_var_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
static const mu_node_t gate_nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                        MU_NODECLASS_OBJECT,
                                        {7, (const opcua_byte_t *)"Objects"},
                                        {7, (const opcua_byte_t *)"Objects"},
                                        gate_obj_refs,
                                        1,
                                        NULL},
                                       {{1, MU_NODEID_NUMERIC, {1000}},
                                        MU_NODECLASS_VARIABLE,
                                        {6, (const opcua_byte_t *)"MyVar1"},
                                        {6, (const opcua_byte_t *)"MyVar1"},
                                        gate_var_refs,
                                        1,
                                        &gate_var_value}};
static const mu_address_space_t gate_space = {gate_nodes, 2};

static mu_server_t *init_activated_server(void *storage, size_t storage_size) {
    static opcua_byte_t receive_buffer[MU_MIN_CHUNK_SIZE];
    static opcua_byte_t send_buffer[MU_MIN_CHUNK_SIZE];
    mu_server_config_t config;
    mu_server_t *server = NULL;

    (void)memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://gate:4840";
    config.application_uri = "urn:gate";
    config.product_uri = "urn:gate";
    config.application_name = "gate";
    config.receive_buffer = receive_buffer;
    config.receive_buffer_size = sizeof(receive_buffer);
    config.send_buffer = send_buffer;
    config.send_buffer_size = sizeof(send_buffer);
    config.max_chunk_count = 1;
    config.max_message_size = MU_MIN_CHUNK_SIZE;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.tcp_adapter.listen = gate_listen;
    config.tcp_adapter.accept = gate_accept;
    config.tcp_adapter.read = gate_read;
    config.tcp_adapter.write = gate_write;
    config.tcp_adapter.close_connection = gate_close;
    config.tcp_adapter.shutdown = gate_shutdown;
    config.time_adapter.get_time = gate_time;
    config.time_adapter.get_tick_ms = gate_tick_ms;
    config.entropy_adapter.generate_random = gate_random;
    config.address_space = &gate_space;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, storage_size, &config, &server));
    server->secure_channel.is_open = true;

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id;
    opcua_uint32_t auth_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_session_create(&server->sessions[0], 0, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL(12345u, auth_token);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token,
                                                          MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY));
    return server;
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle) {
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {12345}};
    mu_nodeid_t null_nodeid = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(writer, &auth_token);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, request_handle);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_string);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &null_nodeid, 0);
}

static size_t build_browse_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, 1);
    {
        mu_nodeid_t null_view = {0, MU_NODEID_NUMERIC, {0}};
        mu_binary_write_nodeid(&writer, &null_view);
        mu_binary_write_int64(&writer, 0);
        mu_binary_write_uint32(&writer, 0);
    }
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_int32(&writer, 1);
    {
        mu_nodeid_t objects = {0, MU_NODEID_NUMERIC, {85}};
        mu_nodeid_t hierarchical = {0, MU_NODEID_NUMERIC, {33}};
        mu_binary_write_nodeid(&writer, &objects);
        mu_binary_write_uint32(&writer, 0);
        mu_binary_write_nodeid(&writer, &hierarchical);
        mu_binary_write_boolean(&writer, true);
        mu_binary_write_uint32(&writer, 0);
        mu_binary_write_uint32(&writer, 0x3Fu);
    }
    return writer.position;
}

static size_t build_browse_next_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, 3);
    mu_binary_write_boolean(&writer, false);
    mu_binary_write_int32(&writer, 0);
    return writer.position;
}

static void write_browse_path(mu_binary_writer_t *writer, const char *target_name) {
    mu_nodeid_t starting_node = {0, MU_NODEID_NUMERIC, {85}};
    mu_nodeid_t hierarchical = {0, MU_NODEID_NUMERIC, {33}};
    mu_string_t target = {(opcua_int32_t)strlen(target_name), (const opcua_byte_t *)target_name};

    mu_binary_write_nodeid(writer, &starting_node);
    mu_binary_write_int32(writer, 1);
    mu_binary_write_nodeid(writer, &hierarchical);
    mu_binary_write_boolean(writer, false);
    mu_binary_write_boolean(writer, true);
    mu_binary_write_uint16(writer, 0);
    mu_binary_write_string(writer, &target);
}

static size_t build_translate_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, 2);
    mu_binary_write_int32(&writer, 1);
    write_browse_path(&writer, "MyVar1");
    return writer.position;
}

static opcua_uint32_t read_response_type(const opcua_byte_t *buffer, size_t length) {
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    TEST_ASSERT_GREATER_THAN(0u, length);
    mu_binary_reader_init(&reader, buffer, length);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, response_type.identifier_type);
    return response_type.identifier.numeric;
}

void test_translatebrowsepaths_dedicated_gate_is_independent_of_browse_aggregate(void) {
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t request[256];
    opcua_byte_t response[512];
    size_t request_len;
    size_t response_len;
    mu_server_t *server = init_activated_server(storage, sizeof(storage));

#if MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH && !MUC_OPCUA_CU_VIEW_BASIC_2 && MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH
    request_len = build_browse_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED,
                      mu_service_dispatch(server, MU_ID_BROWSEREQUEST, request, request_len, response, &response_len));

    request_len = build_browse_next_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED, mu_service_dispatch(server, MU_ID_BROWSENEXTREQUEST, request,
                                                                            request_len, response, &response_len));

    request_len = build_translate_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, request,
                                                          request_len, response, &response_len));
    TEST_ASSERT_EQUAL(MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, read_response_type(response, response_len));
#elif !MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH && MUC_OPCUA_CU_VIEW_BASIC_2 &&                                     \
    !MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH
    request_len = build_browse_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(server, MU_ID_BROWSEREQUEST, request, request_len, response, &response_len));
    TEST_ASSERT_EQUAL(MU_ID_BROWSERESPONSE, read_response_type(response, response_len));

    request_len = build_browse_next_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_BROWSENEXTREQUEST, request, request_len,
                                                          response, &response_len));
    TEST_ASSERT_EQUAL(MU_ID_BROWSENEXTRESPONSE, read_response_type(response, response_len));

    request_len = build_translate_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED,
                      mu_service_dispatch(server, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, request, request_len,
                                          response, &response_len));
#elif !MUC_OPCUA_CU_VIEW_BASIC_2 && MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH
    request_len = build_translate_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, request,
                                                          request_len, response, &response_len));
    TEST_ASSERT_EQUAL(MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, read_response_type(response, response_len));

    request_len = build_browse_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED,
                      mu_service_dispatch(server, MU_ID_BROWSEREQUEST, request, request_len, response, &response_len));
#elif MUC_OPCUA_CU_VIEW_BASIC_2 && MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH
    request_len = build_browse_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(server, MU_ID_BROWSEREQUEST, request, request_len, response, &response_len));
    TEST_ASSERT_EQUAL(MU_ID_BROWSERESPONSE, read_response_type(response, response_len));

    request_len = build_browse_next_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_BROWSENEXTREQUEST, request, request_len,
                                                          response, &response_len));
    TEST_ASSERT_EQUAL(MU_ID_BROWSENEXTRESPONSE, read_response_type(response, response_len));

    request_len = build_translate_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, request,
                                                          request_len, response, &response_len));
    TEST_ASSERT_EQUAL(MU_ID_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE, read_response_type(response, response_len));
#else
    request_len = build_browse_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED,
                      mu_service_dispatch(server, MU_ID_BROWSEREQUEST, request, request_len, response, &response_len));

    request_len = build_translate_request(request, sizeof(request));
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED,
                      mu_service_dispatch(server, MU_ID_TRANSLATEBROWSEPATHSTONODEIDSREQUEST, request, request_len,
                                          response, &response_len));
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_translatebrowsepaths_dedicated_gate_is_independent_of_browse_aggregate);
    return UNITY_END();
}
