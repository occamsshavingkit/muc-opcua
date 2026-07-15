/* tests/unit/test_session.c */
#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/session.h"

/* Wire SessionTimeout is a Duration (Double); the API works on its raw bits. */
static opcua_uint64_t bits(double d) {
    opcua_uint64_t b;
    (void)memcpy(&b, &d, sizeof(b));
    return b;
}

static opcua_datetime_t fake_time(void *context) {
    (void)context;
    return 0;
}

static opcua_uint64_t fake_tick_ms(void *context) {
    (void)context;
    return 0;
}

static opcua_statuscode_t fake_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    if (buffer != NULL) {
        (void)memset(buffer, 0x42, length);
    }
    return MU_STATUS_GOOD;
}

typedef struct {
    opcua_byte_t next;
} sequential_entropy_t;

static opcua_statuscode_t sequential_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    sequential_entropy_t *entropy = (sequential_entropy_t *)context;
    if (entropy == NULL || buffer == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    for (size_t i = 0; i < length; ++i) {
        buffer[i] = entropy->next++;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t failing_entropy(void *context, opcua_byte_t *buffer, size_t length) {
    (void)context;
    (void)buffer;
    (void)length;
    return MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

static void write_request_header(mu_binary_writer_t *writer, opcua_uint32_t request_handle) {
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {12345}};
    mu_nodeid_t no_header = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(writer, &auth_token);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, request_handle);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_string);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &no_header, 0);
}

static void write_request_header_with_token(mu_binary_writer_t *writer, opcua_uint32_t auth_token_id,
                                            opcua_uint32_t request_handle) {
    mu_nodeid_t auth_token = {0, MU_NODEID_NUMERIC, {auth_token_id}};
    mu_nodeid_t no_header = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(writer, &auth_token);
    mu_binary_write_int64(writer, 0);
    mu_binary_write_uint32(writer, request_handle);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_string(writer, &null_string);
    mu_binary_write_uint32(writer, 0);
    mu_binary_write_extension_object_header(writer, &no_header, 0);
}

static size_t build_activate_anonymous_body(opcua_byte_t *buffer, size_t capacity, opcua_uint32_t auth_token,
                                            opcua_uint32_t request_handle, const mu_string_t *policy_id) {
    opcua_byte_t token_body[64];
    mu_binary_writer_t token_writer;
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};
    mu_nodeid_t anonymous_token = {0, MU_NODEID_NUMERIC, {MU_ID_ANONYMOUSIDENTITYTOKEN_ENCODING_DEFAULTBINARY}};

    mu_binary_writer_init(&token_writer, token_body, sizeof(token_body));
    mu_binary_write_string(&token_writer, policy_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, token_writer.status);

    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header_with_token(&writer, auth_token, request_handle);
    mu_binary_write_string(&writer, &null_string);    /* ClientSignature.algorithm */
    mu_binary_write_bytestring(&writer, &null_bytes); /* ClientSignature.signature */
    mu_binary_write_int32(&writer, 0);                /* ClientSoftwareCertificates[] */
    mu_binary_write_int32(&writer, 0);                /* LocaleIds[] */
    mu_binary_write_extension_object_header(&writer, &anonymous_token, token_writer.position);
    TEST_ASSERT_TRUE(writer.position + token_writer.position <= capacity);
    (void)memcpy(buffer + writer.position, token_body, token_writer.position);
    writer.position += token_writer.position;
    mu_binary_write_string(&writer, &null_string);    /* UserTokenSignature.algorithm */
    mu_binary_write_bytestring(&writer, &null_bytes); /* UserTokenSignature.signature */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    return writer.position;
}

