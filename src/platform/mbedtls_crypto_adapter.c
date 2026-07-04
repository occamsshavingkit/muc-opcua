/* src/platform/mbedtls_crypto_adapter.c */
#ifdef MUC_OPCUA_HAVE_MBEDTLS
#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include <mbedtls/aes.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <stdlib.h>
#include <string.h>

struct mbedtls_crypto_context {
    const opcua_byte_t *cert_der;
    size_t cert_len;
    const opcua_byte_t *key_der;
    size_t key_len;
    mbedtls_pk_context pk;
};

static opcua_statuscode_t m_sha256(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *digest) {
    (void)context;
    int ret = mbedtls_sha256_ret(data, length, digest, 0);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_hmac_sha256(void *context, const opcua_byte_t *key, size_t key_length,
                                        const opcua_byte_t *data, size_t data_length, opcua_byte_t *mac) {
    (void)context;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    int ret = mbedtls_md_hmac(md_info, key, key_length, data, data_length, mac);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_aes256_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_byte_t iv_copy[16];
    (void)memcpy(iv_copy, iv, 16);
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iv_copy, input, output);
    mbedtls_aes_free(&aes);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_aes256_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_dec(&aes, key, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_byte_t iv_copy[16];
    (void)memcpy(iv_copy, iv, 16);
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv_copy, input, output);
    mbedtls_aes_free(&aes);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_aes128_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_enc(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_byte_t iv_copy[16];
    memcpy(iv_copy, iv, 16);
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, length, iv_copy, input, output);
    mbedtls_aes_free(&aes);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_aes128_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_dec(&aes, key, 128);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_byte_t iv_copy[16];
    memcpy(iv_copy, iv, 16);
    ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv_copy, input, output);
    mbedtls_aes_free(&aes);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_rsa_sha256_sign(void *context, const opcua_byte_t *data, size_t length,
                                            opcua_byte_t *signature, size_t *signature_length) {
    struct mbedtls_crypto_context *ctx = (struct mbedtls_crypto_context *)context;
    opcua_byte_t hash[32];
    int ret = mbedtls_sha256_ret(data, length, hash, 0);
    if (ret != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    ret = mbedtls_pk_sign(&ctx->pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, signature_length, NULL, NULL);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_rsa_sha256_verify(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                              const opcua_byte_t *data, size_t data_length,
                                              const opcua_byte_t *signature, size_t signature_length) {
    (void)context;
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    int ret = mbedtls_x509_crt_parse_der(&crt, certificate, certificate_length);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_byte_t hash[32];
    ret = mbedtls_sha256_ret(data, data_length, hash, 0);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    ret = mbedtls_pk_verify(&crt.pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, signature_length);
    mbedtls_x509_crt_free(&crt);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

static opcua_statuscode_t m_rsa_oaep_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                             opcua_byte_t *output, size_t *output_length) {
    (void)length;
    struct mbedtls_crypto_context *ctx = (struct mbedtls_crypto_context *)context;
    if (mbedtls_pk_get_type(&ctx->pk) != MBEDTLS_PK_RSA) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(ctx->pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1);

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"oaep", 4);
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    ret = mbedtls_rsa_rsaes_oaep_decrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE, NULL, 0,
                                         output_length, input, output, *output_length);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

static opcua_statuscode_t m_rsa_oaep_encrypt(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                             const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                             size_t *output_length) {
    (void)context;
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    int ret = mbedtls_x509_crt_parse_der(&crt, certificate, certificate_length);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    if (mbedtls_pk_get_type(&crt.pk) != MBEDTLS_PK_RSA) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(crt.pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1);

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"oaep", 4);
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    ret = mbedtls_rsa_rsaes_oaep_encrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PUBLIC, NULL, 0, length,
                                         input, output);
    *output_length = rsa->len;

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&crt);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_rsa_pss_sha256_sign(void *context, const opcua_byte_t *data, size_t length,
                                                opcua_byte_t *signature, size_t *signature_length) {
    struct mbedtls_crypto_context *ctx = (struct mbedtls_crypto_context *)context;
    if (mbedtls_pk_get_type(&ctx->pk) != MBEDTLS_PK_RSA)
        return MU_STATUS_BAD_INTERNALERROR;
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(ctx->pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

    opcua_byte_t hash[32];
    int ret = mbedtls_sha256_ret(data, length, hash, 0);
    if (ret != 0)
        return MU_STATUS_BAD_INTERNALERROR;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"pss", 3);
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    ret = mbedtls_rsa_rsassa_pss_sign(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE, MBEDTLS_MD_SHA256,
                                      sizeof(hash), hash, signature);
    if (ret == 0) {
        *signature_length = mbedtls_rsa_get_len(rsa);
    }
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

static opcua_statuscode_t m_rsa_pss_sha256_verify(void *context, const opcua_byte_t *certificate,
                                                  size_t certificate_length, const opcua_byte_t *data,
                                                  size_t data_length, const opcua_byte_t *signature,
                                                  size_t signature_length) {
    (void)context;
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    int ret = mbedtls_x509_crt_parse_der(&crt, certificate, certificate_length);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    if (mbedtls_pk_get_type(&crt.pk) != MBEDTLS_PK_RSA) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(crt.pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

    opcua_byte_t hash[32];
    ret = mbedtls_sha256_ret(data, data_length, hash, 0);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (signature_length != mbedtls_rsa_get_len(rsa)) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    ret = mbedtls_rsa_rsassa_pss_verify(rsa, mbedtls_ctr_drbg_random, NULL, MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA256,
                                        sizeof(hash), hash, signature);
    mbedtls_x509_crt_free(&crt);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

static opcua_statuscode_t m_rsa_oaep_sha256_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                                    opcua_byte_t *output, size_t *output_length) {
    (void)length;
    struct mbedtls_crypto_context *ctx = (struct mbedtls_crypto_context *)context;
    if (mbedtls_pk_get_type(&ctx->pk) != MBEDTLS_PK_RSA)
        return MU_STATUS_BAD_INTERNALERROR;
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(ctx->pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"oaep", 4);
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    ret = mbedtls_rsa_rsaes_oaep_decrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PRIVATE, NULL, 0,
                                         output_length, input, output, *output_length);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

static opcua_statuscode_t m_rsa_oaep_sha256_encrypt(void *context, const opcua_byte_t *certificate,
                                                    size_t certificate_length, const opcua_byte_t *input, size_t length,
                                                    opcua_byte_t *output, size_t *output_length) {
    (void)context;
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    int ret = mbedtls_x509_crt_parse_der(&crt, certificate, certificate_length);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    if (mbedtls_pk_get_type(&crt.pk) != MBEDTLS_PK_RSA) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_INTERNALERROR;
    }
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(crt.pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)"oaep", 4);
    if (ret != 0) {
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    ret = mbedtls_rsa_rsaes_oaep_encrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PUBLIC, NULL, 0, length,
                                         input, output);
    if (ret == 0) {
        *output_length = mbedtls_rsa_get_len(rsa);
    }
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&crt);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_SECURITYCHECKSFAILED;
}

