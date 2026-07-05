/* src/platform/host_crypto/hash.c
 * SHA-256 hash functions for host crypto adapter. */
#include "common.h"

opcua_statuscode_t h_sha256(void *c, const opcua_byte_t *data, size_t len, opcua_byte_t *digest) {
    (void)c;
    unsigned int dlen = 0;
    if (EVP_Digest(data, len, digest, &dlen, EVP_sha256(), NULL) != 1) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}

opcua_statuscode_t h_hmac_sha256(void *c, const opcua_byte_t *key, size_t key_len, const opcua_byte_t *data,
                                 size_t data_len, opcua_byte_t *mac) {
    (void)c;
    unsigned int mlen = 0;
    if (HMAC(EVP_sha256(), key, (int)key_len, data, data_len, mac, &mlen) == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    return MU_STATUS_GOOD;
}
