/* src/security/key_derivation.h
 * P-SHA256 pseudo-random function (RFC 5246 §5), used by OPC UA Basic256Sha256 to
 * derive channel keys from the client/server nonces (OPC 10000-6 6.7.5). */
#ifndef MUC_OPCUA_KEY_DERIVATION_H
#define MUC_OPCUA_KEY_DERIVATION_H

#include "muc_opcua/platform.h"
#include "muc_opcua/status.h"
#include <stddef.h>

void mu_secure_zero(void *v, size_t n);
bool mu_secure_memeq(const void *a, const void *b, size_t n);

/* P_SHA256(secret, seed) expanded to `output_length` bytes, using the crypto
   adapter's HMAC-SHA256. Returns Bad_* on adapter failure or if seed is too long. */
opcua_statuscode_t mu_p_sha256(const mu_crypto_adapter_t *crypto, const opcua_byte_t *secret, size_t secret_length,
                               const opcua_byte_t *seed, size_t seed_length, opcua_byte_t *output,
                               size_t output_length);

#ifdef MUC_OPCUA_ECC
/* HKDF-SHA256 (RFC 5869), the key-derivation function for the ECC SecurityPolicies
   (OPC-10000-6 §6.8.1), built on the adapter's HMAC-SHA256:
     PRK = HMAC(salt, ikm);  OKM = T(1)|T(2)|... truncated to `okm_length`, where
     T(i) = HMAC(PRK, T(i-1) | info | i).
   `okm_length` must be <= 255*32. Returns Bad_* on adapter failure, over-long
   output, or if info is too long for the internal scratch. */
opcua_statuscode_t mu_hkdf_sha256(const mu_crypto_adapter_t *crypto, const opcua_byte_t *salt, size_t salt_length,
                                  const opcua_byte_t *ikm, size_t ikm_length, const opcua_byte_t *info,
                                  size_t info_length, opcua_byte_t *okm, size_t okm_length);
#endif /* MUC_OPCUA_ECC */

#endif /* MUC_OPCUA_KEY_DERIVATION_H */
