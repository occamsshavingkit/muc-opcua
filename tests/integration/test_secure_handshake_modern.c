/* tests/integration/test_secure_handshake_modern.c
 * Verifies that secure channels can be established and read from using
 * Aes128_Sha256_RsaOaep and Aes256_Sha256_RsaPss policies. */
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

#ifdef MUC_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"
#include "security/asym_chunk.h"
#include "security/security_policy.h"
#include "security/sym_chunk.h"

void setUp(void) {}
void tearDown(void) {}

#define CHUNK_CAP 8192
#define MAX_INBOUND 8
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
    memcpy(buf, m->inbound[m->read_index], len);
    m->read_index++;
    *n = len;
    return MU_STATUS_GOOD;
}
static opcua_statuscode_t mock_write(void *c, void *h, const opcua_byte_t *buf, size_t len, size_t *n) {
    mock_t *m = (mock_t *)c;
    (void)h;
    memcpy(m->last_write, buf, len);
    m->last_write_len = len;
    *n = len;
    return MU_STATUS_GOOD;
}

static void enqueue(mock_t *m, const opcua_byte_t *bytes, size_t len) {
    memcpy(m->inbound[m->inbound_count], bytes, len);
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

static opcua_uint32_t parse_decoded(const opcua_byte_t *body, size_t len, mu_binary_reader_t *out) {
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

static void secure_call(mock_t *mock, mu_server_t *server, mu_crypto_adapter_t *cc, const mu_sym_keys_t *c2s,
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
    memset(&si, 0, sizeof(si));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_unwrap(cc, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, s2c,
                                                          mock->last_write, mock->last_write_len, &rbody, &rblen, &si));
    opcua_uint32_t type = parse_decoded(rbody, rblen, resp);
    TEST_ASSERT_EQUAL(expect_type, type);
}

static opcua_uint32_t parse_create_session_auth_token(mu_binary_reader_t *resp) {
    mu_nodeid_t session_id;
    mu_nodeid_t auth_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(resp, &session_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(resp, &auth_token));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, session_id.identifier_type);
    TEST_ASSERT_EQUAL(0, session_id.namespace_index);
    TEST_ASSERT_NOT_EQUAL(0, session_id.identifier.numeric);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, auth_token.identifier_type);
    TEST_ASSERT_EQUAL(0, auth_token.namespace_index);
    TEST_ASSERT_NOT_EQUAL(0, auth_token.identifier.numeric);
    return auth_token.identifier.numeric;
}