static opcua_statuscode_t m_get_own_certificate(void *context, const opcua_byte_t **certificate, size_t *length) {
    struct mbedtls_crypto_context *ctx = (struct mbedtls_crypto_context *)context;
    if (!ctx->cert_der) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    *certificate = ctx->cert_der;
    *length = ctx->cert_len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t m_get_certificate_key_bits(void *context, const opcua_byte_t *certificate,
                                                     size_t certificate_length, size_t *bits) {
    (void)context;
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    int ret = mbedtls_x509_crt_parse_der(&crt, certificate, certificate_length);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    *bits = mbedtls_pk_get_bitlen(&crt.pk);
    mbedtls_x509_crt_free(&crt);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t m_verify_certificate_validity(void *context, const opcua_byte_t *certificate,
                                                        size_t certificate_length) {
    (void)context;
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    int ret = mbedtls_x509_crt_parse_der(&crt, certificate, certificate_length);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    /* OPC-10000-4 §5.5: reject not-yet-valid or expired certificates. */
    int invalid = mbedtls_x509_time_is_future(&crt.valid_from) || mbedtls_x509_time_is_past(&crt.valid_to);
    mbedtls_x509_crt_free(&crt);
    return invalid ? MU_STATUS_BAD_CERTIFICATETIMEINVALID : MU_STATUS_GOOD;
}

static opcua_statuscode_t m_get_certificate_thumbprint(void *context, const opcua_byte_t *certificate,
                                                       size_t certificate_length, opcua_byte_t *thumbprint) {
    (void)context;
    int ret = mbedtls_sha1_ret(certificate, certificate_length, thumbprint);
    return (ret == 0) ? MU_STATUS_GOOD : MU_STATUS_BAD_INTERNALERROR;
}

opcua_statuscode_t mu_mbedtls_crypto_adapter_init(mu_crypto_adapter_t *adapter, const opcua_byte_t *cert_der,
                                                  size_t cert_len, const opcua_byte_t *key_der, size_t key_len) {
    if (!adapter) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    struct mbedtls_crypto_context *ctx = (struct mbedtls_crypto_context *)calloc(1, sizeof(*ctx));
    if (!ctx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    ctx->cert_der = cert_der;
    ctx->cert_len = cert_len;
    ctx->key_der = key_der;
    ctx->key_len = key_len;

    mbedtls_pk_init(&ctx->pk);
    int ret = mbedtls_pk_parse_key(&ctx->pk, key_der, key_len, NULL, 0);
    if (ret != 0) {
        mbedtls_pk_free(&ctx->pk);
        free(ctx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    adapter->context = ctx;
    adapter->sha256 = m_sha256;
    adapter->hmac_sha256 = m_hmac_sha256;
    adapter->aes256_cbc_encrypt = m_aes256_cbc_encrypt;
    adapter->aes256_cbc_decrypt = m_aes256_cbc_decrypt;
    adapter->aes128_cbc_encrypt = m_aes128_cbc_encrypt;
    adapter->aes128_cbc_decrypt = m_aes128_cbc_decrypt;
    adapter->cipher_ctx_init = NULL;
    adapter->aes256_cbc_encrypt_ctx = NULL;
    adapter->aes256_cbc_decrypt_ctx = NULL;
    adapter->cipher_ctx_free = NULL;
    adapter->rsa_sha256_sign = m_rsa_sha256_sign;
    adapter->rsa_sha256_verify = m_rsa_sha256_verify;
    adapter->rsa_oaep_decrypt = m_rsa_oaep_decrypt;
    adapter->rsa_oaep_encrypt = m_rsa_oaep_encrypt;
    adapter->rsa_pss_sha256_sign = m_rsa_pss_sha256_sign;
    adapter->rsa_pss_sha256_verify = m_rsa_pss_sha256_verify;
    adapter->rsa_oaep_sha256_decrypt = m_rsa_oaep_sha256_decrypt;
    adapter->rsa_oaep_sha256_encrypt = m_rsa_oaep_sha256_encrypt;
    adapter->get_own_certificate = m_get_own_certificate;
    adapter->get_certificate_key_bits = m_get_certificate_key_bits;
    adapter->get_certificate_thumbprint = m_get_certificate_thumbprint;
    adapter->verify_certificate_validity = m_verify_certificate_validity;

    return MU_STATUS_GOOD;
}

void mu_mbedtls_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter) {
    if (!adapter || !adapter->context) {
        return;
    }
    struct mbedtls_crypto_context *ctx = (struct mbedtls_crypto_context *)adapter->context;
    mbedtls_pk_free(&ctx->pk);
    free(ctx);
    adapter->context = NULL;
}
#else
typedef int mu_mbedtls_dummy;
#endif
