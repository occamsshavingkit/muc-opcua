/* src/platform/host_crypto_adapter.c
 * OpenSSL-backed crypto adapter for SecurityPolicy Basic256Sha256 (host/dev). */
#include "host_crypto_adapter.h"
#include "muc_opcua/config.h"
#include "muc_opcua/status.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(void *) <= MU_CIPHER_CTX_SIZE, "MU_CIPHER_CTX_SIZE must fit a host cipher context handle");

struct host_crypto_context {
    EVP_PKEY *key;           /* server RSA private key */
    unsigned char *cert_der; /* server certificate, DER-encoded */
    int cert_der_len;
};

struct host_cipher_context {
    EVP_CIPHER_CTX *encrypt;
    EVP_CIPHER_CTX *decrypt;
};

/* ---- digest / mac / symmetric ---- */

static opcua_statuscode_t h_sha256(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *digest) {
    (void)c;
    unsigned int dlen = 0;
    if (EVP_Digest(data, len, digest, &dlen, EVP_sha256(), NULL) != 1) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t h_hmac_sha256(void *c, const opcua_byte_t *key, size_t key_len, const opcua_byte_t *data,
                                        size_t data_len, opcua_byte_t *mac) {
    (void)c;
    unsigned int mlen = 0;
    if (HMAC(EVP_sha256(), key, (int)key_len, data, data_len, mac, &mlen) == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t aes_cbc(const EVP_CIPHER *cipher, const opcua_byte_t *key, const opcua_byte_t *iv,
                                  const opcua_byte_t *in, size_t len, opcua_byte_t *out, int encrypt) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    int outl = 0, finl = 0;
    int ok = encrypt
                 ? (EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                    EVP_EncryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                    EVP_EncryptFinal_ex(ctx, out + outl, &finl) == 1)
                 : (EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                    EVP_DecryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                    EVP_DecryptFinal_ex(ctx, out + outl, &finl) == 1);
    if (ok) {
        rc = MU_STATUS_GOOD;
    }
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

static opcua_statuscode_t h_aes_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_256_cbc(), key, iv, in, len, out, 1);
}
static opcua_statuscode_t h_aes_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_256_cbc(), key, iv, in, len, out, 0);
}
static opcua_statuscode_t h_aes128_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv,
                                           const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_128_cbc(), key, iv, in, len, out, 1);
}
static opcua_statuscode_t h_aes128_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv,
                                           const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c;
    return aes_cbc(EVP_aes_128_cbc(), key, iv, in, len, out, 0);
}

static void store_cipher_handle(opcua_byte_t *ctx_storage, struct host_cipher_context *handle) {
    (void)memcpy(ctx_storage, &handle, sizeof(handle));
}

static struct host_cipher_context *load_cipher_handle(const opcua_byte_t *ctx_storage) {
    struct host_cipher_context *handle = NULL;
    (void)memcpy(&handle, ctx_storage, sizeof(handle));
    return handle;
}

static void free_cipher_handle(struct host_cipher_context *handle) {
    if (!handle) {
        return;
    }
    EVP_CIPHER_CTX_free(handle->encrypt);
    EVP_CIPHER_CTX_free(handle->decrypt);
    free(handle);
}