static size_t build_create_session_body(opcua_byte_t *buffer, size_t capacity, opcua_double_t requested_timeout) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};

    mu_binary_writer_init(&writer, buffer, capacity);
    write_request_header(&writer, 25);

    mu_binary_write_string(&writer, &null_string);    /* ClientDescription.applicationUri */
    mu_binary_write_string(&writer, &null_string);    /* productUri */
    mu_binary_write_byte(&writer, 0x00);              /* applicationName */
    mu_binary_write_uint32(&writer, 1);               /* applicationType = CLIENT */
    mu_binary_write_string(&writer, &null_string);    /* gatewayServerUri */
    mu_binary_write_string(&writer, &null_string);    /* discoveryProfileUri */
    mu_binary_write_int32(&writer, 0);                /* discoveryUrls[] */
    mu_binary_write_string(&writer, &null_string);    /* serverUri */
    mu_binary_write_string(&writer, &null_string);    /* endpointUrl */
    mu_binary_write_string(&writer, &null_string);    /* sessionName */
    mu_binary_write_bytestring(&writer, &null_bytes); /* clientNonce */
    mu_binary_write_bytestring(&writer, &null_bytes); /* clientCertificate */
    mu_binary_write_double(&writer, requested_timeout);
    mu_binary_write_uint32(&writer, 0); /* maxResponseMessageSize */

    return writer.position;
}

static void assert_sessions_unchanged(const mu_session_t *expected, const mu_session_t *actual) {
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        TEST_ASSERT_EQUAL(expected[i].state, actual[i].state);
        TEST_ASSERT_EQUAL(expected[i].session_id, actual[i].session_id);
        TEST_ASSERT_EQUAL(expected[i].auth_token, actual[i].auth_token);
        TEST_ASSERT_EQUAL(expected[i].revised_session_timeout_ms, actual[i].revised_session_timeout_ms);
        TEST_ASSERT_EQUAL_MEMORY(expected[i].server_nonce, actual[i].server_nonce, sizeof(expected[i].server_nonce));
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
        TEST_ASSERT_EQUAL(expected[i].secure_channel_id, actual[i].secure_channel_id);
#endif
    }
}

static void skip_response_header(mu_binary_reader_t *reader, opcua_uint32_t *request_handle,
                                 opcua_statuscode_t *service_result) {
    opcua_int64_t timestamp;
    opcua_byte_t diagnostics_encoding;
    opcua_int32_t string_table_length;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(reader, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(reader, request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &diagnostics_encoding));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(reader, &string_table_length));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(reader, &additional_header_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &additional_header_encoding));
}

static void dispatch_create_session_and_read_ids(mu_server_t *server, mu_nodeid_t *session_id,
                                                 mu_nodeid_t *authentication_token) {
    opcua_byte_t request[256];
    size_t request_length = build_create_session_body(request, sizeof(request), 1200000.0);

    opcua_byte_t response[1024];
    size_t response_length = sizeof(response);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_CREATESESSIONREQUEST, request, request_length,
                                                          response, &response_length));

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, response, response_length);

    mu_nodeid_t response_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, response_type.identifier.numeric);

    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL(25, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, session_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, authentication_token));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, session_id->identifier_type);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, authentication_token->identifier_type);
}

static opcua_statuscode_t auth_reject_policy(void *handle, const mu_string_t *username, const mu_bytestring_t *password,
                                             const mu_string_t *policy_id) {
    (void)handle;
    (void)username;
    (void)password;
    static const opcua_byte_t reject_policy[] = "reject";
    if (policy_id != NULL && policy_id->data != NULL && policy_id->length == 6 &&
        memcmp(policy_id->data, reject_policy, 6) == 0) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }
    return MU_STATUS_GOOD;
}

static void init_activate_test_server(mu_server_t *server, sequential_entropy_t *entropy, opcua_uint32_t auth_token) {
    (void)memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    server->config.time_adapter.get_time = fake_time;
    server->config.time_adapter.get_tick_ms = fake_tick_ms;
    server->config.entropy_adapter.generate_random = sequential_entropy;
    server->config.entropy_adapter.context = entropy;
    server->config.user_auth_handler = auth_reject_policy;
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server->sessions[i]);
    }

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id;
    opcua_uint32_t created_auth_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_session_create_with_identifiers(&server->sessions[0], bits(60000.0), 1001u, auth_token, 0u,
                                                         &revised_timeout, &session_id, &created_auth_token));
    TEST_ASSERT_EQUAL(auth_token, created_auth_token);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&server->sessions[0], auth_token, 324));
    (void)memset(server->sessions[0].server_nonce, 0xA5, sizeof(server->sessions[0].server_nonce));
