/* tests/unit/test_wolfssl_adapter.c */
#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include "platform/host_crypto_adapter.h"
#if defined(MUC_OPCUA_HAVE_WOLFSSL) && defined(MUC_OPCUA_ECC)
#include "platform/wolfssl_crypto_adapter.h"
#endif
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MUC_OPCUA_HAVE_WOLFSSL
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

#ifdef MUC_OPCUA_ECC
/* Build a self-signed cert for `key` (md NULL for Ed25519), returning DER in a
   freshly OPENSSL_malloc'd buffer. Mirrors host_crypto/common.c make_self_signed. */
static int ecc_make_self_signed(EVP_PKEY *key, const EVP_MD *md, unsigned char **der_out, int *der_len_out) {
    X509 *x = X509_new();
    if (!x) {
        return 0;
    }
    int ok = 0;
    if (X509_set_version(x, 2) == 1) {
        ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
        X509_gmtime_adj(X509_getm_notBefore(x), 0);
        X509_gmtime_adj(X509_getm_notAfter(x), 60L * 60L * 24L * 365L);
        X509_NAME *name = X509_get_subject_name(x);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)"muc-opcua-ecc-test", -1, -1, 0);
        if (X509_set_issuer_name(x, name) == 1 && X509_set_pubkey(x, key) == 1 && X509_sign(x, key, md) != 0) {
            int len = i2d_X509(x, NULL);
            if (len > 0) {
                unsigned char *der = (unsigned char *)OPENSSL_malloc((size_t)len);
                if (der) {
                    unsigned char *p = der;
                    if (i2d_X509(x, &p) > 0) {
                        *der_out = der;
                        *der_len_out = len;
                        ok = 1;
                    } else {
                        OPENSSL_free(der);
                    }
                }
            }
        }
    }
    X509_free(x);
    return ok;
}

/* Generate an independent OpenSSL ECC identity (private-key DER + self-signed
   cert DER) with which to provision the wolfSSL adapter. */
static EVP_PKEY *ecc_gen_identity(int is_ed25519, unsigned char **key_der, int *key_len, unsigned char **cert_der,
                                  int *cert_len) {
    EVP_PKEY *pk = is_ed25519 ? EVP_PKEY_Q_keygen(NULL, NULL, "ED25519") : EVP_PKEY_Q_keygen(NULL, NULL, "EC", "P-256");
    TEST_ASSERT_NOT_NULL(pk);
    *key_der = NULL;
    *key_len = i2d_PrivateKey(pk, key_der);
    TEST_ASSERT_TRUE(*key_len > 0);
    TEST_ASSERT_TRUE(ecc_make_self_signed(pk, is_ed25519 ? NULL : EVP_sha256(), cert_der, cert_len));
    return pk;
}

/* Cross-validate one curve's sign/verify/ECDH between the wolfSSL adapter and the
   OpenSSL host oracle. `indep_cert` is the certificate matching the identity that
   the wolfSSL adapter was provisioned with. */
static void ecc_cross_validate_curve(mu_crypto_adapter_t *host, mu_crypto_adapter_t *wolf, mu_ecc_curve_t curve,
                                     const unsigned char *indep_cert, size_t indep_cert_len) {
    const opcua_byte_t data[] = "ECC SecurityPolicy cross-validation payload";
    opcua_byte_t sig[MU_ECC_SIGNATURE_LENGTH];
    size_t sig_len;

    /* 1. host signs -> wolfSSL verifies against the host's own ECC certificate. */
    const opcua_byte_t *host_cert = NULL;
    size_t host_cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host->get_own_ecc_certificate(host->context, curve, &host_cert, &host_cert_len));
    sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host->ecc_sign(host->context, curve, data, sizeof(data), sig, &sig_len));
    TEST_ASSERT_EQUAL(MU_ECC_SIGNATURE_LENGTH, sig_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf->ecc_verify(wolf->context, curve, host_cert, host_cert_len, data,
                                                       sizeof(data), sig, sig_len));

    /* 2. wolfSSL signs -> host verifies against the independent (provisioned) cert. */
    sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf->ecc_sign(wolf->context, curve, data, sizeof(data), sig, &sig_len));
    TEST_ASSERT_EQUAL(MU_ECC_SIGNATURE_LENGTH, sig_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host->ecc_verify(host->context, curve, indep_cert, indep_cert_len, data,
                                                       sizeof(data), sig, sig_len));

    /* 3. ECDH: each side derives the same shared secret from the other's pubkey. */
    opcua_byte_t pubW[MU_ECC_MAX_PUBKEY_LENGTH], pubH[MU_ECC_MAX_PUBKEY_LENGTH];
    size_t pubW_len = sizeof(pubW), pubH_len = sizeof(pubH);
    opcua_byte_t kpW[MU_ECC_KEYPAIR_CTX_SIZE], kpH[MU_ECC_KEYPAIR_CTX_SIZE];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf->ecc_generate_ephemeral(wolf->context, curve, pubW, &pubW_len, kpW));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host->ecc_generate_ephemeral(host->context, curve, pubH, &pubH_len, kpH));
    TEST_ASSERT_EQUAL(pubH_len, pubW_len);

    opcua_byte_t shW[64], shH[64];
    size_t shW_len = sizeof(shW), shH_len = sizeof(shH);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, wolf->ecc_ecdh_derive(wolf->context, curve, kpW, pubH, pubH_len, shW, &shW_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host->ecc_ecdh_derive(host->context, curve, kpH, pubW, pubW_len, shH, &shH_len));
    TEST_ASSERT_EQUAL(shH_len, shW_len);
    TEST_ASSERT_EQUAL_MEMORY(shH, shW, shW_len);

    wolf->ecc_keypair_free(wolf->context, kpW);
    host->ecc_keypair_free(host->context, kpH);
}

