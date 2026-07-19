/* tests/integration/test_jwt_activate_session.c
 *
 * End-to-end integration test for spec 093 (User Story 1):
 *   Create a server with a JWT issuer configured (test RSA key)
 *   -> CreateSession
 *   -> ActivateSession with an IssuedIdentityToken carrying a valid RS256 JWT
 *   -> Verify session activates with Good status (spec.md SC-001).
 *
 * Spec grounding:
 *   OPC-10000-4 §5.7.3 -- ActivateSession with JWT UserIdentityToken
 *   OPC-10000-4 §7.40.6 -- IssuedIdentityToken structure (policyId/tokenData/encryptionAlgorithm)
 *   OPC-10000-6 §5.2.3 / §6.5.2.1 -- JWT as the IssuedIdentityToken body
 *   OPC-10000-7 CU 1697 -- User Token JWT Server Facet
 *   RFC 7515 §5 / RFC 7519 §4 -- JWS compact serialization, registered claims
 */
#include "../../src/core/server_internal.h"
#include "../../src/services/discovery.h"
#include "fake_platform.h"
#include "muc_opcua/config.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#if MUC_OPCUA_CU_USER_TOKEN_JWT

#include <stdbool.h>
#include <string.h>

#if defined(MUC_OPCUA_HAVE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/x509.h>

#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* JWT fixture: one RSA-2048 keypair, a DER public key for the server  */
/* config, and a helper to build a compact JWS token signed by it.     */
/* ------------------------------------------------------------------ */

static EVP_PKEY *s_signing_key = NULL;
static unsigned char *s_trusted_der = NULL;
static size_t s_trusted_der_len = 0;

static const char s_b64url_tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void b64url_encode(const unsigned char *in, size_t in_len, char *out) {
    size_t i = 0, o = 0;
    while (i + 2 < in_len) {
        unsigned int v = ((unsigned int)in[i] << 16) | ((unsigned int)in[i + 1] << 8) | (unsigned int)in[i + 2];
        out[o++] = s_b64url_tab[(v >> 18) & 0x3F];
        out[o++] = s_b64url_tab[(v >> 12) & 0x3F];
        out[o++] = s_b64url_tab[(v >> 6) & 0x3F];
        out[o++] = s_b64url_tab[v & 0x3F];
        i += 3;
    }
    if (i < in_len) {
        unsigned int v = (unsigned int)in[i] << 16;
        if (i + 1 < in_len) {
            v |= (unsigned int)in[i + 1] << 8;
        }
        out[o++] = s_b64url_tab[(v >> 18) & 0x3F];
        out[o++] = s_b64url_tab[(v >> 12) & 0x3F];
        if (i + 1 < in_len) {
            out[o++] = s_b64url_tab[(v >> 6) & 0x3F];
        }
    }
    out[o] = '\0';
}

static size_t build_test_jwt(const char *header_json, const char *payload_json, char *out, size_t out_max) {
    char header_b64[256];
    char payload_b64[1024];
    b64url_encode((const unsigned char *)header_json, strlen(header_json), header_b64);
    b64url_encode((const unsigned char *)payload_json, strlen(payload_json), payload_b64);

    char signing_input[1280];
    int si_len = snprintf(signing_input, sizeof(signing_input), "%s.%s", header_b64, payload_b64);
    TEST_ASSERT_GREATER_THAN(0, si_len);

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    TEST_ASSERT_NOT_NULL(mdctx);
    TEST_ASSERT_EQUAL(1, EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, s_signing_key));
    size_t sig_len = 0;
    TEST_ASSERT_EQUAL(1, EVP_DigestSign(mdctx, NULL, &sig_len, (const unsigned char *)signing_input, (size_t)si_len));
    unsigned char *sig = (unsigned char *)malloc(sig_len);
    TEST_ASSERT_NOT_NULL(sig);
    TEST_ASSERT_EQUAL(1, EVP_DigestSign(mdctx, sig, &sig_len, (const unsigned char *)signing_input, (size_t)si_len));
    EVP_MD_CTX_free(mdctx);

    char sig_b64[1024];
    b64url_encode(sig, sig_len, sig_b64);
    free(sig);

    int total = snprintf(out, out_max, "%s.%s", signing_input, sig_b64);
    TEST_ASSERT_TRUE((size_t)total < out_max);
    return (size_t)total;
}

