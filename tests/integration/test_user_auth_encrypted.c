/* tests/integration/test_user_auth_encrypted.c */
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "service_builders.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define MAX_INBOUND 8
typedef struct {
    int accept_count;
    opcua_byte_t inbound[MAX_INBOUND][512];
    size_t inbound_len[MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    opcua_byte_t last_write[8192];
    size_t last_write_len;
} mock_t;

static opcua_statuscode_t mock_listen(void *c, const char *u) {
    (void)c;
    (void)u;
    return MU_STATUS_GOOD;
}
static void mock_shutdown(void *c) {
    (void)c;
}
static void mock_close(void *c, void *h) {
    (void)c;
    (void)h;
}

static opcua_statuscode_t mock_accept(void *c, void **handle) {
    mock_t *m = (mock_t *)c;
    m->accept_count++;
    *handle = (m->accept_count == 1) ? (void *)1 : NULL;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_read(void *c, void *h, opcua_byte_t *buf, size_t cap, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    if (m->read_index >= m->inbound_count) {
        *n = 0;
        return MU_STATUS_GOOD;
    }
    size_t len = m->inbound_len[m->read_index];
    TEST_ASSERT_TRUE(len <= cap);
    (void)memcpy(buf, m->inbound[m->read_index], len);
    m->read_index++;
    *n = len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    (void)memcpy(m->last_write, buf, len);
    m->last_write_len = len;
    *n = len;
    return MU_STATUS_GOOD;
}

static void enqueue(mock_t *m, const opcua_byte_t *bytes, size_t len) {
    (void)memcpy(m->inbound[m->inbound_count], bytes, len);
    m->inbound_len[m->inbound_count] = len;
    m->inbound_count++;
}

static void write_request_header(mu_binary_writer_t *w, opcua_uint32_t auth_token, opcua_uint32_t handle) {
    mu_nodeid_t auth = {0, MU_NODEID_NUMERIC, {auth_token}};
    mu_nodeid_t null_id = {0, MU_NODEID_NUMERIC, {0}};
    mu_string_t null_str = {-1, NULL};
    mu_binary_write_nodeid(w, &auth);
    mu_binary_write_int64(w, 0);
    mu_binary_write_uint32(w, handle);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_string(w, &null_str);
    mu_binary_write_uint32(w, 0);
    mu_binary_write_extension_object_header(w, &null_id, 0);
}

static size_t build_msg(opcua_byte_t *out, size_t cap, opcua_uint32_t seq, opcua_uint32_t reqid,
                        const opcua_byte_t *body, size_t body_len) {
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, out, cap);
    out[0] = 'M';
    out[1] = 'S';
    out[2] = 'G';
    out[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, (opcua_uint32_t)(24 + body_len));
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    (void)memcpy(out + 24, body, body_len);
    return 24 + body_len;
}

static opcua_statuscode_t mock_rsa_oaep_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                                opcua_byte_t *output, size_t *output_length) {
    (void)context;
    if (length > *output_length) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    /* Decryption mock: XOR with 0x5A to simulate decryption */
    for (size_t i = 0; i < length; ++i) {
        output[i] = input[i] ^ 0x5A;
    }
    *output_length = length;
    return MU_STATUS_GOOD;
}

