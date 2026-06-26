/* src/security/key_derivation.h
 * P-SHA256 pseudo-random function (RFC 5246 §5), used by OPC UA Basic256Sha256 to
 * derive channel keys from the client/server nonces (OPC 10000-6 6.7.5). */
#ifndef MICRO_OPCUA_KEY_DERIVATION_H
#define MICRO_OPCUA_KEY_DERIVATION_H

#include "micro_opcua/platform.h"
#include "micro_opcua/status.h"
#include <stddef.h>

/* P_SHA256(secret, seed) expanded to `output_length` bytes, using the crypto
   adapter's HMAC-SHA256. Returns Bad_* on adapter failure or if seed is too long. */
opcua_statuscode_t mu_p_sha256(const mu_crypto_adapter_t *crypto,
                               const opcua_byte_t *secret, size_t secret_length,
                               const opcua_byte_t *seed, size_t seed_length,
                               opcua_byte_t *output, size_t output_length);

#endif /* MICRO_OPCUA_KEY_DERIVATION_H */
