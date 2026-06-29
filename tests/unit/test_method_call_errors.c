/* tests/unit/test_method_call_errors.c
 *
 * Negative coverage for Feature 005 US3 Server Call support.
 *
 * OPC-10000-4 5.12.2.2: Call Service.
 * OPC-10000-5 9.1/9.2: Base Info methods on Server.
 */
#include "unity.h"

#include "../../src/core/server_internal.h"
#include "../../src/services/secure_channel.h"
#include "../../src/services/session.h"
#include "micro_opcua/encoding.h"
#include "micro_opcua/micro_opcua.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MICRO_OPCUA_SUBSCRIPTIONS && MICRO_OPCUA_SUBSCRIPTIONS_STANDARD && MICRO_OPCUA_BASE_TYPE_SYSTEM

#define AUTH_TOKEN 12345u
#define ID_SERVER_OBJECT 2253u
#define ID_OBJECTS_FOLDER 85u
#define ID_SERVER_GETMONITOREDITEMS 11492u
#define ID_SERVER_RESENDDATA 12873u

#ifndef MU_STATUS_BAD_NODEIDINVALID
#define MU_STATUS_BAD_NODEIDINVALID ((opcua_statuscode_t)0x80330000u)
#endif
#ifndef MU_STATUS_BAD_METHODINVALID
#define MU_STATUS_BAD_METHODINVALID ((opcua_statuscode_t)0x80750000u)
#endif
#ifndef MU_STATUS_BAD_ARGUMENTSMISSING
#define MU_STATUS_BAD_ARGUMENTSMISSING ((opcua_statuscode_t)0x80760000u)
#endif
#ifndef MU_STATUS_BAD_INVALIDARGUMENT
#define MU_STATUS_BAD_INVALIDARGUMENT ((opcua_statuscode_t)0x80AB0000u)
#endif

static const mu_reference_t s_empty_refs[1] = {{0}};
static const mu_node_t s_user_nodes[] = {
    {{0, MU_NODEID_NUMERIC, {ID_SERVER_OBJECT}},
     MU_NODECLASS_OBJECT,
     {6, (const opcua_byte_t *)"Server"},
     {6, (const opcua_byte_t *)"Server"},
     s_empty_refs,
     0,
     NULL}
};
static const mu_address_space_t s_user_space = {s_user_nodes, sizeof(s_user_nodes) / sizeof(s_user_nodes[0])};

static void prepare_server(mu_server_t *server) {
    opcua_uint32_t revised_lifetime = 0u;
    opcua_uint64_t revised_timeout = 0u;
    opcua_uint32_t session_id = 0u;
    opcua_uint32_t auth_token = 0u;

    memset(server, 0, sizeof(*server));
    server->config.address_space = &s_user_space;
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
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token,
                                                                MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY));
    server->sessions[0].secure_channel_id = server->secure_channel.channel_id;
    server->conns[0].client_handle = (void *)1;
    server->conns[0].secure_channel = server->secure_channel;
    mu_subscriptions_init(&server->subs);
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle) {
    mu_nodeid_t token = {0u, MU_NODEID_NUMERIC, {AUTH_TOKEN}};
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

static void write_string_arg(mu_binary_writer_t *writer) {
    static const opcua_byte_t bytes[] = {'n', 'o', 't', '-', 'u', 'i', 'n', 't'};
    mu_variant_t variant;
    memset(&variant, 0, sizeof(variant));
    variant.type = MU_TYPE_STRING;
    variant.value.str.length = (opcua_int32_t)sizeof(bytes);
    variant.value.str.data = bytes;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_variant(writer, &variant));
}

static size_t write_single_method_request(opcua_byte_t *buffer, size_t capacity, opcua_uint32_t request_handle,
                                          opcua_uint32_t object_id, opcua_uint32_t method_id,
                                          opcua_int32_t input_arg_count, bool string_arg) {
    mu_binary_writer_t writer;
    mu_nodeid_t object = {0u, MU_NODEID_NUMERIC, {object_id}};
    mu_nodeid_t method = {0u, MU_NODEID_NUMERIC, {method_id}};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 1));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &object));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &method));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int32(&writer, input_arg_count));
    for (opcua_int32_t i = 0; i < input_arg_count; ++i) {
        if (string_arg) {
            write_string_arg(&writer);
        } else {
            write_uint32_arg(&writer, 1u);
        }
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, writer.status);
    return writer.position;
}

static size_t write_many_methods_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, 99u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_int32(&writer, 33));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, writer.status);
    return writer.position;
}

static void skip_response_header(mu_binary_reader_t *reader) {
    opcua_int64_t timestamp;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_byte_t diagnostics_mask;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int64(reader, &timestamp));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(reader, &request_handle));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, &service_result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(reader, &diagnostics_mask));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(reader, &string_table_count));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(reader, &additional_header_type));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(reader, &additional_header_encoding));
}

