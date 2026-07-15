/* tests/unit/test_discovery_endpoint.c
 * Discovery services (FindServers, GetEndpoints) are usable on an open
 * SecureChannel without an activated Session (OPC 10000-4 5.5). */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/discovery.h"
#include "../../src/services/service_header.h"
#include <string.h>

#define TEST_ENDPOINT_URL "opc.tcp://localhost:4840"
#define TEST_APPLICATION_URI "urn:test:app"
#define TEST_PRODUCT_URI "urn:test:product"
#define TEST_APPLICATION_NAME "Test Server"
#define TEST_SECURITY_POLICY_NONE_URI "http://opcfoundation.org/UA/SecurityPolicy#None"
#define TEST_UATCP_UASC_UABINARY_PROFILE_URI "http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary"

typedef struct parsed_endpoint {
    mu_string_t endpoint_url;
    mu_string_t server_application_uri;
    mu_string_t server_product_uri;
    opcua_byte_t application_name_mask;
    mu_string_t application_name_locale;
    mu_string_t application_name_text;
    opcua_uint32_t application_type;
    mu_string_t gateway_server_uri;
    mu_string_t discovery_profile_uri;
    opcua_int32_t discovery_url_count;
    mu_string_t discovery_url;
    mu_string_t server_certificate;
    opcua_uint32_t security_mode;
    mu_string_t security_policy_uri;
    opcua_int32_t user_identity_token_count;
    mu_string_t token_policy_id;
    opcua_uint32_t token_type;
    mu_string_t token_issued_token_type;
    mu_string_t token_issuer_endpoint_url;
    mu_string_t token_security_policy_uri;
    mu_string_t transport_profile_uri;
    opcua_byte_t security_level;
} parsed_endpoint_t;

static void discovery_server(mu_server_t *server) {
    (void)memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true; /* channel open, no session */
    server->config.endpoint_url = TEST_ENDPOINT_URL;
    server->config.application_uri = TEST_APPLICATION_URI;
    server->config.product_uri = TEST_PRODUCT_URI;
    server->config.application_name = TEST_APPLICATION_NAME;
}

/* FindServers requires RequestHeader plus endpointUrl, localeIds[], serverUris[],
   and serverTypes[] per OPC-10000-4 section 5.5.2.2. */
static size_t build_header_only(opcua_byte_t *buf, size_t cap) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(&w, &null_id);
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_extension_object_header(&w, &null_id, 0);
    mu_binary_write_string(&w, &null_str); /* endpointUrl */
    mu_binary_write_int32(&w, 0);          /* localeIds[] */
    mu_binary_write_int32(&w, 0);          /* serverUris[] */
    mu_binary_write_int32(&w, 0);          /* serverTypes[] */
    return w.position;
}

static void write_string_literal(mu_binary_writer_t *w, const char *text) {
    mu_string_t s = {(opcua_int32_t)strlen(text), (const opcua_byte_t *)text};
    mu_binary_write_string(w, &s);
}

static size_t build_getendpoints_request_with_endpoint_url(opcua_byte_t *buf, size_t cap, const char *endpoint_url,
                                                           const char *profile_uri) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(&w, &null_id);
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_extension_object_header(&w, &null_id, 0);

    if (endpoint_url == NULL) {
        mu_binary_write_string(&w, &null_str); /* endpointUrl */
    } else {
        write_string_literal(&w, endpoint_url); /* endpointUrl */
    }
    mu_binary_write_int32(&w, 0); /* localeIds[] */
    if (profile_uri == NULL) {
        mu_binary_write_int32(&w, 0); /* profileUris[] */
    } else {
        mu_binary_write_int32(&w, 1);
        write_string_literal(&w, profile_uri);
    }
    return w.position;
}

static size_t build_getendpoints_request(opcua_byte_t *buf, size_t cap, const char *profile_uri) {
    return build_getendpoints_request_with_endpoint_url(buf, cap, NULL, profile_uri);
}

