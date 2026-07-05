/* src/core/ctz.h
 * Portable count-trailing-zeros for 32-bit values.
 * Uses __builtin_ctz on GCC/Clang, de Bruijn 32-entry lookup table otherwise.
 * Like __builtin_ctz, result is undefined for zero input — callers must guard. */
#ifndef MU_CTZ_H
#define MU_CTZ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define mu_ctz_u32(x) ((unsigned)__builtin_ctz(x))
#else
static inline unsigned mu_ctz_u32(uint32_t x) {
    static const unsigned char debruijn[32] = {0,  1,  28, 2,  29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4,  8,
                                               31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6,  11, 5,  10, 9};
    return debruijn[((x & (~x + 1u)) * 0x077CB531u) >> 27];
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* MU_CTZ_H */