static opcua_statuscode_t dispatch_and_read_method_status(mu_server_t *server, const opcua_byte_t *request_body,
                                                          size_t request_length, opcua_int32_t *input_results_count,
                                                          opcua_statuscode_t *first_input_result,
                                                          opcua_int32_t *output_arg_count) {
    opcua_byte_t response_body[1024];
    size_t response_len = sizeof(response_body);
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_int32_t result_count;
    opcua_statuscode_t method_status;
    opcua_int32_t input_diag_count;
    opcua_int32_t response_diag_count;

    *input_results_count = 0;
    *first_input_result = MU_STATUS_GOOD;
    *output_arg_count = 0;

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_CALLREQUEST, request_body, request_length,
                                                                response_body, &response_len));
    mu_binary_reader_init(&reader, response_body, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CALLRESPONSE, response_type.identifier.numeric);
    skip_response_header(&reader);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &result_count));
    TEST_ASSERT_EQUAL_INT32(1, result_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, &method_status));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, input_results_count));
    if (*input_results_count > 0) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, first_input_result));
        for (opcua_int32_t i = 1; i < *input_results_count; ++i) {
            opcua_statuscode_t ignored;
            TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_statuscode(&reader, &ignored));
        }
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &input_diag_count));
    TEST_ASSERT_EQUAL_INT32(0, input_diag_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, output_arg_count));
    for (opcua_int32_t i = 0; i < *output_arg_count; ++i) {
        mu_variant_t ignored;
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &ignored));
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &response_diag_count));
    TEST_ASSERT_EQUAL_INT32(0, response_diag_count);
    TEST_ASSERT_EQUAL_size_t(response_len, reader.position);
    return method_status;
}

void test_unknown_method_returns_bad_method_invalid(void) {
    mu_server_t server;
    opcua_byte_t request_body[256];
    opcua_int32_t input_count;
    opcua_statuscode_t input_status;
    opcua_int32_t output_count;

    prepare_server(&server);
    size_t request_len =
        write_single_method_request(request_body, sizeof(request_body), 20u, ID_SERVER_OBJECT, 999999u, 1, false);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_METHODINVALID,
                            dispatch_and_read_method_status(&server, request_body, request_len, &input_count,
                                                            &input_status, &output_count));
    TEST_ASSERT_EQUAL_INT32(0, input_count);
    TEST_ASSERT_EQUAL_INT32(0, output_count);
}

void test_wrong_object_returns_bad_nodeid_invalid(void) {
    mu_server_t server;
    opcua_byte_t request_body[256];
    opcua_int32_t input_count;
    opcua_statuscode_t input_status;
    opcua_int32_t output_count;

    prepare_server(&server);
    size_t request_len = write_single_method_request(request_body, sizeof(request_body), 21u, ID_OBJECTS_FOLDER,
                                                     ID_SERVER_GETMONITOREDITEMS, 1, false);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NODEIDINVALID,
                            dispatch_and_read_method_status(&server, request_body, request_len, &input_count,
                                                            &input_status, &output_count));
    TEST_ASSERT_EQUAL_INT32(0, input_count);
    TEST_ASSERT_EQUAL_INT32(0, output_count);
}

void test_missing_argument_returns_bad_arguments_missing(void) {
    mu_server_t server;
    opcua_byte_t request_body[256];
    opcua_int32_t input_count;
    opcua_statuscode_t input_status;
    opcua_int32_t output_count;

    prepare_server(&server);
    size_t request_len = write_single_method_request(request_body, sizeof(request_body), 22u, ID_SERVER_OBJECT,
                                                     ID_SERVER_RESENDDATA, 0, false);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_ARGUMENTSMISSING,
                            dispatch_and_read_method_status(&server, request_body, request_len, &input_count,
                                                            &input_status, &output_count));
    TEST_ASSERT_EQUAL_INT32(0, input_count);
    TEST_ASSERT_EQUAL_INT32(0, output_count);
}

void test_wrong_argument_type_returns_bad_invalid_argument(void) {
    mu_server_t server;
    opcua_byte_t request_body[256];
    opcua_int32_t input_count;
    opcua_statuscode_t input_status;
    opcua_int32_t output_count;

    prepare_server(&server);
    size_t request_len = write_single_method_request(request_body, sizeof(request_body), 23u, ID_SERVER_OBJECT,
                                                     ID_SERVER_GETMONITOREDITEMS, 1, true);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_INVALIDARGUMENT,
                            dispatch_and_read_method_status(&server, request_body, request_len, &input_count,
                                                            &input_status, &output_count));
    TEST_ASSERT_EQUAL_INT32(1, input_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_INVALIDARGUMENT, input_status);
    TEST_ASSERT_EQUAL_INT32(0, output_count);
}

void test_too_many_methods_returns_bad_too_many_operations(void) {
    mu_server_t server;
    opcua_byte_t request_body[256];
    opcua_byte_t response_body[1024];
    size_t response_len = sizeof(response_body);

    prepare_server(&server);
    size_t request_len = write_many_methods_request(request_body, sizeof(request_body));
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_TOOMANYOPERATIONS,
        mu_service_dispatch(&server, MU_ID_CALLREQUEST, request_body, request_len, response_body, &response_len));
}

#else

void test_method_call_error_coverage_is_gated_to_embedded_standard_builds(void) {
    TEST_PASS();
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MICRO_OPCUA_SUBSCRIPTIONS && MICRO_OPCUA_SUBSCRIPTIONS_STANDARD && MICRO_OPCUA_BASE_TYPE_SYSTEM
    RUN_TEST(test_unknown_method_returns_bad_method_invalid);
    RUN_TEST(test_wrong_object_returns_bad_nodeid_invalid);
    RUN_TEST(test_missing_argument_returns_bad_arguments_missing);
    RUN_TEST(test_wrong_argument_type_returns_bad_invalid_argument);
    RUN_TEST(test_too_many_methods_returns_bad_too_many_operations);
#else
    RUN_TEST(test_method_call_error_coverage_is_gated_to_embedded_standard_builds);
#endif
    return UNITY_END();
}