void test_wolfssl_ecc_cross_validation(void) {
    mu_crypto_adapter_t host_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&host_crypto));

    /* Provision the wolfSSL adapter's RSA identity with the host's RSA key/cert
       (as the RSA test does), then layer on independent ECC identities. */
    struct host_crypto_context *hc = (struct host_crypto_context *)host_crypto.context;
    unsigned char *rsa_key_der = NULL;
    int rsa_key_len = i2d_PrivateKey(hc->key, &rsa_key_der);
    TEST_ASSERT_TRUE(rsa_key_len > 0);
    const opcua_byte_t *rsa_cert = NULL;
    size_t rsa_cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, host_crypto.get_own_certificate(hc, &rsa_cert, &rsa_cert_len));

    mu_crypto_adapter_t wolf_crypto;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_wolfssl_crypto_adapter_init(&wolf_crypto, rsa_cert, rsa_cert_len, rsa_key_der,
                                                                     (size_t)rsa_key_len));

    /* Independent ECC identities (kept alive until after wolfSSL cleanup). */
    unsigned char *ed_key = NULL, *ed_cert = NULL, *p256_key = NULL, *p256_cert = NULL;
    int ed_key_len = 0, ed_cert_len = 0, p256_key_len = 0, p256_cert_len = 0;
    EVP_PKEY *ed_pk = ecc_gen_identity(1, &ed_key, &ed_key_len, &ed_cert, &ed_cert_len);
    EVP_PKEY *p256_pk = ecc_gen_identity(0, &p256_key, &p256_key_len, &p256_cert, &p256_cert_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_wolfssl_crypto_adapter_set_ecc_identity(&wolf_crypto, MU_ECC_CURVE_25519, ed_cert,
                                                                 (size_t)ed_cert_len, ed_key, (size_t)ed_key_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_wolfssl_crypto_adapter_set_ecc_identity(&wolf_crypto, MU_ECC_CURVE_NISTP256, p256_cert,
                                                                 (size_t)p256_cert_len, p256_key, (size_t)p256_key_len));

    ecc_cross_validate_curve(&host_crypto, &wolf_crypto, MU_ECC_CURVE_NISTP256, p256_cert, (size_t)p256_cert_len);
    ecc_cross_validate_curve(&host_crypto, &wolf_crypto, MU_ECC_CURVE_25519, ed_cert, (size_t)ed_cert_len);

    /* 4. ChaCha20-Poly1305 round-trips both directions. */
    opcua_byte_t cc_key[MU_CHACHA20_POLY1305_KEY_LENGTH] = "chacha20poly1305-key-32bytes!!!!";
    opcua_byte_t cc_nonce[MU_CHACHA20_POLY1305_NONCE_LENGTH] = "nonce-12byte";
    const opcua_byte_t cc_aad[] = "associated-data";
    const opcua_byte_t cc_plain[] = "ChaCha20-Poly1305 AEAD roundtrip payload";
    opcua_byte_t cc_cipher[sizeof(cc_plain)], cc_tag[MU_CHACHA20_POLY1305_TAG_LENGTH], cc_out[sizeof(cc_plain)];

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.chacha20_poly1305_encrypt(wolf_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_plain, sizeof(cc_plain), cc_cipher,
                                                            cc_tag));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.chacha20_poly1305_decrypt(host_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_cipher, sizeof(cc_plain), cc_tag, cc_out));
    TEST_ASSERT_EQUAL_MEMORY(cc_plain, cc_out, sizeof(cc_plain));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      host_crypto.chacha20_poly1305_encrypt(host_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_plain, sizeof(cc_plain), cc_cipher,
                                                            cc_tag));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      wolf_crypto.chacha20_poly1305_decrypt(wolf_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_cipher, sizeof(cc_plain), cc_tag, cc_out));
    TEST_ASSERT_EQUAL_MEMORY(cc_plain, cc_out, sizeof(cc_plain));

    /* A tampered ciphertext must fail the tag check. */
    cc_cipher[0] ^= 0xFF;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURITYCHECKSFAILED,
                      wolf_crypto.chacha20_poly1305_decrypt(wolf_crypto.context, cc_key, cc_nonce, cc_aad,
                                                            sizeof(cc_aad), cc_cipher, sizeof(cc_plain), cc_tag, cc_out));

    mu_wolfssl_crypto_adapter_cleanup(&wolf_crypto);
    mu_host_crypto_adapter_cleanup(&host_crypto);
    EVP_PKEY_free(ed_pk);
    EVP_PKEY_free(p256_pk);
    OPENSSL_free(ed_key);
    OPENSSL_free(ed_cert);
    OPENSSL_free(p256_key);
    OPENSSL_free(p256_cert);
    free(rsa_key_der);
}
#endif /* MUC_OPCUA_ECC */

#else

void setUp(void) {}
void tearDown(void) {}
void test_wolfssl_dummy(void) {
    TEST_IGNORE_MESSAGE("wolfSSL support not enabled in CMake");
}

#endif

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_HAVE_WOLFSSL
    RUN_TEST(test_wolfssl_adapter_all);
#ifdef MUC_OPCUA_ECC
    RUN_TEST(test_wolfssl_ecc_cross_validation);
#endif
#else
    RUN_TEST(test_wolfssl_dummy);
#endif
    return UNITY_END();
}
