/* tests/unit/test_transfer_subscriptions.c
 *
 * T032 [P] [US5]: Behavioral tests for TransferSubscriptions handler.
 * Valid transfer, 16-item cap overflow, negative count -> Bad_DecodingError,
 * response encoding.
 *
 * Grounding:
 *   OPC-10000-4 §5.14.7 TransferSubscriptions
 */

#include "unity.h"

#include "../../src/address_space/base_nodes.h"
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/core/service_dispatch/common.h"
#include "../../src/services/subscription.h"
#include "muc_opcua/muc_opcua.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_REDUNDANCY

static opcua_statuscode_t test_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer != NULL) {
        (void)memset(buffer, 0x5A, length);
    }
    return MU_STATUS_GOOD;
}

static opcua_uint64_t test_tick_ms(void *context) {
    (void)context;
    return 1000u;
}

static opcua_datetime_t test_time(void *context) {
    (void)context;
    return 0;
}

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t handle) {
    mu_nodeid_t auth_token_nodeid = {0u, MU_NODEID_NUMERIC, {12345u}};
    mu_nodeid_t null_id = {0u, MU_NODEID_NUMERIC, {0u}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(w, &auth_token_nodeid);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &null_string);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &null_id, 0u);
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

static void prepare_server(mu_server_t *server) {
    opcua_uint64_t revised_timeout = 0u;
    opcua_uint32_t session_id = 0u;
    opcua_uint32_t auth_token = 0u;
    opcua_uint32_t sc_lifetime = 0u;

    (void)memset(server, 0, sizeof(*server));
    server->config.entropy_adapter.generate_random = test_entropy;
    server->config.time_adapter.get_tick_ms = test_tick_ms;
    server->config.time_adapter.get_time = test_time;

    mu_secure_channel_init(&server->secure_channel);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_secure_channel_open(&server->secure_channel, NULL, MU_MESSAGE_SECURITY_MODE_NONE,
                                                   3600000u, &server->config.entropy_adapter, &sc_lifetime));
    for (size_t i = 0u; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_session_create(&server->sessions[0], 0u, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token, 321u));
    server->sessions[0].secure_channel_id = server->secure_channel.channel_id;
    server->active_session = &server->sessions[0];

    mu_subscriptions_init(&server->subs);
}

static opcua_uint32_t create_subscription(mu_server_t *server) {
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t subscription_id = 0u;
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t result = 0u;

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 100u);
    mu_binary_write_double(&w, 500.0);
    mu_binary_write_uint32(&w, 30u);
    mu_binary_write_uint32(&w, 10u);
    mu_binary_write_uint32(&w, 0u);
    mu_binary_write_boolean(&w, true);
    mu_binary_write_byte(&w, 0u);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_CREATESUBSCRIPTIONREQUEST, request,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_CREATESUBSCRIPTIONRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &subscription_id));
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, subscription_id);

    return subscription_id;
}

#if MUC_OPCUA_BASE_NODES
/* FR-3: Server.ServerRedundancy.RedundancySupport (i=3709) reads None(0) — this server
   supports client redundancy (TransferSubscriptions) without being itself redundant. */
void test_redundancy_support_reads_none(void) {
    mu_base_runtime_nodes_t rn;
    (void)memset(&rn, 0, sizeof(rn));
#if MUC_OPCUA_SERVER_DIAGNOSTICS
    mu_diagnostics_summary_t diag;
    (void)memset(&diag, 0, sizeof(diag));
    mu_base_runtime_init(&rn, NULL, 0, &diag);
#else
    mu_base_runtime_init(&rn, NULL, 0);
#endif
    const mu_node_t *node = NULL;
    for (size_t i = 0; i < rn.space.node_count; ++i) {
        if (rn.space.nodes[i].node_id.identifier.numeric == 3709u) {
            node = &rn.space.nodes[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "RedundancySupport (3709) must be resolvable");
    mu_variant_t v;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_value_source_read(node->value, &node->node_id, &v));
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, v.type);
    TEST_ASSERT_EQUAL_INT32(0, v.value.i32); /* RedundancySupport None */
}
#endif

