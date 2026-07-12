/* src/platform/host_crypto/ecc.c
 * OpenSSL backend for the ECC SecurityPolicies (spec 059): X25519/P-256 ephemeral
 * keys + ECDH, Ed25519/ECDSA-SHA256 sign/verify, and ChaCha20-Poly1305 AEAD.
 * OPC UA wire encodings: curve25519 public keys are 32-byte little-endian (RFC
 * 7748, exactly OpenSSL's raw form); nistP256 public keys are 64-byte big-endian
 * x||y (OpenSSL's uncompressed point minus its 0x04 prefix); both policies use
 * 64-byte signatures (Ed25519 native; ECDSA as raw r||s, converted from DER). */
#include "common.h"

#ifdef MUC_OPCUA_CU_SECURITY_ECC

#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/param_build.h>

#define P256_UNCOMPRESSED_LEN 65 /* 0x04 || X(32) || Y(32) */
#define P256_COORD_LEN 32

/* --- ephemeral keys + ECDH --- */

opcua_statuscode_t h_ecc_generate_ephemeral(void *c, mu_ecc_curve_t curve, opcua_byte_t *pubkey, size_t *pubkey_len,
                                            opcua_byte_t *keypair_ctx) {
    (void)c;
    if (!pubkey || !pubkey_len || !keypair_ctx) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    EVP_PKEY *pk = NULL;
    if (curve == MU_ECC_CURVE_25519) {
        pk = EVP_PKEY_Q_keygen(NULL, NULL, "X25519");
        if (!pk) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        size_t raw = *pubkey_len;
        if (EVP_PKEY_get_raw_public_key(pk, pubkey, &raw) != 1) {
            EVP_PKEY_free(pk);
            return MU_STATUS_BAD_INTERNALERROR;
        }
        *pubkey_len = raw;
    } else {
        pk = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "P-256");
        if (!pk) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        opcua_byte_t enc[P256_UNCOMPRESSED_LEN];
        size_t enc_len = 0;
        if (EVP_PKEY_get_octet_string_param(pk, OSSL_PKEY_PARAM_PUB_KEY, enc, sizeof(enc), &enc_len) != 1 ||
            enc_len != P256_UNCOMPRESSED_LEN || enc[0] != 0x04 || *pubkey_len < (size_t)(P256_UNCOMPRESSED_LEN - 1)) {
            EVP_PKEY_free(pk);
            return MU_STATUS_BAD_INTERNALERROR;
        }
        (void)memcpy(pubkey, enc + 1, P256_UNCOMPRESSED_LEN - 1); /* strip 0x04 prefix */
        *pubkey_len = P256_UNCOMPRESSED_LEN - 1;
    }
    _Static_assert(sizeof(EVP_PKEY *) <= MU_ECC_KEYPAIR_CTX_SIZE,
                   "MU_ECC_KEYPAIR_CTX_SIZE must fit an EVP_PKEY handle");
    (void)memcpy(keypair_ctx, &pk, sizeof(pk));
    return MU_STATUS_GOOD;
}

/* Rebuild a peer EVP_PKEY from its OPC UA-encoded public key. */
static EVP_PKEY *peer_pubkey(mu_ecc_curve_t curve, const opcua_byte_t *peer, size_t peer_len) {
    if (curve == MU_ECC_CURVE_25519) {
        return EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, peer, peer_len);
    }
    if (peer_len != (size_t)(P256_UNCOMPRESSED_LEN - 1)) {
        return NULL;
    }
    opcua_byte_t enc[P256_UNCOMPRESSED_LEN];
    enc[0] = 0x04;
    (void)memcpy(enc + 1, peer, P256_UNCOMPRESSED_LEN - 1);

    EVP_PKEY *out = NULL;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    OSSL_PARAM params[] = {OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, (char *)"prime256v1", 0),
                           OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, enc, sizeof(enc)),
                           OSSL_PARAM_construct_end()};
    if (ctx && EVP_PKEY_fromdata_init(ctx) == 1) {
        (void)EVP_PKEY_fromdata(ctx, &out, EVP_PKEY_PUBLIC_KEY, params);
    }
    EVP_PKEY_CTX_free(ctx);
    return out;
}

