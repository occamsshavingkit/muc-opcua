/* tests/unit/test_mbedtls_adapter.c */
#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include "platform/host_crypto_adapter.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

#ifdef MUC_OPCUA_HAVE_MBEDTLS
#include "platform/mbedtls_crypto_adapter.h"
#include <openssl/evp.h>
#include <openssl/x509.h>

struct host_crypto_context {
    EVP_PKEY *key;
    unsigned char *cert_der;
    int cert_der_len;
};

void setUp(void) {}
void tearDown(void) {}

void test_mbedtls_adapter_all(void) {
    mu_crypto_adapter_t host_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&host_crypto));

    struct host_crypto_context *hc = (struct host_crypto_context *)host_crypto.context;

    unsigned char *key_der = NULL;
    int key_len = i2d_PrivateKey(hc->key, &key_der);
    TEST_ASSERT_TRUE(key_len > 0);

    const opcua_byte_t *cert_der = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.get_own_certificate(hc, &cert_der, &cert_len));

    mu_crypto_adapter_t mbed_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_mbedtls_crypto_adapter_init(&mbed_crypto, cert_der, cert_len, key_der, (size_t)key_len));

    const opcua_byte_t test_data[] = "Hello OPC UA Security!";
    opcua_byte_t h_digest[32], m_digest[32];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.sha256(host_crypto.context, test_data, sizeof(test_data), h_digest));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.sha256(mbed_crypto.context, test_data, sizeof(test_data), m_digest));
    TEST_ASSERT_EQUAL_MEMORY(h_digest, m_digest, 32);

    const opcua_byte_t test_key[16] = "0123456789abcdef";
    opcua_byte_t h_mac[32], m_mac[32];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.hmac_sha256(host_crypto.context, test_key, sizeof(test_key),
                                                              test_data, sizeof(test_data), h_mac));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.hmac_sha256(mbed_crypto.context, test_key, sizeof(test_key),
                                                              test_data, sizeof(test_data), m_mac));
    TEST_ASSERT_EQUAL_MEMORY(h_mac, m_mac, 32);

    opcua_byte_t aes256_key[32] = "abcdefghijklmnopqrstuvwxyz123456";
    opcua_byte_t iv[16] = "1234567890abcdef";
    opcua_byte_t input_block[32] = "aes block must be mult of 16!!!";
    opcua_byte_t h_cipher[32], m_cipher[32];
    opcua_byte_t h_plain[32], m_plain[32];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes256_cbc_encrypt(host_crypto.context, aes256_key, iv, input_block, 32, h_cipher));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.aes256_cbc_encrypt(mbed_crypto.context, aes256_key, iv, input_block, 32, m_cipher));
    TEST_ASSERT_EQUAL_MEMORY(h_cipher, m_cipher, 32);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes256_cbc_decrypt(host_crypto.context, aes256_key, iv, h_cipher, 32, h_plain));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.aes256_cbc_decrypt(mbed_crypto.context, aes256_key, iv, m_cipher, 32, m_plain));
    TEST_ASSERT_EQUAL_MEMORY(h_plain, m_plain, 32);

    opcua_byte_t aes128_key[16] = "1234567890abcdef";
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes128_cbc_encrypt(host_crypto.context, aes128_key, iv, input_block, 32, h_cipher));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.aes128_cbc_encrypt(mbed_crypto.context, aes128_key, iv, input_block, 32, m_cipher));
    TEST_ASSERT_EQUAL_MEMORY(h_cipher, m_cipher, 32);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes128_cbc_decrypt(host_crypto.context, aes128_key, iv, h_cipher, 32, h_plain));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.aes128_cbc_decrypt(mbed_crypto.context, aes128_key, iv, m_cipher, 32, m_plain));
    TEST_ASSERT_EQUAL_MEMORY(h_plain, m_plain, 32);

    opcua_byte_t sig[256];
    size_t sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.rsa_sha256_sign(mbed_crypto.context, test_data, sizeof(test_data), sig, &sig_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.rsa_sha256_verify(host_crypto.context, cert_der, cert_len, test_data,
                                                                    sizeof(test_data), sig, sig_len));

    size_t h_sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.rsa_sha256_sign(host_crypto.context, test_data, sizeof(test_data), sig, &h_sig_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.rsa_sha256_verify(mbed_crypto.context, cert_der, cert_len, test_data,
                                                                    sizeof(test_data), sig, h_sig_len));

    opcua_byte_t oaep_cipher[256];
    size_t oaep_cipher_len = sizeof(oaep_cipher);
    opcua_byte_t oaep_plain[256];
    size_t oaep_plain_len = sizeof(oaep_plain);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.rsa_oaep_encrypt(host_crypto.context, cert_der, cert_len, test_data,
                                                                   sizeof(test_data), oaep_cipher, &oaep_cipher_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.rsa_oaep_decrypt(mbed_crypto.context, oaep_cipher, oaep_cipher_len,
                                                                   oaep_plain, &oaep_plain_len));
    TEST_ASSERT_EQUAL(sizeof(test_data), oaep_plain_len);
    TEST_ASSERT_EQUAL_MEMORY(test_data, oaep_plain, oaep_plain_len);

    oaep_cipher_len = sizeof(oaep_cipher);
    oaep_plain_len = sizeof(oaep_plain);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.rsa_oaep_encrypt(mbed_crypto.context, cert_der, cert_len, test_data,
                                                                   sizeof(test_data), oaep_cipher, &oaep_cipher_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.rsa_oaep_decrypt(host_crypto.context, oaep_cipher, oaep_cipher_len,
                                                                   oaep_plain, &oaep_plain_len));
    TEST_ASSERT_EQUAL(sizeof(test_data), oaep_plain_len);
    TEST_ASSERT_EQUAL_MEMORY(test_data, oaep_plain, oaep_plain_len);

    opcua_byte_t h_thumb[20], m_thumb[20];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.get_certificate_thumbprint(host_crypto.context, cert_der, cert_len, h_thumb));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.get_certificate_thumbprint(mbed_crypto.context, cert_der, cert_len, m_thumb));
    TEST_ASSERT_EQUAL_MEMORY(h_thumb, m_thumb, 20);

    size_t h_bits = 0, m_bits = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.get_certificate_key_bits(host_crypto.context, cert_der, cert_len, &h_bits));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.get_certificate_key_bits(mbed_crypto.context, cert_der, cert_len, &m_bits));
    TEST_ASSERT_EQUAL(h_bits, m_bits);

    mu_mbedtls_crypto_adapter_cleanup(&mbed_crypto);
    mu_host_crypto_adapter_cleanup(&host_crypto);
    free(key_der);
}

