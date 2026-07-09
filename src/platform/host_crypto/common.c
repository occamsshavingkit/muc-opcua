/* src/platform/host_crypto/common.c
 * Adapter init/cleanup and self-signed certificate generation. */
#include "common.h"

static int build_self_signed(struct host_crypto_context *cx) {
    cx->key = EVP_RSA_gen(2048);
    if (!cx->key) {
        return 0;
    }

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
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)"muc-opcua", -1, -1, 0);
        if (X509_set_issuer_name(x, name) == 1 && X509_set_pubkey(x, cx->key) == 1 &&
            X509_sign(x, cx->key, EVP_sha256()) != 0) {
            int len = i2d_X509(x, NULL);
            if (len > 0) {
                cx->cert_der = (unsigned char *)OPENSSL_malloc((size_t)len);
                if (cx->cert_der) {
                    unsigned char *p = cx->cert_der;
                    cx->cert_der_len = i2d_X509(x, &p);
                    ok = (cx->cert_der_len > 0);
                }
            }
        }
    }
    X509_free(x);
    return ok;
}

opcua_statuscode_t mu_host_crypto_adapter_init(mu_crypto_adapter_t *adapter) {
    if (!adapter) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Host crypto adapter: runs only on the host platform where a full heap
       exists, never on the freestanding no-heap core, so its context allocation
       is intentionally not gated by MUC_OPCUA_ALLOW_HEAP. */
    struct host_crypto_context *cx = (struct host_crypto_context *)calloc(1, sizeof(*cx));
    if (!cx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    if (!build_self_signed(cx)) {
        if (cx->key) {
            EVP_PKEY_free(cx->key);
        }
        if (cx->cert_der) {
            OPENSSL_free(cx->cert_der);
        }
        free(cx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    adapter->context = cx;
    adapter->sha256 = h_sha256;
    adapter->hmac_sha256 = h_hmac_sha256;
    adapter->aes256_cbc_encrypt = h_aes_encrypt;
    adapter->aes256_cbc_decrypt = h_aes_decrypt;
    adapter->aes128_cbc_encrypt = h_aes128_encrypt;
    adapter->aes128_cbc_decrypt = h_aes128_decrypt;
    adapter->cipher_ctx_init = h_cipher_ctx_init;
    adapter->aes256_cbc_encrypt_ctx = h_aes_encrypt_ctx;
    adapter->aes256_cbc_decrypt_ctx = h_aes_decrypt_ctx;
    adapter->cipher_ctx_free = h_cipher_ctx_free;
    adapter->rsa_sha256_sign = h_rsa_sign;
    adapter->rsa_sha256_verify = h_rsa_verify;
    adapter->rsa_oaep_decrypt = h_rsa_oaep_decrypt;
    adapter->rsa_oaep_encrypt = h_rsa_oaep_encrypt;
    adapter->rsa_pss_sha256_sign = h_rsa_pss_sha256_sign;
    adapter->rsa_pss_sha256_verify = h_rsa_pss_sha256_verify;
    adapter->rsa_oaep_sha256_decrypt = h_rsa_oaep_sha256_decrypt;
    adapter->rsa_oaep_sha256_encrypt = h_rsa_oaep_sha256_encrypt;
    adapter->get_own_certificate = h_get_own_certificate;
    adapter->get_certificate_key_bits = h_get_certificate_key_bits;
    adapter->get_certificate_thumbprint = h_get_certificate_thumbprint;
    adapter->verify_certificate_validity = h_verify_certificate_validity;
    adapter->verify_certificate_application_uri = h_verify_certificate_application_uri;
    return MU_STATUS_GOOD;
}

void mu_host_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter) {
    if (!adapter || !adapter->context) {
        return;
    }
    struct host_crypto_context *cx = (struct host_crypto_context *)adapter->context;
    if (cx->key) {
        EVP_PKEY_free(cx->key);
    }
    if (cx->cert_der) {
        OPENSSL_free(cx->cert_der);
    }
    free(cx);
    adapter->context = NULL;
}