/* ------------------------------------------------------------------ */
/*  Transferring your OWN subscription -> Bad_NothingToDo              */
/*  (SessionChanged() == FALSE, OPC-10000-4 §5.14.7.4)                 */
/* ------------------------------------------------------------------ */

void test_transfer_subscriptions_own_returns_nothing_to_do(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t result_count = 0;
    opcua_uint32_t status_code = 0u;
    opcua_int32_t avail_seq = 0;
    opcua_int32_t diag_count = 0;

    prepare_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 1u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_boolean(&w, true);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, request,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(1u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &result_count));
    TEST_ASSERT_EQUAL_INT32(1, result_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &status_code));
    /* The subscription is already owned by the calling session -> Bad_NothingToDo. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTHINGTODO, status_code);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &avail_seq));
    TEST_ASSERT_EQUAL_INT32(0, avail_seq);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));
    TEST_ASSERT_EQUAL_INT32(0, diag_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

/* ------------------------------------------------------------------ */
/*  Real cross-session transfer (the load-bearing case): a subscription  */
/*  owned by session A moves to session B (same user). Direct handler   */
/*  call to control the active session (OPC-10000-4 §5.14.7, row 23).   */
/* ------------------------------------------------------------------ */

/* Set up a second activated session (index 1) sharing session 0's user identity. */
static void add_same_user_session(mu_server_t *server) {
    opcua_uint64_t revised_timeout = 0u;
    opcua_uint32_t session_id = 0u;
    opcua_uint32_t auth_token = 0u;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_session_create(&server->sessions[1], 0u, &revised_timeout, &session_id, &auth_token));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_session_activate(&server->sessions[1], auth_token, 321u));
    server->sessions[1].secure_channel_id = server->secure_channel.channel_id;
    /* The constant-entropy test harness collides session_ids; force a distinct id so
       SessionChanged() is TRUE (this is a genuinely different Session). */
    server->sessions[1].session_id = server->sessions[0].session_id + 1000u;
    /* Both anonymous (kind 321, empty discriminator) -> same user. */
    server->sessions[1].user_identity_kind = server->sessions[0].user_identity_kind;
    server->sessions[1].user_identity_len = server->sessions[0].user_identity_len;
}

void test_transfer_cross_session_succeeds(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    opcua_uint32_t status_code = 0u;
    opcua_int32_t result_count = 0, avail_seq = 0, diag_count = 0;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;

    prepare_server(&server);
    server.sessions[0].user_identity_kind = 321u;         /* anonymous */
    opcua_uint32_t sub_id = create_subscription(&server); /* owned by session 0 */
    add_same_user_session(&server);

    /* Session B (index 1) is now the caller. */
    server.active_session = &server.sessions[1];

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 7u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_boolean(&w, false);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    mu_binary_reader_t req_r;
    mu_binary_reader_init(&req_r, request, w.position);
    mu_binary_writer_t resp_w;
    mu_binary_writer_init(&resp_w, response, sizeof(response));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, handle_transfer_subscriptions(&server, &req_r, &resp_w, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &result_count));
    TEST_ASSERT_EQUAL_INT32(1, result_count);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &status_code));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status_code); /* transferred */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &avail_seq));
    TEST_ASSERT_EQUAL_INT32(0, avail_seq); /* no retransmit yet */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));

    /* The subscription is now owned by session B. */
    mu_subscription_t *moved = mu_subscription_find_any(&server.subs, sub_id);
    TEST_ASSERT_NOT_NULL(moved);
    TEST_ASSERT_EQUAL_UINT32(server.sessions[1].session_id, moved->session_id);
}

