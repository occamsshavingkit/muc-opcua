/* tests/unit/test_dispatch_services.c
 * Dispatch-level tests: drive mu_service_dispatch with a crafted request body
 * (positioned after the request-type NodeId, as mu_server_poll delivers it) and
 * assert on the encoded response body (response-type NodeId + ResponseHeader +
 * service response). */
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/service_header.h"
#include "../../src/services/subscription.h"
#include "micro_opcua/micro_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static opcua_datetime_t fake_time(void *c) {
    (void)c;
    return 0;
}
#if MICRO_OPCUA_SUBSCRIPTIONS
static opcua_uint64_t fake_tick_ms(void *c) {
    (void)c;
    return 0;
}
#endif
static opcua_statuscode_t fake_entropy(void *c, opcua_byte_t *buf, size_t len) {
    (void)c;
    if (buf)
        memset(buf, 0x42, len);
    return MU_STATUS_GOOD;
}
#define TEST_ENTROPY_FIRST_SESSION_ID 0x42424242u
#define TEST_ENTROPY_FIRST_AUTH_TOKEN 0xE7E7E7E7u

/* Write a RequestHeader (OPC 10000-4 7.32) with the given requestHandle. */
static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    /* authenticationToken = the first session slot's token (12345). Session-requiring
       services (Read/Browse) are routed by this token to the activated session;
       non-session services ignore it. */
    mu_nodeid_t auth_id = {0, MU_NODEID_NUMERIC, {12345}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(w, &auth_id);                     /* authenticationToken */
    mu_binary_write_int64(w, 0);                             /* timestamp */
    mu_binary_write_uint32(w, handle);                       /* requestHandle */
    mu_binary_write_uint32(w, 0);                            /* returnDiagnostics */
    mu_binary_write_string(w, &null_str);                    /* auditEntryId */
    mu_binary_write_uint32(w, 0);                            /* timeoutHint */
    mu_binary_write_extension_object_header(w, &null_id, 0); /* additionalHeader */
}

/* Skip a ResponseHeader the reader is positioned at. */
static void skip_response_header(mu_binary_reader_t *r, opcua_uint32_t *handle, opcua_statuscode_t *result) {
    opcua_int64_t ts;
    opcua_byte_t diag, enc;
    opcua_int32_t strtbl;
    mu_nodeid_t addl;
    mu_binary_read_int64(r, &ts);
    mu_binary_read_uint32(r, handle);
    mu_binary_read_statuscode(r, result);
    mu_binary_read_byte(r, &diag);    /* serviceDiagnostics (empty) */
    mu_binary_read_int32(r, &strtbl); /* stringTable (null) */
    mu_binary_read_nodeid(r, &addl);  /* additionalHeader typeId */
    mu_binary_read_byte(r, &enc);     /* additionalHeader encoding */
}

void test_dispatch_open_secure_channel(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    mu_secure_channel_init(&server.secure_channel);
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;

    /* Build the OpenSecureChannelRequest body (after the request-type NodeId). */
    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 42);
    mu_binary_write_uint32(&w, 0); /* ClientProtocolVersion */
    mu_binary_write_uint32(&w, 0); /* RequestType = ISSUE */
    mu_binary_write_uint32(&w, 1); /* SecurityMode = NONE */
    mu_bytestring_t client_nonce = {-1, NULL};
    mu_binary_write_bytestring(&w, &client_nonce);
    mu_binary_write_uint32(&w, 3600000); /* RequestedLifetime */
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

    opcua_uint32_t handle;
    opcua_statuscode_t result;
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
    mu_string_t ns = {-1, NULL};
    mu_bytestring_t nb = {-1, NULL};
    /* ClientDescription (ApplicationDescription) */
    mu_binary_write_string(&w, &ns);     /* applicationUri */
    mu_binary_write_string(&w, &ns);     /* productUri */
    mu_binary_write_byte(&w, 0x00);      /* applicationName LocalizedText (empty) */
    mu_binary_write_uint32(&w, 1);       /* applicationType = CLIENT */
    mu_binary_write_string(&w, &ns);     /* gatewayServerUri */
    mu_binary_write_string(&w, &ns);     /* discoveryProfileUri */
    mu_binary_write_int32(&w, 0);        /* discoveryUrls[] */
    mu_binary_write_string(&w, &ns);     /* ServerUri */
    mu_binary_write_string(&w, &ns);     /* EndpointUrl */
    mu_binary_write_string(&w, &ns);     /* SessionName */
    mu_binary_write_bytestring(&w, &nb); /* ClientNonce */
    mu_binary_write_bytestring(&w, &nb); /* ClientCertificate */
    mu_binary_write_double(&w, requested_timeout);
    mu_binary_write_uint32(&w, 0); /* MaxResponseMessageSize */
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
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    mu_nodeid_t sid, tok;
    mu_binary_read_nodeid(&r, &sid);
    mu_binary_read_nodeid(&r, &tok);
    opcua_double_t revised;
    mu_binary_read_double(&r, &revised);
    TEST_ASSERT_EQUAL(1200000, (opcua_int64_t)revised); /* the client's requested timeout, honored */
}