static size_t build_getendpoints_request_with_locale(opcua_byte_t *buf, size_t cap, const char *locale_id) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(&w, &null_id);
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 2);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_extension_object_header(&w, &null_id, 0);

    mu_binary_write_string(&w, &null_str); /* endpointUrl */
    mu_binary_write_int32(&w, 1);          /* localeIds[] */
    write_string_literal(&w, locale_id);
    mu_binary_write_int32(&w, 0); /* profileUris[] */
    return w.position;
}

static opcua_uint32_t response_type(const opcua_byte_t *resp, size_t len) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    return type.identifier.numeric;
}

static opcua_int32_t endpoint_count(const opcua_byte_t *resp, size_t len) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, len);
    mu_nodeid_t type;
    opcua_int64_t ts;
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    opcua_byte_t diag;
    opcua_int32_t string_table_len;
    mu_nodeid_t additional_header;
    opcua_byte_t additional_encoding;
    opcua_int32_t count;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, type.identifier.numeric);
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &handle);
    mu_binary_read_statuscode(&r, &result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, result);
    mu_binary_read_byte(&r, &diag);
    mu_binary_read_int32(&r, &string_table_len);
    mu_binary_read_nodeid(&r, &additional_header);
    mu_binary_read_byte(&r, &additional_encoding);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, r.status);

    mu_binary_read_int32(&r, &count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, r.status);
    return count;
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

static void assert_null_string(const mu_string_t *actual) {
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_INT32(-1, actual->length);
}

static void read_getendpoints_response_header(mu_binary_reader_t *r, const opcua_byte_t *resp, size_t len,
                                              opcua_int32_t *endpoint_array_count) {
    mu_binary_reader_init(r, resp, len);
    mu_nodeid_t type;
    opcua_int64_t ts;
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    opcua_byte_t diag;
    opcua_int32_t string_table_len;
    mu_nodeid_t additional_header;
    opcua_byte_t additional_encoding;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(r, &type));
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, type.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(r, &ts));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(r, &handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(r, &result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(r, &diag));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(r, &string_table_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(r, &additional_header));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(r, &additional_encoding));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(r, endpoint_array_count));

    (void)ts;
    (void)handle;
    (void)diag;
    (void)string_table_len;
    (void)additional_header;
    (void)additional_encoding;
}

static void read_first_endpoint(const opcua_byte_t *resp, size_t len, opcua_int32_t expected_count,
                                parsed_endpoint_t *endpoint) {
    mu_binary_reader_t r;
    opcua_int32_t count;

    (void)memset(endpoint, 0, sizeof(*endpoint));
    endpoint->application_name_locale = (mu_string_t){-1, NULL};

    read_getendpoints_response_header(&r, resp, len, &count);
    TEST_ASSERT_EQUAL_INT32(expected_count, count);
    TEST_ASSERT_GREATER_THAN_INT32(0, count);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->endpoint_url));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->server_application_uri));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->server_product_uri));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&r, &endpoint->application_name_mask));
    if ((endpoint->application_name_mask & 0x01u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->application_name_locale));
    }
    if ((endpoint->application_name_mask & 0x02u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->application_name_text));
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &endpoint->application_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->gateway_server_uri));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->discovery_profile_uri));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &endpoint->discovery_url_count));
    TEST_ASSERT_EQUAL_INT32(1, endpoint->discovery_url_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->discovery_url));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->server_certificate));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &endpoint->security_mode));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->security_policy_uri));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &endpoint->user_identity_token_count));
    TEST_ASSERT_EQUAL_INT32(1, endpoint->user_identity_token_count);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->token_policy_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &endpoint->token_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->token_issued_token_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->token_issuer_endpoint_url));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->token_security_policy_uri));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &endpoint->transport_profile_uri));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&r, &endpoint->security_level));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, r.status);
    TEST_ASSERT_EQUAL_size_t(len, r.position);
}

