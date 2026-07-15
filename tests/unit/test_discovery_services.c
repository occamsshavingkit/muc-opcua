/* tests/unit/test_discovery_services.c */
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/discovery.h"
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

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t request_handle) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};

    mu_binary_write_nodeid(w, &null_id);                     /* authenticationToken */
    mu_binary_write_int64(w, 0);                             /* timestamp */
    mu_binary_write_uint32(w, request_handle);               /* requestHandle */
    mu_binary_write_uint32(w, 0);                            /* returnDiagnostics */
    mu_binary_write_string(w, &null_str);                    /* auditEntryId */
    mu_binary_write_uint32(w, 0);                            /* timeoutHint */
    mu_binary_write_extension_object_header(w, &null_id, 0); /* additionalHeader */
}

static size_t build_findservers_malformed_array_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    write_request_header(&writer, 24);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, -2);         /* localeIds[] malformed count */

    return writer.position;
}

static size_t build_findservers_truncated_body_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    write_request_header(&writer, 25);
    mu_binary_write_int32(&writer, 1); /* endpointUrl length without string bytes */

    return writer.position;
}

static size_t build_findservers_unfiltered_request(opcua_byte_t *buffer, size_t capacity,
                                                   opcua_uint32_t request_handle) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    write_request_header(&writer, request_handle);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);          /* localeIds[] */
    mu_binary_write_int32(&writer, 0);          /* serverUris[] */
    mu_binary_write_int32(&writer, 0);          /* serverTypes[] */

    return writer.position;
}

static size_t build_findservers_endpoint_filter_request(opcua_byte_t *buffer, size_t capacity,
                                                        const char *endpoint_url) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t filter = {(opcua_int32_t)strlen(endpoint_url), (const opcua_byte_t *)endpoint_url};
    write_request_header(&writer, 28);
    mu_binary_write_string(&writer, &filter); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);        /* localeIds[] */
    mu_binary_write_int32(&writer, 0);        /* serverUris[] */

    return writer.position;
}

static size_t build_findservers_locale_filter_request(opcua_byte_t *buffer, size_t capacity, const char *locale_id) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    mu_string_t locale = {(opcua_int32_t)strlen(locale_id), (const opcua_byte_t *)locale_id};
    write_request_header(&writer, 29);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 1);          /* localeIds[] */
    mu_binary_write_string(&writer, &locale);
    mu_binary_write_int32(&writer, 0); /* serverUris[] */

    return writer.position;
}

static size_t build_findservers_server_uri_filter_request(opcua_byte_t *buffer, size_t capacity,
                                                          const char *server_uri) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    mu_string_t uri = {(opcua_int32_t)strlen(server_uri), (const opcua_byte_t *)server_uri};
    write_request_header(&writer, 30);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);          /* localeIds[] */
    mu_binary_write_int32(&writer, 1);          /* serverUris[] */
    mu_binary_write_string(&writer, &uri);

    return writer.position;
}

static size_t build_findservers_server_type_filter_request(opcua_byte_t *buffer, size_t capacity,
                                                           mu_application_type_t server_type) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    write_request_header(&writer, 31);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);          /* localeIds[] */
    mu_binary_write_int32(&writer, 0);          /* serverUris[] */
    mu_binary_write_int32(&writer, 1);          /* serverTypes[] */
    mu_binary_write_uint32(&writer, (opcua_uint32_t)server_type);

    return writer.position;
}

static size_t build_getendpoints_malformed_array_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    write_request_header(&writer, 26);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);          /* localeIds[] */
    mu_binary_write_int32(&writer, -2);         /* profileUris[] malformed count */

    return writer.position;
}

static size_t build_getendpoints_truncated_body_request(opcua_byte_t *buffer, size_t capacity) {
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, buffer, capacity);

    mu_string_t null_str = {-1, NULL};
    write_request_header(&writer, 27);
    mu_binary_write_string(&writer, &null_str); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);          /* localeIds[] */
    mu_binary_write_int32(&writer, 1);          /* profileUris[] count without array item */

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

    (void)timestamp;
    TEST_ASSERT_EQUAL_UINT8(0u, diagnostics_mask);
    TEST_ASSERT_TRUE(string_table_count == 0 || string_table_count == -1);
    TEST_ASSERT_EQUAL_UINT32(0u, additional_header_type.identifier.numeric);
    TEST_ASSERT_EQUAL_UINT8(0u, additional_header_encoding);
}