void test_dispatch_truncated_create_session_body_returns_bad_decodingerror_without_session_mutation(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    mu_session_init(&server.sessions[0]);
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;

    mu_session_t sessions_before[MU_MAX_SESSIONS];
    memcpy(sessions_before, server.sessions, sizeof(sessions_before));
    mu_session_t *active_session_before = server.active_session;

    opcua_byte_t req[256];
    size_t full_req_len = build_create_session_body(req, sizeof(req), 1200000.0);
    TEST_ASSERT_TRUE(full_req_len > 1u);
    size_t truncated_req_len = full_req_len - 1u;

    opcua_byte_t resp[1024];
    size_t resp_len = sizeof(resp);

    /* OPC-10000-4 section 7.38.2: Bad_DecodingError covers invalid stream data;
       a truncated mandatory service body must reject before creating session state. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, req,
                                                                             truncated_req_len, resp, &resp_len));
    TEST_ASSERT_EQUAL_PTR(active_session_before, server.active_session);
    for (size_t i = 0; i < MU_MAX_SESSIONS; ++i) {
        TEST_ASSERT_EQUAL(sessions_before[i].state, server.sessions[i].state);
        TEST_ASSERT_EQUAL(sessions_before[i].session_id, server.sessions[i].session_id);
        TEST_ASSERT_EQUAL(sessions_before[i].auth_token, server.sessions[i].auth_token);
        TEST_ASSERT_EQUAL(sessions_before[i].revised_session_timeout_ms, server.sessions[i].revised_session_timeout_ms);
        TEST_ASSERT_EQUAL_MEMORY(sessions_before[i].server_nonce, server.sessions[i].server_nonce,
                                 sizeof(sessions_before[i].server_nonce));
#ifdef MICRO_OPCUA_MULTIPLE_CONNECTIONS
        TEST_ASSERT_EQUAL(sessions_before[i].secure_channel_id, server.sessions[i].secure_channel_id);
#endif
    }
}

void test_dispatch_create_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    mu_session_init(&server.sessions[0]);
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;

    opcua_byte_t req[256];
    size_t req_len = build_create_session_body(req, sizeof(req), 0.0);

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

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(7, handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    mu_nodeid_t session_id, auth_token;
    mu_binary_read_nodeid(&r, &session_id);
    mu_binary_read_nodeid(&r, &auth_token);
    opcua_double_t revised;
    mu_binary_read_double(&r, &revised);
    TEST_ASSERT_EQUAL_HEX32(TEST_ENTROPY_FIRST_SESSION_ID, session_id.identifier.numeric);
    TEST_ASSERT_EQUAL_HEX32(TEST_ENTROPY_FIRST_AUTH_TOKEN, auth_token.identifier.numeric);
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
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {auth_token_id}};
    mu_string_t null_str = {-1, NULL};
    mu_bytestring_t null_bs = {-1, NULL};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
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
    mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {321}};
    mu_binary_write_extension_object_header(&w, &anon, 0);
    return w.position;
}

void test_dispatch_activate_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.time_adapter.get_time = fake_time;
    mu_session_init(&server.sessions[0]);
    opcua_uint64_t revised;
    opcua_uint32_t sid, tok;
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
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);
}

void test_dispatch_close_session(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    mu_session_init(&server.sessions[0]);
    opcua_uint64_t revised;
    opcua_uint32_t sid, tok;
    mu_session_create(&server.sessions[0], 0, &revised, &sid, &tok);
    mu_session_activate(&server.sessions[0], tok, 321); /* -> ACTIVATED */

    /* CloseSessionRequest: RequestHeader (authToken=tok) + DeleteSubscriptions(Boolean) */
    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {tok}};
    mu_string_t null_str = {-1, NULL};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
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

#include "../../src/services/browse.h"
#include "../../src/services/read.h"