#if MUC_OPCUA_CU_REDUNDANCY
    server->sessions[0].user_identity_kind = 2u;
    server->sessions[0].user_identity_len = 5u;
    (void)memcpy(server->sessions[0].user_identity, "admin", 5u);
#endif
}

static void dispatch_activate_anonymous_and_read_result(mu_server_t *server, opcua_uint32_t auth_token,
                                                        opcua_uint32_t request_handle, const mu_string_t *policy_id,
                                                        opcua_statuscode_t *service_result,
                                                        opcua_byte_t *response_nonce,
                                                        opcua_int32_t *response_nonce_length) {
    opcua_byte_t request[256];
    size_t request_length =
        build_activate_anonymous_body(request, sizeof(request), auth_token, request_handle, policy_id);
    opcua_byte_t response[512];
    size_t response_length = sizeof(response);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_service_dispatch(server, MU_ID_ACTIVATESESSIONREQUEST, request, request_length,
                                                          response, &response_length));

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, response, response_length);

    mu_nodeid_t response_type;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, response_type.identifier.numeric);

    opcua_uint32_t actual_request_handle;
    skip_response_header(&reader, &actual_request_handle, service_result);
    TEST_ASSERT_EQUAL(request_handle, actual_request_handle);

    mu_bytestring_t nonce;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&reader, &nonce));
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, nonce.length);
    *response_nonce_length = nonce.length;
    if (nonce.length > 0) {
        TEST_ASSERT_LESS_OR_EQUAL_INT32((opcua_int32_t)sizeof(server->sessions[0].server_nonce), nonce.length);
        (void)memcpy(response_nonce, nonce.data, (size_t)nonce.length);
    }
}

void test_session_create(void) {
    mu_session_t session;
    mu_session_init(&session);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, session.state);

    opcua_uint64_t revised;
    opcua_uint32_t session_id, auth_token;

    /* Below the 10000 ms minimum -> clamped up. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_create(&session, bits(5000.0), &revised, &session_id, &auth_token));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, session.state);
    TEST_ASSERT_EQUAL_HEX64(bits(10000.0), revised);
    TEST_ASSERT_EQUAL_UINT32(10000u, session.revised_session_timeout_ms);
    TEST_ASSERT_EQUAL(1, session_id);

    /* In range -> unchanged. */
    mu_session_init(&session);
    mu_session_create(&session, bits(60000.0), &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(60000.0), revised);
    TEST_ASSERT_EQUAL_UINT32(60000u, session.revised_session_timeout_ms);

    /* Fractional in-range values remain on the wire and truncate for local timers. */
    mu_session_init(&session);
    mu_session_create(&session, bits(60000.75), &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(60000.75), revised);
    TEST_ASSERT_EQUAL_UINT32(60000u, session.revised_session_timeout_ms);

    /* Above the 3600000 ms maximum -> clamped down. */
    mu_session_init(&session);
    mu_session_create(&session, bits(9999999.0), &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(3600000.0), revised);
    TEST_ASSERT_EQUAL_UINT32(3600000u, session.revised_session_timeout_ms);

    /* Negative / zero -> minimum. */
    mu_session_init(&session);
    mu_session_create(&session, bits(-1.0), &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(10000.0), revised);
    TEST_ASSERT_EQUAL_UINT32(10000u, session.revised_session_timeout_ms);
    mu_session_init(&session);
    mu_session_create(&session, 0, &revised, &session_id, &auth_token);
    TEST_ASSERT_EQUAL_HEX64(bits(10000.0), revised);
    TEST_ASSERT_EQUAL_UINT32(10000u, session.revised_session_timeout_ms);
}

void test_session_activate_anonymous(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);

    /* Unknown identity token */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_IDENTITYTOKENINVALID,
                      mu_session_activate(&session, auth_token, 325)); /* IssuedIdentityToken */

    /* Wrong auth token */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, mu_session_activate(&session, 9999, 321));

    /* Success */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_activate(&session, auth_token, 321)); /* AnonymousIdentityToken */

    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, session.state);
}

