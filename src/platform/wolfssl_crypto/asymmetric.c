#ifdef MUC_OPCUA_HAVE_WOLFSSL
#include "common.h"

opcua_statuscode_t w_rsa_oaep_decrypt(void *context, const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                      size_t *output_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;

    int ret = wc_RsaPrivateDecrypt_ex(input, (word32)length, output, (word32)*output_length, &ctx->key, WC_RSA_OAEP_PAD,
                                      WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);
    if (ret < 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    *output_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_rsa_oaep_encrypt(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                      const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                      size_t *output_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    DecodedCert decoded;
    InitDecodedCert(&decoded, certificate, (word32)certificate_length, NULL);
    int ret = ParseCert(&decoded, CERT_TYPE, NO_VERIFY, NULL);
    if (ret != 0) {
        FreeDecodedCert(&decoded);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    RsaKey peer_key;
    if (wc_InitRsaKey(&peer_key, NULL) != 0) {
        FreeDecodedCert(&decoded);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    word32 idx = 0;
    ret = wc_RsaPublicKeyDecode(decoded.publicKey, &idx, &peer_key, decoded.pubKeySize);
    FreeDecodedCert(&decoded);
    if (ret != 0) {
        wc_FreeRsaKey(&peer_key);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    ret = wc_RsaPublicEncrypt_ex(input, (word32)length, output, (word32)*output_length, &peer_key, &ctx->rng,
                                 WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);
    wc_FreeRsaKey(&peer_key);

    if (ret < 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    *output_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_rsa_oaep_sha256_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                             opcua_byte_t *output, size_t *output_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;

    int ret = wc_RsaPrivateDecrypt_ex(input, (word32)length, output, (word32)*output_length, &ctx->key, WC_RSA_OAEP_PAD,
                                      WC_HASH_TYPE_SHA256, WC_MGF1SHA256, NULL, 0);
    if (ret < 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    *output_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_rsa_oaep_sha256_encrypt(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                             const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                             size_t *output_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    DecodedCert decoded;
    InitDecodedCert(&decoded, certificate, (word32)certificate_length, NULL);
    int ret = ParseCert(&decoded, CERT_TYPE, NO_VERIFY, NULL);
    if (ret != 0) {
        FreeDecodedCert(&decoded);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    RsaKey peer_key;
    if (wc_InitRsaKey(&peer_key, NULL) != 0) {
        FreeDecodedCert(&decoded);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    word32 idx = 0;
    ret = wc_RsaPublicKeyDecode(decoded.publicKey, &idx, &peer_key, decoded.pubKeySize);
    FreeDecodedCert(&decoded);
    if (ret != 0) {
        wc_FreeRsaKey(&peer_key);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    ret = wc_RsaPublicEncrypt_ex(input, (word32)length, output, (word32)*output_length, &peer_key, &ctx->rng,
                                 WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA256, WC_MGF1SHA256, NULL, 0);
    wc_FreeRsaKey(&peer_key);

    if (ret < 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    *output_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_get_own_certificate(void *context, const opcua_byte_t **certificate, size_t *length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    if (!ctx->cert_der) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    *certificate = ctx->cert_der;
    *length = ctx->cert_len;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_get_certificate_key_bits(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                              size_t *bits) {
    (void)context;
    DecodedCert decoded;
    InitDecodedCert(&decoded, certificate, (word32)certificate_length, NULL);
    int ret = ParseCert(&decoded, CERT_TYPE, NO_VERIFY, NULL);
    if (ret != 0) {
        FreeDecodedCert(&decoded);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    RsaKey peer_key;
    if (wc_InitRsaKey(&peer_key, NULL) != 0) {
        FreeDecodedCert(&decoded);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    word32 idx = 0;
    ret = wc_RsaPublicKeyDecode(decoded.publicKey, &idx, &peer_key, decoded.pubKeySize);
    FreeDecodedCert(&decoded);
    if (ret != 0) {
        wc_FreeRsaKey(&peer_key);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    *bits = (size_t)(wc_RsaEncryptSize(&peer_key) * 8);

    wc_FreeRsaKey(&peer_key);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_verify_certificate_validity(void *context, const opcua_byte_t *certificate,
                                                 size_t certificate_length) {
    (void)context;
    DecodedCert decoded;
    InitDecodedCert(&decoded, certificate, (word32)certificate_length, NULL);
    int ret = ParseCert(&decoded, CERT_TYPE, VERIFY, NULL);
    FreeDecodedCert(&decoded);
    if (ret == 0) {
        return MU_STATUS_GOOD;
    }
#ifdef ASN_BEFORE_DATE_E
    if (ret == ASN_BEFORE_DATE_E) {
        return MU_STATUS_BAD_CERTIFICATETIMEINVALID;
    }
#endif
#ifdef ASN_AFTER_DATE_E
    if (ret == ASN_AFTER_DATE_E) {
        return MU_STATUS_BAD_CERTIFICATETIMEINVALID;
    }
#endif
    return MU_STATUS_BAD_CERTIFICATEINVALID;
}

opcua_statuscode_t w_get_certificate_thumbprint(void *context, const opcua_byte_t *certificate,
                                                size_t certificate_length, opcua_byte_t *thumbprint) {
    (void)context;
    wc_Sha sha;
    if (wc_InitSha(&sha) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_ShaUpdate(&sha, certificate, (word32)certificate_length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_ShaFinal(&sha, thumbprint) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_wolfssl_crypto_adapter_init(mu_crypto_adapter_t *adapter, const opcua_byte_t *cert_der,
                                                  size_t cert_len, const opcua_byte_t *key_der, size_t key_len) {
    if (!adapter) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* wolfSSL crypto adapter (host/RTOS platforms with a heap); not gated by
       MUC_OPCUA_ALLOW_HEAP, which governs the freestanding no-heap core. */
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)calloc(1, sizeof(*ctx));
    if (!ctx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    ctx->cert_der = cert_der;
    ctx->cert_len = cert_len;
    ctx->key_der = key_der;
    ctx->key_len = key_len;

    if (wc_InitRsaKey(&ctx->key, NULL) != 0) {
        free(ctx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (wc_InitRng(&ctx->rng) != 0) {
        wc_FreeRsaKey(&ctx->key);
        free(ctx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

#ifdef WC_RSA_BLINDING
    if (wc_RsaSetRNG(&ctx->key, &ctx->rng) != 0) {
        wc_FreeRng(&ctx->rng);
        wc_FreeRsaKey(&ctx->key);
        free(ctx);
        return MU_STATUS_BAD_INTERNALERROR;
    }
#endif

    word32 idx = 0;
    int ret = wc_RsaPrivateKeyDecode(key_der, &idx, &ctx->key, (word32)key_len);
    if (ret != 0) {
        wc_FreeRng(&ctx->rng);
        wc_FreeRsaKey(&ctx->key);
        free(ctx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    adapter->context = ctx;
    adapter->sha256 = w_sha256;
    adapter->hmac_sha256 = w_hmac_sha256;
    adapter->aes256_cbc_encrypt = w_aes256_cbc_encrypt;
    adapter->aes256_cbc_decrypt = w_aes256_cbc_decrypt;
    adapter->aes128_cbc_encrypt = w_aes128_cbc_encrypt;
    adapter->aes128_cbc_decrypt = w_aes128_cbc_decrypt;
    adapter->cipher_ctx_init = NULL;
    adapter->aes256_cbc_encrypt_ctx = NULL;
    adapter->aes256_cbc_decrypt_ctx = NULL;
    adapter->cipher_ctx_free = NULL;
    adapter->rsa_sha256_sign = w_rsa_sha256_sign;
    adapter->rsa_sha256_verify = w_rsa_sha256_verify;
    adapter->rsa_oaep_decrypt = w_rsa_oaep_decrypt;
    adapter->rsa_oaep_encrypt = w_rsa_oaep_encrypt;
    adapter->rsa_pss_sha256_sign = w_rsa_pss_sha256_sign;
    adapter->rsa_pss_sha256_verify = w_rsa_pss_sha256_verify;
    adapter->rsa_oaep_sha256_decrypt = w_rsa_oaep_sha256_decrypt;
    adapter->rsa_oaep_sha256_encrypt = w_rsa_oaep_sha256_encrypt;
    adapter->get_own_certificate = w_get_own_certificate;
    adapter->get_certificate_key_bits = w_get_certificate_key_bits;
    adapter->get_certificate_thumbprint = w_get_certificate_thumbprint;
    adapter->verify_certificate_validity = w_verify_certificate_validity;
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    /* ECC identity keys are provisioned later via
       mu_wolfssl_crypto_adapter_set_ecc_identity; the vtable slots are wired now. */
    adapter->ecc_generate_ephemeral = w_ecc_generate_ephemeral;
    adapter->ecc_ecdh_derive = w_ecc_ecdh_derive;
    adapter->ecc_keypair_free = w_ecc_keypair_free;
    adapter->ecc_sign = w_ecc_sign;
    adapter->ecc_verify = w_ecc_verify;
    adapter->get_own_ecc_certificate = w_get_own_ecc_certificate;
    adapter->chacha20_poly1305_encrypt = w_chacha20_poly1305_encrypt;
    adapter->chacha20_poly1305_decrypt = w_chacha20_poly1305_decrypt;
#endif

    return MU_STATUS_GOOD;
}

void mu_wolfssl_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter) {
    if (!adapter || !adapter->context) {
        return;
    }
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)adapter->context;
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    if (ctx->p256_key_set) {
        wc_ecc_free(&ctx->p256_key);
    }
    if (ctx->ed_key_set) {
        wc_ed25519_free(&ctx->ed_key);
    }
#endif
    wc_FreeRng(&ctx->rng);
    wc_FreeRsaKey(&ctx->key);
    free(ctx);
    adapter->context = NULL;
}
#else
typedef int mu_wolfssl_dummy_asymmetric;
#endif
