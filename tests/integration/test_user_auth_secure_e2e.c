/* tests/integration/test_user_auth_secure_e2e.c
 * Feature 028 US3: real-crypto user identity tokens over a secured channel.
 * Drives mu_server_poll through HEL -> Basic256Sha256 OPN -> CreateSession ->
 * ActivateSession using OpenSSL host crypto, then asserts the accept/reject
 * service results for UserName and X509 tokens.
 */
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
/* The static-analysis environment may not provide system headers; the build does. */
/* cppcheck-suppress missingIncludeSystem */
#include <stdbool.h>
/* cppcheck-suppress missingIncludeSystem */
#include <string.h>

#ifdef MUC_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"
#include "security/asym_chunk.h"
#include "security/security_policy.h"
#include "security/sym_chunk.h"

#define MAX_INBOUND 8
#define CHUNK_CAP 8192
#define SERVICE_BODY_CAP 4096
#define SIGNATURE_CAP 512

static const char SIG_ALG_RSA_SHA256[] = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
static const char PASSWORD_ENCRYPTION_ALG[] = "http://www.w3.org/2001/04/xmlenc#rsa-oaep";
static const char USERNAME_POLICY[] = "username_policy";
static const char CERT_POLICY[] = "cert_user_policy";
#define SIG_ALG_RSA_SHA256_LEN (sizeof(SIG_ALG_RSA_SHA256) - 1u)
#define PASSWORD_ENCRYPTION_ALG_LEN (sizeof(PASSWORD_ENCRYPTION_ALG) - 1u)
#define USERNAME_POLICY_LEN (sizeof(USERNAME_POLICY) - 1u)
#define CERT_POLICY_LEN (sizeof(CERT_POLICY) - 1u)