/* A different-user session cannot steal the subscription -> Bad_UserAccessDenied. */
void test_transfer_different_user_denied(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w, resp_w;
    mu_binary_reader_t r, req_r;
    opcua_uint32_t status_code = 0u;
    opcua_int32_t result_count = 0;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;

    prepare_server(&server);
    server.sessions[0].user_identity_kind = 324u; /* username "alice" */
    server.sessions[0].user_identity_len = 5u;
    (void)memcpy(server.sessions[0].user_identity, "alice", 5u);
    opcua_uint32_t sub_id = create_subscription(&server);
    add_same_user_session(&server);
    /* Session B is a DIFFERENT user ("bob"). */
    server.sessions[1].user_identity_kind = 324u;
    server.sessions[1].user_identity_len = 3u;
    (void)memcpy(server.sessions[1].user_identity, "bob", 3u);
    server.active_session = &server.sessions[1];

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 8u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_boolean(&w, false);

    mu_binary_reader_init(&req_r, request, w.position);
    mu_binary_writer_init(&resp_w, response, sizeof(response));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, handle_transfer_subscriptions(&server, &req_r, &resp_w, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &result_count));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &status_code));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_USERACCESSDENIED, status_code);

    /* Ownership unchanged (still session A). */
    mu_subscription_t *sub = mu_subscription_find_any(&server.subs, sub_id);
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_EQUAL_UINT32(server.sessions[0].session_id, sub->session_id);
}

/* ------------------------------------------------------------------ */
/*  Valid Transfer: unknown subscription -> Bad_SubscriptionIdInvalid */
/* ------------------------------------------------------------------ */

void test_transfer_subscriptions_unknown_id_returns_bad_id(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t result_count = 0;
    opcua_uint32_t status_code = 0u;
    opcua_int32_t avail_seq = 0;
    opcua_int32_t diag_count = 0;
    const opcua_uint32_t unknown_id = 0xF00Du;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 2u);
    mu_binary_write_int32(&w, 1);
    mu_binary_write_uint32(&w, unknown_id);
    mu_binary_write_boolean(&w, true);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, request,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &result_count));
    TEST_ASSERT_EQUAL_INT32(1, result_count);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &status_code));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID, status_code);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &avail_seq));
    TEST_ASSERT_EQUAL_INT32(0, avail_seq);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));
    TEST_ASSERT_EQUAL_INT32(0, diag_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

/* ------------------------------------------------------------------ */
/*  16-item cap overflow                                               */
/* ------------------------------------------------------------------ */

void test_transfer_subscriptions_16_item_cap_overflow(void) {
    mu_server_t server;
    opcua_byte_t request[1024];
    opcua_byte_t response[1024];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t result_count = 0;
    opcua_uint32_t status_code = 0u;
    opcua_int32_t avail_seq = 0;
    opcua_int32_t diag_count = 0;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 3u);
    mu_binary_write_int32(&w, 20);
    for (opcua_int32_t i = 0; i < 20; ++i) {
        mu_binary_write_uint32(&w, (opcua_uint32_t)(0x100u + i));
    }
    mu_binary_write_boolean(&w, true);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, request,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    /* Response should contain at most 16 results (the cap). */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &result_count));
    TEST_ASSERT_EQUAL_INT32(16, result_count);

    for (opcua_int32_t i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &status_code));
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID, status_code);
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &avail_seq));
        TEST_ASSERT_EQUAL_INT32(0, avail_seq);
    }
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));
    TEST_ASSERT_EQUAL_INT32(0, diag_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

/* ------------------------------------------------------------------ */
/*  Response encoding: verify each field                               */
/* ------------------------------------------------------------------ */

void test_transfer_subscriptions_mixed_ids_response_encoding(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t result_count = 0;
    opcua_uint32_t status_code = 0u;
    opcua_int32_t avail_seq = 0;
    opcua_int32_t diag_count = 0;
    const opcua_uint32_t unknown_id = 0xDEADu;

    prepare_server(&server);
    opcua_uint32_t sub_id = create_subscription(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 4u);
    mu_binary_write_int32(&w, 2);
    mu_binary_write_uint32(&w, sub_id);
    mu_binary_write_uint32(&w, unknown_id);
    mu_binary_write_boolean(&w, true);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, request,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(4u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &result_count));
    TEST_ASSERT_EQUAL_INT32(2, result_count);

    /* First: own subscription (same session) -> Bad_NothingToDo */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &status_code));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTHINGTODO, status_code);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &avail_seq));
    TEST_ASSERT_EQUAL_INT32(0, avail_seq);

    /* Second: unknown subscription -> Bad_SubscriptionIdInvalid */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &status_code));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SUBSCRIPTIONIDINVALID, status_code);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &avail_seq));
    TEST_ASSERT_EQUAL_INT32(0, avail_seq);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));
    TEST_ASSERT_EQUAL_INT32(0, diag_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

/* ------------------------------------------------------------------ */
/*  Zero count: transfers nothing, returns empty results               */
/* ------------------------------------------------------------------ */

void test_transfer_subscriptions_zero_count(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;
    opcua_int32_t result_count = 0;
    opcua_int32_t diag_count = 0;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 5u);
    mu_binary_write_int32(&w, 0);
    mu_binary_write_boolean(&w, true);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, request,
                                                                w.position, response, &response_len));

    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    /* Empty subscriptionIds -> service-level Bad_NothingToDo (§5.14.7.3). */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTHINGTODO, service_result);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &result_count));
    TEST_ASSERT_EQUAL_INT32(0, result_count);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));
    TEST_ASSERT_EQUAL_INT32(0, diag_count);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

