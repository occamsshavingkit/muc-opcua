/* tests/unit/test_dispatch_services.c
 * Dispatch-level tests: drive mu_service_dispatch with a crafted request body
 * (positioned after the request-type NodeId, as mu_server_poll delivers it) and
 * assert on the encoded response body (response-type NodeId + ResponseHeader +
 * service response). */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/service_header.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static opcua_datetime_t fake_time(void *c) { (void)c; return 0; }
static opcua_statuscode_t fake_entropy(void *c, opcua_byte_t *buf, size_t len) {
    (void)c; if (buf) memset(buf, 0x42, len); return MU_STATUS_GOOD;
}

/* Write a RequestHeader (OPC 10000-4 7.32) with the given requestHandle. */
static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t null_id = { 0, MU_NODEID_NUMERIC, { 0 } };
    /* authenticationToken = the first session slot's token (12345). Session-requiring
       services (Read/Browse) are routed by this token to the activated session;
       non-session services ignore it. */
    mu_nodeid_t auth_id = { 0, MU_NODEID_NUMERIC, { 12345 } };
    mu_string_t null_str = { -1, NULL };
    mu_binary_write_nodeid(w, &auth_id);                    /* authenticationToken */
    mu_binary_write_int64(w, 0);                            /* timestamp */
    mu_binary_write_uint32(w, handle);                      /* requestHandle */
    mu_binary_write_uint32(w, 0);                           /* returnDiagnostics */
    mu_binary_write_string(w, &null_str);                  /* auditEntryId */
    mu_binary_write_uint32(w, 0);                           /* timeoutHint */
    mu_binary_write_extension_object_header(w, &null_id, 0);/* additionalHeader */
}

/* Skip a ResponseHeader the reader is positioned at. */
static void skip_response_header(mu_binary_reader_t *r, opcua_uint32_t *handle, opcua_statuscode_t *result) {
    opcua_int64_t ts; opcua_byte_t diag, enc; opcua_int32_t strtbl; mu_nodeid_t addl;
    mu_binary_read_int64(r, &ts);
    mu_binary_read_uint32(r, handle);
    mu_binary_read_statuscode(r, result);
    mu_binary_read_byte(r, &diag);          /* serviceDiagnostics (empty) */
    mu_binary_read_int32(r, &strtbl);       /* stringTable (null) */
    mu_binary_read_nodeid(r, &addl);        /* additionalHeader typeId */
    mu_binary_read_byte(r, &enc);           /* additionalHeader encoding */
}

void test_dispatch_open_secure_channel(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    mu_secure_channel_init(&server.secure_channel);
    server.config.time_adapter.get_time = fake_time;

    /* Build the OpenSecureChannelRequest body (after the request-type NodeId). */
    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 42);
    mu_binary_write_uint32(&w, 0);          /* ClientProtocolVersion */
    mu_binary_write_uint32(&w, 0);          /* RequestType = ISSUE */
    mu_binary_write_uint32(&w, 1);          /* SecurityMode = NONE */
    mu_bytestring_t client_nonce = { -1, NULL };
    mu_binary_write_bytestring(&w, &client_nonce);
    mu_binary_write_uint32(&w, 3600000);    /* RequestedLifetime */
    size_t req_len = w.position;

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_OPENSECURECHANNELREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_TRUE(server.secure_channel.is_open);

    /* Parse the response body. */
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL(MU_ID_OPENSECURECHANNELRESPONSE, type.identifier.numeric);

    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(42, handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    opcua_uint32_t server_proto, channel_id, token_id, revised;
    opcua_int64_t created;
    mu_binary_read_uint32(&r, &server_proto);
    mu_binary_read_uint32(&r, &channel_id);
    mu_binary_read_uint32(&r, &token_id);
    mu_binary_read_int64(&r, &created);
    mu_binary_read_uint32(&r, &revised);
    TEST_ASSERT_EQUAL(1, channel_id);
    TEST_ASSERT_EQUAL(1, token_id);
    TEST_ASSERT_EQUAL(3600000, revised);
}