static opcua_int32_t read_findservers_server_count(const opcua_byte_t *response, size_t response_len,
                                                   opcua_uint32_t expected_request_handle) {
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_int32_t server_count;

    mu_binary_reader_init(&reader, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, response_type.identifier_type);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_FINDSERVERSRESPONSE, response_type.identifier.numeric);
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(expected_request_handle, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &server_count));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, reader.status);

    return server_count;
}

static void assert_string_equal_cstr(const mu_string_t *actual, const char *expected) {
    size_t expected_len = strlen(expected);

    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_INT32((opcua_int32_t)expected_len, actual->length);
    if (expected_len > 0u) {
        TEST_ASSERT_NOT_NULL(actual->data);
        TEST_ASSERT_EQUAL_MEMORY(expected, actual->data, expected_len);
    }
}

typedef struct {
    mu_string_t application_uri;
    mu_string_t product_uri;
    opcua_byte_t application_name_mask;
    mu_string_t application_name_locale;
    mu_string_t application_name_text;
    opcua_uint32_t application_type;
    mu_string_t gateway_server_uri;
    mu_string_t discovery_profile_uri;
    opcua_int32_t discovery_url_count;
    mu_string_t discovery_url;
} findservers_application_t;

static void read_findservers_single_application(const opcua_byte_t *response, size_t response_len,
                                                opcua_uint32_t expected_request_handle,
                                                findservers_application_t *app) {
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_int32_t server_count;

    *app = (findservers_application_t){0};
    app->application_name_locale = (mu_string_t){-1, NULL};

    mu_binary_reader_init(&reader, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, response_type.identifier_type);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_FINDSERVERSRESPONSE, response_type.identifier.numeric);
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(expected_request_handle, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &server_count));
    TEST_ASSERT_EQUAL_INT32(1, server_count);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &app->application_uri));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &app->product_uri));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &app->application_name_mask));
    if ((app->application_name_mask & 0x01u) != 0u) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &app->application_name_locale));
    }
    if ((app->application_name_mask & 0x02u) != 0u) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &app->application_name_text));
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &app->application_type));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &app->gateway_server_uri));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &app->discovery_profile_uri));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &app->discovery_url_count));
    TEST_ASSERT_EQUAL_INT32(1, app->discovery_url_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &app->discovery_url));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, reader.status);
    TEST_ASSERT_EQUAL_UINT(response_len, reader.position);
}

static void assert_findservers_self_application(const opcua_byte_t *response, size_t response_len,
                                                opcua_uint32_t expected_request_handle,
                                                const mu_server_config_t *config) {
    findservers_application_t app;
    read_findservers_single_application(response, response_len, expected_request_handle, &app);

    assert_string_equal_cstr(&app.application_uri, config->application_uri);
    assert_string_equal_cstr(&app.product_uri, config->product_uri);
    TEST_ASSERT_EQUAL_UINT8(0x02u, app.application_name_mask);
    assert_string_equal_cstr(&app.application_name_text, config->application_name);
    TEST_ASSERT_EQUAL_UINT32((opcua_uint32_t)MU_APPLICATION_TYPE_SERVER, app.application_type);
    assert_string_equal_cstr(&app.discovery_url, config->endpoint_url);
}

static void read_findservers_first_application_name(const opcua_byte_t *response, size_t response_len,
                                                    opcua_uint32_t expected_request_handle,
                                                    opcua_byte_t *localized_text_mask, mu_string_t *locale,
                                                    mu_string_t *text) {
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    opcua_int32_t server_count;
    mu_string_t ignored;

    *localized_text_mask = 0u;
    *locale = (mu_string_t){-1, NULL};
    *text = (mu_string_t){-1, NULL};

    mu_binary_reader_init(&reader, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, response_type.identifier_type);
    TEST_ASSERT_EQUAL_UINT32(MU_ID_FINDSERVERSRESPONSE, response_type.identifier.numeric);
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(expected_request_handle, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &server_count));
    TEST_ASSERT_EQUAL_INT32(1, server_count);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &ignored)); /* applicationUri */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, &ignored)); /* productUri */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(&reader, localized_text_mask));
    if ((*localized_text_mask & 0x01u) != 0u) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, locale));
    }
    if ((*localized_text_mask & 0x02u) != 0u) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_string(&reader, text));
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, reader.status);
}

