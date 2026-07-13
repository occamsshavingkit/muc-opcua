#ifdef MUC_OPCUA_HAVE_WOLFSSL
#ifndef WOLFSSL_CRYPTO_COMMON_H
#define WOLFSSL_CRYPTO_COMMON_H

#include "muc_opcua/config.h"
#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
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
#ifdef MUC_OPCUA_CU_SECURITY_ECC
#include <wolfssl/wolfcrypt/chacha20_poly1305.h>
#include <wolfssl/wolfcrypt/curve25519.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/ed25519.h>
#endif

struct wolfssl_crypto_context {
    const opcua_byte_t *cert_der;
    size_t cert_len;
    const opcua_byte_t *key_der;
    size_t key_len;
    RsaKey key;
    WC_RNG rng;
#ifdef MUC_OPCUA_CU_SECURITY_ECC
    /* spec 059: per-curve server ECC signing identity, provisioned by
       mu_wolfssl_crypto_adapter_set_ecc_identity. The DER pointers are
       caller-owned (borrowed, not copied). */
    ecc_key p256_key;
    int p256_key_set;
    const opcua_byte_t *p256_cert_der;
    size_t p256_cert_len;
    ed25519_key ed_key;
    int ed_key_set;
    const opcua_byte_t *ed_cert_der;
    size_t ed_cert_len;
#endif
};

extern const opcua_byte_t sha256_digest_info[];

opcua_statuscode_t w_sha256(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *digest);
opcua_statuscode_t w_hmac_sha256(void *context, const opcua_byte_t *key, size_t key_length, const opcua_byte_t *data,
                                 size_t data_length, opcua_byte_t *mac);
opcua_statuscode_t w_aes256_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output);
opcua_statuscode_t w_aes256_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output);
opcua_statuscode_t w_aes128_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output);
opcua_statuscode_t w_aes128_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output);
opcua_statuscode_t w_rsa_sha256_sign(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *signature,
                                     size_t *signature_length);
opcua_statuscode_t w_rsa_sha256_verify(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                       const opcua_byte_t *data, size_t data_length, const opcua_byte_t *signature,
                                       size_t signature_length);
opcua_statuscode_t w_rsa_oaep_decrypt(void *context, const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                      size_t *output_length);
opcua_statuscode_t w_rsa_oaep_encrypt(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                      const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                      size_t *output_length);
opcua_statuscode_t w_rsa_pss_sha256_sign(void *context, const opcua_byte_t *data, size_t length,
                                         opcua_byte_t *signature, size_t *signature_length);
opcua_statuscode_t w_rsa_pss_sha256_verify(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                           const opcua_byte_t *data, size_t data_length, const opcua_byte_t *signature,
                                           size_t signature_length);
opcua_statuscode_t w_rsa_oaep_sha256_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                             opcua_byte_t *output, size_t *output_length);
opcua_statuscode_t w_rsa_oaep_sha256_encrypt(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                             const opcua_byte_t *input, size_t length, opcua_byte_t *output,
                                             size_t *output_length);
opcua_statuscode_t w_get_own_certificate(void *context, const opcua_byte_t **certificate, size_t *length);
opcua_statuscode_t w_get_certificate_key_bits(void *context, const opcua_byte_t *certificate, size_t certificate_length,
                                              size_t *bits);
opcua_statuscode_t w_verify_certificate_validity(void *context, const opcua_byte_t *certificate,
                                                 size_t certificate_length);
opcua_statuscode_t w_get_certificate_thumbprint(void *context, const opcua_byte_t *certificate,
                                                size_t certificate_length, opcua_byte_t *thumbprint);

#ifdef MUC_OPCUA_CU_SECURITY_ECC
/* ecc.c (spec 059) */
opcua_statuscode_t w_ecc_generate_ephemeral(void *context, mu_ecc_curve_t curve, opcua_byte_t *pubkey,
                                            size_t *pubkey_length, opcua_byte_t *keypair_ctx);
opcua_statuscode_t w_ecc_ecdh_derive(void *context, mu_ecc_curve_t curve, const opcua_byte_t *keypair_ctx,
                                     const opcua_byte_t *peer_pubkey, size_t peer_pubkey_length, opcua_byte_t *shared,
                                     size_t *shared_length);
void w_ecc_keypair_free(void *context, opcua_byte_t *keypair_ctx);
opcua_statuscode_t w_ecc_sign(void *context, mu_ecc_curve_t curve, const opcua_byte_t *data, size_t length,
                              opcua_byte_t *signature, size_t *signature_length);
opcua_statuscode_t w_ecc_verify(void *context, mu_ecc_curve_t curve, const opcua_byte_t *certificate,
                                size_t certificate_length, const opcua_byte_t *data, size_t data_length,
                                const opcua_byte_t *signature, size_t signature_length);
opcua_statuscode_t w_get_own_ecc_certificate(void *context, mu_ecc_curve_t curve, const opcua_byte_t **certificate,
                                             size_t *length);
opcua_statuscode_t w_chacha20_poly1305_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *nonce,
                                               const opcua_byte_t *aad, size_t aad_length, const opcua_byte_t *input,
                                               size_t length, opcua_byte_t *output, opcua_byte_t *tag);
opcua_statuscode_t w_chacha20_poly1305_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *nonce,
                                               const opcua_byte_t *aad, size_t aad_length, const opcua_byte_t *input,
                                               size_t length, const opcua_byte_t *tag, opcua_byte_t *output);
#endif /* MUC_OPCUA_CU_SECURITY_ECC */

#endif
#endif