#ifdef MUC_OPCUA_ECC
/* Build an INDEPENDENT self-signed P-256 identity with OpenSSL (the oracle),
   exporting the private key + certificate as DER. Used to provision the mbedTLS
   adapter, so the sign/verify cross-checks below are genuinely non-circular. */
static EVP_PKEY *make_p256_identity(unsigned char **cert_der, int *cert_der_len, unsigned char **key_der,
                                    int *key_der_len) {
    EVP_PKEY *key = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "P-256");
    TEST_ASSERT_NOT_NULL(key);

    X509 *x = X509_new();
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_EQUAL(1, X509_set_version(x, 2));
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_getm_notBefore(x), 0);
    X509_gmtime_adj(X509_getm_notAfter(x), 60L * 60L * 24L * 365L);
    X509_NAME *name = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)"muc-opcua-ecc-test", -1, -1, 0);
    TEST_ASSERT_EQUAL(1, X509_set_issuer_name(x, name));
    TEST_ASSERT_EQUAL(1, X509_set_pubkey(x, key));
    TEST_ASSERT_TRUE(X509_sign(x, key, EVP_sha256()) != 0);

    *cert_der = NULL;
    *cert_der_len = i2d_X509(x, cert_der);
    TEST_ASSERT_TRUE(*cert_der_len > 0);
    *key_der = NULL;
    *key_der_len = i2d_PrivateKey(key, key_der);
    TEST_ASSERT_TRUE(*key_der_len > 0);

    X509_free(x);
    return key;
}

