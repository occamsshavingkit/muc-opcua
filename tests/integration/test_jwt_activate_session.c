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

#if defined(MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
#include "platform/host_crypto_adapter.h"
#include "security/asym_chunk.h"
#include "security/security_policy.h"
#include "security/sym_chunk.h"
#endif

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
#define CHUNK_CAP 8192
typedef struct {
    int accept_count;
    opcua_byte_t inbound[MAX_INBOUND][CHUNK_CAP];
    size_t inbound_len[MAX_INBOUND];
    size_t inbound_count;
    size_t read_index;
    opcua_byte_t last_write[CHUNK_CAP];
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
    /* Defensive bounds check: the mock's last_write buffer is fixed-size; a
       negative test that somehow produced an oversized write would otherwise
       corrupt heap/stack. */
    TEST_ASSERT_TRUE(len <= sizeof(m->last_write));
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

void test_activate_session_jwt_over_securitypolicy_none_is_rejected(void) {
    /* Spec 093 FR-008 / OPC-10000-4 §7.40.2.1: JWT bearer tokens MUST NOT be
       accepted over SecurityPolicy#None (unencrypted connections). This test
       feeds an otherwise-valid JWT over a None secure channel and asserts the
       server rejects it with Bad_IdentityTokenRejected before any JWT
       validation runs. End-to-end JWT-success coverage requires a non-None
       SecurityPolicy variant of this fixture (tracked separately). */
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

    /* Expect Bad_IdentityTokenRejected -- JWT bearer tokens must not be
       accepted over SecurityPolicy#None (spec 093 FR-008). */
    assert_response_service_result(mock.last_write, mock.last_write_len, MU_ID_ACTIVATESESSIONRESPONSE, 3,
                                   MU_STATUS_BAD_IDENTITYTOKENREJECTED);

    /* And the session must NOT have transitioned to Activated. */
    TEST_ASSERT_NOT_EQUAL(MU_SESSION_STATE_ACTIVATED, server->sessions[0].state);
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

    /* The expired JWT would fail JWT validation with Bad_IdentityTokenInvalid,
       but the SecurityPolicy#None guard (spec 093 FR-008) rejects first with
       Bad_IdentityTokenRejected. Verifies the policy check takes precedence
       over JWT expiry validation. */
    assert_response_service_result(mock.last_write, mock.last_write_len, MU_ID_ACTIVATESESSIONRESPONSE, 3,
                                   MU_STATUS_BAD_IDENTITYTOKENREJECTED);

    /* Session must NOT have transitioned to Activated. */
    TEST_ASSERT_NOT_EQUAL(MU_SESSION_STATE_ACTIVATED, server->sessions[0].state);
}

#if defined(MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
/* ------------------------------------------------------------------ */
/* Secure-channel success path (spec 093 SC-001). Reuses the JWT        */
/* fixture above but drives HEL/OPN/MSG through Basic256Sha256         */
/* SignAndEncrypt so the server's SecurityPolicy#None guard passes and  */
/* the JWT validator actually runs to a Good result.                   */
/* ------------------------------------------------------------------ */

/* Skip a decrypted response's type NodeId + ResponseHeader; return the type id. */
static opcua_uint32_t parse_decoded_secure(const opcua_byte_t *body, size_t len, mu_binary_reader_t *out) {
    mu_binary_reader_t r;
    mu_binary_reader_init(&r, body, len);
    mu_nodeid_t type;
    mu_binary_read_nodeid(&r, &type);
    opcua_int64_t ts;
    opcua_uint32_t h;
    opcua_statuscode_t res;
    opcua_byte_t diag, enc;
    opcua_int32_t st;
    mu_nodeid_t addl;
    mu_binary_read_int64(&r, &ts);
    mu_binary_read_uint32(&r, &h);
    mu_binary_read_statuscode(&r, &res);
    mu_binary_read_byte(&r, &diag);
    mu_binary_read_int32(&r, &st);
    mu_binary_read_nodeid(&r, &addl);
    mu_binary_read_byte(&r, &enc);
    *out = r;
    return type.identifier.numeric;
}

/* Wrap a service body as a secured MSG, drive one poll, and unwrap+parse the reply. */
static void secure_call(mock_t *mock, mu_server_t *server, const mu_crypto_adapter_t *cc, const mu_sym_keys_t *c2s,
                        const mu_sym_keys_t *s2c, opcua_uint32_t scid, opcua_uint32_t token, opcua_uint32_t seq,
                        const opcua_byte_t *body, size_t body_len, opcua_uint32_t expect_type,
                        mu_binary_reader_t *resp) {
    opcua_byte_t chunk[CHUNK_CAP];
    size_t mlen = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap(cc, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, c2s, "MSG", scid,
                                                        token, seq, seq, body, body_len, chunk, sizeof(chunk), &mlen));
    mock->inbound_count = 0;
    mock->read_index = 0;
    enqueue(mock, chunk, mlen);
    mu_server_poll(server);
    const opcua_byte_t *rbody = NULL;
    size_t rblen = 0;
    mu_sym_chunk_info_t si;
    (void)memset(&si, 0, sizeof(si));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_unwrap(cc, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, s2c,
                                                          mock->last_write, mock->last_write_len, &rbody, &rblen, &si));
    TEST_ASSERT_EQUAL(expect_type, parse_decoded_secure(rbody, rblen, resp));
}

