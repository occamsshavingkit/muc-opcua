/* src/platform/host_crypto/common.c
 * Adapter init/cleanup and self-signed certificate generation. */
#include "common.h"

/* Build a self-signed certificate for `key` (signed with `md`; NULL md for
   Ed25519), returning the DER encoding in a freshly OPENSSL_malloc'd buffer. */
static int make_self_signed(EVP_PKEY *key, const EVP_MD *md, unsigned char **der_out, int *der_len_out) {
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
        if (X509_set_issuer_name(x, name) == 1 && X509_set_pubkey(x, key) == 1 && X509_sign(x, key, md) != 0) {
            int len = i2d_X509(x, NULL);
            if (len > 0) {
                unsigned char *der = (unsigned char *)OPENSSL_malloc((size_t)len);
                if (der) {
                    unsigned char *p = der;
                    int wrote = i2d_X509(x, &p);
                    if (wrote > 0) {
                        *der_out = der;
                        *der_len_out = wrote;
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

static int build_self_signed(struct host_crypto_context *cx) {
    cx->key = EVP_RSA_gen(2048);
    if (!cx->key) {
        return 0;
    }
    return make_self_signed(cx->key, EVP_sha256(), &cx->cert_der, &cx->cert_der_len);
}

#ifdef MUC_OPCUA_ECC
/* Generate the per-curve server ECC identities. A failure here is non-fatal to
   RSA policies, so init keeps the adapter usable and simply leaves the ECC keys
   NULL (ECC sign then reports Bad_SecurityChecksFailed). */
static void build_ecc_identities(struct host_crypto_context *cx) {
    cx->ed25519_key = EVP_PKEY_Q_keygen(NULL, NULL, "ED25519");
    if (cx->ed25519_key &&
        !make_self_signed(cx->ed25519_key, NULL, &cx->ed25519_cert_der, &cx->ed25519_cert_der_len)) {
        EVP_PKEY_free(cx->ed25519_key);
        cx->ed25519_key = NULL;
    }
    cx->p256_key = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "P-256");
    if (cx->p256_key && !make_self_signed(cx->p256_key, EVP_sha256(), &cx->p256_cert_der, &cx->p256_cert_der_len)) {
        EVP_PKEY_free(cx->p256_key);
        cx->p256_key = NULL;
    }
}
#endif

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
#ifdef MUC_OPCUA_ECC
    build_ecc_identities(cx);
    adapter->ecc_generate_ephemeral = h_ecc_generate_ephemeral;
    adapter->ecc_ecdh_derive = h_ecc_ecdh_derive;
    adapter->ecc_keypair_free = h_ecc_keypair_free;
    adapter->ecc_sign = h_ecc_sign;
    adapter->ecc_verify = h_ecc_verify;
    adapter->get_own_ecc_certificate = h_get_own_ecc_certificate;
    adapter->chacha20_poly1305_encrypt = h_chacha20_poly1305_encrypt;
    adapter->chacha20_poly1305_decrypt = h_chacha20_poly1305_decrypt;
#endif
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
#ifdef MUC_OPCUA_ECC
    if (cx->ed25519_key) {
        EVP_PKEY_free(cx->ed25519_key);
    }
    if (cx->ed25519_cert_der) {
        OPENSSL_free(cx->ed25519_cert_der);
    }
    if (cx->p256_key) {
        EVP_PKEY_free(cx->p256_key);
    }
    if (cx->p256_cert_der) {
        OPENSSL_free(cx->p256_cert_der);
    }
#endif
    free(cx);
    adapter->context = NULL;
}