static void assert_local_none_endpoint(const parsed_endpoint_t *endpoint) {
    assert_string_equal_cstr(&endpoint->endpoint_url, TEST_ENDPOINT_URL);
    assert_string_equal_cstr(&endpoint->server_application_uri, TEST_APPLICATION_URI);
    assert_string_equal_cstr(&endpoint->server_product_uri, TEST_PRODUCT_URI);
    TEST_ASSERT_EQUAL_UINT8(0x02u, endpoint->application_name_mask);
    assert_string_equal_cstr(&endpoint->application_name_text, TEST_APPLICATION_NAME);
    TEST_ASSERT_EQUAL_UINT32(MU_APPLICATION_TYPE_SERVER, endpoint->application_type);
    assert_null_string(&endpoint->gateway_server_uri);
    assert_null_string(&endpoint->discovery_profile_uri);
    assert_string_equal_cstr(&endpoint->discovery_url, TEST_ENDPOINT_URL);
    assert_null_string(&endpoint->server_certificate);
    TEST_ASSERT_EQUAL_UINT32(MU_MESSAGE_SECURITY_MODE_NONE, endpoint->security_mode);
    assert_string_equal_cstr(&endpoint->security_policy_uri, TEST_SECURITY_POLICY_NONE_URI);
    assert_string_equal_cstr(&endpoint->token_policy_id, "anonymous");
    TEST_ASSERT_EQUAL_UINT32(MU_USER_TOKEN_TYPE_ANONYMOUS, endpoint->token_type);
    assert_null_string(&endpoint->token_issued_token_type);
    assert_null_string(&endpoint->token_issuer_endpoint_url);
    assert_null_string(&endpoint->token_security_policy_uri);
    assert_string_equal_cstr(&endpoint->transport_profile_uri, TEST_UATCP_UASC_UABINARY_PROFILE_URI);
    TEST_ASSERT_EQUAL_UINT8(0u, endpoint->security_level);
}

static void read_getendpoints_first_application_name(const opcua_byte_t *resp, size_t len,
                                                     opcua_byte_t *localized_text_mask, mu_string_t *locale,
                                                     mu_string_t *text) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, len);
    mu_nodeid_t type;
    opcua_int64_t ts;
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    opcua_byte_t diag;
    opcua_int32_t string_table_len;
    mu_nodeid_t additional_header;
    opcua_byte_t additional_encoding;
    opcua_int32_t count;
    mu_string_t ignored;

    *localized_text_mask = 0u;
    *locale = (mu_string_t){-1, NULL};
    *text = (mu_string_t){-1, NULL};

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, type.identifier.numeric);
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &handle);
    mu_binary_read_statuscode(&r, &result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, result);
    mu_binary_read_byte(&r, &diag);
    mu_binary_read_int32(&r, &string_table_len);
    mu_binary_read_nodeid(&r, &additional_header);
    mu_binary_read_byte(&r, &additional_encoding);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, r.status);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &count));
    TEST_ASSERT_GREATER_THAN_INT32(0, count);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &ignored)); /* endpointUrl */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &ignored)); /* server.applicationUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, &ignored)); /* server.productUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&r, localized_text_mask));
    if ((*localized_text_mask & 0x01u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, locale));
    }
    if ((*localized_text_mask & 0x02u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&r, text));
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, r.status);

    (void)ts;
    (void)handle;
    (void)diag;
    (void)string_table_len;
    (void)additional_header;
    (void)additional_encoding;
}

void test_findservers_no_session(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[64];
    size_t req_len = build_header_only(req, sizeof(req));
    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL(MU_ID_FINDSERVERSRESPONSE, response_type(resp, resp_len));
}

void test_getendpoints_no_session(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request(req, sizeof(req), NULL);
    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, response_type(resp, resp_len));
}

void test_getendpoints_scn001_case001_opc_cu_2328_profile_uri_match_returns_local_endpoint_details(void) {
    /* SCN-001 / CASE-001 / opc_cu_2328: OPC-10000-4 sections 5.5.1 and
       5.5.4.2 require GetEndpoints to return endpoints that support a matching
       profileUris[] transport profile. */
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request(req, sizeof(req), TEST_UATCP_UASC_UABINARY_PROFILE_URI);
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));

    parsed_endpoint_t endpoint;
    read_first_endpoint(resp, resp_len, 1, &endpoint);
    assert_local_none_endpoint(&endpoint);
}