/* Small address space: Objects(85) Organizes MyVar1(1000, Int32=42). */
static const mu_reference_t s_obj_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {1, MU_NODEID_NUMERIC, {1000}}, true}};
static const mu_reference_t s_var_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
static const mu_value_source_t s_var_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
static const mu_node_t s_nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                     MU_NODECLASS_OBJECT,
                                     {7, (const opcua_byte_t *)"Objects"},
                                     {7, (const opcua_byte_t *)"Objects"},
                                     s_obj_refs,
                                     1,
                                     NULL},
                                    {{1, MU_NODEID_NUMERIC, {1000}},
                                     MU_NODECLASS_VARIABLE,
                                     {6, (const opcua_byte_t *)"MyVar1"},
                                     {6, (const opcua_byte_t *)"MyVar1"},
                                     s_var_refs,
                                     1,
                                     &s_var_value}};
static const mu_address_space_t s_address_space = {s_nodes, 2};

static void activated_server(mu_server_t *server) {
    memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    server->config.time_adapter.get_time = fake_time;
    server->config.address_space = &s_address_space;
#if MICRO_OPCUA_SUBSCRIPTIONS
    server->config.time_adapter.get_tick_ms = fake_tick_ms;
    mu_subscriptions_init(&server->subs);
#endif
    mu_session_init(&server->sessions[0]);
    opcua_uint64_t rev;
    opcua_uint32_t sid, tok;
    mu_session_create(&server->sessions[0], 0, &rev, &sid, &tok);
    mu_session_activate(&server->sessions[0], tok, 321);
}

#if MICRO_OPCUA_SUBSCRIPTIONS
static opcua_uint32_t create_subscription_via_dispatch(mu_server_t *server) {
    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 21);
    mu_binary_write_double(&w, 1000.0); /* RequestedPublishingInterval */
    mu_binary_write_uint32(&w, 30);     /* RequestedLifetimeCount */
    mu_binary_write_uint32(&w, 10);     /* RequestedMaxKeepAliveCount */
    mu_binary_write_uint32(&w, 0);      /* MaxNotificationsPerPublish */
    mu_binary_write_boolean(&w, true);  /* PublishingEnabled */
    mu_binary_write_byte(&w, 0);        /* Priority */

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(server, MU_ID_CREATESUBSCRIPTIONREQUEST, req, w.position, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL(MU_ID_CREATESUBSCRIPTIONRESPONSE, type.identifier.numeric);

    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(21, handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    opcua_uint32_t subscription_id;
    mu_binary_read_uint32(&r, &subscription_id);
    TEST_ASSERT_NOT_EQUAL(0, subscription_id);
    return subscription_id;
}

static void write_monitored_item_create_request(mu_binary_writer_t *w, const mu_nodeid_t *node_id,
                                                opcua_uint32_t client_handle) {
    mu_string_t null_str = {-1, NULL};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};

    mu_binary_write_nodeid(w, node_id);   /* itemToMonitor.nodeId */
    mu_binary_write_uint32(w, 13);        /* attributeId = Value */
    mu_binary_write_string(w, &null_str); /* indexRange */
    mu_binary_write_uint16(w, 0);         /* dataEncoding.namespaceIndex */
    mu_binary_write_string(w, &null_str); /* dataEncoding.name */
    mu_binary_write_uint32(w, 2);         /* monitoringMode = Reporting */
    mu_binary_write_uint32(w, client_handle);
    mu_binary_write_double(w, 500.0); /* requested samplingInterval */
    mu_binary_write_extension_object_header(w, &null_id, 0);
    mu_binary_write_uint32(w, 1);     /* queueSize */
    mu_binary_write_boolean(w, true); /* discardOldest */
}

static opcua_statuscode_t read_monitored_item_create_result(mu_binary_reader_t *r, opcua_uint32_t *id) {
    opcua_statuscode_t status;
    opcua_double_t revised_sampling_interval;
    opcua_uint32_t revised_queue_size;
    mu_nodeid_t filter_type;
    size_t filter_length;

    mu_binary_read_statuscode(r, &status);
    mu_binary_read_uint32(r, id);
    mu_binary_read_double(r, &revised_sampling_interval);
    mu_binary_read_uint32(r, &revised_queue_size);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(r, &filter_type, &filter_length));
    TEST_ASSERT_EQUAL_size_t(0, filter_length);
    (void)revised_sampling_interval;
    (void)revised_queue_size;
    return status;
}