void test_session_close(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;

    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, mu_session_close(&session, 0, true));

    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);

    /* Wrong auth token */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SESSIONIDINVALID, mu_session_close(&session, 9999, true));

    /* Success */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_session_close(&session, auth_token, true));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, session.state);
}

/* OPC-10000-4 section 5.7.2.1: mu_session_close_timeout transitions an idle
   Session to CLOSED without requiring the authenticationToken. The session_id
   and auth_token are preserved so dispatch can return Bad_SessionClosed (not
   Bad_SessionIdInvalid) on subsequent use of the stale token. */
#ifdef MUC_OPCUA_MULTI_CHUNK
void test_session_close_timeout(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, bits(5000.0), &revised_timeout, &session_id, &auth_token);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, session.state);

    /* CloseSession zeroed the nonce as hygiene; timeout close must do the same. */
    (void)memset(session.server_nonce, 0xAA, sizeof(session.server_nonce));

    mu_session_close_timeout(&session);

    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, session.state);
    /* auth_token and session_id preserved for Bad_SessionClosed dispatch. */
    TEST_ASSERT_EQUAL(session_id, session.session_id);
    TEST_ASSERT_EQUAL(auth_token, session.auth_token);
    /* Nonce cleared. */
    opcua_byte_t zero_nonce[32];
    (void)memset(zero_nonce, 0, sizeof(zero_nonce));
    TEST_ASSERT_EQUAL_MEMORY(zero_nonce, session.server_nonce, sizeof(zero_nonce));

    /* Closing an already-closed session is a no-op (no crash). */
    mu_session_close_timeout(&session);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CLOSED, session.state);

    /* NULL session is a safe no-op. */
    mu_session_close_timeout(NULL);
}

/* A timed-out Session that has been transitioned to CLOSED (but whose slot has
   not yet been reused) must be found by mu_session_find_closed_by_token so that
   dispatch returns Bad_SessionClosed — not Bad_SessionIdInvalid. */
void test_session_timeout_rejects_request_on_closed_slot(void) {
    mu_session_t sessions[2];
    for (size_t i = 0; i < 2; ++i) {
        mu_session_init(&sessions[i]);
    }

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&sessions[0], bits(5000.0), &revised_timeout, &session_id, &auth_token);

    mu_session_close_timeout(&sessions[0]);

    /* OPC-10000-4 section 5.7.4.1: a request on a closed session's token is
       Bad_SessionClosed, distinguished from an unknown token. */
    mu_session_t *found = mu_session_find_closed_by_token(sessions, 2, auth_token);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_PTR(&sessions[0], found);
}
#endif /* MUC_OPCUA_MULTI_CHUNK */

void test_create_session_truncated_body_returns_bad_decodingerror_without_allocating_session(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server.sessions[i]);
    }

    mu_session_t sessions_before[MU_INTERN_MAX_SESSIONS];
    (void)memcpy(sessions_before, server.sessions, sizeof(sessions_before));
    mu_session_t *active_session_before = server.active_session;

    opcua_byte_t request[256];
    size_t request_length = build_create_session_body(request, sizeof(request), 1200000.0);
    TEST_ASSERT_TRUE(request_length > 1u);

    opcua_byte_t response[1024];
    size_t response_length = sizeof(response);

    /* OPC-10000-4 section 5.7.2: truncated CreateSession parameters fail before allocation. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, request, request_length - 1u,
                                                response, &response_length));
    TEST_ASSERT_EQUAL_PTR(active_session_before, server.active_session);
    assert_sessions_unchanged(sessions_before, server.sessions);
}

void test_create_session_two_successful_responses_use_fresh_session_ids_and_tokens(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server.sessions[i]);
    }

    TEST_ASSERT_GREATER_OR_EQUAL_UINT(2u, MU_INTERN_MAX_SESSIONS);

    mu_nodeid_t first_session_id;
    mu_nodeid_t first_authentication_token;
    mu_nodeid_t second_session_id;
    mu_nodeid_t second_authentication_token;

    dispatch_create_session_and_read_ids(&server, &first_session_id, &first_authentication_token);
    dispatch_create_session_and_read_ids(&server, &second_session_id, &second_authentication_token);

    /* OPC-10000-4 section 5.7.2.2 defines the CreateSession sessionId and
       authenticationToken response parameters; each successful Session creation
       must return identifiers that do not reuse an existing Session's values. */
    TEST_ASSERT_NOT_EQUAL(first_session_id.identifier.numeric, second_session_id.identifier.numeric);
    TEST_ASSERT_NOT_EQUAL(first_authentication_token.identifier.numeric,
                          second_authentication_token.identifier.numeric);
}

