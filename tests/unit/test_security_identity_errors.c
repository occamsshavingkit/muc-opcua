/* tests/unit/test_security_identity_errors.c */
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <stdbool.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"
#include "../../src/services/discovery.h"
#include "../../src/services/secure_channel.h"
#include "../../src/services/session.h"

void test_unsupported_security_policy(void) {
    mu_secure_channel_t channel;
    mu_secure_channel_init(&channel);

    opcua_uint32_t revised_lifetime = 0;

    mu_string_t invalid_policy;
    invalid_policy.data = (opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";
    invalid_policy.length = 57;

    TEST_ASSERT_EQUAL(
        MU_STATUS_BAD_SECURITYPOLICYREJECTED,
        mu_secure_channel_open(&channel, &invalid_policy, MU_MESSAGE_SECURITY_MODE_NONE, 1000, &revised_lifetime));
}

void test_unsupported_identity_token(void) {
    mu_session_t session;
    mu_session_init(&session);

    opcua_uint64_t revised_timeout;
    opcua_uint32_t session_id, auth_token;
    mu_session_create(&session, 0, &revised_timeout, &session_id, &auth_token);

    /* 321 is AnonymousIdentityToken. 325 is IssuedIdentityToken, which is unsupported */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_IDENTITYTOKENINVALID, mu_session_activate(&session, auth_token, 325));
}

static void prepare_created_session(mu_server_t *server, opcua_uint32_t *auth_token) {
    opcua_uint64_t revised_timeout = 0;
    opcua_uint32_t session_id = 0;

    (void)memset(server, 0, sizeof(*server));
    server->secure_channel.is_open = true;
    fake_platform_init(NULL, &server->config.time_adapter, &server->config.entropy_adapter);
    mu_session_init(&server->sessions[0]);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_session_create(&server->sessions[0], 0, &revised_timeout, &session_id, auth_token));
}

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t auth_token, opcua_uint32_t request_handle) {
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {auth_token}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_string = {-1, NULL};

    mu_binary_write_nodeid(w, &auth);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, request_handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &null_string);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &null_id, 0);
}

static bool string_equals_cstr(const mu_string_t *s, const char *expected) {
    size_t expected_len = strlen(expected);
    return s->length == (opcua_int32_t)expected_len && s->data != NULL && memcmp(s->data, expected, expected_len) == 0;
}

typedef struct {
    unsigned calls;
} accepting_auth_context_t;

static opcua_statuscode_t accepting_username_auth(void *handle, const mu_string_t *username,
                                                  const mu_bytestring_t *password, const mu_string_t *policy_id) {
    accepting_auth_context_t *context = (accepting_auth_context_t *)handle;
    context->calls++;
    TEST_ASSERT_TRUE(string_equals_cstr(username, "admin"));
    TEST_ASSERT_NOT_NULL(password);
    TEST_ASSERT_EQUAL(5, password->length);
    TEST_ASSERT_EQUAL_MEMORY("admin", password->data, 5);
    TEST_ASSERT_TRUE(string_equals_cstr(policy_id, "username"));
    return MU_STATUS_GOOD;
}

static void skip_response_header(mu_binary_reader_t *reader, opcua_uint32_t *request_handle,
                                 opcua_statuscode_t *service_result) {
    opcua_int64_t timestamp;
    opcua_byte_t diagnostics_mask;
    opcua_int32_t string_table_count;
    mu_nodeid_t additional_header_type;
    opcua_byte_t additional_header_encoding;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(reader, &timestamp));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(reader, request_handle));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(reader, service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &diagnostics_mask));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(reader, &string_table_count));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(reader, &additional_header_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &additional_header_encoding));
}

static void skip_application_description(mu_binary_reader_t *reader) {
    mu_string_t s;
    opcua_byte_t localized_text_mask;
    opcua_uint32_t application_type;
    opcua_int32_t discovery_url_count;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* applicationUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* productUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(reader, &localized_text_mask));
    if ((localized_text_mask & 0x01u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* locale */
    }
    if ((localized_text_mask & 0x02u) != 0u) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* text */
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(reader, &application_type));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* gatewayServerUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s)); /* discoveryProfileUri */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(reader, &discovery_url_count));
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, discovery_url_count);
    for (opcua_int32_t i = 0; i < discovery_url_count; ++i) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(reader, &s));
    }
}

static size_t build_getendpoints_request(opcua_byte_t *request, size_t capacity) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};

    mu_binary_writer_init(&writer, request, capacity);
    write_request_header(&writer, 0, 47);
    mu_binary_write_string(&writer, &null_string); /* endpointUrl */
    mu_binary_write_int32(&writer, 0);             /* localeIds[] */
    mu_binary_write_int32(&writer, 0);             /* profileUris[] */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    return writer.position;
}

