/* src/platform/host_crypto_adapter.c
 * OpenSSL-backed crypto adapter for SecurityPolicy Basic256Sha256 (host/dev). */
#include "host_crypto_adapter.h"
#include "micro_opcua/status.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <stdlib.h>
#include <string.h>

struct host_crypto_context {
    EVP_PKEY *key;            /* server RSA private key */
    unsigned char *cert_der;  /* server certificate, DER-encoded */
    int cert_der_len;
};

/* ---- digest / mac / symmetric ---- */

static opcua_statuscode_t h_sha256(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *digest) {
    (void)c;
    unsigned int dlen = 0;
    if (EVP_Digest(data, len, digest, &dlen, EVP_sha256(), NULL) != 1) return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t h_hmac_sha256(void *c, const opcua_byte_t *key, size_t key_len,
                                        const opcua_byte_t *data, size_t data_len, opcua_byte_t *mac) {
    (void)c;
    unsigned int mlen = 0;
    if (HMAC(EVP_sha256(), key, (int)key_len, data, data_len, mac, &mlen) == NULL) return MU_STATUS_BAD_INTERNALERROR;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t aes_cbc(const opcua_byte_t *key, const opcua_byte_t *iv,
                                  const opcua_byte_t *in, size_t len, opcua_byte_t *out, int encrypt) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return MU_STATUS_BAD_OUTOFMEMORY;
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    int outl = 0, finl = 0;
    int ok = encrypt
        ? (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) == 1 &&
           EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
           EVP_EncryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
           EVP_EncryptFinal_ex(ctx, out + outl, &finl) == 1)
        : (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) == 1 &&
           EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
           EVP_DecryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
           EVP_DecryptFinal_ex(ctx, out + outl, &finl) == 1);
    if (ok) rc = MU_STATUS_GOOD;
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

static opcua_statuscode_t h_aes_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c; return aes_cbc(key, iv, in, len, out, 1);
}
static opcua_statuscode_t h_aes_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c; return aes_cbc(key, iv, in, len, out, 0);
}

/* ---- RSA ---- */

static opcua_statuscode_t h_rsa_sign(void *c, const opcua_byte_t *data, size_t len,
                                     opcua_byte_t *sig, size_t *sig_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    if (!md) return MU_STATUS_BAD_OUTOFMEMORY;
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

static EVP_PKEY *pubkey_from_cert(const opcua_byte_t *cert, size_t cert_len) {
    const unsigned char *p = cert;
    X509 *x = d2i_X509(NULL, &p, (long)cert_len);
    if (!x) return NULL;
    EVP_PKEY *pk = X509_get_pubkey(x);
    X509_free(x);
    return pk;
}

static opcua_statuscode_t h_rsa_verify(void *c, const opcua_byte_t *cert, size_t cert_len,
                                       const opcua_byte_t *data, size_t data_len,
                                       const opcua_byte_t *sig, size_t sig_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) return MU_STATUS_BAD_SECURITYCHECKSFAILED;
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

static opcua_statuscode_t rsa_oaep(EVP_PKEY *pk, int encrypt,
                                   const opcua_byte_t *in, size_t in_len,
                                   opcua_byte_t *out, size_t *out_len) {
    EVP_PKEY_CTX *pc = EVP_PKEY_CTX_new(pk, NULL);
    if (!pc) return MU_STATUS_BAD_INTERNALERROR;
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    size_t ol = *out_len;
    int init_ok = encrypt ? (EVP_PKEY_encrypt_init(pc) == 1) : (EVP_PKEY_decrypt_init(pc) == 1);
    if (init_ok && EVP_PKEY_CTX_set_rsa_padding(pc, RSA_PKCS1_OAEP_PADDING) == 1) {
        int op = encrypt ? EVP_PKEY_encrypt(pc, out, &ol, in, in_len)
                         : EVP_PKEY_decrypt(pc, out, &ol, in, in_len);
        if (op == 1) {
            *out_len = ol;
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_PKEY_CTX_free(pc);
    return rc;
}

static opcua_statuscode_t h_rsa_oaep_decrypt(void *c, const opcua_byte_t *in, size_t len,
                                             opcua_byte_t *out, size_t *out_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    return rsa_oaep(cx->key, 0, in, len, out, out_len);
}

static opcua_statuscode_t h_rsa_oaep_encrypt(void *c, const opcua_byte_t *cert, size_t cert_len,
                                             const opcua_byte_t *in, size_t len,
                                             opcua_byte_t *out, size_t *out_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    opcua_statuscode_t rc = rsa_oaep(pk, 1, in, len, out, out_len);
    EVP_PKEY_free(pk);
    return rc;
}

static opcua_statuscode_t h_get_own_certificate(void *c, const opcua_byte_t **cert, size_t *len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    *cert = cx->cert_der;
    *len = (size_t)cx->cert_der_len;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t h_get_certificate_key_bits(void *c, const opcua_byte_t *cert, size_t cert_len, size_t *bits) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    *bits = (size_t)EVP_PKEY_get_bits(pk);
    EVP_PKEY_free(pk);
    return MU_STATUS_GOOD;
}

/* ---- init / cleanup ---- */

static int build_self_signed(struct host_crypto_context *cx) {
    cx->key = EVP_RSA_gen(2048);
    if (!cx->key) return 0;

    X509 *x = X509_new();
    if (!x) return 0;
    int ok = 0;
    if (X509_set_version(x, 2) == 1) {
        ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
        X509_gmtime_adj(X509_getm_notBefore(x), 0);
        X509_gmtime_adj(X509_getm_notAfter(x), 60L * 60L * 24L * 365L);
        X509_NAME *name = X509_get_subject_name(x);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)"micro-opcua", -1, -1, 0);
        if (X509_set_issuer_name(x, name) == 1 &&
            X509_set_pubkey(x, cx->key) == 1 &&
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
    if (!adapter) return MU_STATUS_BAD_INTERNALERROR;

    struct host_crypto_context *cx = (struct host_crypto_context *)calloc(1, sizeof(*cx));
    if (!cx) return MU_STATUS_BAD_OUTOFMEMORY;

    if (!build_self_signed(cx)) {
        if (cx->key) EVP_PKEY_free(cx->key);
        if (cx->cert_der) OPENSSL_free(cx->cert_der);
        free(cx);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    adapter->context = cx;
    adapter->sha256 = h_sha256;
    adapter->hmac_sha256 = h_hmac_sha256;
    adapter->aes256_cbc_encrypt = h_aes_encrypt;
    adapter->aes256_cbc_decrypt = h_aes_decrypt;
    adapter->rsa_sha256_sign = h_rsa_sign;
    adapter->rsa_sha256_verify = h_rsa_verify;
    adapter->rsa_oaep_decrypt = h_rsa_oaep_decrypt;
    adapter->rsa_oaep_encrypt = h_rsa_oaep_encrypt;
    adapter->get_own_certificate = h_get_own_certificate;
    adapter->get_certificate_key_bits = h_get_certificate_key_bits;
    return MU_STATUS_GOOD;
}

void mu_host_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter) {
    if (!adapter || !adapter->context) return;
    struct host_crypto_context *cx = (struct host_crypto_context *)adapter->context;
    if (cx->key) EVP_PKEY_free(cx->key);
    if (cx->cert_der) OPENSSL_free(cx->cert_der);
    free(cx);
    adapter->context = NULL;
}