#define TEST_ISSUER_URL "https://auth.example.com"
#define TEST_AUDIENCE "opcua-server"
#define TEST_SUBJECT "operator1"
#define TEST_EXP_OFFSET_SECONDS 3600

/* ------------------------------------------------------------------ */
/* Mock TCP transport (carries HEL/OPN/MSG chunks through the server) */
/* ------------------------------------------------------------------ */

#define MAX_INBOUND 8
typedef struct {
    int accept_count;
    opcua_byte_t inbound[MAX_INBOUND][4096];
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

/* Channel ID assigned by the server; read from the OPN response. */
static opcua_uint32_t s_channel_id = 0;

static opcua_uint32_t read_opn_channel_id(const opcua_byte_t *buf, size_t len) {
    if (len < 12) {
        return 1;
    }
    return (opcua_uint32_t)buf[8] | ((opcua_uint32_t)buf[9] << 8u) | ((opcua_uint32_t)buf[10] << 16u) |
           ((opcua_uint32_t)buf[11] << 24u);
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

static void write_create_session_body(mu_binary_writer_t *w) {
    mu_string_t null_string = {-1, NULL};
    mu_bytestring_t null_bytes = {-1, NULL};
    mu_binary_write_string(w, &null_string);    /* applicationUri */
    mu_binary_write_string(w, &null_string);    /* productUri */
    mu_binary_write_byte(w, 0x00);              /* applicationName */
    mu_binary_write_uint32(w, 1);               /* applicationType = Client */
    mu_binary_write_string(w, &null_string);    /* gatewayServerUri */
    mu_binary_write_string(w, &null_string);    /* discoveryProfileUri */
    mu_binary_write_int32(w, 0);                /* discoveryUrls[] */
    mu_binary_write_string(w, &null_string);    /* serverUri */
    mu_binary_write_string(w, &null_string);    /* endpointUrl */
    mu_binary_write_string(w, &null_string);    /* sessionName */
    mu_binary_write_bytestring(w, &null_bytes); /* clientNonce */
    mu_binary_write_bytestring(w, &null_bytes); /* clientCertificate */
    mu_binary_write_double(w, 0.0);             /* requestedSessionTimeout */
    mu_binary_write_uint32(w, 0);               /* maxResponseMessageSize */
}

static void assert_response_service_result(const opcua_byte_t *buf, size_t len, opcua_uint32_t expected_type,
                                           opcua_uint32_t expected_request_handle,
                                           opcua_statuscode_t expected_service_result) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, len);
    r.position = 24;

    mu_nodeid_t resp_type;
    opcua_uint64_t ts;
    opcua_uint32_t request_handle;
    opcua_statuscode_t service_result;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &resp_type));
    TEST_ASSERT_EQUAL(expected_type, resp_type.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&r, &ts));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &request_handle));
    TEST_ASSERT_EQUAL(expected_request_handle, request_handle);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &service_result));
    TEST_ASSERT_EQUAL_HEX32(expected_service_result, service_result);
}

/* ------------------------------------------------------------------ */
/* Test                                                                */
/* ------------------------------------------------------------------ */

void setUp(void) {
    if (s_signing_key == NULL) {
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        TEST_ASSERT_NOT_NULL(pctx);
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen_init(pctx));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen(pctx, &s_signing_key));
        EVP_PKEY_CTX_free(pctx);
    }
    if (s_trusted_der == NULL) {
        unsigned char *tmp = NULL;
        int len = i2d_PUBKEY(s_signing_key, &tmp);
        TEST_ASSERT_GREATER_THAN(0, len);
        s_trusted_der = tmp;
        s_trusted_der_len = (size_t)len;
    }
}

void tearDown(void) {}