opcua_statuscode_t h_ecc_ecdh_derive(void *c, mu_ecc_curve_t curve, const opcua_byte_t *keypair_ctx,
                                     const opcua_byte_t *peer, size_t peer_len, opcua_byte_t *shared,
                                     size_t *shared_len) {
    (void)c;
    if (!keypair_ctx || !peer || !shared || !shared_len) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    EVP_PKEY *local = NULL;
    (void)memcpy(&local, keypair_ctx, sizeof(local));
    EVP_PKEY *pub = peer_pubkey(curve, peer, peer_len);
    if (!local || !pub) {
        EVP_PKEY_free(pub);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(local, NULL);
    size_t out = *shared_len;
    if (ctx && EVP_PKEY_derive_init(ctx) == 1 && EVP_PKEY_derive_set_peer(ctx, pub) == 1 &&
        EVP_PKEY_derive(ctx, shared, &out) == 1) {
        *shared_len = out;
        rc = MU_STATUS_GOOD;
    }
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pub);
    return rc;
}

void h_ecc_keypair_free(void *c, opcua_byte_t *keypair_ctx) {
    (void)c;
    if (!keypair_ctx) {
        return;
    }
    EVP_PKEY *pk = NULL;
    (void)memcpy(&pk, keypair_ctx, sizeof(pk));
    if (pk) {
        EVP_PKEY_free(pk);
    }
    (void)memset(keypair_ctx, 0, sizeof(pk));
}

/* --- signatures --- */

/* Convert an OpenSSL DER ECDSA signature to OPC UA's fixed 64-byte r||s. */
static int ecdsa_der_to_raw(const unsigned char *der, size_t der_len, opcua_byte_t *raw) {
    const unsigned char *p = der;
    ECDSA_SIG *sig = d2i_ECDSA_SIG(NULL, &p, (long)der_len);
    if (!sig) {
        return 0;
    }
    const BIGNUM *r = NULL, *s = NULL;
    ECDSA_SIG_get0(sig, &r, &s);
    int ok = (BN_bn2binpad(r, raw, P256_COORD_LEN) == P256_COORD_LEN &&
              BN_bn2binpad(s, raw + P256_COORD_LEN, P256_COORD_LEN) == P256_COORD_LEN);
    ECDSA_SIG_free(sig);
    return ok;
}

/* Convert OPC UA's 64-byte r||s to a DER ECDSA signature. `der` must hold >=72 B. */
static int ecdsa_raw_to_der(const opcua_byte_t *raw, size_t raw_len, unsigned char *der, size_t *der_len) {
    if (raw_len != 2 * P256_COORD_LEN) {
        return 0;
    }
    ECDSA_SIG *sig = ECDSA_SIG_new();
    BIGNUM *r = BN_bin2bn(raw, P256_COORD_LEN, NULL);
    BIGNUM *s = BN_bin2bn(raw + P256_COORD_LEN, P256_COORD_LEN, NULL);
    int ok = 0;
    if (sig && r && s && ECDSA_SIG_set0(sig, r, s) == 1) { /* set0 takes ownership of r,s */
        unsigned char *p = der;
        int len = i2d_ECDSA_SIG(sig, &p);
        if (len > 0) {
            *der_len = (size_t)len;
            ok = 1;
        }
        r = NULL;
        s = NULL;
    }
    if (r) {
        BN_free(r);
    }
    if (s) {
        BN_free(s);
    }
    if (sig) {
        ECDSA_SIG_free(sig);
    }
    return ok;
}