static void run_handshake_for_policy(mu_security_policy_id_t policy_id, int tamper_signature, int allow_untrusted,
                                     int wrong_algorithm) {
    mock_t mock;
    memset(&mock, 0, sizeof(mock));

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

    static const mu_reference_t var_refs[] = {{{0, MU_NODEID_NUMERIC, {35}}, {0, MU_NODEID_NUMERIC, {85}}, false}};
    static const mu_value_source_t var_value = {MU_VALUESOURCE_STATIC, {.static_value = {MU_TYPE_INT32, {.i32 = 42}}}};
    static const mu_node_t nodes[] = {{{0, MU_NODEID_NUMERIC, {85}},
                                       MU_NODECLASS_OBJECT,
                                       {7, (const opcua_byte_t *)"Objects"},
                                       {7, (const opcua_byte_t *)"Objects"},
                                       NULL,
                                       0,
                                       NULL},
                                      {{1, MU_NODEID_NUMERIC, {1000}},
                                       MU_NODECLASS_VARIABLE,
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       {6, (const opcua_byte_t *)"MyVar1"},
                                       var_refs,
                                       1,
                                       &var_value}};
    static const mu_address_space_t space = {nodes, 2};

    opcua_byte_t tmp[1024], chunk[CHUNK_CAP];
    mu_binary_writer_t w;
    size_t clen;
    opcua_byte_t client_nonce[32];
    for (int i = 0; i < 32; i++)
        client_nonce[i] = (opcua_byte_t)(i + 1);

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
    {
        mu_string_t url = {19, (const opcua_byte_t *)"opc.tcp://host:4840"};
        mu_binary_write_string(&w, &url);
    }
    {
        mu_binary_writer_t hs;
        mu_binary_writer_init(&hs, tmp, sizeof(tmp));
        hs.position = 4;
        mu_binary_write_uint32(&hs, (opcua_uint32_t)w.position);
    }
    enqueue(&mock, tmp, w.position);

    /* OPN: OpenSecureChannelRequest */
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
                      mu_asym_chunk_wrap(&client_crypto, policy_id, 0, 1, 1, server_cert, server_cert_len, tmp,
                                         w.position, chunk, sizeof(chunk), &clen));
    enqueue(&mock, chunk, clen);

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
    config.crypto_adapter = &server_crypto;
    config.address_space = &space;
    /* Secured policies require application authentication: either a trust list
       containing the client cert (fail-closed default), or an explicit opt-in to
       accept untrusted clients (the demo/interop path). Exercise both. */
    const opcua_byte_t *trusted_certs[1] = {client_cert};
    const size_t trusted_lens[1] = {client_cert_len};
    mu_trust_list_t trust = {trusted_certs, trusted_lens, 1};
    if (allow_untrusted) {
        config.trust_list = NULL;
        config.allow_untrusted_clients = 1;
    } else {
        config.trust_list = &trust;
    }

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.last_write, 4);

    mu_server_poll(server); /* OPN -> secured OpenSecureChannelResponse */
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock.last_write, 4);

    opcua_byte_t opn_body[8192];
    size_t opn_body_len = 0;
    mu_asym_chunk_info_t ai;
    memset(&ai, 0, sizeof(ai));
    opcua_byte_t scratch[6144];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_asym_chunk_unwrap(&client_crypto, mock.last_write, mock.last_write_len, opn_body,
                                           sizeof(opn_body), &opn_body_len, scratch, sizeof(scratch), &ai));
    TEST_ASSERT_EQUAL(policy_id, ai.policy);

    mu_binary_reader_t br;
    TEST_ASSERT_EQUAL(MU_ID_OPENSECURECHANNELRESPONSE, parse_decoded(opn_body, opn_body_len, &br));
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

    mu_sym_keys_t c2s, s2c;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_keys_derive(&client_crypto, policy_id, server_nonce.data, 32, client_nonce, 32, &c2s));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_keys_derive(&client_crypto, policy_id, client_nonce, 32, server_nonce.data, 32, &s2c));

    mu_binary_reader_t resp;

    /* GetEndpoints over the secured channel. */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_GETENDPOINTSREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 2);
    {
        mu_string_t null_str = {-1, NULL};
        mu_binary_write_string(&w, &null_str);
        mu_binary_write_int32(&w, 0);
        mu_binary_write_int32(&w, 0);
    }
    secure_call(&mock, server, &client_crypto, &c2s, &s2c, scid, token_id, 2, tmp, w.position,
                MU_ID_GETENDPOINTSRESPONSE, &resp);

    /* CreateSession */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 3);
    {
        mu_string_t ns = {-1, NULL};
        mu_binary_write_string(&w, &ns);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_byte(&w, 0x00);
        mu_binary_write_uint32(&w, 1);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_int32(&w, 0);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_string(&w, &ns);
        mu_bytestring_t cn = {32, client_nonce};
        mu_binary_write_bytestring(&w, &cn);
        mu_bytestring_t cc = {(opcua_int32_t)client_cert_len, client_cert};
        mu_binary_write_bytestring(&w, &cc);
        mu_binary_write_double(&w, 60000.0);
        mu_binary_write_uint32(&w, 0);
    }
    secure_call(&mock, server, &client_crypto, &c2s, &s2c, scid, token_id, 3, tmp, w.position,
                MU_ID_CREATESESSIONRESPONSE, &resp);
    opcua_uint32_t session_auth_token = parse_create_session_auth_token(&resp);

    /* The CreateSession response reader is now positioned at RevisedSessionTimeout,
       followed by the session ServerNonce. Extract the nonce so the client can
       produce a valid application ClientSignature over (serverCert || serverNonce)
       for ActivateSession (OPC-10000-4 §5.7.3). */
    opcua_uint64_t revised_bits;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint64(&resp, &revised_bits));
    mu_bytestring_t cs_server_nonce;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&resp, &cs_server_nonce));
    TEST_ASSERT_EQUAL(32, cs_server_nonce.length);
    opcua_byte_t sess_nonce[32];
    memcpy(sess_nonce, cs_server_nonce.data, 32);

    /* Sign the ClientSignature with the scheme the negotiated policy requires
       (feature 026): RSA-PSS for Aes256_Sha256_RsaPss, RSA-PKCS#1.5 otherwise, and
       advertise the matching algorithm URI. */
    static const char SIG_ALG_PKCS15[] = "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
    static const char SIG_ALG_PSS[] = "http://opcfoundation.org/UA/security/rsa-pss-sha2-256";
    bool use_pss = (policy_id == MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID);
    const char *sig_alg = use_pss ? SIG_ALG_PSS : SIG_ALG_PKCS15;
    /* Negative path for feature 026: advertise the wrong algorithm URI for the
       policy (the server must reject on the algorithm mismatch). */
    if (wrong_algorithm) {
        sig_alg = use_pss ? SIG_ALG_PKCS15 : SIG_ALG_PSS;
    }
    opcua_byte_t to_sign[1536];
    TEST_ASSERT_TRUE(server_cert_len + 32 <= sizeof(to_sign));
    memcpy(to_sign, server_cert, server_cert_len);
    memcpy(to_sign + server_cert_len, sess_nonce, 32);
    opcua_byte_t client_sig[512];
    size_t client_sig_len = sizeof(client_sig);
    opcua_statuscode_t sign_rc =
        use_pss ? client_crypto.rsa_pss_sha256_sign(client_crypto.context, to_sign, server_cert_len + 32, client_sig,
                                                    &client_sig_len)
                : client_crypto.rsa_sha256_sign(client_crypto.context, to_sign, server_cert_len + 32, client_sig,
                                                &client_sig_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, sign_rc);

    /* Negative path: corrupt the signature so verification must fail
       (OPC-10000-4 §5.7.3 — the server must reject an unproven activation). */
    if (tamper_signature && client_sig_len > 0) {
        client_sig[0] ^= 0xFFu;
    }

    /* ActivateSession (anonymous, with a valid application ClientSignature). */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, session_auth_token, 4);
    {
        mu_string_t sig_alg_str = {(opcua_int32_t)strlen(sig_alg), (const opcua_byte_t *)sig_alg};
        mu_bytestring_t sig_bs = {(opcua_int32_t)client_sig_len, client_sig};
        mu_binary_write_string(&w, &sig_alg_str);
        mu_binary_write_bytestring(&w, &sig_bs);
        mu_binary_write_int32(&w, 0);
        mu_binary_write_int32(&w, 0);
        mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {321}};
        mu_binary_write_extension_object_header(&w, &anon, 0);
    }
    secure_call(&mock, server, &client_crypto, &c2s, &s2c, scid, token_id, 4, tmp, w.position,
                MU_ID_ACTIVATESESSIONRESPONSE, &resp);

    /* Read MyVar1 Value -> 42 */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_READREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, session_auth_token, 5);
    mu_binary_write_double(&w, 0.0);
    mu_binary_write_uint32(&w, 3);
    mu_binary_write_int32(&w, 1);
    {
        mu_nodeid_t v = {1, MU_NODEID_NUMERIC, {1000}};
        mu_string_t ns = {-1, NULL};
        mu_binary_write_nodeid(&w, &v);
        mu_binary_write_uint32(&w, 13);
        mu_binary_write_string(&w, &ns);
        mu_binary_write_uint16(&w, 0);
        mu_binary_write_string(&w, &ns);
    }
    /* When the ClientSignature was tampered or advertised the wrong algorithm URI,
       activation must have failed, so a subsequent Read on the (still unactivated)
       session is answered with a ServiceFault rather than a ReadResponse. */
    int expect_reject = tamper_signature || wrong_algorithm;
    secure_call(&mock, server, &client_crypto, &c2s, &s2c, scid, token_id, 5, tmp, w.position,
                expect_reject ? MU_ID_SERVICEFAULT : MU_ID_READRESPONSE, &resp);

    if (!expect_reject) {
        opcua_int32_t nres;
        mu_binary_read_int32(&resp, &nres);
        TEST_ASSERT_EQUAL(1, nres);
        opcua_byte_t mask;
        mu_binary_read_byte(&resp, &mask);
        TEST_ASSERT_EQUAL(0x01, mask);
        opcua_byte_t vt;
        mu_binary_read_byte(&resp, &vt);
        TEST_ASSERT_EQUAL(MU_TYPE_INT32, vt);
        opcua_int32_t val;
        mu_binary_read_int32(&resp, &val);
        TEST_ASSERT_EQUAL(42, val);
    }

    mu_server_close(server);
    mu_host_crypto_adapter_cleanup(&server_crypto);
    mu_host_crypto_adapter_cleanup(&client_crypto);
}