typedef struct {
    int accept_count;
    int close_count;
    opcua_byte_t inbound[MAX_INBOUND][CHUNK_CAP];
    size_t inbound_len[MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    opcua_byte_t last_write[CHUNK_CAP];
    size_t last_write_len;
} mock_t;

typedef struct {
    mu_binary_reader_t body;
    opcua_uint32_t type_id;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;
} decoded_response_t;

typedef struct {
    mock_t mock;
    mu_crypto_adapter_t server_crypto;
    mu_crypto_adapter_t client_crypto;
    mu_crypto_adapter_t user_crypto;
    mu_crypto_adapter_t other_crypto;
    const opcua_byte_t *server_cert;
    const opcua_byte_t *client_cert;
    const opcua_byte_t *user_cert;
    const opcua_byte_t *other_cert;
    size_t server_cert_len;
    size_t client_cert_len;
    size_t user_cert_len;
    size_t other_cert_len;
    const opcua_byte_t *trusted_certs[1];
    size_t trusted_lens[1];
    mu_trust_list_t trust;
    mu_sym_keys_t c2s;
    mu_sym_keys_t s2c;
    opcua_uint32_t secure_channel_id;
    opcua_uint32_t token_id;
    opcua_uint32_t next_sequence;
    opcua_uint32_t session_auth_token;
    opcua_byte_t session_server_nonce[32];
    opcua_byte_t client_nonce[32];
    mu_server_t *server;
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    opcua_byte_t rx[CHUNK_CAP];
    opcua_byte_t tx[CHUNK_CAP];
} secure_fixture_t;

static secure_fixture_t g_fixture;

/* Intentionally included after the fixture types/g_fixture so the mock scaffolding
   stays private to this test. */
/* cppcheck-suppress misra-c2012-20.1 */
#include "../support/user_auth_secure_e2e_helpers.h"

static bool auth_username_matches(const mu_string_t *username, const mu_bytestring_t *password) {
    bool matches = false;

    if (username != NULL) {
        if (password != NULL) {
            if (username->length == (opcua_int32_t)5) {
                if (password->length == (opcua_int32_t)5) {
                    if (memcmp(username->data, "admin", 5u) == 0) {
                        if (memcmp(password->data, "admin", 5u) == 0) {
                            matches = true;
                        }
                    }
                }
            }
        }
    }
    return matches;
}

static bool auth_certificate_matches(const secure_fixture_t *fixture, const mu_string_t *username,
                                     const mu_bytestring_t *password) {
    bool matches = false;

    if (username == NULL) {
        if (password != NULL) {
            if (fixture != NULL) {
                if (password->length == (opcua_int32_t)fixture->user_cert_len) {
                    if (memcmp(password->data, fixture->user_cert, fixture->user_cert_len) == 0) {
                        matches = true;
                    }
                }
            }
        }
    }
    return matches;
}

static opcua_statuscode_t test_auth_handler(void *handle, const mu_string_t *username, const mu_bytestring_t *password,
                                            const mu_string_t *policy_id) {
    /* cppcheck-suppress misra-c2012-11.5 */
    secure_fixture_t *fixture = (secure_fixture_t *)handle;
    (void)policy_id;

    if (auth_username_matches(username, password)) {
        /* cppcheck-suppress misra-c2012-15.5 */
        return MU_STATUS_GOOD;
    }
    if (auth_certificate_matches(fixture, username, password)) {
        return MU_STATUS_GOOD;
    }
    return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
}

static void configure_server(secure_fixture_t *fixture, bool trust_client_cert) {
    mu_server_config_t config = {0};

    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:test";
    config.product_uri = "urn:test";
    config.application_name = "test";
    config.receive_buffer = fixture->rx;
    config.receive_buffer_size = sizeof(fixture->rx);
    config.send_buffer = fixture->tx;
    config.send_buffer_size = sizeof(fixture->tx);
    config.max_chunk_count = 1;
    config.max_message_size = CHUNK_CAP;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = &fixture->mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;
    config.crypto_adapter = &fixture->server_crypto;
    config.user_auth_handler = test_auth_handler;
    config.user_auth_handler_handle = fixture;

    fixture->trusted_certs[0] = trust_client_cert ? fixture->client_cert : fixture->other_cert;
    fixture->trusted_lens[0] = trust_client_cert ? fixture->client_cert_len : fixture->other_cert_len;
    fixture->trust.certificates = fixture->trusted_certs;
    fixture->trust.lengths = fixture->trusted_lens;
    fixture->trust.count = 1;
    config.trust_list = &fixture->trust;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_server_init(fixture->storage, sizeof(fixture->storage), &config, &fixture->server));
}

static void enqueue_hello(secure_fixture_t *fixture) {
    opcua_byte_t hello[SERVICE_BODY_CAP];
    mu_binary_writer_t writer;

    mu_binary_writer_init(&writer, hello, sizeof(hello));
    hello[0] = 'H';
    hello[1] = 'E';
    hello[2] = 'L';
    hello[3] = 'F';
    writer.position = 4;
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, CHUNK_CAP);
    mu_binary_write_uint32(&writer, CHUNK_CAP);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    {
        mu_string_t url = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&writer, &url);
    }
    {
        mu_binary_writer_t patch;
        mu_binary_writer_init(&patch, hello, sizeof(hello));
        patch.position = 4;
        mu_binary_write_uint32(&patch, (opcua_uint32_t)writer.position);
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    enqueue(&fixture->mock, hello, writer.position);
}

static void enqueue_basic256_opn(secure_fixture_t *fixture) {
    opcua_byte_t body[SERVICE_BODY_CAP];
    opcua_byte_t chunk[CHUNK_CAP];
    mu_binary_writer_t writer;
    size_t chunk_len = 0;

    for (size_t i = 0; i < sizeof(fixture->client_nonce); ++i) {
        fixture->client_nonce[i] = (opcua_byte_t)(i + 1u);
    }

    mu_binary_writer_init(&writer, body, sizeof(body));
    {
        mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
        mu_binary_write_nodeid(&writer, &type);
    }
    write_request_header(&writer, 0, 1);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, 0);
    mu_binary_write_uint32(&writer, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT);
    {
        mu_bytestring_t nonce = {(opcua_int32_t)sizeof(fixture->client_nonce), fixture->client_nonce};
        mu_binary_write_bytestring(&writer, &nonce);
    }
    mu_binary_write_uint32(&writer, 3600000);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_asym_chunk_wrap(&fixture->client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                                                         0, 1, 1, fixture->server_cert, fixture->server_cert_len, body,
                                                         writer.position, chunk, sizeof(chunk), &chunk_len));
    enqueue(&fixture->mock, chunk, chunk_len);
}