#include "../../src/services/session.h"

/* A full CreateSessionRequest body (after the request-type NodeId): RequestHeader,
   ClientDescription, ServerUri, EndpointUrl, SessionName, ClientNonce,
   ClientCertificate, RequestedSessionTimeout, MaxResponseMessageSize. */
static size_t build_create_session_body(opcua_byte_t *buf, size_t cap, opcua_double_t requested_timeout) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, cap);
    write_request_header(&w, 7);
    mu_string_t ns = { -1, NULL };
    mu_bytestring_t nb = { -1, NULL };
    /* ClientDescription (ApplicationDescription) */
    mu_binary_write_string(&w, &ns);           /* applicationUri */
    mu_binary_write_string(&w, &ns);           /* productUri */
    mu_binary_write_byte(&w, 0x00);            /* applicationName LocalizedText (empty) */
    mu_binary_write_uint32(&w, 1);             /* applicationType = CLIENT */
    mu_binary_write_string(&w, &ns);           /* gatewayServerUri */
    mu_binary_write_string(&w, &ns);           /* discoveryProfileUri */
    mu_binary_write_int32(&w, 0);              /* discoveryUrls[] */
    mu_binary_write_string(&w, &ns);           /* ServerUri */
    mu_binary_write_string(&w, &ns);           /* EndpointUrl */
    mu_binary_write_string(&w, &ns);           /* SessionName */
    mu_binary_write_bytestring(&w, &nb);       /* ClientNonce */
    mu_binary_write_bytestring(&w, &nb);       /* ClientCertificate */
    mu_binary_write_double(&w, requested_timeout);
    mu_binary_write_uint32(&w, 0);             /* MaxResponseMessageSize */
    return w.position;
}

void test_dispatch_create_session_honors_timeout(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    mu_session_init(&server.sessions[0]);
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;

    opcua_byte_t req[256];
    size_t req_len = build_create_session_body(req, sizeof(req), 1200000.0); /* within [10000, 3600000] */

    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type; mu_binary_read_nodeid(&r, &type);
    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    mu_nodeid_t sid, tok; mu_binary_read_nodeid(&r, &sid); mu_binary_read_nodeid(&r, &tok);
    opcua_double_t revised; mu_binary_read_double(&r, &revised);
    TEST_ASSERT_EQUAL(1200000, (opcua_int64_t)revised); /* the client's requested timeout, honored */
}

void test_dispatch_create_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    mu_session_init(&server.sessions[0]);
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;

    /* CreateSessionRequest body: just the RequestHeader is parsed in the thin path. */
    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 7);
    size_t req_len = w.position;

    opcua_byte_t resp[1024]; /* CreateSessionResponse now carries ServerEndpoints */
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, type.identifier.numeric);

    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(7, handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    mu_nodeid_t session_id, auth_token;
    mu_binary_read_nodeid(&r, &session_id);
    mu_binary_read_nodeid(&r, &auth_token);
    opcua_double_t revised;
    mu_binary_read_double(&r, &revised);
    TEST_ASSERT_EQUAL(1, session_id.identifier.numeric);
    TEST_ASSERT_EQUAL(12345, auth_token.identifier.numeric);
    TEST_ASSERT_EQUAL(10000, (opcua_int64_t)revised); /* revised session timeout, bounded to min */

    mu_string_t server_nonce;
    mu_binary_read_string(&r, &server_nonce); /* ServerNonce: a real entropy-filled 32-byte nonce */
    TEST_ASSERT_EQUAL(32, server_nonce.length);
    TEST_ASSERT_EQUAL(0x42, server_nonce.data[0]);
}

/* Append an ActivateSession body: ClientSignature + empty cert/locale arrays +
   an AnonymousIdentityToken (DefaultBinary encoding NodeId i=321). */
