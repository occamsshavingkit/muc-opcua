/* tests/integration/test_ecc_handshake_e2e.c
 * Full client<->server ECC secure-channel handshake for the spec-059
 * SecurityPolicies #ECC_curve25519 (ECC-A, ChaCha20-Poly1305 AEAD) and
 * #ECC_nistP256 (ECC-B, AES128-CBC + HMAC-SHA256). Like the RSA handshake test,
 * the "client" is the library's own primitives: it generates an ephemeral ECDH
 * keypair, sends its public key as the ClientNonce inside an ECC-signed OPN chunk,
 * derives the channel keys from ECDH+HKDF (OPC-10000-6 §6.8.1), then drives a
 * GetEndpoints request over the secured channel. Exercises the server OPN ECC
 * path (osc_handler), the ECC asym-chunk codec, and the AEAD/CBC MSG codecs. */
#include "../../src/core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

#if defined(MUC_OPCUA_HAVE_OPENSSL) && defined(MUC_OPCUA_ECC)
#include "muc_opcua/config.h"
#include "platform/host_crypto_adapter.h"
#include "security/asym_chunk.h"
#include "security/asym_ecc.h"
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

/* Decode a response body: read the type NodeId, then the ResponseHeader, leaving
   `*out` positioned at the first service-specific field (as the RSA e2e does). */
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

/* Wrap a request as the client (AEAD for curve25519, CBC otherwise), poll the
   server, and unwrap the response — tracking the per-direction last-SequenceNumber
   needed for the AEAD nonce. `*srv_last` is the server's previous outbound seq. */
static void secure_call(mock_t *mock, mu_server_t *server, mu_crypto_adapter_t *cc, mu_sym_mode_t sym_mode,
                        const mu_sym_keys_t *c2s, const mu_sym_keys_t *s2c, opcua_uint32_t scid, opcua_uint32_t token,
                        opcua_uint32_t seq, opcua_uint32_t *srv_last, const opcua_byte_t *body, size_t body_len,
                        opcua_uint32_t expect_type, mu_binary_reader_t *resp) {
    opcua_byte_t chunk[CHUNK_CAP];
    size_t mlen = 0;
    if (sym_mode == MU_SYM_MODE_AEAD_CHACHA20POLY1305) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap_aead(cc, c2s, "MSG", scid, token, seq, seq - 1u, seq, body,
                                                                 body_len, chunk, sizeof(chunk), &mlen));
    } else {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                          mu_sym_chunk_wrap(cc, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, c2s, "MSG", scid, token, seq,
                                            seq, body, body_len, chunk, sizeof(chunk), &mlen));
    }
    mock->inbound_count = 0;
    mock->read_index = 0;
    enqueue(mock, chunk, mlen);
    mu_server_poll(server);

    const opcua_byte_t *rbody = NULL;
    size_t rblen = 0;
    mu_sym_chunk_info_t si;
    (void)memset(&si, 0, sizeof(si));
    if (sym_mode == MU_SYM_MODE_AEAD_CHACHA20POLY1305) {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_unwrap_aead(cc, s2c, mock->last_write, mock->last_write_len,
                                                                   *srv_last, &rbody, &rblen, &si));
        *srv_last = si.sequence_number;
    } else {
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                          mu_sym_chunk_unwrap(cc, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, s2c, mock->last_write,
                                              mock->last_write_len, &rbody, &rblen, &si));
    }
    mu_binary_reader_t r;
    opcua_uint32_t type = parse_decoded(rbody, rblen, &r);
    if (resp) {
        *resp = r;
    }
    TEST_ASSERT_EQUAL(expect_type, type);
}

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