/* ------------------------------------------------------------------ */
/*  Negative count -> Bad_DecodingError                                */
/* ------------------------------------------------------------------ */

void test_transfer_subscriptions_negative_count_returns_bad_decoding_error(void) {
    mu_server_t server;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t response_len = sizeof(response);
    mu_binary_writer_t w;
    mu_binary_reader_t r;
    mu_nodeid_t response_type = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_uint32_t handle = 0u;
    opcua_statuscode_t service_result = 0u;

    prepare_server(&server);

    mu_binary_writer_init(&w, request, sizeof(request));
    write_request_header(&w, 6u);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_boolean(&w, true);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, w.status);

    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_DECODINGERROR,
        mu_service_dispatch(&server, MU_ID_TRANSFERSUBSCRIPTIONSREQUEST, request, w.position, response, &response_len));

    /* Verify an error response is still a valid TransferSubscriptionsResponse
     * with the failing status code. */
    mu_binary_reader_init(&r, response, response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &response_type));
    TEST_ASSERT_EQUAL_UINT32(MU_ID_TRANSFERSUBSCRIPTIONSRESPONSE, response_type.identifier.numeric);
    read_response_header(&r, &handle, &service_result);
    TEST_ASSERT_EQUAL_UINT32(6u, handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, service_result);
    TEST_ASSERT_EQUAL_size_t(response_len, r.position);
}

#else

void test_transfer_subscriptions_tests_require_redundancy_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_REDUNDANCY is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_REDUNDANCY
#if MUC_OPCUA_BASE_NODES
    RUN_TEST(test_redundancy_support_reads_none);
#endif
    RUN_TEST(test_transfer_subscriptions_own_returns_nothing_to_do);
    RUN_TEST(test_transfer_cross_session_succeeds);
    RUN_TEST(test_transfer_different_user_denied);
    RUN_TEST(test_transfer_subscriptions_unknown_id_returns_bad_id);
    RUN_TEST(test_transfer_subscriptions_16_item_cap_overflow);
    RUN_TEST(test_transfer_subscriptions_mixed_ids_response_encoding);
    RUN_TEST(test_transfer_subscriptions_zero_count);
    RUN_TEST(test_transfer_subscriptions_negative_count_returns_bad_decoding_error);
#else
    RUN_TEST(test_transfer_subscriptions_tests_require_redundancy_build);
#endif
    return UNITY_END();
}
