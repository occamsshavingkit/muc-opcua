/* tests/unit/test_ecc_crypto.c
 * Known-answer and roundtrip tests for the ECC SecurityPolicy primitives (spec 059),
 * exercised through the host OpenSSL adapter: X25519/P-256 ECDH, Ed25519/ECDSA-P256
 * sign+verify, HKDF-SHA256 (RFC 5869), and ChaCha20-Poly1305 (RFC 8439). HKDF and
 * ChaCha20-Poly1305 use fixed RFC vectors (true KATs); ECDH/signature use roundtrip
 * + tamper-rejection since the adapter generates keys internally. */
#include "muc_opcua/muc_opcua.h"
#include "security/asym_ecc.h"
#include "security/key_derivation.h"
#include "security/security_policy.h"
#include "security/sym_chunk.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

#if defined(MUC_OPCUA_HAVE_OPENSSL) && defined(MUC_OPCUA_ECC)
#include "platform/host_crypto_adapter.h"

/* Backend accessor for the server's self-signed ECC cert (declared in the host
   backend's internal common.h; forward-declared here to avoid pulling OpenSSL into
   the test). */
opcua_statuscode_t h_get_own_ecc_certificate(void *c, mu_ecc_curve_t curve, const opcua_byte_t **cert, size_t *len);

static mu_crypto_adapter_t crypto;

void setUp(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&crypto));
}
void tearDown(void) {
    mu_host_crypto_adapter_cleanup(&crypto);
}

static size_t from_hex(const char *hex, opcua_byte_t *out) {
    size_t n = 0;
    for (; hex[0] && hex[1]; hex += 2, n++) {
        char b[3] = {hex[0], hex[1], 0};
        out[n] = (opcua_byte_t)strtoul(b, NULL, 16);
    }
    return n;
}

/* ECDH roundtrip: two ephemeral parties derive the same shared secret. */
static void ecdh_roundtrip(mu_ecc_curve_t curve) {
    opcua_byte_t a_pub[MU_ECC_MAX_PUBKEY_LENGTH], b_pub[MU_ECC_MAX_PUBKEY_LENGTH];
    opcua_byte_t a_ctx[MU_ECC_KEYPAIR_CTX_SIZE], b_ctx[MU_ECC_KEYPAIR_CTX_SIZE];
    size_t a_len = sizeof(a_pub), b_len = sizeof(b_pub);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.ecc_generate_ephemeral(crypto.context, curve, a_pub, &a_len, a_ctx));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.ecc_generate_ephemeral(crypto.context, curve, b_pub, &b_len, b_ctx));
    TEST_ASSERT_EQUAL(curve == MU_ECC_CURVE_25519 ? 32 : 64, a_len);
    TEST_ASSERT_EQUAL(a_len, b_len);

    opcua_byte_t s_ab[64], s_ba[64];
    size_t s_ab_len = sizeof(s_ab), s_ba_len = sizeof(s_ba);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto.ecc_ecdh_derive(crypto.context, curve, a_ctx, b_pub, b_len, s_ab, &s_ab_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto.ecc_ecdh_derive(crypto.context, curve, b_ctx, a_pub, a_len, s_ba, &s_ba_len));
    TEST_ASSERT_EQUAL(s_ab_len, s_ba_len);
    TEST_ASSERT_EQUAL(32, s_ab_len); /* both curves yield a 32-byte shared secret */
    TEST_ASSERT_EQUAL_MEMORY(s_ab, s_ba, s_ab_len);

    crypto.ecc_keypair_free(crypto.context, a_ctx);
    crypto.ecc_keypair_free(crypto.context, b_ctx);
}

void test_x25519_ecdh_roundtrip(void) {
    ecdh_roundtrip(MU_ECC_CURVE_25519);
}
void test_p256_ecdh_roundtrip(void) {
    ecdh_roundtrip(MU_ECC_CURVE_NISTP256);
}

/* Sign with the server's ECC key, verify with the server's ECC cert; a tampered
   message must not verify. */
