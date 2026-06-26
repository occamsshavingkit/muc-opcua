/* tests/unit/test_discovery_endpoint.c
 * Discovery services (FindServers, GetEndpoints) are usable on an open
 * SecureChannel without an activated Session (OPC 10000-4 5.5). */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

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

/* A request body is just a RequestHeader for these thin-path discovery services. */
static size_t build_header_only(opcua_byte_t *buf, size_t cap) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    mu_nodeid_t null_id = { 0, MU_NODEID_NUMERIC, { 0 } };
    mu_string_t null_str = { -1, NULL };
    mu_binary_write_nodeid(&w, &null_id);
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_extension_object_header(&w, &null_id, 0);
    return w.position;
}

static opcua_uint32_t response_type(const opcua_byte_t *resp, size_t len) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    return type.identifier.numeric;
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

    opcua_byte_t req[64];
    size_t req_len = build_header_only(req, sizeof(req));
    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, response_type(resp, resp_len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_findservers_no_session);
    RUN_TEST(test_getendpoints_no_session);
    return UNITY_END();
}
