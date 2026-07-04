/* src/security/key_derivation.c */
#include "key_derivation.h"
#include <string.h>

/* Maximum seed (the peer nonce) we accommodate on the stack. Channel nonces are
   32 bytes for Basic256Sha256; 64 is generous headroom. */
#define MU_KDF_MAX_SEED 64

void mu_secure_zero(void *v, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)v;

    /* Volatile byte stores keep the wipe observable so compilers cannot elide
       clearing soon-dead key material. */
    while (n != 0u) {
        *p++ = 0u;
        --n;
    }
}

bool mu_secure_memeq(const void *a, const void *b, size_t n) {
    const volatile unsigned char *pa = (const volatile unsigned char *)a;
    const volatile unsigned char *pb = (const volatile unsigned char *)b;
    unsigned char d = 0;
    for (size_t i = 0; i < n; i++) {
        d |= (pa[i] ^ pb[i]);
    }
    return d == 0;
}

/* P_SHA256(secret, seed) per RFC 5246:
     A(0) = seed
     A(i) = HMAC(secret, A(i-1))
     output = HMAC(secret, A(1)+seed) | HMAC(secret, A(2)+seed) | ... */
opcua_statuscode_t mu_p_sha256(const mu_crypto_adapter_t *crypto, const opcua_byte_t *secret, size_t secret_length,
                               const opcua_byte_t *seed, size_t seed_length, opcua_byte_t *output,
                               size_t output_length) {
    if (!crypto || !crypto->hmac_sha256 || !secret || !seed || !output) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (seed_length > MU_KDF_MAX_SEED) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    opcua_byte_t a[MU_SHA256_LENGTH];
    opcua_byte_t hmac_input[MU_SHA256_LENGTH + MU_KDF_MAX_SEED];
    opcua_byte_t block[MU_SHA256_LENGTH];
    opcua_statuscode_t status;

    /* A(1) = HMAC(secret, seed) */
    status = crypto->hmac_sha256(crypto->context, secret, secret_length, seed, seed_length, a);
    if (status != MU_STATUS_GOOD) {
        mu_secure_zero(a, sizeof(a));
        mu_secure_zero(hmac_input, sizeof(hmac_input));
        mu_secure_zero(block, sizeof(block));
        return status;
    }

    size_t produced = 0;
    while (produced < output_length) {
        /* block = HMAC(secret, A(i) + seed) */
        (void)memcpy(hmac_input, a, MU_SHA256_LENGTH);
        (void)memcpy(hmac_input + MU_SHA256_LENGTH, seed, seed_length);
        status = crypto->hmac_sha256(crypto->context, secret, secret_length, hmac_input, MU_SHA256_LENGTH + seed_length,
                                     block);
        if (status != MU_STATUS_GOOD) {
            mu_secure_zero(a, sizeof(a));
            mu_secure_zero(hmac_input, sizeof(hmac_input));
            mu_secure_zero(block, sizeof(block));
            return status;
        }

        size_t take = output_length - produced;
        if (take > MU_SHA256_LENGTH) {
            take = MU_SHA256_LENGTH;
        }
        (void)memcpy(output + produced, block, take);
        produced += take;

        /* A(i+1) = HMAC(secret, A(i)) */
        status = crypto->hmac_sha256(crypto->context, secret, secret_length, a, MU_SHA256_LENGTH, a);
        if (status != MU_STATUS_GOOD) {
            mu_secure_zero(a, sizeof(a));
            mu_secure_zero(hmac_input, sizeof(hmac_input));
            mu_secure_zero(block, sizeof(block));
            return status;
        }
    }

    mu_secure_zero(a, sizeof(a));
    mu_secure_zero(hmac_input, sizeof(hmac_input));
    mu_secure_zero(block, sizeof(block));
    return MU_STATUS_GOOD;
}
