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
 *
 * The current implementation uses the OpenSSL EVP_DigestVerify* API when
 * MUC_OPCUA_HAVE_OPENSSL is defined. Other backends (mbedTLS, wolfSSL) can be
 * added behind the same interface without changes to jwt.c.
 */
#include "crypto_jwt.h"

#if defined(MUC_OPCUA_HAVE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/x509.h>

#include <stdlib.h>
#include <string.h>

/* Map a JWS algorithm to its digest. Returns NULL for unsupported algs. */
static const EVP_MD *alg_to_md(mu_jwt_alg_t alg) {
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

/* Determine whether an algorithm selects RSA vs ECDSA. */
static int alg_is_rsa(mu_jwt_alg_t alg) {
    return alg == MU_JWT_ALG_RS256 || alg == MU_JWT_ALG_RS384 || alg == MU_JWT_ALG_RS512;
}

static int alg_is_ecdsa(mu_jwt_alg_t alg) {
    return alg == MU_JWT_ALG_ES256 || alg == MU_JWT_ALG_ES384 || alg == MU_JWT_ALG_ES512;
}

/* Convert a fixed-length r||s ECDSA signature (RFC 7518 §3.4) to DER-encoded
   ECDSA-Sig-Value (SEQUENCE OF two INTEGERs) for OpenSSL's EVP_DigestVerify.
   Returns the DER length written into out (caller-provided buffer), or -1 on
   malformed input. The out buffer must be at least signature_len + 10 bytes. */
static int raw_ecdsa_to_der(const unsigned char *rs, size_t rs_len, unsigned char *out, size_t out_max) {
    size_t half = rs_len / 2;
    if (half == 0 || rs_len != half * 2) {
        return -1;
    }
    const unsigned char *r = rs;
    const unsigned char *s = rs + half;

    /* DER-encode r and s as INTEGERs, stripping leading zeros but adding a
       leading 0x00 if the high bit would make them negative. */
    unsigned char r_der[260];
    unsigned char s_der[260];
    size_t r_der_len = 0;
    size_t s_der_len = 0;

    /* Build INTEGER contents for r */
    size_t r_start = 0;
    while (r_start < half - 1 && r[r_start] == 0) {
        r_start++;
    }
    int r_pad = (r[r_start] & 0x80) ? 1 : 0;
    r_der[0] = 0x02; /* INTEGER tag */
    size_t r_content_len = half - r_start + (size_t)r_pad;
    if (r_content_len < 128) {
        r_der[1] = (unsigned char)r_content_len;
        r_der_len = 2;
    } else if (r_content_len < 256) {
        r_der[1] = 0x81;
        r_der[2] = (unsigned char)r_content_len;
        r_der_len = 3;
    } else {
        return -1;
    }
    if (r_pad) {
        r_der[r_der_len++] = 0x00;
    }
    memcpy(r_der + r_der_len, r + r_start, half - r_start);
    r_der_len += half - r_start;

    /* Build INTEGER contents for s */
    size_t s_start = 0;
    while (s_start < half - 1 && s[s_start] == 0) {
        s_start++;
    }
    int s_pad = (s[s_start] & 0x80) ? 1 : 0;
    s_der[0] = 0x02;
    size_t s_content_len = half - s_start + (size_t)s_pad;
    if (s_content_len < 128) {
        s_der[1] = (unsigned char)s_content_len;
        s_der_len = 2;
    } else if (s_content_len < 256) {
        s_der[1] = 0x81;
        s_der[2] = (unsigned char)s_content_len;
        s_der_len = 3;
    } else {
        return -1;
    }
    if (s_pad) {
        s_der[s_der_len++] = 0x00;
    }
    memcpy(s_der + s_der_len, s + s_start, half - s_start);
    s_der_len += half - s_start;

    /* SEQUENCE wrap: 0x30 <len> r_der s_der */
    size_t seq_content_len = r_der_len + s_der_len;
    size_t header_len;
    if (seq_content_len < 128) {
        if (out_max < 2 + seq_content_len) {
            return -1;
        }
        out[0] = 0x30;
        out[1] = (unsigned char)seq_content_len;
        header_len = 2;
    } else if (seq_content_len < 256) {
        if (out_max < 3 + seq_content_len) {
            return -1;
        }
        out[0] = 0x30;
        out[1] = 0x81;
        out[2] = (unsigned char)seq_content_len;
        header_len = 3;
    } else {
        return -1;
    }
    memcpy(out + header_len, r_der, r_der_len);
    memcpy(out + header_len + r_der_len, s_der, s_der_len);
    return (int)(header_len + seq_content_len);
}

/* Verify the signature using EVP_DigestVerify*. The DER public key may encode
   either an RSA or an EC key. The algorithm selects the digest and signature
   form (PKCS#1 v1.5 for RSA, raw r||s for ECDSA). */
static opcua_boolean_t verify_with_key(EVP_PKEY *pk, const EVP_MD *md, const unsigned char *signing_input,
                                       size_t signing_input_len, const unsigned char *signature,
                                       size_t signature_len, mu_jwt_alg_t alg) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) {
        return 0;
    }
    opcua_boolean_t ok = 0;
    if (EVP_DigestVerifyInit(mdctx, NULL, md, NULL, pk) != 1) {
        EVP_MD_CTX_free(mdctx);
        return 0;
    }

    if (alg_is_rsa(alg)) {
        if (EVP_DigestVerify(mdctx, signature, signature_len, signing_input, signing_input_len) == 1) {
            ok = 1;
        }
    } else if (alg_is_ecdsa(alg)) {
        /* Convert r||s to DER for OpenSSL. signature_len is fixed by the curve:
           ES256 -> 64, ES384 -> 96, ES512 -> 132. The DER buffer is at most
           signature_len + ~10 bytes of overhead. */
        unsigned char der_buf[280];
        int der_len = raw_ecdsa_to_der(signature, signature_len, der_buf, sizeof(der_buf));
        if (der_len > 0) {
            if (EVP_DigestVerify(mdctx, der_buf, (size_t)der_len, signing_input, signing_input_len) == 1) {
                ok = 1;
            }
        }
    }

    EVP_MD_CTX_free(mdctx);
    return ok;
}

