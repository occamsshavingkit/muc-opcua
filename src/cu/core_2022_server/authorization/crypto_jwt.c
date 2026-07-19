/* src/cu/core_2022_server/authorization/crypto_jwt.c
 *
 * Crypto backend wrapper for the JWT/JWS validator (spec 093). Wraps the
 * platform crypto adapter's RSA / ECDSA verify primitives so the JWT layer
 * has no direct dependency on OpenSSL / mbedTLS / wolfSSL.
 *
 * Spec grounding:
 *   RFC 7515 §5 -- JWS signature input is header.payload
 *   RFC 7518 §3.1 -- alg -> hash mapping (RS256/384/512, ES256/384/512)
 *   RFC 7518 §3.4 -- JWS ECDSA signatures are fixed-length r||s, NOT DER
 */
#include "crypto_jwt.h"

#include <string.h>

/* ---- Shared helpers (all backends) ---- */

static int alg_is_rsa(mu_jwt_alg_t alg) {
    return alg == MU_JWT_ALG_RS256 || alg == MU_JWT_ALG_RS384 || alg == MU_JWT_ALG_RS512;
}

static int alg_is_ecdsa(mu_jwt_alg_t alg) {
    return alg == MU_JWT_ALG_ES256 || alg == MU_JWT_ALG_ES384 || alg == MU_JWT_ALG_ES512;
}

/* DER-encode a raw r||s ECDSA signature (RFC 7518 §3.4) into the SEQUENCE
   OF two INTEGERs format expected by OpenSSL/mbedTLS/wolfSSL. Returns DER
   length or -1 on malformed input. */
static int raw_ecdsa_to_der(const unsigned char *rs, size_t rs_len, unsigned char *out, size_t out_max) {
    size_t half = rs_len / 2;
    if (half == 0 || rs_len != half * 2) {
        return -1;
    }

    unsigned char int_buf[270];
    size_t int_len = 0;

    for (int which = 0; which < 2; which++) {
        const unsigned char *val = (which == 0) ? rs : rs + half;
        size_t start = 0;
        while (start < half - 1 && val[start] == 0) {
            start++;
        }
        int pad = (val[start] & 0x80) ? 1 : 0;
        size_t content = half - start + (size_t)pad;

        if (int_len + 2 + content > sizeof(int_buf)) {
            return -1;
        }
        int_buf[int_len++] = 0x02; /* INTEGER tag */
        if (content < 128) {
            int_buf[int_len++] = (unsigned char)content;
        } else if (content < 256) {
            int_buf[int_len++] = 0x81;
            int_buf[int_len++] = (unsigned char)content;
        } else {
            return -1;
        }
        if (pad) {
            int_buf[int_len++] = 0x00;
        }
        memcpy(int_buf + int_len, val + start, half - start);
        int_len += half - start;
    }

    size_t hdr;
    if (int_len < 128) {
        if (out_max < 2 + int_len) {
            return -1;
        }
        out[0] = 0x30;
        out[1] = (unsigned char)int_len;
        hdr = 2;
    } else if (int_len < 256) {
        if (out_max < 3 + int_len) {
            return -1;
        }
        out[0] = 0x30;
        out[1] = 0x81;
        out[2] = (unsigned char)int_len;
        hdr = 3;
    } else {
        return -1;
    }
    memcpy(out + hdr, int_buf, int_len);
    return (int)(hdr + int_len);
}

/* ---- OpenSSL backend ---- */
#if defined(MUC_OPCUA_HAVE_OPENSSL)

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <string.h>

static const EVP_MD *alg_to_md_openssl(mu_jwt_alg_t alg) {
    switch (alg) {
    case MU_JWT_ALG_RS256:
    case MU_JWT_ALG_ES256:
        return EVP_sha256();
    case MU_JWT_ALG_RS384:
    case MU_JWT_ALG_ES384:
        return EVP_sha384();
    case MU_JWT_ALG_RS512:
    case MU_JWT_ALG_ES512:
        return EVP_sha512();
    default:
        return NULL;
    }
}

