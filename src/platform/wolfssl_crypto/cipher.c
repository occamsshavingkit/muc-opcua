#ifdef MUC_OPCUA_HAVE_WOLFSSL
#include "common.h"

opcua_statuscode_t w_aes256_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 32, iv, AES_ENCRYPTION) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_AesCbcEncrypt(&aes, output, input, (word32)length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_aes256_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 32, iv, AES_DECRYPTION) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_AesCbcDecrypt(&aes, output, input, (word32)length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_aes128_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 16, iv, AES_ENCRYPTION) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_AesCbcEncrypt(&aes, output, input, (word32)length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_aes128_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                        const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    (void)context;
    Aes aes;
    if (wc_AesSetKey(&aes, key, 16, iv, AES_DECRYPTION) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_AesCbcDecrypt(&aes, output, input, (word32)length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}
#else
typedef int mu_wolfssl_dummy_cipher;
#endif