opcua_boolean_t mu_crypto_jwt_verify(const opcua_byte_t *signing_input, size_t signing_input_len,
                                     const opcua_byte_t *signature, size_t signature_len,
                                     const opcua_byte_t *public_key_der, size_t public_key_len, mu_jwt_alg_t alg) {
    if (signing_input == NULL || signature == NULL || public_key_der == NULL || signing_input_len == 0 ||
        signature_len == 0 || public_key_len == 0) {
        return 0;
    }

#if !defined(MUC_OPCUA_CU_SECURITY_ECC)
    /* ECDSA backends are gated on CU_SECURITY_ECC. Without it, ES* algorithms
       are not available even if the JWT layer requests them. */
    if (alg_is_ecdsa(alg)) {
        return 0;
    }
#endif

    const EVP_MD *md = alg_to_md(alg);
    if (md == NULL) {
        return 0;
    }

    /* DER SubjectPublicKeyInfo via d2i_PUBKEY (RFC 5280). */
    const unsigned char *p = public_key_der;
    EVP_PKEY *pk = d2i_PUBKEY(NULL, &p, (long)public_key_len);
    if (pk == NULL) {
        return 0;
    }

    opcua_boolean_t ok = verify_with_key(pk, md, signing_input, signing_input_len, signature, signature_len, alg);
    EVP_PKEY_free(pk);
    return ok;
}

#else  /* no OpenSSL backend */

/* Stub backend: signature verification always fails. The JWT validator still
   compiles so the rest of the stack (Kconfig gating, address-space nodes,
   token-type dispatch) remains testable on bare-metal targets. */
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

#endif /* MUC_OPCUA_HAVE_OPENSSL */