opcua_statuscode_t h_ecc_sign(void *c, mu_ecc_curve_t curve, const opcua_byte_t *data, size_t len, opcua_byte_t *sig,
                              size_t *sig_len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    EVP_PKEY *key = (curve == MU_ECC_CURVE_25519) ? cx->ed25519_key : cx->p256_key;
    if (!key || !sig || !sig_len) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    if (!md) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    if (curve == MU_ECC_CURVE_25519) {
        size_t sl = *sig_len;
        if (EVP_DigestSignInit(md, NULL, NULL, NULL, key) == 1 && EVP_DigestSign(md, sig, &sl, data, len) == 1) {
            *sig_len = sl;
            rc = MU_STATUS_GOOD;
        }
    } else {
        unsigned char der[80];
        size_t der_len = sizeof(der);
        if (EVP_DigestSignInit(md, NULL, EVP_sha256(), NULL, key) == 1 &&
            EVP_DigestSign(md, der, &der_len, data, len) == 1 && ecdsa_der_to_raw(der, der_len, sig)) {
            *sig_len = 2 * P256_COORD_LEN;
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_MD_CTX_free(md);
    return rc;
}

opcua_statuscode_t h_ecc_verify(void *c, mu_ecc_curve_t curve, const opcua_byte_t *cert, size_t cert_len,
                                const opcua_byte_t *data, size_t data_len, const opcua_byte_t *sig, size_t sig_len) {
    (void)c;
    EVP_PKEY *pk = pubkey_from_cert(cert, cert_len);
    if (!pk) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;
    EVP_MD_CTX *md = EVP_MD_CTX_new();
    if (md) {
        if (curve == MU_ECC_CURVE_25519) {
            if (EVP_DigestVerifyInit(md, NULL, NULL, NULL, pk) == 1 &&
                EVP_DigestVerify(md, sig, sig_len, data, data_len) == 1) {
                rc = MU_STATUS_GOOD;
            }
        } else {
            unsigned char der[80];
            size_t der_len = sizeof(der);
            if (ecdsa_raw_to_der(sig, sig_len, der, &der_len) &&
                EVP_DigestVerifyInit(md, NULL, EVP_sha256(), NULL, pk) == 1 &&
                EVP_DigestVerify(md, der, der_len, data, data_len) == 1) {
                rc = MU_STATUS_GOOD;
            }
        }
    }
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(pk);
    return rc;
}

opcua_statuscode_t h_get_own_ecc_certificate(void *c, mu_ecc_curve_t curve, const opcua_byte_t **cert, size_t *len) {
    struct host_crypto_context *cx = (struct host_crypto_context *)c;
    if (!cert || !len) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    const unsigned char *der = (curve == MU_ECC_CURVE_25519) ? cx->ed25519_cert_der : cx->p256_cert_der;
    int der_len = (curve == MU_ECC_CURVE_25519) ? cx->ed25519_cert_der_len : cx->p256_cert_der_len;
    if (!der || der_len <= 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    *cert = der;
    *len = (size_t)der_len;
    return MU_STATUS_GOOD;
}

/* --- ChaCha20-Poly1305 AEAD (RFC 8439) --- */

opcua_statuscode_t h_chacha20_poly1305_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *nonce,
                                               const opcua_byte_t *aad, size_t aad_len, const opcua_byte_t *in,
                                               size_t len, opcua_byte_t *out, opcua_byte_t *tag) {
    (void)c;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    int outl = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, MU_CHACHA20_POLY1305_NONCE_LENGTH, NULL) == 1 &&
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1) {
        int ok = 1;
        if (aad && aad_len) {
            ok = (EVP_EncryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) == 1);
        }
        if (ok && EVP_EncryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
            EVP_EncryptFinal_ex(ctx, out + outl, &outl) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, MU_CHACHA20_POLY1305_TAG_LENGTH, tag) == 1) {
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

opcua_statuscode_t h_chacha20_poly1305_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *nonce,
                                               const opcua_byte_t *aad, size_t aad_len, const opcua_byte_t *in,
                                               size_t len, const opcua_byte_t *tag, opcua_byte_t *out) {
    (void)c;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;
    int outl = 0;
    if (EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, MU_CHACHA20_POLY1305_NONCE_LENGTH, NULL) == 1 &&
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) == 1) {
        int ok = 1;
        if (aad && aad_len) {
            ok = (EVP_DecryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) == 1);
        }
        /* Poly1305 tag is verified in DecryptFinal; a mismatch fails closed. */
        if (ok && EVP_DecryptUpdate(ctx, out, &outl, in, (int)len) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, MU_CHACHA20_POLY1305_TAG_LENGTH, (void *)tag) == 1 &&
            EVP_DecryptFinal_ex(ctx, out + outl, &outl) == 1) {
            rc = MU_STATUS_GOOD;
        }
    }
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

#endif /* MUC_OPCUA_CU_SECURITY_ECC */