static void sign_verify_roundtrip(mu_ecc_curve_t curve) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, h_get_own_ecc_certificate(crypto.context, curve, &cert, &cert_len));

    const opcua_byte_t data[] = "OpenSecureChannel signed data";
    opcua_byte_t sig[MU_ECC_SIGNATURE_LENGTH];
    size_t sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.ecc_sign(crypto.context, curve, data, sizeof(data), sig, &sig_len));
    TEST_ASSERT_EQUAL(MU_ECC_SIGNATURE_LENGTH, sig_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto.ecc_verify(crypto.context, curve, cert, cert_len, data, sizeof(data), sig, sig_len));

    opcua_byte_t tampered[sizeof(data)];
    (void)memcpy(tampered, data, sizeof(data));
    tampered[0] ^= 0xFF;
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, crypto.ecc_verify(crypto.context, curve, cert, cert_len, tampered,
                                                            sizeof(tampered), sig, sig_len));
}

void test_ed25519_sign_verify_roundtrip(void) {
    sign_verify_roundtrip(MU_ECC_CURVE_25519);
}
void test_ecdsa_p256_sign_verify_roundtrip(void) {
    sign_verify_roundtrip(MU_ECC_CURVE_NISTP256);
}

/* HKDF-SHA256 — RFC 5869 Test Case 1. */
void test_hkdf_sha256_rfc5869_case1(void) {
    opcua_byte_t ikm[22], salt[13], info[10], expected[42];
    (void)from_hex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", ikm);
    (void)from_hex("000102030405060708090a0b0c", salt);
    (void)from_hex("f0f1f2f3f4f5f6f7f8f9", info);
    (void)from_hex("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865", expected);

    opcua_byte_t okm[42];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_hkdf_sha256(&crypto, salt, sizeof(salt), ikm, sizeof(ikm), info, sizeof(info),
                                                     okm, sizeof(okm)));
    TEST_ASSERT_EQUAL_MEMORY(expected, okm, sizeof(okm));
}

/* ChaCha20-Poly1305 AEAD — RFC 8439 §2.8.2. */
void test_chacha20_poly1305_rfc8439(void) {
    opcua_byte_t key[32], nonce[12], aad[12], expected_ct[114], expected_tag[16];
    (void)from_hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f", key);
    (void)from_hex("070000004041424344454647", nonce);
    (void)from_hex("50515253c0c1c2c3c4c5c6c7", aad);
    const opcua_byte_t pt[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for "
                              "the future, sunscreen would be it.";
    const size_t pt_len = sizeof(pt) - 1; /* drop the NUL: 114 bytes */
    (void)from_hex(
        "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71"
        "de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def"
        "08e4b7a9de576d26586cec64b6116",
        expected_ct);
    (void)from_hex("1ae10b594f09e26a7e902ecbd0600691", expected_tag);

    opcua_byte_t ct[114], tag[16];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.chacha20_poly1305_encrypt(crypto.context, key, nonce, aad, sizeof(aad), pt,
                                                                       pt_len, ct, tag));
    TEST_ASSERT_EQUAL_MEMORY(expected_ct, ct, pt_len);
    TEST_ASSERT_EQUAL_MEMORY(expected_tag, tag, sizeof(tag));

    opcua_byte_t back[114];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.chacha20_poly1305_decrypt(crypto.context, key, nonce, aad, sizeof(aad), ct,
                                                                       pt_len, tag, back));
    TEST_ASSERT_EQUAL_MEMORY(pt, back, pt_len);

    /* A corrupted tag must fail authentication. */
    opcua_byte_t bad_tag[16];
    (void)memcpy(bad_tag, tag, sizeof(bad_tag));
    bad_tag[0] ^= 0xFF;
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, crypto.chacha20_poly1305_decrypt(crypto.context, key, nonce, aad, sizeof(aad),
                                                                           ct, pt_len, bad_tag, back));
}

