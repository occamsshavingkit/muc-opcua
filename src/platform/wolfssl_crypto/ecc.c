/* src/platform/wolfssl_crypto/ecc.c
 * wolfSSL backend for the ECC SecurityPolicies (spec 059): X25519/P-256 ephemeral
 * keys + ECDH, Ed25519/ECDSA-SHA256 sign/verify, and ChaCha20-Poly1305 AEAD.
 * Wire encodings match the OpenSSL host backend exactly (cross-validated by
 * tests/unit/test_wolfssl_adapter.c): curve25519 public keys are 32-byte
 * little-endian (RFC 7748); nistP256 public keys are 64-byte big-endian x||y
 * (wolfSSL's X9.63 uncompressed point minus its 0x04 prefix); signatures are
 * 64 bytes (Ed25519 native; ECDSA as raw r||s, each coordinate 32-byte
 * big-endian); the ECDH shared secret is the raw 32-byte output (P-256
 * x-coordinate big-endian; X25519 RFC-7748 little-endian). */
#ifdef MUC_OPCUA_HAVE_WOLFSSL
#include "common.h"

#ifdef MUC_OPCUA_CU_SECURITY_ECC
#include "../wolfssl_crypto_adapter.h"
#include <wolfssl/wolfcrypt/asn_public.h>

#define P256_COORD_LEN 32
#define P256_X963_LEN 65 /* 0x04 || X(32) || Y(32) */
#define C25519_LEN 32
#define ECC_SCALAR_LEN 32

/* --- identity provisioning --- */

