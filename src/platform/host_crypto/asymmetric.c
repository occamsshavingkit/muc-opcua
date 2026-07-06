/* src/platform/host_crypto/asymmetric.c
 * RSA-OAEP encrypt/decrypt, RSA key management, and certificate utilities. */
#include "common.h"

static opcua_statuscode_t rsa_oaep_impl(EVP_PKEY *pk, int encrypt, const opcua_byte_t *in, size_t in_len,
                                        opcua_byte_t *out, size_t *out_len, const EVP_MD *md) {
    EVP_PKEY_CTX *pc = EVP_PKEY_CTX_new(pk, NULL);
    if (!pc) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    size_t ol = *out_len;
    int init_ok = encrypt ? (EVP_PKEY_encrypt_init(pc) == 1) : (EVP_PKEY_decrypt_init(pc) == 1);
    if (init_ok && EVP_PKEY_CTX_set_rsa_padding(pc, RSA_PKCS1_OAEP_PADDING) == 1 &&
        EVP_PKEY_CTX_set_rsa_oaep_md(pc, md) == 1 && EVP_PKEY_CTX_set_rsa_mgf1_md(pc, md) == 1) {
        int op = encrypt ? EVP_PKEY_encrypt(pc, out, &ol, in, in_len) : EVP_PKEY_decrypt(pc, out, &ol, in, in_len);
        if (op == 1) {
            *out_len = ol;
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_PKEY_CTX_free(pc);
    return rc;
}

EVP_PKEY *pubkey_from_cert(const opcua_byte_t *cert, size_t cert_len) {
    const unsigned char *p = cert;
    X509 *x = d2i_X509(NULL, &p, (long)cert_len);
    if (!x) {
        return NULL;
    }
    EVP_PKEY *pk = X509_get_pubkey(x);
    X509_free(x);
    return pk;
}

opcua_statuscode_t h_rsa_oaep_decrypt(void *c, const opcua_byte_t *in, size_t len, opcua_byte_t *out, size_t *out_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    return rsa_oaep_impl(cx->key, 0, in, len, out, out_len, EVP_sha1());
}

opcua_statuscode_t h_rsa_oaep_encrypt(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *in,
                                      size_t len, opcua_byte_t *out, size_t *out_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = rsa_oaep_impl(pk, 1, in, len, out, out_len, EVP_sha1());
    EVP_PKEY_free(pk);
    return rc;
}

opcua_statuscode_t h_rsa_oaep_sha256_decrypt(void *c, const opcua_byte_t *in, size_t len, opcua_byte_t *out,
                                             size_t *out_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    return rsa_oaep_impl(cx->key, 0, in, len, out, out_len, EVP_sha256());
}

opcua_statuscode_t h_rsa_oaep_sha256_encrypt(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *in,
                                             size_t len, opcua_byte_t *out, size_t *out_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = rsa_oaep_impl(pk, 1, in, len, out, out_len, EVP_sha256());
    EVP_PKEY_free(pk);
    return rc;
}

opcua_statuscode_t h_get_own_certificate(void *c, const opcua_byte_t **cert, size_t *len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    *cert = cx->cert_der;
    *len = (size_t)cx->cert_der_len;
    return MU_STATUS_GOOD;
}

opcua_statuscode_t h_get_certificate_key_bits(void *c, const opcua_byte_t *cert, size_t cert_len, size_t *bits) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    *bits = (size_t)EVP_PKEY_get_bits(pk);
    EVP_PKEY_free(pk);
    return MU_STATUS_GOOD;
}

opcua_statuscode_t h_verify_certificate_validity(void *c, const opcua_byte_t *cert, size_t cert_len) {
    (void)c;
    const unsigned char *p = cert;
    X509 *x = d2i_X509(NULL, &p, (long)cert_len);
    if (!x) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    int not_before = X509_cmp_time(X509_get0_notBefore(x), NULL);
    int not_after = X509_cmp_time(X509_get0_notAfter(x), NULL);
    X509_free(x);
    if (not_before == 0 || not_after == 0) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    if (not_before > 0 || not_after < 0) {
        return MU_STATUS_BAD_CERTIFICATETIMEINVALID;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t h_get_certificate_thumbprint(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                opcua_byte_t *thumbprint) {
    (void)c;
    unsigned int len = 0;
    if (EVP_Digest(cert, cert_len, thumbprint, &len, EVP_sha1(), NULL) != 1) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t h_verify_certificate_application_uri(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                        const char *application_uri, size_t application_uri_len) {
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