static mu_monitored_item_t *find_monitored_item_by_id(mu_server_t *server, opcua_uint32_t subscription_id,
                                                      opcua_uint32_t monitored_item_id) {
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (item->in_use && item->subscription_id == subscription_id && item->monitored_item_id == monitored_item_id) {
            return item;
        }
    }
    return NULL;
}

static mu_monitored_item_t *find_monitored_item_by_node(mu_server_t *server, opcua_uint32_t subscription_id,
                                                        const mu_nodeid_t *node_id) {
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server->subs.monitored_items[i];
        if (item->in_use && item->subscription_id == subscription_id && mu_nodeid_equal(&item->node_id, node_id)) {
            return item;
        }
    }
    return NULL;
}

void test_dispatch_create_monitored_item_caches_resolved_node(void) {
    mu_server_t server;
    activated_server(&server);

    opcua_uint32_t subscription_id = create_subscription_via_dispatch(&server);
    mu_nodeid_t known_node_id = {1, MU_NODEID_NUMERIC, {1000}};
    mu_nodeid_t missing_node_id = {1, MU_NODEID_NUMERIC, {9999}};
    const mu_node_t *fresh_node = mu_address_space_find_node(server.config.address_space, &known_node_id);
    TEST_ASSERT_NOT_NULL(fresh_node);
    TEST_ASSERT_NULL(mu_address_space_find_node(server.config.address_space, &missing_node_id));

    opcua_byte_t req[512];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 22);
    mu_binary_write_uint32(&w, subscription_id);
    mu_binary_write_uint32(&w, 3); /* TimestampsToReturn = Neither */
    mu_binary_write_int32(&w, 2);  /* ItemsToCreate */
    write_monitored_item_create_request(&w, &known_node_id, 41);
    write_monitored_item_create_request(&w, &missing_node_id, 42);

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_CREATEMONITOREDITEMSREQUEST, req, w.position,
                                                          resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type));
    TEST_ASSERT_EQUAL(MU_ID_CREATEMONITOREDITEMSRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(22, handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result);

    opcua_int32_t result_count;
    mu_binary_read_int32(&r, &result_count);
    TEST_ASSERT_EQUAL(2, result_count);

    opcua_uint32_t known_item_id;
    opcua_uint32_t missing_item_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, read_monitored_item_create_result(&r, &known_item_id));
    TEST_ASSERT_NOT_EQUAL(0, known_item_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NODEIDUNKNOWN, read_monitored_item_create_result(&r, &missing_item_id));
    TEST_ASSERT_EQUAL(0, missing_item_id);

    mu_monitored_item_t *known_item = find_monitored_item_by_id(&server, subscription_id, known_item_id);
    TEST_ASSERT_NOT_NULL(known_item);
    TEST_ASSERT_NOT_NULL(known_item->resolved_node);
    TEST_ASSERT_EQUAL_PTR(fresh_node, known_item->resolved_node);

    mu_monitored_item_t *missing_item = find_monitored_item_by_node(&server, subscription_id, &missing_node_id);
    TEST_ASSERT_NULL(missing_item);

    size_t items_in_use = 0;
    for (size_t i = 0; i < MU_MAX_MONITORED_ITEMS; ++i) {
        mu_monitored_item_t *item = &server.subs.monitored_items[i];
        if (item->in_use) {
            ++items_in_use;
        } else {
            TEST_ASSERT_NULL(item->resolved_node);
        }
    }
    TEST_ASSERT_EQUAL_size_t(1, items_in_use);
}
#endif

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
        mu_service_dispatch(&server, MU_ID_DELETESUBSCRIPTIONSREQUEST, req, req_len, resp, &resp_len);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS, service_result);
    TEST_ASSERT_EQUAL(response_capacity, resp_len);
    TEST_ASSERT_EQUAL_UINT8(0xA5, resp[0]);
}

#ifdef MICRO_OPCUA_SERVICE_WRITE
static opcua_statuscode_t counting_write_handler(void *handle, const mu_nodeid_t *node_id, opcua_uint32_t attribute_id,
                                                 const mu_variant_t *value) {
    int *count = (int *)handle;
    (void)node_id;
    (void)attribute_id;
    (void)value;
    ++(*count);
    return MU_STATUS_GOOD;
}

