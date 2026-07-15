/* src/security/secure_util.h
 * Small constant-time / defensive-memory utilities that are NOT inherently
 * cryptographic and must remain available to no-crypto builds (spec 072:
 * MUC_OPCUA_SECURE_CHANNEL_CRYPTO off). Defined in the always-compiled core
 * (secure_util.c), not in the gated crypto translation units. */
#ifndef MUC_OPCUA_SECURE_UTIL_H
#define MUC_OPCUA_SECURE_UTIL_H

#include "muc_opcua/platform.h"

#include <stddef.h>

/* Overwrite `n` bytes at `v` with zero via volatile stores so the compiler
   cannot elide the wipe of soon-dead sensitive material. */
void mu_secure_zero(void *v, size_t n);

/* Constant-time equality of `n` bytes at `a` and `b` (no early exit), for
   comparing secrets/MACs without a timing side channel. */
bool mu_secure_memeq(const void *a, const void *b, size_t n);

#endif /* MUC_OPCUA_SECURE_UTIL_H */
