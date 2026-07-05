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

struct wolfssl_crypto_context {
    const opcua_byte_t *cert_der;
    size_t cert_len;
    const opcua_byte_t *key_der;
    size_t key_len;
    RsaKey key;
    WC_RNG rng;
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

#endif
#endif
