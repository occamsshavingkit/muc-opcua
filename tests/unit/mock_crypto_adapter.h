/* tests/unit/mock_crypto_adapter.h */
#ifndef MOCK_CRYPTO_ADAPTER_H
#define MOCK_CRYPTO_ADAPTER_H

#include "muc_opcua/platform.h"
#include <string.h>

/* A simple mock crypto adapter for testing without a real backend */

static opcua_statuscode_t mock_sha256(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *digest) {
    (void)context;
    (void)data;
    (void)length;
    (void)memset(digest, 0, MU_SHA256_LENGTH);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_hmac_sha256(void *context, const opcua_byte_t *key, size_t key_length,
                                           const opcua_byte_t *data, size_t data_length, opcua_byte_t *mac) {
    (void)context;
    (void)key;
    (void)key_length;
    (void)data;
    (void)data_length;
    (void)memset(mac, 0, MU_SHA256_LENGTH);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_get_own_certificate(void *context, const opcua_byte_t **certificate, size_t *length) {
    (void)context;
    static const opcua_byte_t dummy_cert[] = {0x01, 0x02, 0x03};
    *certificate = dummy_cert;
    *length = sizeof(dummy_cert);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_rsa_sha256_sign(void *context, const opcua_byte_t *data, size_t length,
                                               opcua_byte_t *signature, size_t *signature_length) {
    (void)context;
    (void)data;
    (void)length;
    *signature_length = 32;
    (void)memset(signature, 0xAA, 32);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_rsa_sha256_verify(void *context, const opcua_byte_t *certificate,
                                                 size_t certificate_length, const opcua_byte_t *data,
                                                 size_t data_length, const opcua_byte_t *signature,
                                                 size_t signature_length) {
    (void)context;
    (void)certificate;
    (void)certificate_length;
    (void)data;
    (void)data_length;
    (void)signature;
    (void)signature_length;
    return MU_STATUS_GOOD; /* Always succeed in mock */
}

static opcua_statuscode_t mock_rsa_oaep_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                                opcua_byte_t *output, size_t *output_length) {
    (void)context;
    (void)input;
    (void)length;
    (void)output;
    *output_length = length;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_rsa_oaep_encrypt(void *context, const opcua_byte_t *certificate,
                                                size_t certificate_length, const opcua_byte_t *input, size_t length,
                                                opcua_byte_t *output, size_t *output_length) {
    (void)context;
    (void)certificate;
    (void)certificate_length;
    (void)input;
    (void)length;
    (void)output;
    *output_length = length;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_rsa_pss_sha256_sign(void *context, const opcua_byte_t *data, size_t length,
                                                   opcua_byte_t *signature, size_t *signature_length) {
    (void)context;
    (void)data;
    (void)length;
    *signature_length = 32;
    (void)memset(signature, 0xBB, 32);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t mock_rsa_pss_sha256_verify(void *context, const opcua_byte_t *certificate,
                                                     size_t certificate_length, const opcua_byte_t *data,
                                                     size_t data_length, const opcua_byte_t *signature,
                                                     size_t signature_length) {
    (void)context;
    (void)certificate;
    (void)certificate_length;
    (void)data;
    (void)data_length;
    (void)signature;
    (void)signature_length;
    return MU_STATUS_GOOD;
}

static mu_crypto_adapter_t get_mock_crypto_adapter(void) {
    mu_crypto_adapter_t adapter = {0};
    adapter.sha256 = mock_sha256;
    adapter.hmac_sha256 = mock_hmac_sha256;
    adapter.get_own_certificate = mock_get_own_certificate;
    adapter.rsa_sha256_sign = mock_rsa_sha256_sign;
    adapter.rsa_sha256_verify = mock_rsa_sha256_verify;
    adapter.rsa_oaep_decrypt = mock_rsa_oaep_decrypt;
    adapter.rsa_oaep_encrypt = mock_rsa_oaep_encrypt;
    adapter.rsa_pss_sha256_sign = mock_rsa_pss_sha256_sign;
    adapter.rsa_pss_sha256_verify = mock_rsa_pss_sha256_verify;
    return adapter;
}

#endif /* MOCK_CRYPTO_ADAPTER_H */