void test_discovery_findservers_response(void) {
    mu_server_config_t config = {0};
    config.application_uri = "urn:test:app";
    config.product_uri = "urn:test:product";
    config.application_name = "Test App";
    config.endpoint_url = "opc.tcp://localhost:4840";

    mu_application_description_t desc;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_discovery_get_application_description(&config, &desc));

    TEST_ASSERT_EQUAL_STRING("urn:test:app", desc.application_uri);
    TEST_ASSERT_EQUAL_STRING("urn:test:product", desc.product_uri);
    TEST_ASSERT_EQUAL_STRING("Test App", desc.application_name);
    TEST_ASSERT_EQUAL(MU_APPLICATION_TYPE_SERVER, desc.application_type);
    TEST_ASSERT_EQUAL_STRING("opc.tcp://localhost:4840", desc.discovery_url);
}

void test_discovery_getendpoints_response(void) {
    mu_server_config_t config = {0};
    config.application_uri = "urn:test:app";
    config.product_uri = "urn:test:product";
    config.application_name = "Test App";
    config.endpoint_url = "opc.tcp://localhost:4840";

    mu_endpoint_description_t desc;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_discovery_get_endpoint_description(&config, &desc));

    TEST_ASSERT_EQUAL_STRING("opc.tcp://localhost:4840", desc.endpoint_url);
    TEST_ASSERT_EQUAL_STRING("urn:test:app", desc.server.application_uri);

    TEST_ASSERT_EQUAL(MU_MESSAGE_SECURITY_MODE_NONE, desc.security_mode);
    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA/SecurityPolicy#None", desc.security_policy_uri);

    TEST_ASSERT_EQUAL(1, desc.num_user_identity_tokens);
    TEST_ASSERT_EQUAL_STRING("anonymous", desc.user_identity_tokens[0].policy_id);
    TEST_ASSERT_EQUAL(MU_USER_TOKEN_TYPE_ANONYMOUS, desc.user_identity_tokens[0].token_type);

    TEST_ASSERT_EQUAL_STRING("http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary",
                             desc.transport_profile_uri);
    TEST_ASSERT_EQUAL(0, desc.security_level);
}

void test_findservers_malformed_array_count_returns_bad_decodingerror(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_findservers_malformed_array_request(request, sizeof(request));
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    /* OPC-10000-4 section 5.5.2.2 defines FindServers array parameters;
       OPC-10000-6 section 5.2.5 reserves -1 for null arrays, so -2 is malformed. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request,
                                                                             request_len, response, &response_len));
}

void test_findservers_scn001_case001_self_unfiltered_returns_exactly_own_server_opc_cu_2352(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_findservers_unfiltered_request(request, sizeof(request), 32);
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));

    /* SCN-001 CASE-001 opc_cu_2352: OPC-10000-4 section 5.5.2 FindServers
       Self returns exactly this server's ApplicationDescription when unfiltered. */
    assert_findservers_self_application(response, response_len, 32, &server.config);
}

void test_getendpoints_malformed_array_count_returns_bad_decodingerror(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_getendpoints_malformed_array_request(request, sizeof(request));
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    /* OPC-10000-4 section 5.5.4.2 defines GetEndpoints profileUris[] parameters;
       OPC-10000-6 section 5.2.5 reserves -1 for null arrays, so -2 is malformed. */
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_DECODINGERROR,
        mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, request, request_len, response, &response_len));
}

void test_findservers_endpoint_url_filter_excludes_nonmatching_server(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[192];
    size_t request_len =
        build_findservers_endpoint_filter_request(request, sizeof(request), "opc.tcp://otherhost:4840");
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));

    opcua_int32_t server_count = read_findservers_server_count(response, response_len, 28);

    /* OPC-10000-4 section 5.5.2.2 defines endpointUrl as a FindServers
       request filter; a nonmatching URL excludes this server from Servers[]. */
    TEST_ASSERT_EQUAL_INT32(0, server_count);
}

void test_findservers_locale_filter_returns_application_name_in_requested_locale(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_findservers_locale_filter_request(request, sizeof(request), "en");
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));

    opcua_byte_t localized_text_mask;
    mu_string_t locale;
    mu_string_t text;
    read_findservers_first_application_name(response, response_len, 29, &localized_text_mask, &locale, &text);

    /* OPC-10000-4 section 5.5.2.2 defines localeIds[] as the FindServers
       selector for ApplicationDescription.applicationName localized text. */
    TEST_ASSERT_EQUAL_UINT8(0x03u, localized_text_mask);
    assert_string_equal_cstr(&locale, "en");
    assert_string_equal_cstr(&text, "Test Server");
}

