/* src/security/secure_util.c
 * Definitions of the defensive-memory utilities declared in secure_util.h.
 * Always compiled (core source list), independent of MUC_OPCUA_SECURE_CHANNEL_CRYPTO,
 * so SecurityPolicy-None-only builds keep them without linking the crypto stack
 * (spec 072). Relocated here from the (gated) key_derivation.c translation unit. */
#include "security/secure_util.h"

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
