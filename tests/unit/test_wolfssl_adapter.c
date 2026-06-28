/* tests/unit/test_wolfssl_adapter.c */
#include "micro_opcua/platform.h"
#include "micro_opcua/status.h"
#include "platform/host_crypto_adapter.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

#ifdef MICRO_OPCUA_HAVE_WOLFSSL
#include <openssl/evp.h>
#include <openssl/x509.h>

struct host_crypto_context {
    EVP_PKEY *key;
    unsigned char *cert_der;
    int cert_der_len;
};

void setUp(void) {}
void tearDown(void) {}

void test_wolfssl_adapter_all(void) {
    mu_crypto_adapter_t host_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&host_crypto));

    struct host_crypto_context *hc = (struct host_crypto_context *)host_crypto.context;

    unsigned char *key_der = NULL;
    int key_len = i2d_PrivateKey(hc->key, &key_der);
    TEST_ASSERT_TRUE(key_len > 0);

    const opcua_byte_t *cert_der = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.get_own_certificate(hc, &cert_der, &cert_len));

    mu_crypto_adapter_t wolf_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_wolfssl_crypto_adapter_init(&wolf_crypto, cert_der, cert_len, key_der, (size_t)key_len));

    const opcua_byte_t test_data[] = "Hello OPC UA Security!";
    opcua_byte_t h_digest[32], w_digest[32];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.sha256(host_crypto.context, test_data, sizeof(test_data), h_digest));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf_crypto.sha256(wolf_crypto.context, test_data, sizeof(test_data), w_digest));
    TEST_ASSERT_EQUAL_MEMORY(h_digest, w_digest, 32);

    const opcua_byte_t test_key[16] = "0123456789abcdef";
    opcua_byte_t h_mac[32], w_mac[32];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.hmac_sha256(host_crypto.context, test_key, sizeof(test_key),
                                                              test_data, sizeof(test_data), h_mac));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf_crypto.hmac_sha256(wolf_crypto.context, test_key, sizeof(test_key),
                                                              test_data, sizeof(test_data), w_mac));
    TEST_ASSERT_EQUAL_MEMORY(h_mac, w_mac, 32);

    opcua_byte_t aes256_key[32] = "abcdefghijklmnopqrstuvwxyz123456";
    opcua_byte_t iv[16] = "1234567890abcdef";
    opcua_byte_t input_block[32] = "aes block must be mult of 16!!!";
    opcua_byte_t h_cipher[32], w_cipher[32];
    opcua_byte_t h_plain[32], w_plain[32];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes256_cbc_encrypt(host_crypto.context, aes256_key, iv, input_block, 32, h_cipher));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.aes256_cbc_encrypt(wolf_crypto.context, aes256_key, iv, input_block, 32, w_cipher));
    TEST_ASSERT_EQUAL_MEMORY(h_cipher, w_cipher, 32);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes256_cbc_decrypt(host_crypto.context, aes256_key, iv, h_cipher, 32, h_plain));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.aes256_cbc_decrypt(wolf_crypto.context, aes256_key, iv, w_cipher, 32, w_plain));
    TEST_ASSERT_EQUAL_MEMORY(h_plain, w_plain, 32);

    opcua_byte_t aes128_key[16] = "1234567890abcdef";
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes128_cbc_encrypt(host_crypto.context, aes128_key, iv, input_block, 32, h_cipher));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.aes128_cbc_encrypt(wolf_crypto.context, aes128_key, iv, input_block, 32, w_cipher));
    TEST_ASSERT_EQUAL_MEMORY(h_cipher, w_cipher, 32);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.aes128_cbc_decrypt(host_crypto.context, aes128_key, iv, h_cipher, 32, h_plain));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.aes128_cbc_decrypt(wolf_crypto.context, aes128_key, iv, w_cipher, 32, w_plain));
    TEST_ASSERT_EQUAL_MEMORY(h_plain, w_plain, 32);

    opcua_byte_t sig[256];
    size_t sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.rsa_sha256_sign(wolf_crypto.context, test_data, sizeof(test_data), sig, &sig_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.rsa_sha256_verify(host_crypto.context, cert_der, cert_len, test_data,
                                                                    sizeof(test_data), sig, sig_len));

    size_t h_sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.rsa_sha256_sign(host_crypto.context, test_data, sizeof(test_data), sig, &h_sig_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf_crypto.rsa_sha256_verify(wolf_crypto.context, cert_der, cert_len, test_data,
                                                                    sizeof(test_data), sig, h_sig_len));

    opcua_byte_t oaep_cipher[256];
    size_t oaep_cipher_len = sizeof(oaep_cipher);
    opcua_byte_t oaep_plain[256];
    size_t oaep_plain_len = sizeof(oaep_plain);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.rsa_oaep_encrypt(host_crypto.context, cert_der, cert_len, test_data,
                                                                   sizeof(test_data), oaep_cipher, &oaep_cipher_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf_crypto.rsa_oaep_decrypt(wolf_crypto.context, oaep_cipher, oaep_cipher_len,
                                                                   oaep_plain, &oaep_plain_len));
    TEST_ASSERT_EQUAL(sizeof(test_data), oaep_plain_len);
    TEST_ASSERT_EQUAL_MEMORY(test_data, oaep_plain, oaep_plain_len);

    oaep_cipher_len = sizeof(oaep_cipher);
    oaep_plain_len = sizeof(oaep_plain);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf_crypto.rsa_oaep_encrypt(wolf_crypto.context, cert_der, cert_len, test_data,
                                                                   sizeof(test_data), oaep_cipher, &oaep_cipher_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.rsa_oaep_decrypt(host_crypto.context, oaep_cipher, oaep_cipher_len,
                                                                   oaep_plain, &oaep_plain_len));
    TEST_ASSERT_EQUAL(sizeof(test_data), oaep_plain_len);
    TEST_ASSERT_EQUAL_MEMORY(test_data, oaep_plain, oaep_plain_len);

    opcua_byte_t h_thumb[20], w_thumb[20];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.get_certificate_thumbprint(host_crypto.context, cert_der, cert_len, h_thumb));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.get_certificate_thumbprint(wolf_crypto.context, cert_der, cert_len, w_thumb));
    TEST_ASSERT_EQUAL_MEMORY(h_thumb, w_thumb, 20);

    size_t h_bits = 0, w_bits = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.get_certificate_key_bits(host_crypto.context, cert_der, cert_len, &h_bits));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.get_certificate_key_bits(wolf_crypto.context, cert_der, cert_len, &w_bits));
    TEST_ASSERT_EQUAL(h_bits, w_bits);

    mu_wolfssl_crypto_adapter_cleanup(&wolf_crypto);
    mu_host_crypto_adapter_cleanup(&host_crypto);
    free(key_der);
}

#else

void setUp(void) {}
void tearDown(void) {}
void test_wolfssl_dummy(void) {
    TEST_IGNORE_MESSAGE("wolfSSL support not enabled in CMake");
}

#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MICRO_OPCUA_HAVE_WOLFSSL
    RUN_TEST(test_wolfssl_adapter_all);
#else
    RUN_TEST(test_wolfssl_dummy);
#endif
    return UNITY_END();
}