void test_create_session_overflow_returns_bad_toomanysessions_without_allocating(void) {
    mu_server_t server;
    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = fake_entropy;
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server.sessions[i]);
    }

    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_nodeid_t session_id;
        mu_nodeid_t authentication_token;
        dispatch_create_session_and_read_ids(&server, &session_id, &authentication_token);
    }

    mu_session_t sessions_before[MU_INTERN_MAX_SESSIONS];
    (void)memcpy(sessions_before, server.sessions, sizeof(sessions_before));
    mu_session_t *active_session_before = server.active_session;

    opcua_byte_t request[256];
    size_t request_length = build_create_session_body(request, sizeof(request), 1200000.0);
    opcua_byte_t response[1024];
    size_t response_length = sizeof(response);

    /* OPC-10000-4 sections 5.7.2 and 7.38.2: once all compiled Session slots
       are in use, CreateSession fails with Bad_TooManySessions. */
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_TOOMANYSESSIONS,
        mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, request, request_length, response, &response_length));
    TEST_ASSERT_EQUAL_PTR(active_session_before, server.active_session);
    assert_sessions_unchanged(sessions_before, server.sessions);
}

void test_create_session_server_nonce_entropy_failure_returns_bad_securitychecksfailed_without_allocating_session(
    void) {
    mu_server_t server;
    memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.time_adapter.get_time = fake_time;
    server.config.entropy_adapter.generate_random = failing_entropy;
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server.sessions[i]);
    }

    mu_session_t sessions_before[MU_INTERN_MAX_SESSIONS];
    (void)memcpy(sessions_before, server.sessions, sizeof(sessions_before));
    mu_session_t *active_session_before = server.active_session;

    opcua_byte_t request[256];
    size_t request_length = build_create_session_body(request, sizeof(request), 1200000.0);

    opcua_byte_t response[1024];
    size_t response_length = sizeof(response);

    /* OPC-10000-4 sections 5.7.2.3 and 7.38.2: CreateSession ServerNonce
       freshness is security-sensitive; entropy failure must fail closed. */
    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_SECURITYCHECKSFAILED,
        mu_service_dispatch(&server, MU_ID_CREATESESSIONREQUEST, request, request_length, response, &response_length));
    TEST_ASSERT_EQUAL_PTR(active_session_before, server.active_session);
    assert_sessions_unchanged(sessions_before, server.sessions);
}