static void parse_open_secure_channel_response(secure_fixture_t *fixture, decoded_response_t *response) {
    opcua_uint32_t server_protocol_version;
    opcua_uint32_t revised_lifetime;
    opcua_int64_t created_at;
    mu_bytestring_t server_nonce;

    TEST_ASSERT_EQUAL(MU_ID_OPENSECURECHANNELRESPONSE, response->type_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, response->service_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&response->body, &server_protocol_version));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&response->body, &fixture->secure_channel_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&response->body, &fixture->token_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&response->body, &created_at));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&response->body, &revised_lifetime));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&response->body, &server_nonce));
    TEST_ASSERT_EQUAL(32, server_nonce.length);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&fixture->client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                                                         server_nonce.data, 32, fixture->client_nonce,
                                                         sizeof(fixture->client_nonce), &fixture->c2s));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&fixture->client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                                                         fixture->client_nonce, sizeof(fixture->client_nonce),
                                                         server_nonce.data, 32, &fixture->s2c));
    fixture->next_sequence = 2;
}

static void drive_open_secure_channel(secure_fixture_t *fixture, bool trust_client_cert, decoded_response_t *response) {
    init_crypto(fixture);
    enqueue_hello(fixture);
    enqueue_basic256_opn(fixture);
    configure_server(fixture, trust_client_cert);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(fixture->server));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(fixture->server));
    TEST_ASSERT_TRUE(fixture->mock.last_write_len >= 4u);
    TEST_ASSERT_EQUAL_MEMORY("ACKF", fixture->mock.last_write, 4);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_poll(fixture->server));
    unwrap_opn_response(fixture, response);
}

static void build_create_session_request(secure_fixture_t *fixture, opcua_byte_t *body, size_t body_cap,
                                         size_t *body_len) {
    mu_binary_writer_t writer;
    mu_string_t null_string = {-1, NULL};

    mu_binary_writer_init(&writer, body, body_cap);
    {
        mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&writer, &type);
    }
    write_request_header(&writer, 0, 3);
    mu_binary_write_string(&writer, &null_string);
    mu_binary_write_string(&writer, &null_string);
    mu_binary_write_byte(&writer, 0);
    mu_binary_write_uint32(&writer, 1);
    mu_binary_write_string(&writer, &null_string);
    mu_binary_write_string(&writer, &null_string);
    mu_binary_write_int32(&writer, 0);
    mu_binary_write_string(&writer, &null_string);
    mu_binary_write_string(&writer, &null_string);
    mu_binary_write_string(&writer, &null_string);
    {
        mu_bytestring_t nonce = {(opcua_int32_t)sizeof(fixture->client_nonce), fixture->client_nonce};
        mu_binary_write_bytestring(&writer, &nonce);
    }
    {
        mu_bytestring_t cert = {(opcua_int32_t)fixture->client_cert_len, fixture->client_cert};
        mu_binary_write_bytestring(&writer, &cert);
    }
    mu_binary_write_double(&writer, 60000.0);
    mu_binary_write_uint32(&writer, 0);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    *body_len = writer.position;
}

