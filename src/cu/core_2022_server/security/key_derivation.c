/* src/security/key_derivation.c */
#include "security/key_derivation.h"
#include <string.h>

/* Maximum seed (the peer nonce) we accommodate on the stack. Channel nonces are
   32 bytes for Basic256Sha256; 128 allows for larger nonces (OPC-10000-6 §6.7.6). */
#define MU_KDF_MAX_SEED 128

#ifdef MUC_OPCUA_CU_SECURITY_ECC
/* Maximum HKDF `info` we accommodate. The ECC key-derivation salt/info is
   L | UTF8("opcua-server"|"opcua-client") | Nonce | Nonce; with <=64-byte channel
   nonces this is well under 256 bytes (OPC-10000-6 §6.8.1). */
#define MU_HKDF_MAX_INFO 256
#endif

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

#ifdef MUC_OPCUA_CU_SECURITY_ECC
opcua_statuscode_t mu_hkdf_sha256(const mu_crypto_adapter_t *crypto, const opcua_byte_t *salt, size_t salt_length,
                                  const opcua_byte_t *ikm, size_t ikm_length, const opcua_byte_t *info,
                                  size_t info_length, opcua_byte_t *okm, size_t okm_length) {
    if (!crypto || !crypto->hmac_sha256 || !ikm || !okm) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    if (info_length > MU_HKDF_MAX_INFO || (info_length != 0u && !info)) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    /* RFC 5869 caps output at 255 hash blocks. */
    if (okm_length > (size_t)255 * MU_SHA256_LENGTH) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    /* Extract: PRK = HMAC(salt, IKM). RFC 5869 uses HashLen zero bytes when salt
       is empty; the adapter's HMAC accepts a zero-length key, so a NULL/empty salt
       maps to an all-zero key of the same effect via a static zero block. */
    opcua_byte_t zero_salt[MU_SHA256_LENGTH];
    if (salt == NULL || salt_length == 0u) {
        (void)memset(zero_salt, 0, sizeof(zero_salt));
        salt = zero_salt;
        salt_length = sizeof(zero_salt);
    }

    opcua_byte_t prk[MU_SHA256_LENGTH];
    opcua_byte_t t[MU_SHA256_LENGTH] = {0}; /* T(0) empty; zeroed to satisfy static analysis */
    opcua_byte_t buf[MU_SHA256_LENGTH + MU_HKDF_MAX_INFO + 1];
    opcua_statuscode_t status = crypto->hmac_sha256(crypto->context, salt, salt_length, ikm, ikm_length, prk);
    if (status != MU_STATUS_GOOD) {
        goto done;
    }

    /* Expand: T(i) = HMAC(PRK, T(i-1) | info | i), i = 1.. */
    size_t produced = 0;
    size_t t_len = 0; /* T(0) is empty */
    unsigned char counter = 0;
    while (produced < okm_length) {
        counter++;
        size_t off = 0;
        (void)memcpy(buf, t, t_len);
        off += t_len;
        if (info_length != 0u) {
            (void)memcpy(buf + off, info, info_length);
            off += info_length;
        }
        buf[off++] = counter;
        status = crypto->hmac_sha256(crypto->context, prk, sizeof(prk), buf, off, t);
        if (status != MU_STATUS_GOOD) {
            goto done;
        }
        t_len = MU_SHA256_LENGTH;

        size_t take = okm_length - produced;
        if (take > MU_SHA256_LENGTH) {
            take = MU_SHA256_LENGTH;
        }
        (void)memcpy(okm + produced, t, take);
        produced += take;
    }

done:
    mu_secure_zero(prk, sizeof(prk));
    mu_secure_zero(t, sizeof(t));
    mu_secure_zero(buf, sizeof(buf));
    return status;
}
#endif /* MUC_OPCUA_CU_SECURITY_ECC */