/* User authentication callback: only accepts "admin" / "admin" */
static opcua_statuscode_t test_auth_handler(void *handle, const mu_string_t *username, const mu_bytestring_t *password,
                                            const mu_string_t *policy_id) {
    (void)handle;
    (void)policy_id;

    if (username == NULL || password == NULL) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    if (username->length == 5 && memcmp(username->data, "admin", 5) == 0 && password->length == 5 &&
        memcmp(password->data, "admin", 5) == 0) {
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
}

void test_encrypted_username_token_over_security_policy_none_is_rejected(void) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));

    /* ---- Build the inbound queue ---- */
    opcua_byte_t tmp[512];
    mu_binary_writer_t w;
    opcua_byte_t chunk[512];
    size_t clen;

    /* HEL */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    tmp[0] = 'H';
    tmp[1] = 'E';
    tmp[2] = 'L';
    tmp[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 8192);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t url = {20, (const opcua_byte_t *)"opc.tcp://host:4840"};
    mu_binary_write_string(&w, &url);
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, tmp, w.position);

    /* OPN (OpenSecureChannelRequest) */
    mu_binary_writer_init(&w, chunk, sizeof(chunk));
    chunk[0] = 'O';
    chunk[1] = 'P';
    chunk[2] = 'N';
    chunk[3] = 'F';
    w.position = 4;
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_string_t pol = {47, (const opcua_byte_t *)"http://opcfoundation.org/UA/SecurityPolicy#None"};
    mu_binary_write_string(&w, &pol);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, 1);
    mu_nodeid_t opn_type = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
    mu_binary_write_nodeid(&w, &opn_type);
    write_request_header(&w, 0, 1);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 0);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_int32(&w, -1);
    mu_binary_write_uint32(&w, 3600000);
    {
        mu_binary_writer_t os;
        mu_binary_writer_init(&os, chunk, sizeof(chunk));
        os.position = 4;
        mu_binary_write_uint32(&os, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, chunk, w.position);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_create_session_body(&w, 2, 60000.0);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* ActivateSession (with admin/admin XOR 0x5A) */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 3);
    {
        mu_string_t ns = {-1, NULL};
        mu_bytestring_t nb = {-1, NULL};
        mu_binary_write_string(&w, &ns);
        mu_binary_write_bytestring(&w, &nb);
    }
    mu_binary_write_int32(&w, 0);
    mu_binary_write_int32(&w, 0);

    // UserNameIdentityToken
    {
        mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {324}};
        /* Exact body size: policy(19), username(9), password(45), alg(45) = 118 */
        mu_binary_write_extension_object_header(&w, &token_type, 118);

        mu_string_t policy = {15, (const opcua_byte_t *)"username_policy"};
        mu_binary_write_string(&w, &policy);

        mu_string_t username = {5, (const opcua_byte_t *)"admin"};
        mu_binary_write_string(&w, &username);

        /* UA password block plaintext:
           [4 bytes] length (5)
           [5 bytes] "admin"
           [32 bytes] server nonce (0x42, matching fake_generate_random)
        */
        opcua_byte_t plain_block[41];
        plain_block[0] = 5;
        plain_block[1] = 0;
        plain_block[2] = 0;
        plain_block[3] = 0;
        (void)memcpy(plain_block + 4, "admin", 5);
        memset(plain_block + 9, 0x42, 32);

        /* encrypt by XORing with 0x5A to simulate mock rsa OAEP decryption */
        opcua_byte_t encrypted_pwd[41];
        for (size_t i = 0; i < 41; ++i) {
            encrypted_pwd[i] = plain_block[i] ^ 0x5A;
        }
        mu_bytestring_t password = {41, encrypted_pwd};
        mu_binary_write_bytestring(&w, &password);

        mu_string_t alg = {41, (const opcua_byte_t *)"http://www.w3.org/2001/04/xmlenc#rsa-oaep"};
        mu_binary_write_string(&w, &alg);
    }

    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&mock, chunk, clen);

    /* ---- Configure the server ---- */
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://host:4840";
    config.application_uri = "urn:test";
    config.product_uri = "urn:test";
    config.application_name = "test";
    static opcua_byte_t rx[8192], tx[8192];
    config.receive_buffer = rx;
    config.receive_buffer_size = sizeof(rx);
    config.send_buffer = tx;
    config.send_buffer_size = sizeof(tx);
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    fake_platform_init(NULL, &config.time_adapter, &config.entropy_adapter);
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    /* Attach user auth handler */
    config.user_auth_handler = test_auth_handler;

    /* Attach crypto adapter */
    static mu_crypto_adapter_t crypto_adapter;
    memset(&crypto_adapter, 0, sizeof(crypto_adapter));
    crypto_adapter.rsa_oaep_decrypt = mock_rsa_oaep_decrypt;
    config.crypto_adapter = &crypto_adapter;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> Response */
    mu_server_poll(server); /* CreateSession -> Response */

    mu_server_poll(server); /* ActivateSession -> Response (rejected on SecurityPolicy#None) */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, mock.last_write, mock.last_write_len);
    r.position = 24;
    mu_nodeid_t resp_type;
    mu_binary_read_nodeid(&r, &resp_type);
    TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, resp_type.identifier.numeric);
    opcua_uint64_t ts;
    opcua_uint32_t h;
    opcua_statuscode_t res;
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &h);
    mu_binary_read_statuscode(&r, &res);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_IDENTITYTOKENREJECTED, res);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_encrypted_username_token_over_security_policy_none_is_rejected);
    return UNITY_END();
}