static void parse_create_session_response(secure_fixture_t *fixture, decoded_response_t *response) {
    mu_nodeid_t session_id;
    mu_nodeid_t auth_token;
    opcua_uint64_t revised_timeout;
    mu_bytestring_t session_nonce;

    TEST_ASSERT_EQUAL(MU_ID_CREATESESSIONRESPONSE, response->type_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, response->service_result);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&response->body, &session_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&response->body, &auth_token));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, auth_token.identifier_type);
    TEST_ASSERT_EQUAL(0, auth_token.namespace_index);
    TEST_ASSERT_NOT_EQUAL(0, auth_token.identifier.numeric);
    fixture->session_auth_token = auth_token.identifier.numeric;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint64(&response->body, &revised_timeout));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&response->body, &session_nonce));
    TEST_ASSERT_EQUAL(32, session_nonce.length);
    copy_bytes(fixture->session_server_nonce, session_nonce.data, sizeof(fixture->session_server_nonce));
}

static void drive_to_create_session(secure_fixture_t *fixture) {
    opcua_byte_t body[SERVICE_BODY_CAP];
    size_t body_len = 0;
    decoded_response_t response;

    drive_open_secure_channel(fixture, true, &response);
    parse_open_secure_channel_response(fixture, &response);
    build_create_session_request(fixture, body, sizeof(body), &body_len);
    secure_call(fixture, body, body_len, &response);
    parse_create_session_response(fixture, &response);
}

static size_t sign_with_identity(secure_fixture_t *fixture, mu_crypto_adapter_t *crypto, opcua_byte_t *signature,
                                 size_t signature_cap) {
    opcua_byte_t signed_data[2048];
    size_t signature_len = signature_cap;

    TEST_ASSERT_TRUE((fixture->server_cert_len + sizeof(fixture->session_server_nonce)) <= sizeof(signed_data));
    copy_bytes(signed_data, fixture->server_cert, fixture->server_cert_len);
    copy_bytes(signed_data + fixture->server_cert_len, fixture->session_server_nonce,
               sizeof(fixture->session_server_nonce));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto->rsa_sha256_sign(crypto->context, signed_data,
                                              fixture->server_cert_len + sizeof(fixture->session_server_nonce),
                                              signature, &signature_len));
    return signature_len;
}

static void write_extension_body(mu_binary_writer_t *writer, opcua_uint32_t type_id, const opcua_byte_t *body,
                                 size_t body_len) {
    mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {type_id}};

    mu_binary_write_extension_object_header(writer, &token_type, body_len);
    TEST_ASSERT_TRUE((writer->position + body_len) <= writer->length);
    copy_bytes(writer->buffer + writer->position, body, body_len);
    writer->position += body_len;
}

static void begin_activate_request(secure_fixture_t *fixture, mu_binary_writer_t *writer, opcua_byte_t *body,
                                   size_t body_cap) {
    opcua_byte_t client_signature[SIGNATURE_CAP];
    size_t client_signature_len;

    mu_binary_writer_init(writer, body, body_cap);
    {
        mu_nodeid_t type = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(writer, &type);
    }
    write_request_header(writer, fixture->session_auth_token, 4);
    client_signature_len =
        sign_with_identity(fixture, &fixture->client_crypto, client_signature, sizeof(client_signature));
    {
        mu_string_t algorithm = {(opcua_int32_t)SIG_ALG_RSA_SHA256_LEN, (const opcua_byte_t *)SIG_ALG_RSA_SHA256};
        mu_bytestring_t signature = {(opcua_int32_t)client_signature_len, client_signature};
        mu_binary_write_string(writer, &algorithm);
        mu_binary_write_bytestring(writer, &signature);
    }
    mu_binary_write_int32(writer, 0);
    mu_binary_write_int32(writer, 0);
}

static void activate_with_prebuilt_body(opcua_byte_t *body, size_t body_len, opcua_statuscode_t expected_status) {
    decoded_response_t response;

    secure_call(&g_fixture, body, body_len, &response);
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, response.type_id);
    TEST_ASSERT_EQUAL_HEX32(expected_status, response.service_result);
}