static opcua_statuscode_t h_cipher_ctx_init(void *c, const opcua_byte_t *key, opcua_byte_t *ctx_storage) {
    (void)c;
    if (!key || !ctx_storage) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

#if MUC_OPCUA_ALLOW_HEAP
    struct host_cipher_context *handle = (struct host_cipher_context *)calloc(1, sizeof(*handle));
    if (!handle) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
#else
    return MU_STATUS_BAD_OUTOFMEMORY;
#endif

    handle->encrypt = EVP_CIPHER_CTX_new();
    handle->decrypt = EVP_CIPHER_CTX_new();
    if (!handle->encrypt || !handle->decrypt) {
        free_cipher_handle(handle);
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    int ok = EVP_EncryptInit_ex(handle->encrypt, EVP_aes_256_cbc(), NULL, key, NULL) == 1 &&
             EVP_CIPHER_CTX_set_padding(handle->encrypt, 0) == 1 &&
             EVP_DecryptInit_ex(handle->decrypt, EVP_aes_256_cbc(), NULL, key, NULL) == 1 &&
             EVP_CIPHER_CTX_set_padding(handle->decrypt, 0) == 1;
    if (!ok) {
        free_cipher_handle(handle);
        return MU_STATUS_BAD_INTERNALERROR;
    }

    store_cipher_handle(ctx_storage, handle);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t aes_cbc_ctx(EVP_CIPHER_CTX *ctx, const opcua_byte_t *iv, const opcua_byte_t *in, size_t len,
                                      opcua_byte_t *out, int encrypt) {
    if (!ctx || !iv || (!in && len) || !out) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    int outl = 0, finl = 0;
    int ok = encrypt ? (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                        EVP_EncryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                        EVP_EncryptFinal_ex(ctx, out + outl, &finl) == 1)
                     : (EVP_DecryptInit_ex(ctx, NULL, NULL, NULL, iv) == 1 && EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
                        EVP_DecryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
                        EVP_DecryptFinal_ex(ctx, out + outl, &finl) == 1);
    if (ok) {
        rc = MU_STATUS_GOOD;
    }
    return rc;
}

static opcua_statuscode_t h_aes_encrypt_ctx(void *c, opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
                                            const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c;
    if (!ctx_storage) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    struct host_cipher_context *handle = load_cipher_handle(ctx_storage);
    if (!handle) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return aes_cbc_ctx(handle->encrypt, iv, in, len, out, 1);
}

static opcua_statuscode_t h_aes_decrypt_ctx(void *c, opcua_byte_t *ctx_storage, const opcua_byte_t *iv,
                                            const opcua_byte_t *in, size_t len, opcua_byte_t *out) {
    (void)c;
    if (!ctx_storage) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    struct host_cipher_context *handle = load_cipher_handle(ctx_storage);
    if (!handle) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return aes_cbc_ctx(handle->decrypt, iv, in, len, out, 0);
}

static void h_cipher_ctx_free(void *c, opcua_byte_t *ctx_storage) {
    (void)c;
    if (!ctx_storage) {
        return;
    }
    struct host_cipher_context *handle = load_cipher_handle(ctx_storage);
    free_cipher_handle(handle);
    handle = NULL;
    store_cipher_handle(ctx_storage, handle);
}

/* ---- RSA ---- */

static opcua_statuscode_t h_rsa_sign(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *sig,
                                     size_t *sig_len) {
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

static EVP_PKEY *pubkey_from_cert(const opcua_byte_t *cert, size_t cert_len) {
    const unsigned char *p = cert;
    X509 *x = d2i_X509(NULL, &p, (long)cert_len);
    if (!x) {
        return NULL;
    }
    EVP_PKEY *pk = X509_get_pubkey(x);
    X509_free(x);
    return pk;
}

static opcua_statuscode_t h_rsa_verify(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *data,
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

static opcua_statuscode_t rsa_oaep(EVP_PKEY *pk, int encrypt, const opcua_byte_t *in, size_t in_len, opcua_byte_t *out,
                                   size_t *out_len) {
    EVP_PKEY_CTX *pc = EVP_PKEY_CTX_new(pk, NULL);
    if (!pc) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    size_t ol = *out_len;
    int init_ok = encrypt ? (EVP_PKEY_encrypt_init(pc) == 1) : (EVP_PKEY_decrypt_init(pc) == 1);
    if (init_ok && EVP_PKEY_CTX_set_rsa_padding(pc, RSA_PKCS1_OAEP_PADDING) == 1 &&
        EVP_PKEY_CTX_set_rsa_oaep_md(pc, EVP_sha1()) == 1 && EVP_PKEY_CTX_set_rsa_mgf1_md(pc, EVP_sha1()) == 1) {
        int op = encrypt ? EVP_PKEY_encrypt(pc, out, &ol, in, in_len) : EVP_PKEY_decrypt(pc, out, &ol, in, in_len);
        if (op == 1) {
            *out_len = ol;
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_PKEY_CTX_free(pc);
    return rc;
}

static opcua_statuscode_t h_rsa_oaep_decrypt(void *c, const opcua_byte_t *in, size_t len, opcua_byte_t *out,
                                             size_t *out_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    return rsa_oaep(cx->key, 0, in, len, out, out_len);
}

static opcua_statuscode_t h_rsa_oaep_encrypt(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *in,
                                             size_t len, opcua_byte_t *out, size_t *out_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = rsa_oaep(pk, 1, in, len, out, out_len);
    EVP_PKEY_free(pk);
    return rc;
}

static opcua_statuscode_t h_rsa_pss_sha256_sign(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *sig,
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

static opcua_statuscode_t h_rsa_pss_sha256_verify(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                  const opcua_byte_t *data, size_t data_len, const opcua_byte_t *sig,
                                                  size_t sig_len) {
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

static opcua_statuscode_t rsa_oaep_sha256(EVP_PKEY *pk, int encrypt, const opcua_byte_t *in, size_t in_len,
                                          opcua_byte_t *out, size_t *out_len) {
    EVP_PKEY_CTX *pc = EVP_PKEY_CTX_new(pk, NULL);
    if (!pc) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    size_t ol = *out_len;
    int init_ok = encrypt ? (EVP_PKEY_encrypt_init(pc) == 1) : (EVP_PKEY_decrypt_init(pc) == 1);
    if (init_ok && EVP_PKEY_CTX_set_rsa_padding(pc, RSA_PKCS1_OAEP_PADDING) == 1 &&
        EVP_PKEY_CTX_set_rsa_oaep_md(pc, EVP_sha256()) == 1 && EVP_PKEY_CTX_set_rsa_mgf1_md(pc, EVP_sha256()) == 1) {
        int op = encrypt ? EVP_PKEY_encrypt(pc, out, &ol, in, in_len) : EVP_PKEY_decrypt(pc, out, &ol, in, in_len);
        if (op == 1) {
            *out_len = ol;
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_PKEY_CTX_free(pc);
    return rc;
}

static opcua_statuscode_t h_rsa_oaep_sha256_decrypt(void *c, const opcua_byte_t *in, size_t len, opcua_byte_t *out,
                                                    size_t *out_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    return rsa_oaep_sha256(cx->key, 0, in, len, out, out_len);
}

static opcua_statuscode_t h_rsa_oaep_sha256_encrypt(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                    const opcua_byte_t *in, size_t len, opcua_byte_t *out,
                                                    size_t *out_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = rsa_oaep_sha256(pk, 1, in, len, out, out_len);
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
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    *bits = (size_t)EVP_PKEY_get_bits(pk);
    EVP_PKEY_free(pk);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t h_verify_certificate_validity(void *c, const opcua_byte_t *cert, size_t cert_len) {
    (void)c;
    const unsigned char *p = cert;
    X509 *x = d2i_X509(NULL, &p, (long)cert_len);
    if (!x) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    /* X509_cmp_time(t, NULL) compares against the current time: <0 if t is in the
       past, >0 if in the future. notBefore must be past, notAfter must be future. */
    int not_before = X509_cmp_time(X509_get0_notBefore(x), NULL);
    int not_after = X509_cmp_time(X509_get0_notAfter(x), NULL);
    X509_free(x);
    if (not_before == 0 || not_after == 0) {
        return MU_STATUS_BAD_CERTIFICATEINVALID; /* unparseable time field */
    }
    if (not_before > 0 || not_after < 0) {
        return MU_STATUS_BAD_CERTIFICATETIMEINVALID; /* not yet valid, or expired */
    }
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t h_get_certificate_thumbprint(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                       opcua_byte_t *thumbprint) {
    (void)c;
    unsigned int len = 0;
    if (EVP_Digest(cert, cert_len, thumbprint, &len, EVP_sha1(), NULL) != 1) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

/* OPC-10000-4 §5.7.2.1: compare clientDescription.ApplicationUri against the
   URIs in the certificate's SubjectAltName extension, falling back to the
   Subject CN when no SAN URI is present. Returns MU_STATUS_GOOD when any
   certificate identity matches `application_uri`, MU_STATUS_BAD_CERTIFICATEURIINVALID
   otherwise (or MU_STATUS_BAD_CERTIFICATEINVALID on a parse failure). */
static opcua_statuscode_t h_verify_certificate_application_uri(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                               const char *application_uri,
                                                               size_t application_uri_len) {
    (void)c;
    if (application_uri == NULL || application_uri_len == 0 || cert == NULL || cert_len == 0) {
        return MU_STATUS_BAD_CERTIFICATEURIINVALID;
    }
    const unsigned char *p = cert;
    X509 *x = d2i_X509(NULL, &p, (long)cert_len);
    if (x == NULL) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_CERTIFICATEURIINVALID;
    int matched = 0;

    /* Primary check: SubjectAltName URI entries (OPC-10000-4 §5.7.2.1). */
    STACK_OF(GENERAL_NAME) *san = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i(x, NID_subject_alt_name, NULL, NULL);
    int san_count = (san != NULL) ? sk_GENERAL_NAME_num(san) : 0;
    for (int i = 0; i < san_count; ++i) {
        GENERAL_NAME *gn = sk_GENERAL_NAME_value(san, i);
        if (gn == NULL) {
            continue;
        }
        if (gn->type == GEN_URI) {
            const ASN1_STRING *uri = gn->d.uniformResourceIdentifier;
            if (uri != NULL) {
                int uri_len = ASN1_STRING_length(uri);
                if (uri_len >= 0 && (size_t)uri_len == application_uri_len &&
                    memcmp(ASN1_STRING_get0_data(uri), application_uri, application_uri_len) == 0) {
                    matched = 1;
                    break;
                }
            }
        }
    }
    if (san != NULL) {
        sk_GENERAL_NAME_pop_free(san, GENERAL_NAME_free);
    }

    /* Fallback: Subject CN. RFC 6125 permits CN-based matching when no
       SAN URI is present; this preserves backward compatibility with
       self-signed test certificates that only set CN. */
    if (!matched && san_count == 0) {
        X509_NAME *name = X509_get_subject_name(x);
        if (name != NULL) {
            char cn_buf[256];
            int cn_len = X509_NAME_get_text_by_NID(name, NID_commonName, cn_buf, (int)sizeof(cn_buf));
            if (cn_len > 0 && (size_t)cn_len == application_uri_len &&
                memcmp(cn_buf, application_uri, application_uri_len) == 0) {
                matched = 1;
            }
        }
    }

    X509_free(x);
    if (matched) {
        rc = MU_STATUS_GOOD;
    }
    return rc;
}

/* ---- init / cleanup ---- */

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

#if MUC_OPCUA_ALLOW_HEAP
    struct host_crypto_context *cx = (struct host_crypto_context *)calloc(1, sizeof(*cx));
    if (!cx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
#else
    return MU_STATUS_BAD_OUTOFMEMORY;
#endif

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
