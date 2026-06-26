/* tests/unit/test_crypto.c
 * Known-answer and roundtrip tests for the Basic256Sha256 crypto primitives and the
 * P-SHA256 key-derivation function. The primitives are exercised through the host
 * OpenSSL adapter; the KDF is the portable core logic built on top of them. */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "security/key_derivation.h"
#include <string.h>
#include <stdlib.h>

#ifdef MICRO_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"

static mu_crypto_adapter_t crypto;

void setUp(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&crypto));
}
void tearDown(void) {
    mu_host_crypto_adapter_cleanup(&crypto);
}

/* Decode an ASCII hex string into bytes. Returns the byte count. */
static size_t from_hex(const char *hex, opcua_byte_t *out) {
    size_t n = 0;
    for (; hex[0] && hex[1]; hex += 2, n++) {
        char b[3] = { hex[0], hex[1], 0 };
        out[n] = (opcua_byte_t)strtoul(b, NULL, 16);
    }
    return n;
}

/* SHA-256("abc") — FIPS 180-4 example. */
void test_sha256_known_answer(void) {
    const opcua_byte_t msg[] = { 'a', 'b', 'c' };
    opcua_byte_t expected[MU_SHA256_LENGTH];
    from_hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", expected);

    opcua_byte_t digest[MU_SHA256_LENGTH];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.sha256(crypto.context, msg, sizeof(msg), digest));
    TEST_ASSERT_EQUAL_MEMORY(expected, digest, MU_SHA256_LENGTH);
}

/* HMAC-SHA256 — RFC 4231 Test Case 1. */
void test_hmac_sha256_known_answer(void) {
    opcua_byte_t key[20];
    memset(key, 0x0b, sizeof(key));
    const opcua_byte_t data[] = { 'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e' };
    opcua_byte_t expected[MU_SHA256_LENGTH];
    from_hex("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", expected);

    opcua_byte_t mac[MU_SHA256_LENGTH];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.hmac_sha256(crypto.context, key, sizeof(key), data, sizeof(data), mac));
    TEST_ASSERT_EQUAL_MEMORY(expected, mac, MU_SHA256_LENGTH);
}

/* AES-256-CBC single block — NIST SP 800-38A F.2.5/F.2.6. */
void test_aes256_cbc_known_answer(void) {
    opcua_byte_t key[32], iv[16], pt[16], expected_ct[16];
    from_hex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", key);
    from_hex("000102030405060708090a0b0c0d0e0f", iv);
    from_hex("6bc1bee22e409f96e93d7e117393172a", pt);
    from_hex("f58c4c04d6e5f1ba779eabfb5f7bfbd6", expected_ct);

    opcua_byte_t ct[16];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.aes256_cbc_encrypt(crypto.context, key, iv, pt, sizeof(pt), ct));
    TEST_ASSERT_EQUAL_MEMORY(expected_ct, ct, sizeof(ct));

    opcua_byte_t back[16];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.aes256_cbc_decrypt(crypto.context, key, iv, ct, sizeof(ct), back));
    TEST_ASSERT_EQUAL_MEMORY(pt, back, sizeof(pt));
}

/* The self-signed cert is RSA-2048. */
void test_certificate_key_bits(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));
    TEST_ASSERT_NOT_NULL(cert);
    TEST_ASSERT_GREATER_THAN(0, cert_len);

    size_t bits = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_certificate_key_bits(crypto.context, cert, cert_len, &bits));
    TEST_ASSERT_EQUAL(2048, bits);
}

/* RSA-PKCS1.5-SHA256 sign with the server key, verify with the server cert. */
void test_rsa_sign_verify_roundtrip(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));

    const opcua_byte_t data[] = "the quick brown fox";
    opcua_byte_t sig[512];
    size_t sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.rsa_sha256_sign(crypto.context, data, sizeof(data), sig, &sig_len));
    TEST_ASSERT_EQUAL(256, sig_len); /* 2048-bit modulus */

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.rsa_sha256_verify(crypto.context, cert, cert_len, data, sizeof(data), sig, sig_len));

    /* A tampered message must not verify. */
    opcua_byte_t tampered[sizeof(data)];
    memcpy(tampered, data, sizeof(data));
    tampered[0] ^= 0xFF;
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
        crypto.rsa_sha256_verify(crypto.context, cert, cert_len, tampered, sizeof(tampered), sig, sig_len));
}