static void run_ecc_handshake(mu_security_policy_id_t policy_id, int tamper_signature) {
    mock_t mock;
    (void)memset(&mock, 0, sizeof(mock));

    mu_ecc_curve_t curve;
    TEST_ASSERT_TRUE(mu_security_policy_ecc_curve(policy_id, &curve));
    mu_sym_mode_t sym_mode = mu_security_policy_sym_mode(policy_id);
    size_t nonce_len = mu_security_policy_nonce_length(policy_id);

    static mu_crypto_adapter_t server_crypto, client_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&server_crypto));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&client_crypto));

    /* ECC identities (the RSA get_own_certificate is a different key). */
    const opcua_byte_t *server_cert = NULL, *client_cert = NULL;
    size_t server_cert_len = 0, client_cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, server_crypto.get_own_ecc_certificate(server_crypto.context, curve, &server_cert,
                                                                            &server_cert_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, client_crypto.get_own_ecc_certificate(client_crypto.context, curve, &client_cert,
                                                                            &client_cert_len));

    /* Client ephemeral ECDH keypair; its public key is the ClientNonce. */
    opcua_byte_t client_nonce[MU_ECC_MAX_PUBKEY_LENGTH];
    size_t client_nonce_len = sizeof(client_nonce);
    opcua_byte_t client_keypair[MU_ECC_KEYPAIR_CTX_SIZE];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, client_crypto.ecc_generate_ephemeral(client_crypto.context, curve, client_nonce,
                                                                           &client_nonce_len, client_keypair));
    TEST_ASSERT_EQUAL(nonce_len, client_nonce_len);

    opcua_byte_t tmp[2048], chunk[CHUNK_CAP];
    mu_binary_writer_t w;
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

    /* OPN: OpenSecureChannelRequest carrying the ephemeral ClientNonce. */
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
        mu_bytestring_t cn = {(opcua_int32_t)client_nonce_len, client_nonce};
        mu_binary_write_bytestring(&w, &cn);
    }
    mu_binary_write_uint32(&w, 3600000); /* RequestedLifetime */
    clen = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_asym_chunk_wrap(&(mu_asym_wrap_params_t){.crypto = &client_crypto,
                                                                                  .policy = policy_id,
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
    config.address_space = &space;
    /* Trust the client's ECC certificate (the application-authentication check runs
       against the OPN SenderCertificate). */
    const opcua_byte_t *trusted_certs[1] = {client_cert};
    const size_t trusted_lens[1] = {client_cert_len};
    mu_trust_list_t trust = {trusted_certs, trusted_lens, 1};
    config.trust_list = &trust;

    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_server_poll(server); /* accept */
    mu_server_poll(server); /* HEL -> ACK */
    TEST_ASSERT_EQUAL_MEMORY("ACKF", mock.last_write, 4);

    mu_server_poll(server); /* OPN -> secured OpenSecureChannelResponse */
    TEST_ASSERT_EQUAL_MEMORY("OPNF", mock.last_write, 4);

    /* Client unwraps + verifies the server's ECC-signed OPN response. */
    opcua_byte_t opn_body[4096];
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
    TEST_ASSERT_EQUAL(policy_id, ai.policy);
    opcua_uint32_t srv_last = ai.sequence_number; /* server OPN response seq */

    /* Decode the OpenSecureChannelResponse: skip to ServerNonce (the server's
       ephemeral public key). Response body layout follows write_response_prefix. */
    mu_binary_reader_t br;
    mu_binary_reader_init(&br, opn_body, opn_body_len);
    mu_nodeid_t rtype;
    mu_binary_read_nodeid(&br, &rtype);
    TEST_ASSERT_EQUAL(MU_ID_OPENSECURECHANNELRESPONSE, rtype.identifier.numeric);
    opcua_int64_t ts;
    opcua_uint32_t rh;
    opcua_statuscode_t res;
    opcua_byte_t diag, enc;
    opcua_int32_t st;
    mu_nodeid_t addl;
    mu_binary_read_int64(&br, &ts);
    mu_binary_read_uint32(&br, &rh);
    mu_binary_read_statuscode(&br, &res);
    mu_binary_read_byte(&br, &diag);
    mu_binary_read_int32(&br, &st);
    mu_binary_read_nodeid(&br, &addl);
    mu_binary_read_byte(&br, &enc);
    if (rtype.identifier.numeric != MU_ID_OPENSECURECHANNELRESPONSE) {
        fprintf(stderr, "OPN dispatch faulted: type=%u serviceResult=0x%08X\n", rtype.identifier.numeric,
                (unsigned)res);
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, res);
    opcua_uint32_t spv, scid, token_id, revised;
    opcua_int64_t created;
    mu_binary_read_uint32(&br, &spv);
    mu_binary_read_uint32(&br, &scid);
    mu_binary_read_uint32(&br, &token_id);
    mu_binary_read_int64(&br, &created);
    mu_binary_read_uint32(&br, &revised);
    mu_bytestring_t server_nonce;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&br, &server_nonce));
    TEST_ASSERT_EQUAL((opcua_int32_t)nonce_len, server_nonce.length);

    /* Client ECDH + HKDF: same shared secret both sides -> matching channel keys. */
    opcua_byte_t shared[MU_ECC_MAX_PUBKEY_LENGTH];
    size_t shared_len = sizeof(shared);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      client_crypto.ecc_ecdh_derive(client_crypto.context, curve, client_keypair, server_nonce.data,
                                                    (size_t)server_nonce.length, shared, &shared_len));
    if (client_crypto.ecc_keypair_free) {
        client_crypto.ecc_keypair_free(client_crypto.context, client_keypair);
    }
    mu_sym_keys_t c2s, s2c;
    /* c2s protects the client's outbound (ClientSalt, is_server_direction=0);
       s2c protects the server's outbound (ServerSalt, is_server_direction=1). */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_ecc_derive_channel_keys(&client_crypto, policy_id, shared, shared_len, 0, server_nonce.data,
                                                 (size_t)server_nonce.length, client_nonce, client_nonce_len, &c2s));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_ecc_derive_channel_keys(&client_crypto, policy_id, shared, shared_len, 1, server_nonce.data,
                                                 (size_t)server_nonce.length, client_nonce, client_nonce_len, &s2c));
    mu_sym_keys_prepare_cipher(&c2s, &client_crypto);
    mu_sym_keys_prepare_cipher(&s2c, &client_crypto);

    /* GetEndpoints over the secured channel — proves both directions decrypt. */
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
    mu_binary_reader_t resp;
    secure_call(&mock, server, &client_crypto, sym_mode, &c2s, &s2c, scid, token_id, 2, &srv_last, tmp, w.position,
                MU_ID_GETENDPOINTSRESPONSE, NULL);

    /* CreateSession — sends the client's ECC certificate (must match the OPN). */
    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_CREATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, 0, 3);
    {
        mu_string_t ns = {-1, NULL};
        mu_binary_write_string(&w, &ns); /* clientDescription.applicationUri */
        mu_binary_write_string(&w, &ns); /* productUri */
        mu_binary_write_byte(&w, 0x00);  /* applicationName (null LocalizedText) */
        mu_binary_write_uint32(&w, 1);   /* applicationType = Client */
        mu_binary_write_string(&w, &ns); /* gatewayServerUri */
        mu_binary_write_string(&w, &ns); /* discoveryProfileUri */
        mu_binary_write_int32(&w, 0);    /* discoveryUrls */
        mu_binary_write_string(&w, &ns); /* serverUri */
        mu_binary_write_string(&w, &ns); /* endpointUrl */
        mu_binary_write_string(&w, &ns); /* sessionName */
        mu_bytestring_t cn = {(opcua_int32_t)client_nonce_len, client_nonce};
        mu_binary_write_bytestring(&w, &cn); /* clientNonce */
        mu_bytestring_t cc = {(opcua_int32_t)client_cert_len, client_cert};
        mu_binary_write_bytestring(&w, &cc); /* clientCertificate (ECC) */
        mu_binary_write_double(&w, 60000.0); /* requestedSessionTimeout */
        mu_binary_write_uint32(&w, 0);       /* maxResponseMessageSize */
    }
    secure_call(&mock, server, &client_crypto, sym_mode, &c2s, &s2c, scid, token_id, 3, &srv_last, tmp, w.position,
                MU_ID_CREATESESSIONRESPONSE, &resp);

    mu_nodeid_t session_id, auth_token;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp, &session_id));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&resp, &auth_token));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, auth_token.identifier_type);
    TEST_ASSERT_NOT_EQUAL(0, auth_token.identifier.numeric);
    opcua_uint32_t session_auth_token = auth_token.identifier.numeric;
    opcua_uint64_t revised_bits;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint64(&resp, &revised_bits)); /* RevisedSessionTimeout */
    mu_bytestring_t sess_server_nonce;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_bytestring(&resp, &sess_server_nonce));
    TEST_ASSERT_EQUAL(32, sess_server_nonce.length);
    opcua_byte_t sess_nonce[32];
    (void)memcpy(sess_nonce, sess_server_nonce.data, 32);

    /* ActivateSession (anonymous) with an ECC application ClientSignature over
       serverEccCertificate || sessionServerNonce (OPC-10000-4 §5.7.3). The
       SignatureData.Algorithm for ECC is the SecurityPolicy URI. */
    opcua_byte_t to_sign[1600];
    TEST_ASSERT_TRUE(server_cert_len + 32 <= sizeof(to_sign));
    (void)memcpy(to_sign, server_cert, server_cert_len);
    (void)memcpy(to_sign + server_cert_len, sess_nonce, 32);
    opcua_byte_t client_sig[MU_ECC_SIGNATURE_LENGTH];
    size_t client_sig_len = sizeof(client_sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, client_crypto.ecc_sign(client_crypto.context, curve, to_sign,
                                                             server_cert_len + 32, client_sig, &client_sig_len));
    /* Negative path: a corrupted ECC ClientSignature must block activation
       (OPC-10000-4 §5.7.3), so the later Read is answered with a ServiceFault. */
    if (tamper_signature && client_sig_len > 0) {
        client_sig[0] ^= 0xFFu;
    }
    const char *sig_alg = mu_security_policy_uri(policy_id);

    mu_binary_writer_init(&w, tmp, sizeof(tmp));
    {
        mu_nodeid_t t = {0, MU_NODEID_NUMERIC, {MU_ID_ACTIVATESESSIONREQUEST}};
        mu_binary_write_nodeid(&w, &t);
    }
    write_request_header(&w, session_auth_token, 4);
    {
        mu_string_t sig_alg_str = {(opcua_int32_t)strlen(sig_alg), (const opcua_byte_t *)sig_alg};
        mu_bytestring_t sig_bs = {(opcua_int32_t)client_sig_len, client_sig};
        mu_binary_write_string(&w, &sig_alg_str); /* clientSignature.algorithm */
        mu_binary_write_bytestring(&w, &sig_bs);  /* clientSignature.signature */
        mu_binary_write_int32(&w, 0);             /* clientSoftwareCertificates */
        mu_binary_write_int32(&w, 0);             /* localeIds */
        mu_nodeid_t anon = {0, MU_NODEID_NUMERIC, {321}};
        mu_binary_write_extension_object_header(&w, &anon, 0); /* anonymous identity token */
    }
    secure_call(&mock, server, &client_crypto, sym_mode, &c2s, &s2c, scid, token_id, 4, &srv_last, tmp, w.position,
                MU_ID_ACTIVATESESSIONRESPONSE, &resp);

    /* Read MyVar1.Value over the activated ECC session -> 42. */
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
        mu_binary_write_uint32(&w, 13); /* AttributeId = Value */
        mu_binary_write_string(&w, &ns);
        mu_binary_write_uint16(&w, 0);
        mu_binary_write_string(&w, &ns);
    }
    secure_call(&mock, server, &client_crypto, sym_mode, &c2s, &s2c, scid, token_id, 5, &srv_last, tmp, w.position,
                tamper_signature ? MU_ID_SERVICEFAULT : MU_ID_READRESPONSE, &resp);
    if (!tamper_signature) {
        opcua_int32_t nres;
        mu_binary_read_int32(&resp, &nres);
        TEST_ASSERT_EQUAL(1, nres);
        opcua_byte_t mask, vt;
        mu_binary_read_byte(&resp, &mask);
        TEST_ASSERT_EQUAL(0x01, mask);
        mu_binary_read_byte(&resp, &vt);
        TEST_ASSERT_EQUAL(MU_TYPE_INT32, vt);
        opcua_int32_t val;
        mu_binary_read_int32(&resp, &val);
        TEST_ASSERT_EQUAL(42, val);
    }

    mu_sym_keys_release_cipher(&c2s);
    mu_sym_keys_release_cipher(&s2c);
    mu_server_close(server);
    mu_host_crypto_adapter_cleanup(&server_crypto);
    mu_host_crypto_adapter_cleanup(&client_crypto);
}

void test_ecc_handshake_curve25519(void) {
    run_ecc_handshake(MU_SECURITY_POLICY_ECC_CURVE25519_ID, 0);
}

void test_ecc_handshake_nistp256(void) {
    run_ecc_handshake(MU_SECURITY_POLICY_ECC_NISTP256_ID, 0);
}

void test_ecc_handshake_curve25519_rejects_tampered_signature(void) {
    run_ecc_handshake(MU_SECURITY_POLICY_ECC_CURVE25519_ID, 1);
}

void test_ecc_handshake_nistp256_rejects_tampered_signature(void) {
    run_ecc_handshake(MU_SECURITY_POLICY_ECC_NISTP256_ID, 1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ecc_handshake_curve25519);
    RUN_TEST(test_ecc_handshake_nistp256);
    RUN_TEST(test_ecc_handshake_curve25519_rejects_tampered_signature);
    RUN_TEST(test_ecc_handshake_nistp256_rejects_tampered_signature);
    return UNITY_END();
}

#else
void setUp(void) {}
void tearDown(void) {}
void test_ecc_handshake_skipped(void) {
    TEST_IGNORE_MESSAGE("OpenSSL/ECC not available");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ecc_handshake_skipped);
    return UNITY_END();
}
#endif