opcua_boolean_t mu_crypto_jwt_verify(const opcua_byte_t *signing_input, size_t signing_input_len,
                                     const opcua_byte_t *signature, size_t signature_len,
                                     const opcua_byte_t *public_key_der, size_t public_key_len, mu_jwt_alg_t alg) {
    if (signing_input == NULL || signature == NULL || public_key_der == NULL || signing_input_len == 0 ||
        signature_len == 0 || public_key_len == 0) {
        return 0;
    }
#if !defined(MUC_OPCUA_CU_SECURITY_ECC)
    if (alg_is_ecdsa(alg)) {
        return 0;
    }
#endif
    const EVP_MD *md = alg_to_md_openssl(alg);
    if (md == NULL) {
        return 0;
    }
    const unsigned char *p = public_key_der;
    EVP_PKEY *pk = d2i_PUBKEY(NULL, &p, (long)public_key_len);
    if (pk == NULL) {
        return 0;
    }
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    opcua_boolean_t ok = 0;
    if (mdctx && EVP_DigestVerifyInit(mdctx, NULL, md, NULL, pk) == 1) {
        if (alg_is_rsa(alg)) {
            ok = (EVP_DigestVerify(mdctx, signature, signature_len, signing_input, signing_input_len) == 1);
        } else if (alg_is_ecdsa(alg)) {
            unsigned char der_buf[280];
            int der_len = raw_ecdsa_to_der(signature, signature_len, der_buf, sizeof(der_buf));
            if (der_len > 0) {
                ok = (EVP_DigestVerify(mdctx, der_buf, (size_t)der_len, signing_input, signing_input_len) == 1);
            }
        }
    }
    if (mdctx) {
        EVP_MD_CTX_free(mdctx);
    }
    EVP_PKEY_free(pk);
    return ok;
}

/* ---- mbedTLS backend ---- */
#elif defined(MUC_OPCUA_HAVE_MBEDTLS)

#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <string.h>

static mbedtls_md_type_t alg_to_md_mbedtls(mu_jwt_alg_t alg) {
    switch (alg) {
    case MU_JWT_ALG_RS256:
    case MU_JWT_ALG_ES256:
        return MBEDTLS_MD_SHA256;
    case MU_JWT_ALG_RS384:
    case MU_JWT_ALG_ES384:
        return MBEDTLS_MD_SHA384;
    case MU_JWT_ALG_RS512:
    case MU_JWT_ALG_ES512:
        return MBEDTLS_MD_SHA512;
    default:
        return MBEDTLS_MD_NONE;
    }
}

static int compute_hash_mbedtls(mbedtls_md_type_t md_type, const unsigned char *data, size_t data_len,
                                unsigned char *hash_out) {
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(md_type);
    if (info == NULL) {
        return -1;
    }
    return mbedtls_md(info, data, data_len, hash_out);
}

opcua_boolean_t mu_crypto_jwt_verify(const opcua_byte_t *signing_input, size_t signing_input_len,
                                     const opcua_byte_t *signature, size_t signature_len,
                                     const opcua_byte_t *public_key_der, size_t public_key_len, mu_jwt_alg_t alg) {
    if (signing_input == NULL || signature == NULL || public_key_der == NULL || signing_input_len == 0 ||
        signature_len == 0 || public_key_len == 0) {
        return 0;
    }
#if !defined(MUC_OPCUA_CU_SECURITY_ECC)
    if (alg_is_ecdsa(alg)) {
        return 0;
    }
#endif
    mbedtls_md_type_t md_type = alg_to_md_mbedtls(alg);
    if (md_type == MBEDTLS_MD_NONE) {
        return 0;
    }

    /* Parse DER SubjectPublicKeyInfo. */
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ret = mbedtls_pk_parse_public_key(&pk, public_key_der, public_key_len);
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return 0;
    }

    /* Hash the signing input. */
    unsigned char hash[64];
    size_t hash_len = (md_type == MBEDTLS_MD_SHA256) ? 32 : (md_type == MBEDTLS_MD_SHA384) ? 48 : 64;
    if (compute_hash_mbedtls(md_type, signing_input, signing_input_len, hash) != 0) {
        mbedtls_pk_free(&pk);
        return 0;
    }

    opcua_boolean_t ok = 0;
    if (alg_is_rsa(alg)) {
        /* PKCS#1 v1.5: signature is already DER-encoded, mbedtls_pk_verify handles it. */
        ret = mbedtls_pk_verify(&pk, md_type, hash, hash_len, signature, signature_len);
        ok = (ret == 0);
    }
