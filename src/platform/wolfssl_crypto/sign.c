#ifdef MUC_OPCUA_HAVE_WOLFSSL
#include "common.h"

opcua_statuscode_t w_rsa_sha256_sign(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *signature,
                                     size_t *signature_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    opcua_byte_t hash[32];
    wc_Sha256 sha;
    if (wc_InitSha256(&sha) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_Sha256Update(&sha, data, (word32)length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_Sha256Final(&sha, hash) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_byte_t digest_info[51];
    (void)memcpy(digest_info, sha256_digest_info, 19);
    (void)memcpy(digest_info + 19, hash, 32);

    int ret =
        wc_RsaSSL_Sign(digest_info, sizeof(digest_info), signature, (word32)*signature_length, &ctx->key, &ctx->rng);
    if (ret < 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    *signature_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_rsa_sha256_verify(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                       const opcua_byte_t *data, size_t data_length, const opcua_byte_t *signature,
                                       size_t signature_length) {
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

opcua_statuscode_t w_rsa_pss_sha256_sign(void *context, const opcua_byte_t *data, size_t length,
                                         opcua_byte_t *signature, size_t *signature_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    opcua_byte_t hash[32];
    wc_Sha256 sha;
    if (wc_InitSha256(&sha) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_Sha256Update(&sha, data, (word32)length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_Sha256Final(&sha, hash) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    int ret = wc_RsaPSS_Sign(hash, sizeof(hash), signature, (word32)*signature_length, WC_HASH_TYPE_SHA256,
                             WC_MGF1SHA256, &ctx->key, &ctx->rng);
    if (ret < 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    *signature_length = (size_t)ret;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_rsa_pss_sha256_verify(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                           const opcua_byte_t *data, size_t data_length, const opcua_byte_t *signature,
                                           size_t signature_length) {
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

    opcua_byte_t verify_buf[32];
    ret = wc_RsaPSS_Verify((byte *)signature, (word32)signature_length, verify_buf, sizeof(verify_buf),
                           WC_HASH_TYPE_SHA256, WC_MGF1SHA256, &peer_key);
    wc_FreeRsaKey(&peer_key);

    if (ret < 0 || ret != 32 || memcmp(verify_buf, hash, 32) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    return MU_STATUS_GOOD;
}
#else
typedef int mu_wolfssl_dummy_sign;
#endif