void test_mbedtls_ecc_all(void) {
    mu_crypto_adapter_t host_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&host_crypto));
    struct host_crypto_context *hc = (struct host_crypto_context *)host_crypto.context;

    /* RSA identity for the mbedTLS adapter (reuse the host's RSA cert/key). */
    unsigned char *rsa_key_der = NULL;
    int rsa_key_len = i2d_PrivateKey(hc->key, &rsa_key_der);
    TEST_ASSERT_TRUE(rsa_key_len > 0);
    const opcua_byte_t *rsa_cert_der = NULL;
    size_t rsa_cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.get_own_certificate(hc, &rsa_cert_der, &rsa_cert_len));

    mu_crypto_adapter_t mbed_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_mbedtls_crypto_adapter_init(&mbed_crypto, rsa_cert_der, rsa_cert_len,
                                                                     rsa_key_der, (size_t)rsa_key_len));

    /* Provision the mbedTLS adapter with an INDEPENDENT P-256 identity. */
    unsigned char *ecc_cert_der = NULL, *ecc_key_der = NULL;
    int ecc_cert_len = 0, ecc_key_len = 0;
    EVP_PKEY *ecc_key = make_p256_identity(&ecc_cert_der, &ecc_cert_len, &ecc_key_der, &ecc_key_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_mbedtls_crypto_adapter_set_ecc_identity(&mbed_crypto, MU_ECC_CURVE_NISTP256,
                                                                                 ecc_cert_der, (size_t)ecc_cert_len,
                                                                                 ecc_key_der, (size_t)ecc_key_len));

    const opcua_byte_t msg[] = "Hello ECC SecurityPolicy!";
    opcua_byte_t sig[MU_ECC_SIGNATURE_LENGTH];
    size_t sig_len;

    /* 1. host signs (its own P-256 cert), mbedTLS verifies. */
    const opcua_byte_t *host_ecc_cert = NULL;
    size_t host_ecc_cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.get_own_ecc_certificate(host_crypto.context, MU_ECC_CURVE_NISTP256,
                                                                          &host_ecc_cert, &host_ecc_cert_len));
    sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.ecc_sign(host_crypto.context, MU_ECC_CURVE_NISTP256, msg, sizeof(msg),
                                                           sig, &sig_len));
    TEST_ASSERT_EQUAL(MU_ECC_SIGNATURE_LENGTH, sig_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.ecc_verify(mbed_crypto.context, MU_ECC_CURVE_NISTP256, host_ecc_cert,
                                                             host_ecc_cert_len, msg, sizeof(msg), sig, sig_len));

    /* 2. mbedTLS signs (its independent cert), host verifies. */
    sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.ecc_sign(mbed_crypto.context, MU_ECC_CURVE_NISTP256, msg, sizeof(msg),
                                                           sig, &sig_len));
    TEST_ASSERT_EQUAL(MU_ECC_SIGNATURE_LENGTH, sig_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.ecc_verify(host_crypto.context, MU_ECC_CURVE_NISTP256,
                                                             (const opcua_byte_t *)ecc_cert_der, (size_t)ecc_cert_len,
                                                             msg, sizeof(msg), sig, sig_len));

    /* 3. ECDH: both sides derive the same shared secret. */
    opcua_byte_t pub_m[MU_ECC_MAX_PUBKEY_LENGTH], pub_h[MU_ECC_MAX_PUBKEY_LENGTH];
    size_t pub_m_len = sizeof(pub_m), pub_h_len = sizeof(pub_h);
    opcua_byte_t kp_m[MU_ECC_KEYPAIR_CTX_SIZE], kp_h[MU_ECC_KEYPAIR_CTX_SIZE];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.ecc_generate_ephemeral(mbed_crypto.context, MU_ECC_CURVE_NISTP256,
                                                                         pub_m, &pub_m_len, kp_m));
    TEST_ASSERT_EQUAL(64, pub_m_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.ecc_generate_ephemeral(host_crypto.context, MU_ECC_CURVE_NISTP256,
                                                                         pub_h, &pub_h_len, kp_h));
    TEST_ASSERT_EQUAL(64, pub_h_len);

    opcua_byte_t shared_m[32], shared_h[32];
    size_t shared_m_len = sizeof(shared_m), shared_h_len = sizeof(shared_h);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mbed_crypto.ecc_ecdh_derive(mbed_crypto.context, MU_ECC_CURVE_NISTP256, kp_m,
                                                                  pub_h, pub_h_len, shared_m, &shared_m_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.ecc_ecdh_derive(host_crypto.context, MU_ECC_CURVE_NISTP256, kp_h,
                                                                  pub_m, pub_m_len, shared_h, &shared_h_len));
    TEST_ASSERT_EQUAL(32, shared_m_len);
    TEST_ASSERT_EQUAL(32, shared_h_len);
    TEST_ASSERT_EQUAL_MEMORY(shared_h, shared_m, 32);
    mbed_crypto.ecc_keypair_free(mbed_crypto.context, kp_m);
    host_crypto.ecc_keypair_free(host_crypto.context, kp_h);

    /* 4. ChaCha20-Poly1305: RFC-8439 interop both directions. */
    opcua_byte_t cc_key[MU_CHACHA20_POLY1305_KEY_LENGTH] = "chacha20poly1305-key-32-bytes!!!";
    opcua_byte_t cc_nonce[MU_CHACHA20_POLY1305_NONCE_LENGTH] = "nonce-12byte";
    const opcua_byte_t cc_aad[] = "additional-authenticated-data";
    const opcua_byte_t cc_pt[] = "chacha20 secret payload";
    opcua_byte_t cc_ct[sizeof(cc_pt)], cc_out[sizeof(cc_pt)], cc_tag[MU_CHACHA20_POLY1305_TAG_LENGTH];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.chacha20_poly1305_encrypt(mbed_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_pt, sizeof(cc_pt), cc_ct, cc_tag));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.chacha20_poly1305_decrypt(host_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_ct, sizeof(cc_pt), cc_tag, cc_out));
    TEST_ASSERT_EQUAL_MEMORY(cc_pt, cc_out, sizeof(cc_pt));

    (void)memset(cc_out, 0, sizeof(cc_out));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.chacha20_poly1305_encrypt(host_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_pt, sizeof(cc_pt), cc_ct, cc_tag));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mbed_crypto.chacha20_poly1305_decrypt(mbed_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_ct, sizeof(cc_pt), cc_tag, cc_out));
    TEST_ASSERT_EQUAL_MEMORY(cc_pt, cc_out, sizeof(cc_pt));

    /* 5. curve25519 is rejected (no Ed25519 in mbedTLS 2.x). */
    sig_len = sizeof(sig);
    TEST_ASSERT_NOT_EQUAL(
        MU_STATUS_GOOD, mbed_crypto.ecc_sign(mbed_crypto.context, MU_ECC_CURVE_25519, msg, sizeof(msg), sig, &sig_len));

    mu_mbedtls_crypto_adapter_cleanup(&mbed_crypto);
    mu_host_crypto_adapter_cleanup(&host_crypto);
    EVP_PKEY_free(ecc_key);
    OPENSSL_free(ecc_cert_der);
    OPENSSL_free(ecc_key_der);
    free(rsa_key_der);
}
#endif /* MUC_OPCUA_ECC */

#else

void setUp(void) {}
void tearDown(void) {}
void test_mbedtls_dummy(void) {
    TEST_IGNORE_MESSAGE("Mbed TLS support not enabled in CMake");
}

#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_HAVE_MBEDTLS
    RUN_TEST(test_mbedtls_adapter_all);
#ifdef MUC_OPCUA_ECC
    RUN_TEST(test_mbedtls_ecc_all);
#endif
#else
    RUN_TEST(test_mbedtls_dummy);
#endif
    return UNITY_END();
}