static size_t build_activate_body(opcua_byte_t *buf, size_t size, opcua_uint32_t auth_token_id) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, size);
    /* RequestHeader with authenticationToken numeric = auth_token_id */
    mu_nodeid_t auth = { 0, MU_NODEID_NUMERIC, { auth_token_id } };
    mu_string_t null_str = { -1, NULL };
    mu_bytestring_t null_bs = { -1, NULL };
    mu_nodeid_t null_id = { 0, MU_NODEID_NUMERIC, { 0 } };
    mu_binary_write_nodeid(&w, &auth);
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_extension_object_header(&w, &null_id, 0);
    /* ClientSignature: SignatureData { algorithm(String null), signature(ByteString null) } */
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_bytestring(&w, &null_bs);
    /* ClientSoftwareCertificates: empty array */
    mu_binary_write_int32(&w, 0);
    /* LocaleIds: empty array */
    mu_binary_write_int32(&w, 0);
    /* UserIdentityToken: ExtensionObject typeId = 321 (AnonymousIdentityToken DefaultBinary) */
    mu_nodeid_t anon = { 0, MU_NODEID_NUMERIC, { 321 } };
    mu_binary_write_extension_object_header(&w, &anon, 0);
    return w.position;
}

void test_dispatch_activate_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.time_adapter.get_time = fake_time;
    mu_session_init(&server.sessions[0]);
    opcua_uint64_t revised; opcua_uint32_t sid, tok;
    mu_session_create(&server.sessions[0], 0, &revised, &sid, &tok); /* -> CREATED, auth=12345 */

    opcua_byte_t req[256];
    size_t req_len = build_activate_body(req, sizeof(req), tok);

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);
}

void test_dispatch_close_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    mu_session_init(&server.sessions[0]);
    opcua_uint64_t revised; opcua_uint32_t sid, tok;
    mu_session_create(&server.sessions[0], 0, &revised, &sid, &tok);
    mu_session_activate(&server.sessions[0], tok, 321); /* -> ACTIVATED */

    /* CloseSessionRequest: RequestHeader (authToken=tok) + DeleteSubscriptions(Boolean) */
    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    mu_nodeid_t auth = { 0, MU_NODEID_NUMERIC, { tok } };
    mu_string_t null_str = { -1, NULL };
    mu_nodeid_t null_id = { 0, MU_NODEID_NUMERIC, { 0 } };
    mu_binary_write_nodeid(&w, &auth);
    mu_binary_write_int64(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_string(&w, &null_str);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_extension_object_header(&w, &null_id, 0);
    mu_binary_write_boolean(&w, true); /* DeleteSubscriptions */
    size_t req_len = w.position;

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_CLOSESESSIONREQUEST, req, req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, server.sessions[0].state);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_CLOSESESSIONRESPONSE, type.identifier.numeric);
}

#include "../../src/services/read.h"
#include "../../src/services/browse.h"

/* Small address space: Objects(85) Organizes MyVar1(1000, Int32=42). */
static const mu_reference_t s_obj_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 1, MU_NODEID_NUMERIC, { 1000 } }, true }
};
static const mu_reference_t s_var_refs[] = {
    { { 0, MU_NODEID_NUMERIC, { 35 } }, { 0, MU_NODEID_NUMERIC, { 85 } }, false }
};
static const mu_value_source_t s_var_value = {
    MU_VALUESOURCE_STATIC, { .static_value = { MU_TYPE_INT32, { .i32 = 42 } } }
};
static const mu_node_t s_nodes[] = {
    { { 0, MU_NODEID_NUMERIC, { 85 } }, MU_NODECLASS_OBJECT,
      { 7, (const opcua_byte_t *)"Objects" }, { 7, (const opcua_byte_t *)"Objects" }, s_obj_refs, 1, NULL },
    { { 1, MU_NODEID_NUMERIC, { 1000 } }, MU_NODECLASS_VARIABLE,
      { 6, (const opcua_byte_t *)"MyVar1" }, { 6, (const opcua_byte_t *)"MyVar1" }, s_var_refs, 1, &s_var_value }
};
static const mu_address_space_t s_address_space = { s_nodes, 2 };