/* Policy-table classification for the two ECC policies (spec 059 grounded values). */
void test_ecc_policy_table_classification(void) {
    const char *n_uri = "http://opcfoundation.org/UA/SecurityPolicy#ECC_nistP256";
    const char *c_uri = "http://opcfoundation.org/UA/SecurityPolicy#ECC_curve25519";
    mu_security_policy_id_t np = mu_security_policy_from_uri((const opcua_byte_t *)n_uri, strlen(n_uri));
    mu_security_policy_id_t cp = mu_security_policy_from_uri((const opcua_byte_t *)c_uri, strlen(c_uri));
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_ECC_NISTP256_ID, np);
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_ECC_CURVE25519_ID, cp);

    /* nistP256 [ECC-B]: ECC family, CBC-HMAC, AES-128, 64-byte nonce, 16-byte IV. */
    TEST_ASSERT_EQUAL(MU_ASYM_FAMILY_ECC, mu_security_policy_asym_family(np));
    TEST_ASSERT_EQUAL(MU_SYM_MODE_CBC_HMAC, mu_security_policy_sym_mode(np));
    TEST_ASSERT_EQUAL(16, mu_security_policy_encryption_key_length(np));
    TEST_ASSERT_EQUAL(64, mu_security_policy_nonce_length(np));
    TEST_ASSERT_EQUAL(16, mu_security_policy_iv_length(np));
    mu_ecc_curve_t curve = MU_ECC_CURVE_25519;
    TEST_ASSERT_EQUAL(1, mu_security_policy_ecc_curve(np, &curve));
    TEST_ASSERT_EQUAL(MU_ECC_CURVE_NISTP256, curve);

    /* curve25519 [ECC-A]: ECC family, ChaCha20-Poly1305 AEAD, 32-byte key/nonce,
       12-byte IV, no separate MAC key. */
    TEST_ASSERT_EQUAL(MU_ASYM_FAMILY_ECC, mu_security_policy_asym_family(cp));
    TEST_ASSERT_EQUAL(MU_SYM_MODE_AEAD_CHACHA20POLY1305, mu_security_policy_sym_mode(cp));
    TEST_ASSERT_EQUAL(32, mu_security_policy_encryption_key_length(cp));
    TEST_ASSERT_EQUAL(32, mu_security_policy_nonce_length(cp));
    TEST_ASSERT_EQUAL(12, mu_security_policy_iv_length(cp));
    TEST_ASSERT_EQUAL(0, mu_security_policy_signature_key_length(cp));
    TEST_ASSERT_EQUAL(1, mu_security_policy_ecc_curve(cp, &curve));
    TEST_ASSERT_EQUAL(MU_ECC_CURVE_25519, curve);
}

/* Set up a curve25519 AEAD key set with fixed key/IV for chunk tests. */
static void aead_keys(mu_sym_keys_t *keys) {
    (void)memset(keys, 0, sizeof(*keys));
    keys->policy = MU_SECURITY_POLICY_ECC_CURVE25519_ID;
    (void)memset(keys->encrypting_key, 0xA5, MU_ECC_CURVE25519_ENCRYPTION_KEY_LENGTH);
    (void)memset(keys->iv, 0x11, MU_ECC_CURVE25519_IV_LENGTH);
    keys->crypto = &crypto;
}

/* AEAD MSG chunk wrap/unwrap roundtrip; tamper and wrong-IV (last-seq) fail closed. */
void test_aead_sym_chunk_roundtrip(void) {
    mu_sym_keys_t keys;
    aead_keys(&keys);
    const opcua_byte_t body[] = "an encoded service message body over the AEAD channel";
    const opcua_uint32_t scid = 7, token = 9, seq = 42, last_seq = 41, req = 100;

    opcua_byte_t out[256];
    size_t out_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap_aead(&crypto, &keys, "MSG", scid, token, seq, last_seq, req,
                                                             body, sizeof(body), out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL(16 + 8 + sizeof(body) + 16, out_len);

    const opcua_byte_t *rbody = NULL;
    size_t rlen = 0;
    mu_sym_chunk_info_t info;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_chunk_unwrap_aead(&crypto, &keys, out, out_len, last_seq, &rbody, &rlen, &info));
    TEST_ASSERT_EQUAL(sizeof(body), rlen);
    TEST_ASSERT_EQUAL_MEMORY(body, rbody, sizeof(body));
    TEST_ASSERT_EQUAL(seq, info.sequence_number);
    TEST_ASSERT_EQUAL(req, info.request_id);
    TEST_ASSERT_EQUAL(token, info.token_id);

    /* Tampered ciphertext byte must fail the AEAD tag. */
    opcua_byte_t out2[256];
    size_t out2_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap_aead(&crypto, &keys, "MSG", scid, token, seq, last_seq, req,
                                                             body, sizeof(body), out2, sizeof(out2), &out2_len));
    out2[16 + 10] ^= 0xFF;
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
                          mu_sym_chunk_unwrap_aead(&crypto, &keys, out2, out2_len, last_seq, &rbody, &rlen, &info));

    /* Wrong last-sequence (=> wrong IV) must fail authentication. */
    opcua_byte_t out3[256];
    size_t out3_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap_aead(&crypto, &keys, "MSG", scid, token, seq, last_seq, req,
                                                             body, sizeof(body), out3, sizeof(out3), &out3_len));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
                          mu_sym_chunk_unwrap_aead(&crypto, &keys, out3, out3_len, last_seq + 1, &rbody, &rlen, &info));
}