static void append_username_token(mu_binary_writer_t *writer, const opcua_byte_t *password, size_t password_len,
                                  bool encrypted, bool flip_nonce) {
    opcua_byte_t token_body[SERVICE_BODY_CAP];
    opcua_byte_t password_block[128];
    opcua_byte_t encrypted_password[SIGNATURE_CAP];
    mu_binary_writer_t token_writer;
    mu_string_t policy = {(opcua_int32_t)USERNAME_POLICY_LEN, (const opcua_byte_t *)USERNAME_POLICY};
    mu_string_t username = {(opcua_int32_t)5, (const opcua_byte_t *)"admin"};
    mu_string_t null_string = {-1, NULL};
    mu_string_t encryption_algorithm = {(opcua_int32_t)PASSWORD_ENCRYPTION_ALG_LEN,
                                        (const opcua_byte_t *)PASSWORD_ENCRYPTION_ALG};
    mu_bytestring_t password_bytes;

    mu_binary_writer_init(&token_writer, token_body, sizeof(token_body));
    mu_binary_write_string(&token_writer, &policy);
    mu_binary_write_string(&token_writer, &username);

    if (encrypted) {
        size_t plain_len = 4u + password_len + sizeof(g_fixture.session_server_nonce);
        size_t encrypted_len = sizeof(encrypted_password);
        opcua_uint32_t secret_len = (opcua_uint32_t)(password_len + sizeof(g_fixture.session_server_nonce));
        TEST_ASSERT_TRUE(plain_len <= sizeof(password_block));
        password_block[0] = (opcua_byte_t)(secret_len & 0xFFu);
        password_block[1] = (opcua_byte_t)((secret_len >> 8) & 0xFFu);
        password_block[2] = (opcua_byte_t)((secret_len >> 16) & 0xFFu);
        password_block[3] = (opcua_byte_t)((secret_len >> 24) & 0xFFu);
        copy_bytes(password_block + 4, password, password_len);
        copy_bytes(password_block + 4u + password_len, g_fixture.session_server_nonce,
                   sizeof(g_fixture.session_server_nonce));
        if (flip_nonce) {
            password_block[4u + password_len] ^= 0x01u;
        }
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                          g_fixture.client_crypto.rsa_oaep_encrypt(
                              g_fixture.client_crypto.context, g_fixture.server_cert, g_fixture.server_cert_len,
                              password_block, plain_len, encrypted_password, &encrypted_len));
        password_bytes.length = (opcua_int32_t)encrypted_len;
        password_bytes.data = encrypted_password;
        mu_binary_write_bytestring(&token_writer, &password_bytes);
        mu_binary_write_string(&token_writer, &encryption_algorithm);
    } else {
        password_bytes.length = (opcua_int32_t)password_len;
        password_bytes.data = (opcua_byte_t *)password;
        mu_binary_write_bytestring(&token_writer, &password_bytes);
        mu_binary_write_string(&token_writer, &null_string);
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, token_writer.status);
    write_extension_body(writer, 324, token_body, token_writer.position);
}

static void activate_username(const char *password, size_t password_len, bool encrypted, bool flip_nonce,
                              opcua_statuscode_t expected) {
    opcua_byte_t body[SERVICE_BODY_CAP];
    mu_binary_writer_t writer;

    begin_activate_request(&g_fixture, &writer, body, sizeof(body));
    append_username_token(&writer, (const opcua_byte_t *)password, password_len, encrypted, flip_nonce);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    activate_with_prebuilt_body(body, writer.position, expected);
}

static void append_x509_token(mu_binary_writer_t *writer, bool corrupt_signature) {
    opcua_byte_t token_body[SERVICE_BODY_CAP];
    opcua_byte_t user_signature[SIGNATURE_CAP];
    size_t user_signature_len;
    mu_binary_writer_t token_writer;
    mu_string_t policy = {(opcua_int32_t)CERT_POLICY_LEN, (const opcua_byte_t *)CERT_POLICY};

    mu_binary_writer_init(&token_writer, token_body, sizeof(token_body));
    mu_binary_write_string(&token_writer, &policy);
    {
        mu_bytestring_t cert = {(opcua_int32_t)g_fixture.user_cert_len, g_fixture.user_cert};
        mu_binary_write_bytestring(&token_writer, &cert);
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, token_writer.status);
    write_extension_body(writer, 327, token_body, token_writer.position);

    user_signature_len = sign_with_identity(&g_fixture, &g_fixture.user_crypto, user_signature, sizeof(user_signature));
    if (corrupt_signature) {
        if (user_signature_len > 0u) {
            user_signature[0] ^= 0x80u;
        }
    }
    {
        mu_string_t algorithm = {(opcua_int32_t)SIG_ALG_RSA_SHA256_LEN, (const opcua_byte_t *)SIG_ALG_RSA_SHA256};
        mu_bytestring_t signature = {(opcua_int32_t)user_signature_len, user_signature};
        mu_binary_write_string(writer, &algorithm);
        mu_binary_write_bytestring(writer, &signature);
    }
}