static void activated_server(mu_server_t *server) {
    memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    server->config.time_adapter.get_time = fake_time;
    server->config.address_space = &s_address_space;
#if MICRO_OPCUA_SUBSCRIPTIONS
    mu_subscriptions_init(&server->subs);
#endif
    mu_session_init(&server->sessions[0]);
    opcua_uint64_t rev; opcua_uint32_t sid, tok;
    mu_session_create(&server->sessions[0], 0, &rev, &sid, &tok);
    mu_session_activate(&server->sessions[0], tok, 321);
}

void test_dispatch_delete_subscriptions_rejects_too_many_operations_before_results(void) {
    mu_server_t server;
    activated_server(&server);

    enum { too_many_operations = 100 };
    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 17);
    mu_binary_write_int32(&w, too_many_operations);
    for (opcua_int32_t i = 0; i < too_many_operations; ++i) {
        mu_binary_write_uint32(&w, (opcua_uint32_t)(i + 1));
    }
    size_t req_len = w.position;

    opcua_byte_t resp[1024];
    memset(resp, 0xA5, sizeof(resp));
    size_t response_capacity = sizeof(resp);
    size_t resp_len = response_capacity;

    /* OPC-10000-4 §7.38.2: Bad_TooManyOperations is a service-level rejection
       for a request that specifies too many operations, before results[] length. */
    opcua_statuscode_t service_result =
        mu_service_dispatch(&server, MU_ID_DELETESUBSCRIPTIONSREQUEST,
                            req, req_len, resp, &resp_len);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS, service_result);
    TEST_ASSERT_EQUAL(response_capacity, resp_len);
    TEST_ASSERT_EQUAL_UINT8(0xA5, resp[0]);
}

void test_dispatch_read_value(void) {
    mu_server_t server;
    activated_server(&server);

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 5);
    mu_string_t null_str = { -1, NULL };
    mu_binary_write_double(&w, 0.0);                 /* MaxAge */
    mu_binary_write_uint32(&w, 3);                   /* TimestampsToReturn = NEITHER */
    mu_binary_write_int32(&w, 1);                    /* NoOfNodesToRead */
    mu_nodeid_t var = { 1, MU_NODEID_NUMERIC, { 1000 } };
    mu_binary_write_nodeid(&w, &var);               /* nodeId */
    mu_binary_write_uint32(&w, 13);                  /* attributeId = Value */
    mu_binary_write_string(&w, &null_str);          /* indexRange */
    mu_binary_write_uint16(&w, 0);                   /* dataEncoding.namespaceIndex */
    mu_binary_write_string(&w, &null_str);          /* dataEncoding.name */
    size_t req_len = w.position;

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_READREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type; mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_READRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(5, handle);

    opcua_int32_t count; mu_binary_read_int32(&r, &count);
    TEST_ASSERT_EQUAL(1, count);
    opcua_byte_t mask; mu_binary_read_byte(&r, &mask);
    TEST_ASSERT_EQUAL(0x01, mask);                   /* has value */
    opcua_byte_t vtype; mu_binary_read_byte(&r, &vtype);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, vtype);
    opcua_int32_t val; mu_binary_read_int32(&r, &val);
    TEST_ASSERT_EQUAL(42, val);
}