#ifdef MUC_OPCUA_CU_SESSION_CHANGE_USER
void test_activate_session_reactivation_accepts_replacement_identity_and_refreshes_nonce(void) {
    mu_server_t server;
    sequential_entropy_t entropy = {0x10u};
    enum { AUTH_TOKEN = 0x44332211u };
    init_activate_test_server(&server, &entropy, AUTH_TOKEN);

    opcua_uint32_t original_session_id = server.sessions[0].session_id;
    opcua_uint32_t original_auth_token = server.sessions[0].auth_token;
    opcua_byte_t original_nonce[sizeof(server.sessions[0].server_nonce)];
    (void)memcpy(original_nonce, server.sessions[0].server_nonce, sizeof(original_nonce));
    mu_string_t policy_id = {7, (const opcua_byte_t *)"anon-ok"};
    opcua_statuscode_t service_result;
    opcua_byte_t response_nonce[sizeof(server.sessions[0].server_nonce)];
    opcua_int32_t response_nonce_length;

    /* SCN-002 / CASE-004 / opc_cu_2400: OPC-10000-4 sections 5.7.2 and 5.7.3
       permit an already activated Session to be activated again with a new
       valid user identity; the Session identifiers stay stable and the server
       nonce is refreshed. */
    dispatch_activate_anonymous_and_read_result(&server, AUTH_TOKEN, 70u, &policy_id, &service_result, response_nonce,
                                                &response_nonce_length);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);
    TEST_ASSERT_EQUAL_UINT32(original_session_id, server.sessions[0].session_id);
    TEST_ASSERT_EQUAL_UINT32(original_auth_token, server.sessions[0].auth_token);
    TEST_ASSERT_EQUAL_INT32((opcua_int32_t)sizeof(server.sessions[0].server_nonce), response_nonce_length);
    TEST_ASSERT_NOT_EQUAL(0, memcmp(original_nonce, response_nonce, sizeof(response_nonce)));
    TEST_ASSERT_EQUAL_MEMORY(response_nonce, server.sessions[0].server_nonce, sizeof(response_nonce));
#if MUC_OPCUA_CU_REDUNDANCY
    TEST_ASSERT_EQUAL_UINT8(1u, server.sessions[0].user_identity_kind);
    TEST_ASSERT_EQUAL_UINT8(0u, server.sessions[0].user_identity_len);
#endif
}
#else
void test_activate_session_without_change_user_cu_allows_initial_activation_but_rejects_reactivation(void) {
    mu_server_t server;
    sequential_entropy_t entropy = {0x20u};
    enum { AUTH_TOKEN = 0x55667788u };
    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.time_adapter.get_time = fake_time;
    server.config.time_adapter.get_tick_ms = fake_tick_ms;
    server.config.entropy_adapter.generate_random = sequential_entropy;
    server.config.entropy_adapter.context = &entropy;
    server.config.user_auth_handler = auth_reject_policy;
    for (size_t i = 0; i < MU_INTERN_MAX_SESSIONS; ++i) {
        mu_session_init(&server.sessions[i]);
    }

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id;
    opcua_uint32_t created_auth_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_session_create_with_identifiers(&server.sessions[0], bits(60000.0), 2002u, AUTH_TOKEN, 0u,
                                                         &revised_timeout, &session_id, &created_auth_token));
    (void)memset(server.sessions[0].server_nonce, 0xA5, sizeof(server.sessions[0].server_nonce));

    mu_string_t first_policy_id = {7, (const opcua_byte_t *)"anon-ok"};
    opcua_statuscode_t service_result;
    opcua_byte_t response_nonce[sizeof(server.sessions[0].server_nonce)];
    opcua_int32_t response_nonce_length;

    /* OPC-10000-4 sections 5.7.2 and 5.7.3: initial ActivateSession is still
       supported when opc_cu_2400 / MUC_OPCUA_CU_SESSION_CHANGE_USER is off. */
    dispatch_activate_anonymous_and_read_result(&server, AUTH_TOKEN, 72u, &first_policy_id, &service_result,
                                                response_nonce, &response_nonce_length);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);
    TEST_ASSERT_EQUAL_UINT32(session_id, server.sessions[0].session_id);
    TEST_ASSERT_EQUAL_UINT32(AUTH_TOKEN, server.sessions[0].auth_token);

    opcua_byte_t activated_nonce[sizeof(server.sessions[0].server_nonce)];
    (void)memcpy(activated_nonce, server.sessions[0].server_nonce, sizeof(activated_nonce));
    mu_string_t replacement_policy_id = {7, (const opcua_byte_t *)"anon-ok"};

    /* SCN-002 / CASE-004 / opc_cu_2400: OPC-10000-4 sections 5.7.2 and 5.7.3
       permit change-user only when the dedicated CU is enabled. */
    dispatch_activate_anonymous_and_read_result(&server, AUTH_TOKEN, 73u, &replacement_policy_id, &service_result,
                                                response_nonce, &response_nonce_length);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SERVICEUNSUPPORTED, service_result);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);
    TEST_ASSERT_EQUAL_UINT32(session_id, server.sessions[0].session_id);
    TEST_ASSERT_EQUAL_UINT32(AUTH_TOKEN, server.sessions[0].auth_token);
    TEST_ASSERT_EQUAL_MEMORY(activated_nonce, server.sessions[0].server_nonce, sizeof(activated_nonce));
}
#endif

