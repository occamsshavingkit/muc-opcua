/* tests/unit/test_discovery_endpoint.c
 * Discovery services (FindServers, GetEndpoints) are usable on an open
 * SecureChannel without an activated Session (OPC 10000-4 5.5). */
#include "micro_opcua/micro_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/service_header.h"
#include <string.h>

static void discovery_server(mu_server_t *server) {
    memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true; /* channel open, no session */
    server->config.endpoint_url = "opc.tcp://localhost:4840";
    server->config.application_uri = "urn:test:app";
    server->config.product_uri = "urn:test:product";
    server->config.application_name = "Test Server";
}

/* A request body is just a RequestHeader for FindServers. */
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
    return w.position;
}

static void write_string_literal(mu_binary_writer_t *w, const char *text) {
    mu_string_t s = {(opcua_int32_t)strlen(text), (const opcua_byte_t *)text};
    mu_binary_write_string(w, &s);
}

static size_t build_getendpoints_request(opcua_byte_t *buf, size_t cap, const char *profile_uri) {
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
    if (profile_uri == NULL) {
        mu_binary_write_int32(&w, 0); /* profileUris[] */
    } else {
        mu_binary_write_int32(&w, 1);
        write_string_literal(&w, profile_uri);
    }
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

void test_getendpoints_profile_uri_match_returns_endpoint(void) {
    /* OPC-10000-4 §5.5.4.2: profileUris[] lists transport profiles that the
       returned endpoints shall support. */
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request(
        req, sizeof(req), "http://opcfoundation.org/UA-Profile/Transport/uatcp-uasc-uabinary");
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_GREATER_THAN_INT32(0, endpoint_count(resp, resp_len));
}

void test_getendpoints_profile_uri_mismatch_returns_empty_array(void) {
    /* OPC-10000-4 §5.5.4.2: a non-empty unmatched profileUris[] filter succeeds
       with no returned endpoints. */
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    size_t req_len = build_getendpoints_request(req, sizeof(req),
                                                "http://opcfoundation.org/UA-Profile/Transport/https-uabinary");
    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL_INT32(0, endpoint_count(resp, resp_len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_findservers_no_session);
    RUN_TEST(test_getendpoints_no_session);
    RUN_TEST(test_getendpoints_profile_uri_match_returns_endpoint);
    RUN_TEST(test_getendpoints_profile_uri_mismatch_returns_empty_array);
    return UNITY_END();
}