opcua_statuscode_t mu_wolfssl_crypto_adapter_set_ecc_identity(mu_crypto_adapter_t *adapter, mu_ecc_curve_t curve,
                                                              const opcua_byte_t *cert_der, size_t cert_len,
                                                              const opcua_byte_t *key_der, size_t key_len) {
    if (!adapter || !adapter->context || !cert_der || !key_der) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)adapter->context;
    word32 idx = 0;

    if (curve == MU_ECC_CURVE_25519) {
        if (ctx->ed_key_set) {
            wc_ed25519_free(&ctx->ed_key);
            ctx->ed_key_set = 0;
        }
        if (wc_ed25519_init(&ctx->ed_key) != 0) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        if (wc_Ed25519PrivateKeyDecode(key_der, &idx, &ctx->ed_key, (word32)key_len) != 0) {
            wc_ed25519_free(&ctx->ed_key);
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        /* PKCS#8 Ed25519 keys carry only the 32-byte seed. Derive the public key
           and re-import the full keypair: wc_ed25519_make_public writes to the
           output buffer but does not populate the key's stored public key, which
           Ed25519 signing embeds. */
        if (!ctx->ed_key.pubKeySet) {
            opcua_byte_t seed[ED25519_KEY_SIZE];
            opcua_byte_t pub[ED25519_PUB_KEY_SIZE];
            word32 seed_len = sizeof(seed);
            if (wc_ed25519_export_private_only(&ctx->ed_key, seed, &seed_len) != 0 || seed_len != ED25519_KEY_SIZE ||
                wc_ed25519_make_public(&ctx->ed_key, pub, sizeof(pub)) != 0 ||
                wc_ed25519_import_private_key(seed, seed_len, pub, sizeof(pub), &ctx->ed_key) != 0) {
                wc_ed25519_free(&ctx->ed_key);
                return MU_STATUS_BAD_SECURITYCHECKSFAILED;
            }
        }
        ctx->ed_key_set = 1;
        ctx->ed_cert_der = cert_der;
        ctx->ed_cert_len = cert_len;
        return MU_STATUS_GOOD;
    }

    if (curve == MU_ECC_CURVE_NISTP256) {
        if (ctx->p256_key_set) {
            wc_ecc_free(&ctx->p256_key);
            ctx->p256_key_set = 0;
        }
        if (wc_ecc_init(&ctx->p256_key) != 0) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        if (wc_EccPrivateKeyDecode(key_der, &idx, &ctx->p256_key, (word32)key_len) != 0) {
            wc_ecc_free(&ctx->p256_key);
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        (void)wc_ecc_set_rng(&ctx->p256_key, &ctx->rng);
        ctx->p256_key_set = 1;
        ctx->p256_cert_der = cert_der;
        ctx->p256_cert_len = cert_len;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_INTERNALERROR;
}

/* --- ephemeral keys + ECDH --- */

opcua_statuscode_t w_ecc_generate_ephemeral(void *context, mu_ecc_curve_t curve, opcua_byte_t *pubkey,
                                            size_t *pubkey_length, opcua_byte_t *keypair_ctx) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    _Static_assert(MU_ECC_KEYPAIR_CTX_SIZE >= ECC_SCALAR_LEN, "MU_ECC_KEYPAIR_CTX_SIZE must hold a 32-byte scalar");
    if (!ctx || !pubkey || !pubkey_length || !keypair_ctx) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (curve == MU_ECC_CURVE_25519) {
        if (*pubkey_length < C25519_LEN) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        curve25519_key key;
        if (wc_curve25519_init(&key) != 0) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
        word32 pub_len = C25519_LEN;
        word32 priv_len = C25519_LEN;
        if (wc_curve25519_make_key(&ctx->rng, C25519_LEN, &key) == 0 &&
            wc_curve25519_export_public_ex(&key, pubkey, &pub_len, EC25519_LITTLE_ENDIAN) == 0 &&
            wc_curve25519_export_private_raw_ex(&key, keypair_ctx, &priv_len, EC25519_LITTLE_ENDIAN) == 0 &&
            pub_len == C25519_LEN && priv_len == C25519_LEN) {
            *pubkey_length = C25519_LEN;
            rc = MU_STATUS_GOOD;
        }
        wc_curve25519_free(&key);
        return rc;
    }

    if (curve == MU_ECC_CURVE_NISTP256) {
        if (*pubkey_length < (size_t)(P256_X963_LEN - 1)) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        ecc_key key;
        if (wc_ecc_init(&key) != 0) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
        opcua_byte_t x963[P256_X963_LEN];
        word32 x963_len = sizeof(x963);
        opcua_byte_t scalar[ECC_SCALAR_LEN + 1];
        word32 scalar_len = sizeof(scalar);
        if (wc_ecc_make_key_ex(&ctx->rng, P256_COORD_LEN, &key, ECC_SECP256R1) == 0 &&
            wc_ecc_export_x963(&key, x963, &x963_len) == 0 && x963_len == P256_X963_LEN && x963[0] == 0x04 &&
            wc_ecc_export_private_only(&key, scalar, &scalar_len) == 0 && scalar_len <= ECC_SCALAR_LEN) {
            (void)memcpy(pubkey, x963 + 1, P256_X963_LEN - 1); /* strip 0x04 prefix */
            *pubkey_length = P256_X963_LEN - 1;
            /* left-pad the scalar to a fixed 32-byte big-endian buffer */
            (void)memset(keypair_ctx, 0, ECC_SCALAR_LEN);
            (void)memcpy(keypair_ctx + (ECC_SCALAR_LEN - scalar_len), scalar, scalar_len);
            rc = MU_STATUS_GOOD;
        }
        wc_ecc_free(&key);
        return rc;
    }

    return MU_STATUS_BAD_INTERNALERROR;
}

opcua_statuscode_t w_ecc_ecdh_derive(void *context, mu_ecc_curve_t curve, const opcua_byte_t *keypair_ctx,
                                     const opcua_byte_t *peer_pubkey, size_t peer_pubkey_length, opcua_byte_t *shared,
                                     size_t *shared_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    if (!ctx || !keypair_ctx || !peer_pubkey || !shared || !shared_length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    if (curve == MU_ECC_CURVE_25519) {
        if (peer_pubkey_length != C25519_LEN || *shared_length < C25519_LEN) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        curve25519_key priv, peer;
        if (wc_curve25519_init(&priv) != 0) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        if (wc_curve25519_init(&peer) != 0) {
            wc_curve25519_free(&priv);
            return MU_STATUS_BAD_INTERNALERROR;
        }
        opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;
        word32 out_len = (word32)*shared_length;
        if (wc_curve25519_import_private_ex(keypair_ctx, C25519_LEN, &priv, EC25519_LITTLE_ENDIAN) == 0 &&
            wc_curve25519_import_public_ex(peer_pubkey, C25519_LEN, &peer, EC25519_LITTLE_ENDIAN) == 0 &&
            wc_curve25519_shared_secret_ex(&priv, &peer, shared, &out_len, EC25519_LITTLE_ENDIAN) == 0) {
            *shared_length = out_len;
            rc = MU_STATUS_GOOD;
        }
        wc_curve25519_free(&peer);
        wc_curve25519_free(&priv);
        return rc;
    }

    if (curve == MU_ECC_CURVE_NISTP256) {
        if (peer_pubkey_length != (size_t)(P256_X963_LEN - 1) || *shared_length < P256_COORD_LEN) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        ecc_key priv, peer;
        if (wc_ecc_init(&priv) != 0) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        if (wc_ecc_init(&peer) != 0) {
            wc_ecc_free(&priv);
            return MU_STATUS_BAD_INTERNALERROR;
        }
        opcua_byte_t x963[P256_X963_LEN];
        x963[0] = 0x04;
        (void)memcpy(x963 + 1, peer_pubkey, P256_X963_LEN - 1);
        opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;
        word32 out_len = (word32)*shared_length;
        if (wc_ecc_import_private_key_ex(keypair_ctx, ECC_SCALAR_LEN, NULL, 0, &priv, ECC_SECP256R1) == 0 &&
            wc_ecc_import_x963_ex(x963, sizeof(x963), &peer, ECC_SECP256R1) == 0 &&
            wc_ecc_set_rng(&priv, &ctx->rng) == 0 && wc_ecc_shared_secret(&priv, &peer, shared, &out_len) == 0) {
            *shared_length = out_len;
            rc = MU_STATUS_GOOD;
        }
        wc_ecc_free(&peer);
        wc_ecc_free(&priv);
        return rc;
    }

    return MU_STATUS_BAD_INTERNALERROR;
}

void w_ecc_keypair_free(void *context, opcua_byte_t *keypair_ctx) {
    (void)context;
    if (keypair_ctx) {
        (void)memset(keypair_ctx, 0, ECC_SCALAR_LEN);
    }
}

/* --- signatures --- */

static opcua_statuscode_t sha256_digest(const opcua_byte_t *data, size_t length, opcua_byte_t *hash) {
    wc_Sha256 sha;
    if (wc_InitSha256(&sha) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
    if (wc_Sha256Update(&sha, data, (word32)length) == 0 && wc_Sha256Final(&sha, hash) == 0) {
        rc = MU_STATUS_GOOD;
    }
    wc_Sha256Free(&sha);
    return rc;
}

opcua_statuscode_t w_ecc_sign(void *context, mu_ecc_curve_t curve, const opcua_byte_t *data, size_t length,
                              opcua_byte_t *signature, size_t *signature_length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    if (!ctx || !data || !signature || !signature_length || *signature_length < MU_ECC_SIGNATURE_LENGTH) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    if (curve == MU_ECC_CURVE_25519) {
        if (!ctx->ed_key_set) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        word32 sig_len = (word32)*signature_length;
        if (wc_ed25519_sign_msg(data, (word32)length, signature, &sig_len, &ctx->ed_key) != 0 ||
            sig_len != MU_ECC_SIGNATURE_LENGTH) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        *signature_length = sig_len;
        return MU_STATUS_GOOD;
    }

    if (curve == MU_ECC_CURVE_NISTP256) {
        if (!ctx->p256_key_set) {
            return MU_STATUS_BAD_SECURITYCHECKSFAILED;
        }
        opcua_byte_t hash[P256_COORD_LEN];
        if (sha256_digest(data, length, hash) != MU_STATUS_GOOD) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        mp_int r, s;
        if (mp_init(&r) != 0) {
            return MU_STATUS_BAD_INTERNALERROR;
        }
        if (mp_init(&s) != 0) {
            mp_clear(&r);
            return MU_STATUS_BAD_INTERNALERROR;
        }
        opcua_statuscode_t rc = MU_STATUS_BAD_INTERNALERROR;
        if (wc_ecc_sign_hash_ex(hash, sizeof(hash), &ctx->rng, &ctx->p256_key, &r, &s) == 0 &&
            mp_to_unsigned_bin_len(&r, signature, P256_COORD_LEN) == 0 &&
            mp_to_unsigned_bin_len(&s, signature + P256_COORD_LEN, P256_COORD_LEN) == 0) {
            *signature_length = 2 * P256_COORD_LEN;
            rc = MU_STATUS_GOOD;
        }
        mp_clear(&s);
        mp_clear(&r);
        return rc;
    }

    return MU_STATUS_BAD_INTERNALERROR;
}

opcua_statuscode_t w_ecc_verify(void *context, mu_ecc_curve_t curve, const opcua_byte_t *certificate,
                                size_t certificate_length, const opcua_byte_t *data, size_t data_length,
                                const opcua_byte_t *signature, size_t signature_length) {
    (void)context;
    if (!certificate || !data || !signature || signature_length != MU_ECC_SIGNATURE_LENGTH) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    DecodedCert decoded;
    InitDecodedCert(&decoded, certificate, (word32)certificate_length, NULL);
    if (ParseCert(&decoded, CERT_TYPE, NO_VERIFY, NULL) != 0) {
        FreeDecodedCert(&decoded);
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }

    opcua_statuscode_t rc = MU_STATUS_BAD_SECURITYCHECKSFAILED;

    if (curve == MU_ECC_CURVE_25519) {
        ed25519_key key;
        if (wc_ed25519_init(&key) == 0) {
            int res = 0;
            /* The parsed certificate exposes the raw 32-byte Ed25519 public key
               (not a SubjectPublicKeyInfo), so import it directly. */
            if (wc_ed25519_import_public(decoded.publicKey, decoded.pubKeySize, &key) == 0 &&
                wc_ed25519_verify_msg(signature, (word32)signature_length, data, (word32)data_length, &res, &key) ==
                    0 &&
                res == 1) {
                rc = MU_STATUS_GOOD;
            }
            wc_ed25519_free(&key);
        }
    } else if (curve == MU_ECC_CURVE_NISTP256) {
        opcua_byte_t hash[P256_COORD_LEN];
        ecc_key key;
        if (sha256_digest(data, data_length, hash) == MU_STATUS_GOOD && wc_ecc_init(&key) == 0) {
            word32 idx = 0;
            mp_int r, s;
            int res = 0;
            if (wc_EccPublicKeyDecode(decoded.publicKey, &idx, &key, decoded.pubKeySize) == 0 && mp_init(&r) == 0) {
                if (mp_init(&s) == 0) {
                    if (mp_read_unsigned_bin(&r, signature, P256_COORD_LEN) == 0 &&
                        mp_read_unsigned_bin(&s, signature + P256_COORD_LEN, P256_COORD_LEN) == 0 &&
                        wc_ecc_verify_hash_ex(&r, &s, hash, sizeof(hash), &res, &key) == 0 && res == 1) {
                        rc = MU_STATUS_GOOD;
                    }
                    mp_clear(&s);
                }
                mp_clear(&r);
            }
            wc_ecc_free(&key);
        }
    }

    FreeDecodedCert(&decoded);
    return rc;
}

opcua_statuscode_t w_get_own_ecc_certificate(void *context, mu_ecc_curve_t curve, const opcua_byte_t **certificate,
                                             size_t *length) {
    struct wolfssl_crypto_context *ctx = (struct wolfssl_crypto_context *)context;
    if (!ctx || !certificate || !length) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    const opcua_byte_t *der = (curve == MU_ECC_CURVE_25519) ? ctx->ed_cert_der : ctx->p256_cert_der;
    size_t der_len = (curve == MU_ECC_CURVE_25519) ? ctx->ed_cert_len : ctx->p256_cert_len;
    if (!der || der_len == 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    *certificate = der;
    *length = der_len;
    return MU_STATUS_GOOD;
}

/* --- ChaCha20-Poly1305 AEAD (RFC 8439) --- */

opcua_statuscode_t w_chacha20_poly1305_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *nonce,
                                               const opcua_byte_t *aad, size_t aad_length, const opcua_byte_t *input,
                                               size_t length, opcua_byte_t *output, opcua_byte_t *tag) {
    (void)context;
    if (wc_ChaCha20Poly1305_Encrypt(key, nonce, aad, (word32)aad_length, input, (word32)length, output, tag) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_chacha20_poly1305_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *nonce,
                                               const opcua_byte_t *aad, size_t aad_length, const opcua_byte_t *input,
                                               size_t length, const opcua_byte_t *tag, opcua_byte_t *output) {
    (void)context;
    /* Poly1305 tag mismatch fails closed with MAC_CMP_FAILED_E. */
    if (wc_ChaCha20Poly1305_Decrypt(key, nonce, aad, (word32)aad_length, input, (word32)length, tag, output) != 0) {
        return MU_STATUS_BAD_SECURITYCHECKSFAILED;
    }
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_CU_SECURITY_ECC */
#else
typedef int mu_wolfssl_dummy_ecc;
#endif /* MUC_OPCUA_HAVE_WOLFSSL */
