/* tests/integration/test_user_auth_certificate.c */
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
    opcua_byte_t captured_server_nonce[32];
    bool nonce_captured;
} mock_t;

static opcua_uint32_t s_channel_id = 1u;

static opcua_uint32_t read_opn_channel_id(const opcua_byte_t *buf, size_t len) {
    if (len < 12) {
        return 1u;
    }
    return (opcua_uint32_t)buf[8] | ((opcua_uint32_t)buf[9] << 8u) | ((opcua_uint32_t)buf[10] << 16u) |
           ((opcua_uint32_t)buf[11] << 24u);
}

static void patch_msg_channel_ids(mock_t *m, opcua_uint32_t channel_id) {
    for (size_t i = 0; i < m->inbound_count; ++i) {
        if (m->inbound_len[i] >= 12 && ((m->inbound[i][0] == 'M' && m->inbound[i][1] == 'S' &&
                                         m->inbound[i][2] == 'G' && m->inbound[i][3] == 'F') ||
                                        (m->inbound[i][0] == 'C' && m->inbound[i][1] == 'L' &&
                                         m->inbound[i][2] == 'O' && m->inbound[i][3] == 'F'))) {
            m->inbound[i][8] = (opcua_byte_t)(channel_id);
            m->inbound[i][9] = (opcua_byte_t)(channel_id >> 8u);
            m->inbound[i][10] = (opcua_byte_t)(channel_id >> 16u);
            m->inbound[i][11] = (opcua_byte_t)(channel_id >> 24u);
        }
    }
}

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

    /* Parse server nonce from CreateSessionResponse if present */
    if (len > 24) {
        mu_binary_reader_t br;
        mu_binary_reader_init(&br, buf, len);
        br.position = 24;
        mu_nodeid_t resp_type;
        mu_binary_read_nodeid(&br, &resp_type);
        if (resp_type.identifier.numeric == MU_ID_CREATESESSIONRESPONSE) {
            /* Skip ResponseHeader (ts:8, handle:4, service_result:4, diag:1, stringTable:4) */
            br.position += 8 + 4 + 4 + 1 + 4;
            /* Read SessionId, AuthenticationToken, RevisedSessionTimeout */
            mu_nodeid_t sid, auth;
            mu_binary_read_nodeid(&br, &sid);
            mu_binary_read_nodeid(&br, &auth);
            opcua_uint64_t revised_timeout;
            mu_binary_read_uint64(&br, &revised_timeout);
            /* Now read serverNonce */
            mu_bytestring_t nonce;
            mu_binary_read_bytestring(&br, &nonce);
            if (nonce.length == 32) {
                (void)memcpy(m->captured_server_nonce, nonce.data, 32);
                m->nonce_captured = true;
            }
        }
    }

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
    mu_binary_write_uint32(&w, s_channel_id);
    mu_binary_write_uint32(&w, 1);
    mu_binary_write_uint32(&w, seq);
    mu_binary_write_uint32(&w, reqid);
    (void)memcpy(out + 24, body, body_len);
    return 24 + body_len;
}

static const opcua_byte_t server_cert_bytes[] = "server_cert_bytes";
static const opcua_byte_t client_cert_bytes[] = "client_cert_bytes";
static const opcua_byte_t client_sig_bytes[] = "client_sig_bytes";

static opcua_statuscode_t mock_get_own_certificate(void *context, const opcua_byte_t **cert_der, size_t *cert_der_len) {
    (void)context;
    *cert_der = server_cert_bytes;
    *cert_der_len = sizeof(server_cert_bytes);
    return MU_STATUS_GOOD;
}

static mock_t global_mock;

static opcua_statuscode_t mock_rsa_sha256_verify(void *context, const opcua_byte_t *cert_der, size_t cert_der_len,
                                                 const opcua_byte_t *data, size_t data_len,
                                                 const opcua_byte_t *signature, size_t signature_len) {
    (void)context;
    /* Verify server certificate prefix */
    if (data_len != sizeof(server_cert_bytes) + 32) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    if (memcmp(data, server_cert_bytes, sizeof(server_cert_bytes)) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    /* Verify captured server nonce */
    if (global_mock.nonce_captured) {
        if (memcmp(data + sizeof(server_cert_bytes), global_mock.captured_server_nonce, 32) != 0) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
    }
    /* Verify signature */
    if (signature_len != sizeof(client_sig_bytes) || memcmp(signature, client_sig_bytes, signature_len) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    /* Verify client certificate */
    if (cert_der_len != sizeof(client_cert_bytes) || memcmp(cert_der, client_cert_bytes, cert_der_len) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t test_auth_handler(void *handle, const mu_string_t *username, const mu_bytestring_t *password,
                                            const mu_string_t *policy_id) {
    (void)handle;
    (void)username;
    (void)policy_id;

    if (password == NULL) {
        return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
    }

    /* Handler receives the client certificate as password bytes */
    if (password->length == sizeof(client_cert_bytes) &&
        memcmp(password->data, client_cert_bytes, password->length) == 0) {
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
}

void test_certificate_user_auth_flow(void) {
    (void)memset(&global_mock, 0, sizeof(global_mock));

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
    enqueue(&global_mock, tmp, w.position);

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
    enqueue(&global_mock, chunk, w.position);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    test_write_create_session_body(&w, 2, 60000.0);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&global_mock, chunk, clen);

    /* ActivateSession */
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

    /* CertificateIdentityToken (NodeId 327) */
    {
        mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {327}};
        /* policyId(15) + certificateData(sizeof(client_cert_bytes)) */
        size_t token_body_len = (4 + 15) + (4 + sizeof(client_cert_bytes));
        mu_binary_write_extension_object_header(&w, &token_type, token_body_len);

        mu_string_t policy = {15, (const opcua_byte_t *)"cert_user_policy"};
        mu_binary_write_string(&w, &policy);

        mu_bytestring_t cert_data = {(opcua_int32_t)sizeof(client_cert_bytes), client_cert_bytes};
        mu_binary_write_bytestring(&w, &cert_data);
    }

    /* UserTokenSignature */
    {
        mu_string_t alg = {45, (const opcua_byte_t *)"http://www.w3.org/2001/04/xmldsig-more#rsa-sha256"};
        mu_binary_write_string(&w, &alg);

        mu_bytestring_t sig = {(opcua_int32_t)sizeof(client_sig_bytes), client_sig_bytes};
        mu_binary_write_bytestring(&w, &sig);
    }

    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&global_mock, chunk, clen);

    /* ---- Configure the server ---- */
    mu_server_config_t config;
    (void)memset(&config, 0, sizeof(config));
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
    config.tcp_adapter.context = &global_mock;
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
    (void)memset(&crypto_adapter, 0, sizeof(crypto_adapter));
    crypto_adapter.get_own_certificate = mock_get_own_certificate;
    crypto_adapter.rsa_sha256_verify = mock_rsa_sha256_verify;
    config.crypto_adapter = &crypto_adapter;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> Response */
    s_channel_id = read_opn_channel_id(global_mock.last_write, global_mock.last_write_len);
    patch_msg_channel_ids(&global_mock, s_channel_id);
    mu_server_poll(server); /* CreateSession -> Response */

    mu_server_poll(server); /* ActivateSession -> Response (Success) */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, global_mock.last_write, global_mock.last_write_len);
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
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, res);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_certificate_user_auth_flow);
    return UNITY_END();
}
