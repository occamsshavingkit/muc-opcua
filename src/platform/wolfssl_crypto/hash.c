#ifdef MUC_OPCUA_HAVE_WOLFSSL
#include "common.h"

const opcua_byte_t sha256_digest_info[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                                           0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

opcua_statuscode_t w_sha256(void *context, const opcua_byte_t *data, size_t length, opcua_byte_t *digest) {
    (void)context;
    wc_Sha256 sha;
    if (wc_InitSha256(&sha) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_Sha256Update(&sha, data, (word32)length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_Sha256Final(&sha, digest) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t w_hmac_sha256(void *context, const opcua_byte_t *key, size_t key_length, const opcua_byte_t *data,
                                 size_t data_length, opcua_byte_t *mac) {
    (void)context;
    Hmac hmac;
    if (wc_HmacSetKey(&hmac, WC_SHA256, key, (word32)key_length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_HmacUpdate(&hmac, data, (word32)data_length) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (wc_HmacFinal(&hmac, mac) != 0) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}
#else
typedef int mu_wolfssl_dummy_hash;
#endif