static size_t build_username_activate_request(opcua_byte_t *request, size_t capacity, opcua_uint32_t auth_token) {
    enum { MU_ID_USERNAMEIDENTITYTOKEN_ENCODING_DEFAULTBINARY = 324 };
    opcua_byte_t token_body[128];
    mu_binary_writer_t token_writer;
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};
    mu_string_t policy_id = {8, (opcua_byte_t *)"username"};
    mu_string_t username = {5, (opcua_byte_t *)"admin"};
    mu_bytestring_t password = {5, (opcua_byte_t *)"admin"};
    mu_nodeid_t username_token = {0, MU_NODEID_NUMERIC, {MU_ID_USERNAMEIDENTITYTOKEN_ENCODING_DEFAULTBINARY}};

    mu_binary_writer_init(&token_writer, token_body, sizeof(token_body));
    mu_binary_write_string(&token_writer, &policy_id);
    mu_binary_write_string(&token_writer, &username);
    mu_binary_write_bytestring(&token_writer, &password);
    mu_binary_write_string(&token_writer, &null_string); /* encryptionAlgorithm */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, token_writer.status);

    mu_binary_writer_init(&writer, request, capacity);
    write_request_header(&writer, auth_token, 48);
    mu_binary_write_string(&writer, &null_string);    /* ClientSignature.algorithm */
    mu_binary_write_bytestring(&writer, &null_bytes); /* ClientSignature.signature */
    mu_binary_write_int32(&writer, 0);                /* ClientSoftwareCertificates[] */
    mu_binary_write_int32(&writer, 0);                /* LocaleIds[] */

    /* OPC-10000-4 section 7.40.2.1 binds UserIdentityToken secrets such as
       passwords to the UserTokenPolicy SecurityPolicy. With SecurityPolicy None,
       a default server must not accept clear-text username/password activation. */
    mu_binary_write_extension_object_header(&writer, &username_token, token_writer.position);
    TEST_ASSERT_TRUE(writer.position + token_writer.position <= capacity);
    (void)memcpy(request + writer.position, token_body, token_writer.position);
    writer.position += token_writer.position;
    mu_binary_write_string(&writer, &null_string);    /* UserTokenSignature.algorithm */
    mu_binary_write_bytestring(&writer, &null_bytes); /* UserTokenSignature.signature */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    return writer.position;
}

void test_activate_session_user_identity_extension_object_body_truncation_returns_decoding_error(void) {
    mu_server_t server;
    opcua_uint32_t auth_token = 0;
    opcua_byte_t request[128];
    opcua_byte_t response[128];
    size_t response_len = sizeof(response);
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};
    mu_nodeid_t anonymous_token = {0, MU_NODEID_NUMERIC, {321}};

    prepare_created_session(&server, &auth_token);

    mu_binary_writer_init(&writer, request, sizeof(request));
    write_request_header(&writer, auth_token, 26);
    mu_binary_write_string(&writer, &null_string);    /* ClientSignature.algorithm */
    mu_binary_write_bytestring(&writer, &null_bytes); /* ClientSignature.signature */
    mu_binary_write_int32(&writer, 0);                /* ClientSoftwareCertificates[] */
    mu_binary_write_int32(&writer, 0);                /* LocaleIds[] */

    /* OPC-10000-4 section 5.7.3: ActivateSession carries userIdentityToken as
       an ExtensionObject. OPC-10000-6 section 5.2.2.15: its body length bounds
       the encoded body; this policyId String declares one byte but omits it. */
    mu_binary_write_extension_object_header(&writer, &anonymous_token, 4);
    mu_binary_write_int32(&writer, 1);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);

    TEST_ASSERT_EQUAL_HEX32(
        MU_STATUS_BAD_DECODINGERROR,
        mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, request, writer.position, response, &response_len));
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);
}