/* RSA-OAEP encrypt to the server cert, decrypt with the server key. */
void test_rsa_oaep_roundtrip(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));

    const opcua_byte_t secret[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    opcua_byte_t ciphertext[256];
    size_t ct_len = sizeof(ciphertext);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.rsa_oaep_encrypt(crypto.context, cert, cert_len, secret, sizeof(secret), ciphertext, &ct_len));
    TEST_ASSERT_EQUAL(256, ct_len);

    opcua_byte_t recovered[256];
    size_t rec_len = sizeof(recovered);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.rsa_oaep_decrypt(crypto.context, ciphertext, ct_len, recovered, &rec_len));
    TEST_ASSERT_EQUAL(sizeof(secret), rec_len);
    TEST_ASSERT_EQUAL_MEMORY(secret, recovered, sizeof(secret));
}

/* P-SHA256: deterministic, fills any requested length, and its first block matches
   the RFC 5246 construction HMAC(secret, A(1) | seed) computed independently. */
void test_p_sha256_first_block_matches_construction(void) {
    const opcua_byte_t secret[] = "client-and-server-secret";
    const opcua_byte_t seed[] = "the-nonce-seed";

    /* Independently: A(1) = HMAC(secret, seed); block1 = HMAC(secret, A(1)|seed). */
    opcua_byte_t a1[MU_SHA256_LENGTH];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.hmac_sha256(crypto.context, secret, sizeof(secret) - 1, seed, sizeof(seed) - 1, a1));
    opcua_byte_t concat[MU_SHA256_LENGTH + 32];
    memcpy(concat, a1, MU_SHA256_LENGTH);
    memcpy(concat + MU_SHA256_LENGTH, seed, sizeof(seed) - 1);
    opcua_byte_t expected_block[MU_SHA256_LENGTH];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        crypto.hmac_sha256(crypto.context, secret, sizeof(secret) - 1,
                           concat, MU_SHA256_LENGTH + (sizeof(seed) - 1), expected_block));

    opcua_byte_t derived[MU_SHA256_LENGTH];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_p_sha256(&crypto, secret, sizeof(secret) - 1, seed, sizeof(seed) - 1, derived, sizeof(derived)));
    TEST_ASSERT_EQUAL_MEMORY(expected_block, derived, MU_SHA256_LENGTH);
}

void test_p_sha256_deterministic_and_length(void) {
    const opcua_byte_t secret[] = "secret";
    const opcua_byte_t seed[] = "seed";

    /* A length that is not a multiple of the block size, to exercise truncation. */
    opcua_byte_t a[100], b[100];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_p_sha256(&crypto, secret, sizeof(secret) - 1, seed, sizeof(seed) - 1, a, sizeof(a)));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_p_sha256(&crypto, secret, sizeof(secret) - 1, seed, sizeof(seed) - 1, b, sizeof(b)));
    TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof(a));

    /* A different seed yields different key material. */
    const opcua_byte_t seed2[] = "different";
    opcua_byte_t c[100];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_p_sha256(&crypto, secret, sizeof(secret) - 1, seed2, sizeof(seed2) - 1, c, sizeof(c)));
    TEST_ASSERT_NOT_EQUAL(0, memcmp(a, c, sizeof(a)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sha256_known_answer);
    RUN_TEST(test_hmac_sha256_known_answer);
    RUN_TEST(test_aes256_cbc_known_answer);
    RUN_TEST(test_certificate_key_bits);
    RUN_TEST(test_rsa_sign_verify_roundtrip);
    RUN_TEST(test_rsa_oaep_roundtrip);
    RUN_TEST(test_p_sha256_first_block_matches_construction);
    RUN_TEST(test_p_sha256_deterministic_and_length);
    return UNITY_END();
}

#else /* !MICRO_OPCUA_HAVE_OPENSSL */

void setUp(void) {}
void tearDown(void) {}
void test_crypto_skipped_without_openssl(void) {
    TEST_IGNORE_MESSAGE("OpenSSL not available; Basic256Sha256 crypto tests skipped");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crypto_skipped_without_openssl);
    return UNITY_END();
}

#endif
