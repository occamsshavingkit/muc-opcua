/* src/platform/wolfssl_crypto_adapter.c */
#ifdef MICRO_OPCUA_HAVE_WOLFSSL
#include "micro_opcua/platform.h"
#include "micro_opcua/status.h"
#include <stdlib.h>
#include <string.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssl/wolfcrypt/hmac.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/sha256.h>

struct wolfssl_crypto_context {
    const opcua_byte_t *cert_der;
    size_t cert_len;
    const opcua_byte_t *key_der;
    size_t key_len;
    RsaKey key;
    WC_RNG rng;
};

static const opcua_byte_t sha256_digest_info[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                                                  0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

static opcua_statuscode_t w_sha256(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *digest) {
    (void)context;
    wc_Sha256 sha;
    if (wc_InitSha256(&sha) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_Sha256Update(&sha, data, (word32)length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_Sha256Final(&sha, digest) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_hmac_sha256(void *context, const opcua_byte_t *key, size_t key_length,
                                        const opcua_byte_t *data, size_t data_length, opcua_byte_t *mac) {
    (void)context;
    Hmac hmac;
    if (wc_HmacSetKey(&hmac, WC_SHA256, key, (word32)key_length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_HmacUpdate(&hmac, data, (word32)data_length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_HmacFinal(&hmac, mac) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_aes256_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 32, iv, AES_ENCRYPTION) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_AesCbcEncrypt(&aes, output, input, (word32)length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_aes256_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 32, iv, AES_DECRYPTION) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_AesCbcDecrypt(&aes, output, input, (word32)length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_aes128_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 16, iv, AES_ENCRYPTION) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_AesCbcEncrypt(&aes, output, input, (word32)length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_aes128_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                               const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 16, iv, AES_DECRYPTION) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_AesCbcDecrypt(&aes, output, input, (word32)length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_rsa_sha256_sign(void *context, const opcua_byte_t *data, size_t length,
                                            opcua_byte_t *signature, size_t *signature_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    opcua_byte_t hash[32];
    wc_Sha256 sha;
    if (wc_InitSha256(&sha) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_Sha256Update(&sha, data, (word32)length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_Sha256Final(&sha, hash) != 0)
        return MU_STATUS_BAD_INTERNALERROR;

    opcua_byte_t digest_info[51];
    memcpy(digest_info, sha256_digest_info, 19);
    memcpy(digest_info + 19, hash, 32);

    int ret =
        wc_RsaSSL_Sign(digest_info, sizeof(digest_info), signature, (word32)*signature_length, &ctx->key, &ctx->rng);
    if (ret < 0)
        return MU_STATUS_BAD_INTERNALERROR;
    *signature_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_rsa_sha256_verify(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                              const opcua_byte_t *data, size_t data_length,
                                              const opcua_byte_t *signature, size_t signature_length) {
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

    opcua_byte_t hash[32];
    wc_Sha256 sha;
    if (wc_InitSha256(&sha) != 0 || wc_Sha256Update(&sha, data, (word32)data_length) != 0 ||
        wc_Sha256Final(&sha, hash) != 0) {
        wc_FreeRsaKey(&peer_key);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_byte_t verify_buf[512];
    ret = wc_RsaSSL_Verify(signature, (word32)signature_length, verify_buf, sizeof(verify_buf), &peer_key);
    wc_FreeRsaKey(&peer_key);

    if (ret < 0 || ret != 51 || memcmp(verify_buf, sha256_digest_info, 19) != 0 ||
        memcmp(verify_buf + 19, hash, 32) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_rsa_oaep_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                             opcua_byte_t *output, size_t *output_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;

    int ret = wc_RsaPrivateDecrypt_ex(input, (word32)length, output, (word32)*output_length, &ctx->key, WC_RSA_OAEP_PAD,
                                      WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);
    if (ret < 0)
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    *output_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_rsa_oaep_encrypt(void *context, const opcua_byte_t *certificate, size_t certificate_length,
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

    if (ret < 0)
        return MU_STATUS_BAD_INTERNALERROR;
    *output_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_get_own_certificate(void *context, const opcua_byte_t **certificate, size_t *length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    if (!ctx->cert_der)
        return MU_STATUS_BAD_INTERNALERROR;
    *certificate = ctx->cert_der;
    *length = ctx->cert_len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t w_get_certificate_key_bits(void *context, const opcua_byte_t *certificate,
                                                     size_t certificate_length, size_t *bits) {
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

static opcua_statuscode_t w_get_certificate_thumbprint(void *context, const opcua_byte_t *certificate,
                                                       size_t certificate_length, opcua_byte_t *thumbprint) {
    (void)context;
    wc_Sha sha;
    if (wc_InitSha(&sha) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_ShaUpdate(&sha, certificate, (word32)certificate_length) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    if (wc_ShaFinal(&sha, thumbprint) != 0)
        return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_wolfssl_crypto_adapter_init(mu_crypto_adapter_t *adapter, const opcua_byte_t *cert_der,
                                                  size_t cert_len, const opcua_byte_t *key_der, size_t key_len) {
    if (!adapter)
        return MU_STATUS_BAD_INTERNALERROR;

    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)calloc(1, sizeof(*ctx));
    if (!ctx)
        return MU_STATUS_BAD_OUTOFMEMORY;

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
    adapter->get_own_certificate = w_get_own_certificate;
    adapter->get_certificate_key_bits = w_get_certificate_key_bits;
    adapter->get_certificate_thumbprint = w_get_certificate_thumbprint;

    return MU_STATUS_GOOD;
}

void mu_wolfssl_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter) {
    if (!adapter || !adapter->context)
        return;
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)adapter->context;
    wc_FreeRng(&ctx->rng);
    wc_FreeRsaKey(&ctx->key);
    free(ctx);
    adapter->context = NULL;
}
#else
typedef int mu_wolfssl_dummy;
#endif