void test_dispatch_truncated_write_body_returns_bad_decodingerror_without_callback(void) {
    mu_server_t server;
    activated_server(&server);

    int callback_count = 0;
    server.config.write_handler = counting_write_handler;
    server.config.write_handler_handle = &callback_count;

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 31);
    mu_binary_write_int32(&w, 1); /* nodesToWrite */
    mu_nodeid_t var = {1, MU_NODEID_NUMERIC, {1000}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(&w, &var);
    mu_binary_write_int32(&w, MU_ATTRIBUTEID_VALUE);
    mu_binary_write_string(&w, &null_str);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, w.status);

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);

    /* OPC-10000-4 section 7.38.2: Bad_DecodingError covers invalid stream data;
       a truncated mandatory service body must reject before application callbacks. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            mu_service_dispatch(&server, MU_ID_WRITEREQUEST, req, w.position, resp, &resp_len));
    TEST_ASSERT_EQUAL(0, callback_count);
}
#endif

void test_dispatch_read_value(void) {
    mu_server_t server;
    activated_server(&server);

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 5);
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_double(&w, 0.0); /* MaxAge */
    mu_binary_write_uint32(&w, 3);   /* TimestampsToReturn = NEITHER */
    mu_binary_write_int32(&w, 1);    /* NoOfNodesToRead */
    mu_nodeid_t var = {1, MU_NODEID_NUMERIC, {1000}};
    mu_binary_write_nodeid(&w, &var);      /* nodeId */
    mu_binary_write_uint32(&w, 13);        /* attributeId = Value */
    mu_binary_write_string(&w, &null_str); /* indexRange */
    mu_binary_write_uint16(&w, 0);         /* dataEncoding.namespaceIndex */
    mu_binary_write_string(&w, &null_str); /* dataEncoding.name */
    size_t req_len = w.position;

    opcua_byte_t resp[256];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_READREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_READRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(5, handle);

    opcua_int32_t count;
    mu_binary_read_int32(&r, &count);
    TEST_ASSERT_EQUAL(1, count);
    opcua_byte_t mask;
    mu_binary_read_byte(&r, &mask);
    TEST_ASSERT_EQUAL(0x01, mask); /* has value */
    opcua_byte_t vtype;
    mu_binary_read_byte(&r, &vtype);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, vtype);
    opcua_int32_t val;
    mu_binary_read_int32(&r, &val);
    TEST_ASSERT_EQUAL(42, val);
}

void test_dispatch_browse(void) {
    mu_server_t server;
    activated_server(&server);

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 9);
    mu_nodeid_t empty = {0, MU_NODEID_NUMERIC, {0}};
    mu_binary_write_nodeid(&w, &empty); /* ViewDescription.viewId */
    mu_binary_write_int64(&w, 0);       /* timestamp */
    mu_binary_write_uint32(&w, 0);      /* viewVersion */
    mu_binary_write_uint32(&w, 0);      /* RequestedMaxReferencesPerNode */
    mu_binary_write_int32(&w, 1);       /* NoOfNodesToBrowse */
    mu_nodeid_t objects = {0, MU_NODEID_NUMERIC, {85}};
    mu_binary_write_nodeid(&w, &objects); /* nodeId */
    mu_binary_write_uint32(&w, 0);        /* browseDirection = FORWARD */
    mu_binary_write_nodeid(&w, &empty);   /* referenceTypeId (any) */
    mu_binary_write_boolean(&w, false);   /* includeSubtypes */
    mu_binary_write_uint32(&w, 0);        /* nodeClassMask */
    mu_binary_write_uint32(&w, 0x3F);     /* resultMask */
    size_t req_len = w.position;

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_BROWSEREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_BROWSERESPONSE, type.identifier.numeric);
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(9, handle);

    opcua_int32_t n_results;
    mu_binary_read_int32(&r, &n_results);
    TEST_ASSERT_EQUAL(1, n_results);
    opcua_statuscode_t op_status;
    mu_binary_read_statuscode(&r, &op_status);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, op_status);
    opcua_int32_t cp_len;
    mu_binary_read_int32(&r, &cp_len); /* continuationPoint (null) */
    opcua_int32_t n_refs;
    mu_binary_read_int32(&r, &n_refs);
    TEST_ASSERT_EQUAL(1, n_refs); /* Objects -> MyVar1 */
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
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_string(&w, &null_str); /* endpointUrl */
    mu_binary_write_int32(&w, 0);          /* localeIds[] */
    mu_binary_write_int32(&w, 0);          /* profileUris[] */
    size_t req_len = w.position;

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL(3, handle);
    opcua_int32_t n_ep;
    mu_binary_read_int32(&r, &n_ep);
    TEST_ASSERT_EQUAL(1, n_ep);
    mu_string_t url;
    mu_binary_read_string(&r, &url);
    TEST_ASSERT_EQUAL_MEMORY("opc.tcp://localhost:4840", url.data, 24);
}