void test_activate_session_with_valid_jwt_succeeds_over_secure_channel(void) {
    /* Spec 093 SC-001 / OPC-10000-4 §5.7.3: an IssuedIdentityToken carrying
       a valid RS256 JWT MUST activate the session when the secure channel
       uses a non-None SecurityPolicy. This is the success-path counterpart
       to the two SecurityPolicy#None rejection tests above. */
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    static mu_crypto_adapter_t server_crypto, client_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&server_crypto));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&client_crypto));
    const opcua_byte_t *server_cert = NULL;
    size_t server_cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      server_crypto.get_own_certificate(server_crypto.context, &server_cert, &server_cert_len));
    const opcua_byte_t *client_cert = NULL;
    size_t client_cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      client_crypto.get_own_certificate(client_crypto.context, &client_cert, &client_cert_len));

    opcua_byte_t tmp[2048], chunk[CHUNK_CAP];
    mu_binary_writer_t w;
    size_t clen;
    opcua_byte_t client_nonce[32];
    for (int i = 0; i < 32; i++) {
        client_nonce[i] = (opcua_byte_t)(i + 1);
    }

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
    {
        mu_string_t url = {20, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&w, &url);
    }
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, tmp, w.position);

    /* ---- OPN: OpenSecureChannelRequest, Basic256Sha256 / SignAndEncrypt, asymmetrically wrapped ---- */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_OPENSECURECHANNELREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 1);
    mu_binary_write_uint32(&w, 0); /* ClientProtocolVersion */
    mu_binary_write_uint32(&w, 0); /* RequestType ISSUE */
    mu_binary_write_uint32(&w, 3); /* SecurityMode SignAndEncrypt */
    {
        mu_bytestring_t cn = {32, client_nonce};
        mu_binary_write_bytestring(&w, &cn);
    }
    mu_binary_write_uint32(&w, 3600000); /* RequestedLifetime */
    clen = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_asym_chunk_wrap(&(mu_asym_wrap_params_t){.crypto = &client_crypto,
                                                                  .policy = MU_SECURITY_POLICY_BASIC256SHA256_ID,
                                                                  .secure_channel_id = 0,
                                                                  .sequence_number = 1,
                                                                  .request_id = 1,
                                                                  .receiver_cert = server_cert,
                                                                  .receiver_cert_len = server_cert_len,
                                                                  .body = tmp,
                                                                  .body_len = w.position,
                                                                  .out = chunk,
                                                                  .out_cap = sizeof(chunk),
                                                                  .out_len = &clen}));
    enqueue(&mock, chunk, clen);

    /* ---- Server config: crypto adapter, trust list, JWT issuer ---- */
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
    config.crypto_adapter = &server_crypto;
    /* Secured policies require a configured trust list (fail-closed). Trust the
       test client's application-instance certificate. */
    const opcua_byte_t *trusted_certs[1] = {client_cert};
    const size_t trusted_lens[1] = {client_cert_len};
    mu_trust_list_t trust = {trusted_certs, trusted_lens, 1};
    config.trust_list = &trust;

    /* Trusted JWT issuer (RS256) -- reused fixture. */
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
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.last_write, 4);
    mu_server_poll(server); /* OPN -> secured OpenSecureChannelResponse */
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock.last_write, 4);

    /* ---- Unwrap the OPN response; derive symmetric keys (client view) ---- */
    opcua_byte_t opn_body[8192];
    size_t opn_body_len = 0;
    mu_asym_chunk_info_t ai;
    (void)memset(&ai, 0, sizeof(ai));
    opcua_byte_t scratch[6144];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_asym_chunk_unwrap(&(mu_asym_unwrap_params_t){.crypto = &client_crypto,
                                                                                      .chunk = mock.last_write,
                                                                                      .chunk_len = mock.last_write_len,
                                                                                      .out_body = opn_body,
                                                                                      .out_cap = sizeof(opn_body),
                                                                                      .out_body_len = &opn_body_len,
                                                                                      .scratch = scratch,
                                                                                      .scratch_len = sizeof(scratch),
                                                                                      .info = &ai}));
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_BASIC256SHA256_ID, ai.policy);

    mu_binary_reader_t br;
    TEST_ASSERT_EQUAL(MU_ID_OPENSECURECHANNELRESPONSE, parse_decoded_secure(opn_body, opn_body_len, &br));
    opcua_uint32_t spv, scid, token_id, revised;
    opcua_int64_t created;
    mu_binary_read_uint32(&br, &spv);
    mu_binary_read_uint32(&br, &scid);
    mu_binary_read_uint32(&br, &token_id);
    mu_binary_read_int64(&br, &created);
    mu_binary_read_uint32(&br, &revised);
    mu_bytestring_t server_nonce;
    mu_binary_read_bytestring(&br, &server_nonce);
    TEST_ASSERT_EQUAL(32, server_nonce.length);

    mu_sym_keys_t c2s, s2c; /* client->server (we send), server->client (we receive) */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                                                         server_nonce.data, 32, client_nonce, 32, &c2s));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                                                         client_nonce, 32, server_nonce.data, 32, &s2c));

    mu_binary_reader_t resp;

    /* ---- CreateSession with a full ClientDescription + ClientNonce + ClientCertificate,
       so the server computes a real ServerSignature over (ClientCert || ClientNonce).
       The ApplicationUri matches the client cert's Subject CN ("muc-opcua") so the
       OPC-10000-4 §5.7.2.1 binding check passes. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    {
        mu_string_t ns = {-1, NULL};
        mu_string_t app_uri = {9, (const opcua_byte_t *)"muc-opcua"};
        mu_binary_write_string(&w, &app_uri); /* ClientDescription.applicationUri */
        mu_binary_write_string(&w, &ns);      /* productUri */
        mu_binary_write_byte(&w, 0x00);       /* applicationName LocalizedText: no fields */
        mu_binary_write_uint32(&w, 1);        /* applicationType Client */
        mu_binary_write_string(&w, &ns);      /* gatewayServerUri */
        mu_binary_write_string(&w, &ns);      /* discoveryProfileUri */
        mu_binary_write_int32(&w, 0);         /* discoveryUrls[] */
        mu_binary_write_string(&w, &ns);      /* ServerUri */
        mu_binary_write_string(&w, &ns);      /* EndpointUrl */
        mu_binary_write_string(&w, &ns);      /* SessionName */
        mu_bytestring_t cn = {32, client_nonce};
        mu_binary_write_bytestring(&w, &cn); /* ClientNonce */
        mu_bytestring_t cc = {(opcua_int32_t)client_cert_len, client_cert};
        mu_binary_write_bytestring(&w, &cc); /* ClientCertificate */
        mu_binary_write_double(&w, 60000.0); /* RequestedSessionTimeout */
        mu_binary_write_uint32(&w, 0);
    } /* MaxResponseMessageSize */
    secure_call(&mock, server, &client_crypto, &c2s, &s2c, scid, token_id, 2, tmp, w.position,
                MU_ID_CREATESESSIONRESPONSE, &resp);

    /* Read SessionId, AuthenticationToken, RevisedSessionTimeout, then the
       ServerNonce so we can compute ClientSignature = sign(serverCert || serverNonce). */
    {
        mu_nodeid_t cs_sid, cs_tok;
        opcua_uint64_t cs_revised;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp, &cs_sid));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp, &cs_tok));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint64(&resp, &cs_revised));
    }
    mu_bytestring_t cs_server_nonce;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&resp, &cs_server_nonce));
    TEST_ASSERT_EQUAL(32, cs_server_nonce.length);
    opcua_byte_t sess_nonce[32];
    (void)memcpy(sess_nonce, cs_server_nonce.data, 32);

    /* ClientSignature over (server_cert || server_nonce) per OPC-10000-4 §5.7.3. */
    static const char SIG_ALG[] = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
    opcua_byte_t to_sign[1536];
    TEST_ASSERT_TRUE(server_cert_len + 32 <= sizeof(to_sign));
    (void)memcpy(to_sign, server_cert, server_cert_len);
    (void)memcpy(to_sign + server_cert_len, sess_nonce, 32);
    opcua_byte_t client_sig[512];
    size_t client_sig_len = sizeof(client_sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, client_crypto.rsa_sha256_sign(client_crypto.context, to_sign,
                                                                    server_cert_len + 32, client_sig, &client_sig_len));

    /* ---- Build a valid RS256 JWT. fake_platform time returns DateTime ticks=0
     * -> server_time_unix = -11644473600, so any positive exp is in the
     * future relative to the server clock and passes the exp check. */
    char jwt_buf[1600];
    const char *header = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
    char payload[512];
    int pn = snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"exp\":%lld,\"iat\":1}",
                      TEST_ISSUER_URL, TEST_SUBJECT, TEST_AUDIENCE, (long long)TEST_EXP_OFFSET_SECONDS);
    TEST_ASSERT_TRUE((size_t)pn < sizeof(payload));
    (void)pn;
    size_t jwt_len = build_test_jwt(header, payload, jwt_buf, sizeof(jwt_buf));

    /* ---- ActivateSession with IssuedIdentityToken (NodeId 940) carrying the JWT ---- */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, TEST_FAKE_FIRST_AUTH_TOKEN, 3);
    {
        mu_string_t sig_alg = {(opcua_int32_t)(sizeof(SIG_ALG) - 1), (const opcua_byte_t *)SIG_ALG};
        mu_bytestring_t sig_bs = {(opcua_int32_t)client_sig_len, client_sig};
        mu_binary_write_string(&w, &sig_alg);    /* clientSignature.algorithm */
        mu_binary_write_bytestring(&w, &sig_bs); /* clientSignature.signature */
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
    /* userTokenSignature: null (JWT bearer tokens do not require a user-token signature). */
    {
        mu_string_t null_string = {-1, NULL};
        mu_bytestring_t null_bytes = {-1, NULL};
        mu_binary_write_string(&w, &null_string);
        mu_binary_write_bytestring(&w, &null_bytes);
    }

    secure_call(&mock, server, &client_crypto, &c2s, &s2c, scid, token_id, 3, tmp, w.position,
                MU_ID_ACTIVATESESSIONRESPONSE, &resp);

    /* parse_decoded_secure consumed the ResponseHeader from `resp`; re-scan the
       decrypted body from the start to read the service result. resp->buffer
       still points at the full decrypted body returned by mu_sym_chunk_unwrap. */
    {
        mu_binary_reader_t hdr;
        mu_binary_reader_init(&hdr, resp.buffer, resp.length);
        mu_nodeid_t type;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&hdr, &type));
        TEST_ASSERT_EQUAL(MU_ID_ACTIVATESESSIONRESPONSE, type.identifier.numeric);
        opcua_int64_t ts;
        opcua_uint32_t handle;
        opcua_statuscode_t service_result;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&hdr, &ts));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&hdr, &handle));
        TEST_ASSERT_EQUAL(3u, handle);
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&hdr, &service_result));
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, service_result);
    }

    /* Spec 093 SC-001: session MUST have transitioned to Activated. */
    TEST_ASSERT_EQUAL(MU_SESSION_STATE_ACTIVATED, server->sessions[0].state);

    mu_sym_keys_release_cipher(&c2s);
    mu_sym_keys_release_cipher(&s2c);
    mu_server_close(server);
    mu_host_crypto_adapter_cleanup(&server_crypto);
    mu_host_crypto_adapter_cleanup(&client_crypto);
}
#endif /* MUC_OPCUA_SECURE_CHANNEL_CRYPTO */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_activate_session_jwt_over_securitypolicy_none_is_rejected);
    RUN_TEST(test_activate_session_with_expired_jwt_is_rejected);
#if defined(MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
    RUN_TEST(test_activate_session_with_valid_jwt_succeeds_over_secure_channel);
#endif
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