#ifdef MUC_OPCUA_CU_SESSION_CHANGE_USER
void test_activate_session_reactivation_rejected_identity_preserves_prior_identity_and_nonce(void) {
    mu_server_t server;
    sequential_entropy_t entropy = {0x40u};
    enum { AUTH_TOKEN = 0x10203040u };
    init_activate_test_server(&server, &entropy, AUTH_TOKEN);

    opcua_byte_t original_nonce[sizeof(server.sessions[0].server_nonce)];
    (void)memcpy(original_nonce, server.sessions[0].server_nonce, sizeof(original_nonce));
#if MUC_OPCUA_CU_REDUNDANCY
    opcua_byte_t original_identity[sizeof(server.sessions[0].user_identity)];
    opcua_byte_t original_identity_kind = server.sessions[0].user_identity_kind;
    opcua_byte_t original_identity_len = server.sessions[0].user_identity_len;
    (void)memcpy(original_identity, server.sessions[0].user_identity, sizeof(original_identity));
#endif
    mu_string_t policy_id = {6, (const opcua_byte_t *)"reject"};
    opcua_statuscode_t service_result;
    opcua_byte_t response_nonce[sizeof(server.sessions[0].server_nonce)];
    opcua_int32_t response_nonce_length;

    /* SCN-002 / CASE-004 / opc_cu_2400: OPC-10000-4 sections 5.7.2 and 5.7.3
       require rejected ActivateSession replacement identities to leave the
       existing activated Session state and accepted identity intact. */
    dispatch_activate_anonymous_and_read_result(&server, AUTH_TOKEN, 71u, &policy_id, &service_result, response_nonce,
                                                &response_nonce_length);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_IDENTITYTOKENREJECTED, service_result);
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server.sessions[0].state);
    TEST_ASSERT_EQUAL_INT32((opcua_int32_t)sizeof(server.sessions[0].server_nonce), response_nonce_length);
    TEST_ASSERT_EQUAL_MEMORY(original_nonce, server.sessions[0].server_nonce, sizeof(original_nonce));
#if MUC_OPCUA_CU_REDUNDANCY
    TEST_ASSERT_EQUAL_UINT8(original_identity_kind, server.sessions[0].user_identity_kind);
    TEST_ASSERT_EQUAL_UINT8(original_identity_len, server.sessions[0].user_identity_len);
    TEST_ASSERT_EQUAL_MEMORY(original_identity, server.sessions[0].user_identity, sizeof(original_identity));
#endif
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_session_create);
    RUN_TEST(test_session_activate_anonymous);
    RUN_TEST(test_session_close);
#ifdef MUC_OPCUA_MULTI_CHUNK
    RUN_TEST(test_session_close_timeout);
    RUN_TEST(test_session_timeout_rejects_request_on_closed_slot);
#endif
    RUN_TEST(test_create_session_truncated_body_returns_bad_decodingerror_without_allocating_session);
    RUN_TEST(test_create_session_two_successful_responses_use_fresh_session_ids_and_tokens);
    RUN_TEST(test_create_session_overflow_returns_bad_toomanysessions_without_allocating);
    RUN_TEST(
        test_create_session_server_nonce_entropy_failure_returns_bad_securitychecksfailed_without_allocating_session);
#ifdef MUC_OPCUA_CU_SESSION_CHANGE_USER
    RUN_TEST(test_activate_session_reactivation_accepts_replacement_identity_and_refreshes_nonce);
    RUN_TEST(test_activate_session_reactivation_rejected_identity_preserves_prior_identity_and_nonce);
#else
    RUN_TEST(test_activate_session_without_change_user_cu_allows_initial_activation_but_rejects_reactivation);
#endif
    return UNITY_END();
}