#if defined(MUC_OPCUA_CU_SECURITY_ECC)
    else if (alg_is_ecdsa(alg)) {
        /* Convert raw r||s to DER for mbedTLS. */
        unsigned char der_buf[280];
        int der_len = raw_ecdsa_to_der(signature, signature_len, der_buf, sizeof(der_buf));
        if (der_len > 0) {
            ret = mbedtls_pk_verify(&pk, md_type, hash, hash_len, der_buf, (size_t)der_len);
            ok = (ret == 0);
        }
    }
#endif

    mbedtls_pk_free(&pk);
    return ok;
}

/* ---- wolfSSL backend ---- */
#elif defined(MUC_OPCUA_HAVE_WOLFSSL)

#include <string.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/signature.h>

static int wc_hash_type(mu_jwt_alg_t alg, enum wc_HashType *out) {
    switch (alg) {
    case MU_JWT_ALG_RS256:
    case MU_JWT_ALG_ES256:
        *out = WC_HASH_TYPE_SHA256;
        return 0;
    case MU_JWT_ALG_RS384:
    case MU_JWT_ALG_ES384:
        *out = WC_HASH_TYPE_SHA384;
        return 0;
    case MU_JWT_ALG_RS512:
    case MU_JWT_ALG_ES512:
        *out = WC_HASH_TYPE_SHA512;
        return 0;
    default:
        return -1;
    }
}

opcua_boolean_t mu_crypto_jwt_verify(const opcua_byte_t *signing_input, size_t signing_input_len,
                                     const opcua_byte_t *signature, size_t signature_len,
                                     const opcua_byte_t *public_key_der, size_t public_key_len, mu_jwt_alg_t alg) {
    if (signing_input == NULL || signature == NULL || public_key_der == NULL || signing_input_len == 0 ||
        signature_len == 0 || public_key_len == 0) {
        return 0;
    }
#if !defined(MUC_OPCUA_CU_SECURITY_ECC)
    if (alg_is_ecdsa(alg)) {
        return 0;
    }
#endif
    enum wc_HashType ht;
    if (wc_hash_type(alg, &ht) != 0) {
        return 0;
    }

    int hash_len = wc_HashGetDigestSize(ht);
    if (hash_len <= 0) {
        return 0;
    }
    unsigned char hash[64];
    if (wc_Hash(ht, signing_input, signing_input_len, hash, (word32)hash_len) != 0) {
        return 0;
    }

    if (alg_is_rsa(alg)) {
        /* Parse DER-encoded RSA public key (SubjectPublicKeyInfo). wolfSSL's
           wc_RsaPublicKeyDecode expects raw PKCS#1 RSA key, not SPKI. We try
           wc_SignatureVerify first which handles SPKI directly. */
        return (wc_SignatureVerify(ht, WC_SIGNATURE_TYPE_RSA_PKCS1, hash, (word32)hash_len, (byte *)signature,
                                   (word32)signature_len, (byte *)public_key_der, (word32)public_key_len,
                                   (enum wc_SignatureType)0) == 0);
    }
#if defined(MUC_OPCUA_CU_SECURITY_ECC)
    else if (alg_is_ecdsa(alg)) {
        return (wc_SignatureVerify(ht, WC_SIGNATURE_TYPE_ECC, hash, (word32)hash_len, (byte *)signature,
                                   (word32)signature_len, (byte *)public_key_der, (word32)public_key_len,
                                   (enum wc_SignatureType)0) == 0);
    }
#endif

    return 0;
}

#else /* no crypto backend */

opcua_boolean_t mu_crypto_jwt_verify(const opcua_byte_t *signing_input, size_t signing_input_len,
                                     const opcua_byte_t *signature, size_t signature_len,
                                     const opcua_byte_t *public_key_der, size_t public_key_len, mu_jwt_alg_t alg) {
    (void)signing_input;
    (void)signing_input_len;
    (void)signature;
    (void)signature_len;
    (void)public_key_der;
    (void)public_key_len;
    (void)alg;
    return 0;
}

#endif
