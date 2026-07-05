/* src/platform/host_crypto/sign.c
 * RSA-PSS sign/verify and RSA-PKCS1.5 sign. */
#include "common.h"

opcua_statuscode_t h_rsa_sign(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *sig, size_t *sig_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    if (!md) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    size_t sl = *sig_len;
    if (EVP_DigestSignInit(md, NULL, EVP_sha256(), NULL, cx->key) == 1 &&
        EVP_DigestSign(md, sig, &sl, data, len) == 1) {
        *sig_len = sl;
        rc = MU_STATUS_GOOD;
    }
    EVP_MD_CTX_free(md);
    return rc;
}

opcua_statuscode_t h_rsa_verify(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *data,
                                size_t data_len, const opcua_byte_t *sig, size_t sig_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    if (md && EVP_DigestVerifyInit(md, NULL, EVP_sha256(), NULL, pk) == 1 &&
        EVP_DigestVerify(md, sig, sig_len, data, data_len) == 1) {
        rc = MU_STATUS_GOOD;
    }
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pk);
    return rc;
}

opcua_statuscode_t h_rsa_pss_sha256_sign(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *sig,
                                         size_t *sig_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    if (!md) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    size_t sl = *sig_len;
    EVP_PKEY_CTX *pctx = NULL;
    if (EVP_DigestSignInit(md, &pctx, EVP_sha256(), NULL, cx->key) == 1) {
        if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) == 1 &&
            EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, 32) == 1 && EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, EVP_sha256()) == 1 &&
            EVP_DigestSign(md, sig, &sl, data, len) == 1) {
            *sig_len = sl;
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_MD_CTX_free(md);
    return rc;
}

opcua_statuscode_t h_rsa_pss_sha256_verify(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *data,
                                           size_t data_len, const opcua_byte_t *sig, size_t sig_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    EVP_PKEY_CTX *pctx = NULL;
    if (md && EVP_DigestVerifyInit(md, &pctx, EVP_sha256(), NULL, pk) == 1) {
        if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) == 1 &&
            EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, 32) == 1 && EVP_PKEY_CTX_set_rsa_mgf1_md(pctx, EVP_sha256()) == 1 &&
            EVP_DigestVerify(md, sig, sig_len, data, data_len) == 1) {
            rc = MU_STATUS_GOOD;
        }
    }
    if (md) {
        EVP_MD_CTX_free(md);
    }
    EVP_PKEY_free(pk);
    return rc;
}