void test_dispatch_browse(void) {
    mu_server_t server;
    activated_server(&server);

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 9);
    mu_nodeid_t empty = { 0, MU_NODEID_NUMERIC, { 0 } };
    mu_binary_write_nodeid(&w, &empty);             /* ViewDescription.viewId */
    mu_binary_write_int64(&w, 0);                    /* timestamp */
    mu_binary_write_uint32(&w, 0);                   /* viewVersion */
    mu_binary_write_uint32(&w, 0);                   /* RequestedMaxReferencesPerNode */
    mu_binary_write_int32(&w, 1);                    /* NoOfNodesToBrowse */
    mu_nodeid_t objects = { 0, MU_NODEID_NUMERIC, { 85 } };
    mu_binary_write_nodeid(&w, &objects);           /* nodeId */
    mu_binary_write_uint32(&w, 0);                   /* browseDirection = FORWARD */
    mu_binary_write_nodeid(&w, &empty);             /* referenceTypeId (any) */
    mu_binary_write_boolean(&w, false);             /* includeSubtypes */
    mu_binary_write_uint32(&w, 0);                   /* nodeClassMask */
    mu_binary_write_uint32(&w, 0x3F);                /* resultMask */
    size_t req_len = w.position;

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_BROWSEREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type; mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_BROWSERESPONSE, type.identifier.numeric);
    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(9, handle);

    opcua_int32_t n_results; mu_binary_read_int32(&r, &n_results);
    TEST_ASSERT_EQUAL(1, n_results);
    opcua_statuscode_t op_status; mu_binary_read_statuscode(&r, &op_status);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, op_status);
    opcua_int32_t cp_len; mu_binary_read_int32(&r, &cp_len);  /* continuationPoint (null) */
    opcua_int32_t n_refs; mu_binary_read_int32(&r, &n_refs);
    TEST_ASSERT_EQUAL(1, n_refs);                    /* Objects -> MyVar1 */
}

static void discovery_server(mu_server_t *server) {
    memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    server->config.endpoint_url = "opc.tcp://localhost:4840";
    server->config.application_uri = "urn:test:app";
    server->config.product_uri = "urn:test:product";
    server->config.application_name = "Test Server";
}

void test_dispatch_get_endpoints(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 3);
    size_t req_len = w.position;

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type; mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(3, handle);
    opcua_int32_t n_ep; mu_binary_read_int32(&r, &n_ep);
    TEST_ASSERT_EQUAL(1, n_ep);
    mu_string_t url; mu_binary_read_string(&r, &url);
    TEST_ASSERT_EQUAL_MEMORY("opc.tcp://localhost:4840", url.data, 24);
}

void test_dispatch_find_servers(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 4);
    size_t req_len = w.position;

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type; mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_FINDSERVERSRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle; opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    opcua_int32_t n_srv; mu_binary_read_int32(&r, &n_srv);
    TEST_ASSERT_EQUAL(1, n_srv);
    mu_string_t app_uri; mu_binary_read_string(&r, &app_uri);
    TEST_ASSERT_EQUAL_MEMORY("urn:test:app", app_uri.data, 12);
}

void test_service_fault_encode(void) {
    opcua_byte_t buf[64];
    size_t len = sizeof(buf);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_write_service_fault(buf, &len, 0, MU_STATUS_BAD_SERVICEUNSUPPORTED));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_SERVICEFAULT, type.identifier.numeric);
    opcua_int64_t ts; opcua_uint32_t h; opcua_statuscode_t res;
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &h);
    mu_binary_read_statuscode(&r, &res);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED, res);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_service_fault_encode);
    RUN_TEST(test_dispatch_open_secure_channel);
    RUN_TEST(test_dispatch_create_session);
    RUN_TEST(test_dispatch_create_session_honors_timeout);
    RUN_TEST(test_dispatch_activate_session);
    RUN_TEST(test_dispatch_close_session);
    RUN_TEST(test_dispatch_delete_subscriptions_rejects_too_many_operations_before_results);
    RUN_TEST(test_dispatch_read_value);
    RUN_TEST(test_dispatch_browse);
    RUN_TEST(test_dispatch_get_endpoints);
    RUN_TEST(test_dispatch_find_servers);
    return UNITY_END();
}