void test_dispatch_find_servers(void) {
    mu_server_t server;
    discovery_server(&server);

    opcua_byte_t req[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, req, sizeof(req));
    write_request_header(&w, 4);
    {
        mu_string_t null_str = {-1, NULL};
        mu_binary_write_string(&w, &null_str); /* endpointUrl */
        mu_binary_write_int32(&w, 0);          /* localeIds[] */
        mu_binary_write_int32(&w, 0);          /* serverUris[] */
    }
    size_t req_len = w.position;

    opcua_byte_t resp[512];
    size_t resp_len = sizeof(resp);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_service_dispatch(&server, MU_ID_FINDSERVERSREQUEST, req, req_len, resp, &resp_len));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, resp, resp_len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_FINDSERVERSRESPONSE, type.identifier.numeric);
    opcua_uint32_t handle;
    opcua_statuscode_t result;
    skip_response_header(&r, &handle, &result);
    opcua_int32_t n_srv;
    mu_binary_read_int32(&r, &n_srv);
    TEST_ASSERT_EQUAL(1, n_srv);
    mu_string_t app_uri;
    mu_binary_read_string(&r, &app_uri);
    TEST_ASSERT_EQUAL_MEMORY("urn:test:app", app_uri.data, 12);
}

void test_service_fault_encode(void) {
    opcua_byte_t buf[64];
    size_t len = sizeof(buf);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_write_service_fault(buf, &len, 0, MU_STATUS_BAD_SERVICEUNSUPPORTED));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    TEST_ASSERT_EQUAL(MU_ID_SERVICEFAULT, type.identifier.numeric);
    opcua_int64_t ts;
    opcua_uint32_t h;
    opcua_statuscode_t res;
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &h);
    mu_binary_read_statuscode(&r, &res);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED, res);
}

void test_dispatch_unsupported_service_returns_bad_serviceunsupported(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;

    opcua_byte_t req[1] = {0};
    opcua_byte_t resp[64];
    size_t resp_len = sizeof(resp);
    enum { unsupported_request_id = 437u }; /* RegisterServerRequest_Encoding_DefaultBinary */

    /* OPC-10000-4 sections 6.3.1 and 7.38.2: a Server that does not support
       the requested Service returns Bad_ServiceUnsupported. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SERVICEUNSUPPORTED,
                            mu_service_dispatch(&server, unsupported_request_id, req, sizeof(req), resp, &resp_len));
}

void test_dispatch_transfer_subscriptions_returns_bad_serviceunsupported(void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;

    opcua_byte_t req[1] = {0};
    opcua_byte_t resp[64];
    size_t resp_len = sizeof(resp);
    enum { transfer_subscriptions_request_id = 841u }; /* TransferSubscriptionsRequest_Encoding_DefaultBinary */

    /* OPC-10000-4 sections 5.14.7 and 7.38.2: unsupported
       TransferSubscriptions requests return Bad_ServiceUnsupported. */
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_SERVICEUNSUPPORTED,
        mu_service_dispatch(&server, transfer_subscriptions_request_id, req, sizeof(req), resp, &resp_len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_service_fault_encode);
    RUN_TEST(test_dispatch_unsupported_service_returns_bad_serviceunsupported);
    RUN_TEST(test_dispatch_transfer_subscriptions_returns_bad_serviceunsupported);
    RUN_TEST(test_dispatch_open_secure_channel);
    RUN_TEST(test_dispatch_create_session);
    RUN_TEST(test_dispatch_create_session_honors_timeout);
    RUN_TEST(test_dispatch_truncated_create_session_body_returns_bad_decodingerror_without_session_mutation);
    RUN_TEST(test_dispatch_activate_session);
    RUN_TEST(test_dispatch_close_session);
    RUN_TEST(test_dispatch_delete_subscriptions_rejects_too_many_operations_before_results);
#if MICRO_OPCUA_SUBSCRIPTIONS
    RUN_TEST(test_dispatch_create_monitored_item_caches_resolved_node);
#endif
#ifdef MICRO_OPCUA_SERVICE_WRITE
    RUN_TEST(test_dispatch_truncated_write_body_returns_bad_decodingerror_without_callback);
#endif
    RUN_TEST(test_dispatch_read_value);
    RUN_TEST(test_dispatch_browse);
    RUN_TEST(test_dispatch_get_endpoints);
    RUN_TEST(test_dispatch_find_servers);
    return UNITY_END();
}