void test_getendpoints_scn001_case001_opc_cu_2328_profile_uri_mismatch_returns_empty_array(void) {
    /* SCN-001 / CASE-001 / opc_cu_2328: OPC-10000-4 section 5.5.4.2 filters
       by profileUris[]; an unmatched filter returns an empty Endpoints[] with
       a Good service result, not a service error. */
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len =
        build_getendpoints_request(req, sizeof(req), "http://opcfoundation.org/UA-Profile/Transport/https-uabinary");
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL_INT32(0, endpoint_count(resp, resp_len));
}

void test_getendpoints_scn001_case001_opc_cu_2328_endpoint_url_match_returns_exactly_local_endpoint(void) {
    /* SCN-001 / CASE-001 / opc_cu_2328: OPC-10000-4 sections 5.5.1 and
       5.5.4.2 require a matching endpointUrl filter to return the local
       EndpointDescription. */
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request_with_endpoint_url(req, sizeof(req), TEST_ENDPOINT_URL, NULL);
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));

    parsed_endpoint_t endpoint;
    read_first_endpoint(resp, resp_len, 1, &endpoint);
    assert_local_none_endpoint(&endpoint);
}

void test_getendpoints_scn001_case001_opc_cu_2328_endpoint_url_filter_excludes_nonmatching_endpoint(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request_with_endpoint_url(req, sizeof(req), "opc.tcp://otherhost:4840", NULL);
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));

    /* SCN-001 / CASE-001 / opc_cu_2328: OPC-10000-4 section 5.5.4.2 defines
       endpointUrl as a GetEndpoints request filter; a nonmatching URL excludes
       endpoints from Endpoints[] without changing the Good service result. */
    TEST_ASSERT_EQUAL_INT32(0, endpoint_count(resp, resp_len));
}

void test_getendpoints_scn001_case001_opc_cu_2328_endpoint_url_and_profile_uri_match_returns_one_local_endpoint(void) {
    /* SCN-001 / CASE-001 / opc_cu_2328: OPC-10000-4 section 5.5.4.2 filters
       endpointUrl and profileUris[] together before returning Endpoints[]. */
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request_with_endpoint_url(req, sizeof(req), TEST_ENDPOINT_URL,
                                                                  TEST_UATCP_UASC_UABINARY_PROFILE_URI);
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));

    parsed_endpoint_t endpoint;
    read_first_endpoint(resp, resp_len, 1, &endpoint);
    assert_local_none_endpoint(&endpoint);
}

void test_getendpoints_locale_filter_returns_server_application_name_in_requested_locale(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request_with_locale(req, sizeof(req), "en");
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));

    opcua_byte_t localized_text_mask;
    mu_string_t locale;
    mu_string_t text;
    read_getendpoints_first_application_name(resp, resp_len, &localized_text_mask, &locale, &text);

    /* OPC-10000-4 section 5.5.4.2 defines localeIds[] as the GetEndpoints
       selector for EndpointDescription.server.applicationName localized text. */
    TEST_ASSERT_EQUAL_UINT8(0x03u, localized_text_mask);
    assert_string_equal_cstr(&locale, "en");
    assert_string_equal_cstr(&text, "Test Server");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_findservers_no_session);
    RUN_TEST(test_getendpoints_no_session);
    RUN_TEST(test_getendpoints_scn001_case001_opc_cu_2328_profile_uri_match_returns_local_endpoint_details);
    RUN_TEST(test_getendpoints_scn001_case001_opc_cu_2328_profile_uri_mismatch_returns_empty_array);
    RUN_TEST(test_getendpoints_scn001_case001_opc_cu_2328_endpoint_url_match_returns_exactly_local_endpoint);
    RUN_TEST(test_getendpoints_scn001_case001_opc_cu_2328_endpoint_url_filter_excludes_nonmatching_endpoint);
    RUN_TEST(
        test_getendpoints_scn001_case001_opc_cu_2328_endpoint_url_and_profile_uri_match_returns_one_local_endpoint);
    RUN_TEST(test_getendpoints_locale_filter_returns_server_application_name_in_requested_locale);
    return UNITY_END();
}