void test_activate_session_with_valid_jwt_succeeds(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    opcua_byte_t tmp[2048];
    mu_binary_writer_t w;
    opcua_byte_t chunk[2048];
    size_t clen;

    /* ---- HEL ---- */
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

    /* ---- OPN (SecurityPolicy None) ---- */
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

    /* ---- Configure the server with a JWT issuer ---- */
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
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    /* Trusted JWT issuer (RS256). */
    mu_jwt_issuer_t issuer;
    memset(&issuer, 0, sizeof(issuer));
    issuer.issuer_url = TEST_ISSUER_URL;
    issuer.public_key = s_trusted_der;
    issuer.public_key_len = s_trusted_der_len;
    issuer.expected_audience = TEST_AUDIENCE;
    issuer.clock_skew_seconds = 0;
    issuer.alg = MU_JWT_ALG_RS256;
    config.jwt_issuers = &issuer;
    config.jwt_issuer_count = 1;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    mu_server_poll(server); /* OPN -> Response */
    s_channel_id = read_opn_channel_id(mock.last_write, mock.last_write_len);

    /* ---- CreateSession ---- */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    write_create_session_body(&w);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server); /* CreateSession -> Response */

    /* ---- Build a valid RS256 JWT (exp in the future relative to the
     * fake_platform time, which is the DateTime epoch = 1601-01-01 -> Unix 0
     * after conversion). The validator receives server_time_unix = -11644473600
     * from the time adapter, but any positive TEST_EXP_OFFSET_SECONDS is in the
     * future relative to that. */
    char jwt_buf[1600];
    const char *header = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
    char payload[512];
    int pn = snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"exp\":%lld,\"iat\":1}",
                      TEST_ISSUER_URL, TEST_SUBJECT, TEST_AUDIENCE, (long long)TEST_EXP_OFFSET_SECONDS);
    TEST_ASSERT_TRUE((size_t)pn < sizeof(payload));
    (void)pn;
    size_t jwt_len = build_test_jwt(header, payload, jwt_buf, sizeof(jwt_buf));

    /* ---- ActivateSession with IssuedIdentityToken ---- */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, server->sessions[0].auth_token, 3);
    {
        mu_string_t ns = {-1, NULL};
        mu_bytestring_t nb = {-1, NULL};
        mu_binary_write_string(&w, &ns);     /* clientSignature.algorithm */
        mu_binary_write_bytestring(&w, &nb); /* clientSignature.signature */
    }
    mu_binary_write_int32(&w, 0); /* clientSoftwareCertificates[] */
    mu_binary_write_int32(&w, 0); /* localeIds[] */
    {
        /* UserIdentityToken: ExtensionObject wrapping IssuedIdentityToken
           (NodeId i=940). Body fields: policyId, tokenData, encryptionAlgorithm. */
        mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {940}};
        mu_string_t policy_id = {9, (const opcua_byte_t *)"jwt-issuer"};
        mu_bytestring_t token_data = {(opcua_int32_t)jwt_len, (const opcua_byte_t *)jwt_buf};
        mu_string_t enc_alg = {-1, NULL}; /* null/empty per OPC-10000-4 §7.40.6 */

        /* body length = policyId (4 + 9) + tokenData (4 + jwt_len) + encAlg (4) */
        size_t body_len = (4u + 9u) + (4u + jwt_len) + 4u;
        mu_binary_write_extension_object_header(&w, &token_type, (opcua_uint32_t)body_len);
        mu_binary_write_string(&w, &policy_id);
        mu_binary_write_bytestring(&w, &token_data);
        mu_binary_write_string(&w, &enc_alg);
    }
    /* userTokenSignature: null */
    {
        mu_string_t null_string = {-1, NULL};
        mu_bytestring_t null_bytes = {-1, NULL};
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_bytestring(&w, &null_bytes);
    }

    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server); /* ActivateSession -> Response */

    /* Expect Good -- spec.md SC-001: valid JWT, session activated. */
    assert_response_service_result(mock.last_write, mock.last_write_len, MU_ID_ACTIVATESESSIONRESPONSE, 3,
                                   MU_STATUS_GOOD);

    /* And the session should be in the Activated state. */
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server->sessions[0].state);
}