void test_secure_handshake_aes128_oaep(void) {
    run_handshake_for_policy(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID, 0, 0, 0);
}

void test_secure_handshake_aes256_pss(void) {
    run_handshake_for_policy(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID, 0, 0, 0);
}

/* OPC-10000-4 §5.7.3/§7.38.2: a bad application ClientSignature must block
   activation on a secured channel. */
void test_secure_handshake_rejects_tampered_signature(void) {
    run_handshake_for_policy(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID, 1, 0, 0);
}

/* Feature 025 (F3): with no trust list but allow_untrusted_clients set, a secured
   handshake from a client presenting a self-signed cert still completes. This is
   the demo/interop path (the .NET reference client generates its own cert) and
   guards against the fail-closed trust check regressing interop. */
void test_secure_handshake_allow_untrusted_client(void) {
    run_handshake_for_policy(MU_SECURITY_POLICY_AES128_SHA256_RSAOAEP_ID, 0, 1, 0);
}

/* Feature 026 (FR-005): a ClientSignature advertising the wrong algorithm URI for
   the negotiated policy must be rejected (here: PKCS#1.5 URI on a PSS channel). */
void test_secure_handshake_rejects_wrong_algorithm(void) {
    run_handshake_for_policy(MU_SECURITY_POLICY_AES256_SHA256_RSAPSS_ID, 0, 0, 1);
}

/* OPC-10000-4 §7.37 / OPC-10000-7 SecurityPolicy: a ClientSignature advertising
   RSA-PSS on a PKCS#1.5 Basic256Sha256 channel is also a policy mismatch. */
void test_secure_handshake_rejects_pss_algorithm_on_basic256sha256(void) {
    run_handshake_for_policy(MU_SECURITY_POLICY_BASIC256SHA256_ID, 0, 0, 1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_secure_handshake_aes128_oaep);
    RUN_TEST(test_secure_handshake_aes256_pss);
    RUN_TEST(test_secure_handshake_rejects_tampered_signature);
    RUN_TEST(test_secure_handshake_allow_untrusted_client);
    RUN_TEST(test_secure_handshake_rejects_wrong_algorithm);
    RUN_TEST(test_secure_handshake_rejects_pss_algorithm_on_basic256sha256);
    return UNITY_END();
}

#else
void setUp(void) {}
void tearDown(void) {}
void test_secure_handshake_skipped(void) {
    TEST_IGNORE_MESSAGE("OpenSSL not available");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_secure_handshake_skipped);
    return UNITY_END();
}
#endif