static void activate_x509(bool corrupt_signature, opcua_statuscode_t expected) {
    opcua_byte_t body[SERVICE_BODY_CAP];
    mu_binary_writer_t writer;

    begin_activate_request(&g_fixture, &writer, body, sizeof(body));
    append_x509_token(&writer, corrupt_signature);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, writer.status);
    activate_with_prebuilt_body(body, writer.position, expected);
}

static bool server_channel_is_open(const secure_fixture_t *fixture) {
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
    return fixture->server->conns[0].secure_channel.is_open;
#else
    return fixture->server->secure_channel.is_open;
#endif
}

void test_username_plaintext_accepts_admin_password(void) {
    drive_to_create_session(&g_fixture);
    activate_username("admin", sizeof("admin") - 1u, false, false, MU_STATUS_GOOD);
}

void test_username_plaintext_rejects_wrong_password(void) {
    drive_to_create_session(&g_fixture);
    activate_username("wrong", sizeof("wrong") - 1u, false, false, MU_STATUS_BAD_IDENTITYTOKENREJECTED);
}

void test_encrypted_username_accepts_spec_secret_layout(void) {
    drive_to_create_session(&g_fixture);
    activate_username("admin", sizeof("admin") - 1u, true, false, MU_STATUS_GOOD);
}

void test_encrypted_username_rejects_wrong_server_nonce(void) {
    drive_to_create_session(&g_fixture);
    activate_username("admin", sizeof("admin") - 1u, true, true, MU_STATUS_BAD_IDENTITYTOKENREJECTED);
}

void test_x509_user_token_accepts_real_signature(void) {
    drive_to_create_session(&g_fixture);
    activate_x509(false, MU_STATUS_GOOD);
}

void test_x509_user_token_rejects_corrupted_signature(void) {
    drive_to_create_session(&g_fixture);
    activate_x509(true, MU_STATUS_BAD_IDENTITYTOKENREJECTED);
}

void test_opn_rejects_untrusted_application_certificate_fail_closed(void) {
    decoded_response_t response;

    drive_open_secure_channel(&g_fixture, false, &response);
    TEST_ASSERT_EQUAL(MU_ID_SERVICEFAULT, response.type_id);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SECURITYCHECKSFAILED, response.service_result);
    TEST_ASSERT_FALSE(server_channel_is_open(&g_fixture));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_username_plaintext_accepts_admin_password);
    RUN_TEST(test_username_plaintext_rejects_wrong_password);
    RUN_TEST(test_encrypted_username_accepts_spec_secret_layout);
    RUN_TEST(test_encrypted_username_rejects_wrong_server_nonce);
    RUN_TEST(test_x509_user_token_accepts_real_signature);
    RUN_TEST(test_x509_user_token_rejects_corrupted_signature);
    RUN_TEST(test_opn_rejects_untrusted_application_certificate_fail_closed);
    return UNITY_END();
}

#else /* !MUC_OPCUA_HAVE_OPENSSL */
void setUp(void) {}
void tearDown(void) {}

void test_user_auth_secure_e2e_skipped(void) {
    TEST_IGNORE_MESSAGE("OpenSSL not available; secure user-auth e2e tests skipped");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_user_auth_secure_e2e_skipped);
    return UNITY_END();
}
#endif