void test_activate_session_with_expired_jwt_is_rejected(void) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    opcua_byte_t tmp[2048];
    mu_binary_writer_t w;
    opcua_byte_t chunk[2048];
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

    /* OPN */
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

    /* Server config with a JWT issuer. */
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
    config.tcp_adapter.context = &mock;
    config.tcp_adapter.listen = mock_listen;
    config.tcp_adapter.accept = mock_accept;
    config.tcp_adapter.read = mock_read;
    config.tcp_adapter.write = mock_write;
    config.tcp_adapter.close_connection = mock_close;
    config.tcp_adapter.shutdown = mock_shutdown;

    mu_jwt_issuer_t issuer;
    memset(&issuer, 0, sizeof(issuer));
    issuer.issuer_url = TEST_ISSUER_URL;
    issuer.public_key = s_trusted_der;
    issuer.public_key_len = s_trusted_der_len;
    issuer.expected_audience = TEST_AUDIENCE;
    issuer.clock_skew_seconds = 0;
    issuer.alg = MU_JWT_ALG_RS256;
    config.jwt_issuers = &issuer;
    config.jwt_issuer_count = 1;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server);
    mu_server_poll(server);
    mu_server_poll(server);
    s_channel_id = read_opn_channel_id(mock.last_write, mock.last_write_len);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    write_create_session_body(&w);
    clen = build_msg(chunk, sizeof(chunk), 2, 2, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);

    /* Build an expired JWT (exp 1 hour in the past, i.e. negative so even with
       the fake_platform time adapter, exp < server_time_unix). fake_platform
       returns DateTime=0 -> Unix=-11644473600. exp=-11644473600-3600 is older. */
    char jwt_buf[1600];
    const char *header = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
    char payload[512];
    long long expired = -11644473600LL - 3600;
    int pn = snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"exp\":%lld,\"iat\":1}",
                      TEST_ISSUER_URL, TEST_SUBJECT, TEST_AUDIENCE, (long long)expired);
    TEST_ASSERT_TRUE((size_t)pn < sizeof(payload));
    (void)pn;
    size_t jwt_len = build_test_jwt(header, payload, jwt_buf, sizeof(jwt_buf));

    /* ActivateSession with the expired JWT. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, server->sessions[0].auth_token, 3);
    {
        mu_string_t ns = {-1, NULL};
        mu_bytestring_t nb = {-1, NULL};
        mu_binary_write_string(&w, &ns);
        mu_binary_write_bytestring(&w, &nb);
    }
    mu_binary_write_int32(&w, 0);
    mu_binary_write_int32(&w, 0);
    {
        mu_nodeid_t token_type = {0, MU_NODEID_NUMERIC, {940}};
        mu_string_t policy_id = {9, (const opcua_byte_t *)"jwt-issuer"};
        mu_bytestring_t token_data = {(opcua_int32_t)jwt_len, (const opcua_byte_t *)jwt_buf};
        mu_string_t enc_alg = {-1, NULL};
        size_t body_len = (4u + 9u) + (4u + jwt_len) + 4u;
        mu_binary_write_extension_object_header(&w, &token_type, (opcua_uint32_t)body_len);
        mu_binary_write_string(&w, &policy_id);
        mu_binary_write_bytestring(&w, &token_data);
        mu_binary_write_string(&w, &enc_alg);
    }
    {
        mu_string_t null_string = {-1, NULL};
        mu_bytestring_t null_bytes = {-1, NULL};
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_bytestring(&w, &null_bytes);
    }

    clen = build_msg(chunk, sizeof(chunk), 3, 3, tmp, w.position);
    enqueue(&mock, chunk, clen);
    mu_server_poll(server);

    /* Expect Bad_IdentityTokenInvalid (spec.md SC-002). */
    assert_response_service_result(mock.last_write, mock.last_write_len, MU_ID_ACTIVATESESSIONRESPONSE, 3,
                                   MU_STATUS_BAD_IDENTITYTOKENINVALID);

    /* Session must NOT have transitioned to Activated. */
    TEST_ASSERT_NOT_EQUAL(MU_SESSION_STATE_ACTIVATED, server->sessions[0].state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_activate_session_with_valid_jwt_succeeds);
    RUN_TEST(test_activate_session_with_expired_jwt_is_rejected);
    return UNITY_END();
}

#else /* !MUC_OPCUA_HAVE_OPENSSL */

int main(void) {
    return 0;
}

#endif /* MUC_OPCUA_HAVE_OPENSSL */

#else /* !MUC_OPCUA_CU_USER_TOKEN_JWT */

int main(void) {
    return 0;
}

#endif /* MUC_OPCUA_CU_USER_TOKEN_JWT */