void test_getendpoints_security_policy_none_does_not_advertise_username_tokens(void) {
    mu_server_t server;
    opcua_byte_t request[128];
    opcua_byte_t response[4096];
    size_t request_len;
    size_t response_len = sizeof(response);
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_uint32_t request_handle = 0;
    opcua_statuscode_t service_result = MU_STATUS_BAD_INTERNALERROR;
    opcua_int32_t endpoint_count = 0;
    bool saw_security_policy_none = false;

    (void)memset(&server, 0, sizeof(server));
    server.secure_channel.is_open = true;
    server.config.endpoint_url = "opc.tcp://localhost:4840";
    server.config.application_uri = "urn:test:app";
    server.config.product_uri = "urn:test:product";
    server.config.application_name = "Test Server";

    request_len = build_getendpoints_request(request, sizeof(request));

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_service_dispatch(&server, MU_ID_GETENDPOINTSREQUEST, request,
                                                                request_len, response, &response_len));

    mu_binary_reader_init(&reader, response, response_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_ID_GETENDPOINTSRESPONSE, response_type.identifier.numeric);
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL(47, request_handle);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &endpoint_count));
    TEST_ASSERT_GREATER_THAN_INT32(0, endpoint_count);

    for (opcua_int32_t endpoint_index = 0; endpoint_index < endpoint_count; ++endpoint_index) {
        mu_string_t s;
        mu_bytestring_t certificate;
        opcua_uint32_t security_mode = 0;
        opcua_byte_t security_level = 0;
        opcua_int32_t token_count = 0;
        bool is_security_policy_none = false;

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &s)); /* endpointUrl */
        skip_application_description(&reader);
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&reader, &certificate)); /* serverCertificate */
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &security_mode));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &s)); /* securityPolicyUri */

        is_security_policy_none = security_mode == MU_MESSAGE_SECURITY_MODE_NONE &&
                                  string_equals_cstr(&s, "http://opcfoundation.org/UA/SecurityPolicy#None");
        if (is_security_policy_none) {
            saw_security_policy_none = true;
        }

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&reader, &token_count));
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, token_count);
        for (opcua_int32_t token_index = 0; token_index < token_count; ++token_index) {
            mu_string_t policy_id;
            opcua_uint32_t token_type = 0;

            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &policy_id));
            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&reader, &token_type));

            /* OPC-10000-4 sections 5.5.4.2 and 5.7.3: GetEndpoints advertises
               UserTokenPolicies clients may use for ActivateSession. The
               SecurityPolicy None endpoint must not advertise plaintext
               username/password identity tokens by default. */
            if (is_security_policy_none) {
                TEST_ASSERT_NOT_EQUAL(MU_USER_TOKEN_TYPE_USERNAME, token_type);
                TEST_ASSERT_FALSE(string_equals_cstr(&policy_id, "username"));
            }

            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &s)); /* issuedTokenType */
            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &s)); /* issuerEndpointUrl */
            TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &s)); /* securityPolicyUri */
        }

        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_string(&reader, &s)); /* transportProfileUri */
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&reader, &security_level));
    }

    TEST_ASSERT_TRUE(saw_security_policy_none);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, reader.status);
}

void test_activate_session_security_policy_none_username_token_returns_identitytokenrejected(void) {
    mu_server_t server;
    accepting_auth_context_t auth_context = {0};
    opcua_uint32_t auth_token = 0;
    opcua_byte_t request[256];
    opcua_byte_t response[256];
    size_t request_len;
    size_t response_len = sizeof(response);
    opcua_statuscode_t dispatch_status;
    mu_binary_reader_t reader;
    mu_nodeid_t response_type;
    opcua_uint32_t request_handle = 0;
    opcua_statuscode_t service_result = MU_STATUS_BAD_INTERNALERROR;

    prepare_created_session(&server, &auth_token);
    server.config.user_auth_handler = accepting_username_auth;
    server.config.user_auth_handler_handle = &auth_context;
    request_len = build_username_activate_request(request, sizeof(request), auth_token);

    dispatch_status =
        mu_service_dispatch(&server, MU_ID_ACTIVATESESSIONREQUEST, request, request_len, response, &response_len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, dispatch_status);

    mu_binary_reader_init(&reader, response, response_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &response_type));
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, response_type.identifier.numeric);
    skip_response_header(&reader, &request_handle, &service_result);
    TEST_ASSERT_EQUAL(48, request_handle);

#ifdef MUC_OPCUA_USER_AUTH
    /* OPC-10000-4 section 5.7.3.3 permits Bad_IdentityTokenRejected for
       ActivateSession. Section 7.38.2 defines it as a valid token rejected by
       the Server; section 7.40.2.1 requires secret-bearing tokens to follow the
       UserTokenPolicy SecurityPolicy, so SecurityPolicy None rejects by default. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_IDENTITYTOKENREJECTED, service_result);
#else
    /* Without MUC_OPCUA_USER_AUTH the server cannot decode a username token at
       all (the token TYPE itself is unsupported), which is the more generic
       Bad_IdentityTokenInvalid rather than the SecurityPolicy-specific
       Bad_IdentityTokenRejected reachable only once username tokens are
       understood (see service_dispatch.c's ActivateSession token-324 branch). */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_IDENTITYTOKENINVALID, service_result);
#endif
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_CREATED, server.sessions[0].state);
    TEST_ASSERT_EQUAL_UINT(0u, auth_context.calls);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unsupported_security_policy);
    RUN_TEST(test_unsupported_identity_token);
    RUN_TEST(test_activate_session_user_identity_extension_object_body_truncation_returns_decoding_error);
    RUN_TEST(test_getendpoints_security_policy_none_does_not_advertise_username_tokens);
    RUN_TEST(test_activate_session_security_policy_none_username_token_returns_identitytokenrejected);
    return UNITY_END();
}