/* Full ECDH -> HKDF channel key derivation, proving both endpoints derive
   identical per-direction keys (the interop-critical property) and that the two
   directions differ. */
void test_ecc_channel_key_derivation(void) {
    const mu_ecc_curve_t curve = MU_ECC_CURVE_NISTP256;
    const mu_security_policy_id_t policy = MU_SECURITY_POLICY_ECC_NISTP256_ID;

    opcua_byte_t s_pub[64], c_pub[64], s_ctx[MU_ECC_KEYPAIR_CTX_SIZE], c_ctx[MU_ECC_KEYPAIR_CTX_SIZE];
    size_t s_len = sizeof(s_pub), c_len = sizeof(c_pub);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.ecc_generate_ephemeral(crypto.context, curve, s_pub, &s_len, s_ctx));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.ecc_generate_ephemeral(crypto.context, curve, c_pub, &c_len, c_ctx));

    opcua_byte_t shared_srv[64], shared_cli[64];
    size_t ss_len = sizeof(shared_srv), sc_len = sizeof(shared_cli);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto.ecc_ecdh_derive(crypto.context, curve, s_ctx, c_pub, c_len, shared_srv, &ss_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto.ecc_ecdh_derive(crypto.context, curve, c_ctx, s_pub, s_len, shared_cli, &sc_len));

    /* Server derives its Server->Client keys from its shared secret; client derives
       the same direction from its (equal) shared secret. They must match. */
    mu_sym_keys_t srv_side, cli_side;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_ecc_derive_channel_keys(&crypto, policy, shared_srv, ss_len, 1, s_pub, s_len,
                                                                 c_pub, c_len, &srv_side));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_ecc_derive_channel_keys(&crypto, policy, shared_cli, sc_len, 1, s_pub, s_len,
                                                                 c_pub, c_len, &cli_side));
    TEST_ASSERT_EQUAL_MEMORY(srv_side.signing_key, cli_side.signing_key, MU_ECC_NISTP256_SIGNATURE_KEY_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(srv_side.encrypting_key, cli_side.encrypting_key, MU_ECC_NISTP256_ENCRYPTION_KEY_LENGTH);
    TEST_ASSERT_EQUAL_MEMORY(srv_side.iv, cli_side.iv, MU_ECC_NISTP256_IV_LENGTH);

    /* The Client->Server direction must derive different key material. */
    mu_sym_keys_t client_dir;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_ecc_derive_channel_keys(&crypto, policy, shared_srv, ss_len, 0, s_pub, s_len,
                                                                 c_pub, c_len, &client_dir));
    TEST_ASSERT_NOT_EQUAL(
        0, memcmp(srv_side.encrypting_key, client_dir.encrypting_key, MU_ECC_NISTP256_ENCRYPTION_KEY_LENGTH));

    crypto.ecc_keypair_free(crypto.context, s_ctx);
    crypto.ecc_keypair_free(crypto.context, c_ctx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_x25519_ecdh_roundtrip);
    RUN_TEST(test_p256_ecdh_roundtrip);
    RUN_TEST(test_ed25519_sign_verify_roundtrip);
    RUN_TEST(test_ecdsa_p256_sign_verify_roundtrip);
    RUN_TEST(test_hkdf_sha256_rfc5869_case1);
    RUN_TEST(test_chacha20_poly1305_rfc8439);
    RUN_TEST(test_ecc_policy_table_classification);
    RUN_TEST(test_aead_sym_chunk_roundtrip);
    RUN_TEST(test_ecc_channel_key_derivation);
    return UNITY_END();
}

#else /* ECC or OpenSSL not available */

void setUp(void) {}
void tearDown(void) {}
void test_ecc_crypto_skipped(void) {
    TEST_IGNORE_MESSAGE("ECC or OpenSSL not available; ECC crypto tests skipped");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ecc_crypto_skipped);
    return UNITY_END();
}

#endif