void test_findservers_server_uri_filter_excludes_nonmatching_server(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[192];
    size_t request_len = build_findservers_server_uri_filter_request(request, sizeof(request), "urn:test:other-app");
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));

    opcua_int32_t server_count = read_findservers_server_count(response, response_len, 30);

    /* OPC-10000-4 section 5.5.2.2 defines serverUris[] as a FindServers
       request filter; a nonmatching URI excludes this server from Servers[]. */
    TEST_ASSERT_EQUAL_INT32(0, server_count);
}

void test_findservers_server_type_filter_excludes_server_application(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len =
        build_findservers_server_type_filter_request(request, sizeof(request), MU_APPLICATION_TYPE_CLIENT);
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));

    opcua_int32_t server_count = read_findservers_server_count(response, response_len, 31);

    /* OPC-10000-4 section 5.5.2.2 defines FindServers request filters;
       a Client serverTypes[] filter excludes this Server application from Servers[]. */
    TEST_ASSERT_EQUAL_INT32(0, server_count);
}

void test_findservers_scn001_case001_self_matching_filters_expose_only_own_server_opc_cu_2352(void) {
    mu_server_t server;
    discovery_server(&server);
    opcua_byte_t request[192];
    opcua_byte_t response[512];
    size_t request_len;
    size_t response_len;

    /* SCN-001 CASE-001 opc_cu_2352: OPC-10000-4 section 5.5.2 filters may
       select the local server, but must not fabricate or expose any other server. */
    request_len = build_findservers_endpoint_filter_request(request, sizeof(request), server.config.endpoint_url);
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));
    assert_findservers_self_application(response, response_len, 28, &server.config);

    request_len = build_findservers_server_uri_filter_request(request, sizeof(request), server.config.application_uri);
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));
    assert_findservers_self_application(response, response_len, 30, &server.config);

    request_len = build_findservers_server_type_filter_request(request, sizeof(request), MU_APPLICATION_TYPE_SERVER);
    response_len = sizeof(response);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request, request_len,
                                                                response, &response_len));
    assert_findservers_self_application(response, response_len, 31, &server.config);
}

void test_findservers_truncated_body_returns_bad_decodingerror(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_findservers_truncated_body_request(request, sizeof(request));
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    /* OPC-10000-4 section 5.5.2.2 defines endpointUrl as part of the
       mandatory FindServers request body; truncated strings are decoding errors. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, request,
                                                                             request_len, response, &response_len));
}

void test_getendpoints_truncated_body_returns_bad_decodingerror(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t request[128];
    size_t request_len = build_getendpoints_truncated_body_request(request, sizeof(request));
    opcua_byte_t response[512];
    size_t response_len = sizeof(response);

    /* OPC-10000-4 section 5.5.4.2 defines profileUris[] as part of the
       mandatory GetEndpoints request body; a missing array item is a decoding error. */
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_DECODINGERROR,
        mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, request, request_len, response, &response_len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_discovery_findservers_response);
    RUN_TEST(test_discovery_getendpoints_response);
    RUN_TEST(test_findservers_malformed_array_count_returns_bad_decodingerror);
    RUN_TEST(test_findservers_scn001_case001_self_unfiltered_returns_exactly_own_server_opc_cu_2352);
    RUN_TEST(test_getendpoints_malformed_array_count_returns_bad_decodingerror);
    RUN_TEST(test_findservers_endpoint_url_filter_excludes_nonmatching_server);
    RUN_TEST(test_findservers_locale_filter_returns_application_name_in_requested_locale);
    RUN_TEST(test_findservers_server_uri_filter_excludes_nonmatching_server);
    RUN_TEST(test_findservers_server_type_filter_excludes_server_application);
    RUN_TEST(test_findservers_scn001_case001_self_matching_filters_expose_only_own_server_opc_cu_2352);
    RUN_TEST(test_findservers_truncated_body_returns_bad_decodingerror);
    RUN_TEST(test_getendpoints_truncated_body_returns_bad_decodingerror);
    return UNITY_END();
}
