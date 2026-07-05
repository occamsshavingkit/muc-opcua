#ifndef HOST_CRYPTO_COMMON_H
#define HOST_CRYPTO_COMMON_H

#include "../host_crypto_adapter.h"
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
    EVP_PKEY *key;
    unsigned char *cert_der;
    int cert_der_len;
};

struct host_cipher_context {
    EVP_CIPHER_CTX *encrypt;
    EVP_CIPHER_CTX *decrypt;
};

/* common.c */
opcua_statuscode_t mu_host_crypto_adapter_init(mu_crypto_adapter_t *adapter);
void mu_host_crypto_adapter_cleanup(mu_crypto_adapter_t *adapter);

/* hash.c */
opcua_statuscode_t h_sha256(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *digest);
opcua_statuscode_t h_hmac_sha256(void *c, const opcua_byte_t *key, size_t key_len, const opcua_byte_t *data,
                                 size_t data_len, opcua_byte_t *mac);

/* cipher.c */
opcua_statuscode_t h_aes_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                 size_t len, opcua_byte_t *out);
opcua_statuscode_t h_aes_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                 size_t len, opcua_byte_t *out);
opcua_statuscode_t h_aes128_encrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                    size_t len, opcua_byte_t *out);
opcua_statuscode_t h_aes128_decrypt(void *c, const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *in,
                                    size_t len, opcua_byte_t *out);
opcua_statuscode_t h_cipher_ctx_init(void *c, const opcua_byte_t *key, opcua_byte_t *ctx_storage);
opcua_statuscode_t h_aes_encrypt_ctx(void *c, opcua_byte_t *ctx_storage, const opcua_byte_t *iv, const opcua_byte_t *in,
                                     size_t len, opcua_byte_t *out);
opcua_statuscode_t h_aes_decrypt_ctx(void *c, opcua_byte_t *ctx_storage, const opcua_byte_t *iv, const opcua_byte_t *in,
                                     size_t len, opcua_byte_t *out);
void h_cipher_ctx_free(void *c, opcua_byte_t *ctx_storage);

/* asymmetric.c */
EVP_PKEY *pubkey_from_cert(const opcua_byte_t *cert, size_t cert_len);
opcua_statuscode_t h_rsa_oaep_decrypt(void *c, const opcua_byte_t *in, size_t len, opcua_byte_t *out, size_t *out_len);
opcua_statuscode_t h_rsa_oaep_encrypt(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *in,
                                      size_t len, opcua_byte_t *out, size_t *out_len);
opcua_statuscode_t h_rsa_oaep_sha256_decrypt(void *c, const opcua_byte_t *in, size_t len, opcua_byte_t *out,
                                             size_t *out_len);
opcua_statuscode_t h_rsa_oaep_sha256_encrypt(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *in,
                                             size_t len, opcua_byte_t *out, size_t *out_len);
opcua_statuscode_t h_get_own_certificate(void *c, const opcua_byte_t **cert, size_t *len);
opcua_statuscode_t h_get_certificate_key_bits(void *c, const opcua_byte_t *cert, size_t cert_len, size_t *bits);
opcua_statuscode_t h_verify_certificate_validity(void *c, const opcua_byte_t *cert, size_t cert_len);
opcua_statuscode_t h_get_certificate_thumbprint(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                opcua_byte_t *thumbprint);
opcua_statuscode_t h_verify_certificate_application_uri(void *c, const opcua_byte_t *cert, size_t cert_len,
                                                        const char *application_uri, size_t application_uri_len);

/* sign.c */
opcua_statuscode_t h_rsa_sign(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *sig, size_t *sig_len);
opcua_statuscode_t h_rsa_verify(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *data,
                                size_t data_len, const opcua_byte_t *sig, size_t sig_len);
opcua_statuscode_t h_rsa_pss_sha256_sign(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *sig,
                                         size_t *sig_len);
opcua_statuscode_t h_rsa_pss_sha256_verify(void *c, const opcua_byte_t *cert, size_t cert_len, const opcua_byte_t *data,
                                           size_t data_len, const opcua_byte_t *sig, size_t sig_len);

#endif
