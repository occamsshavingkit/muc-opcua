/* src/cu/core_2022_server/authorization/crypto_jwt.h
 *
 * Crypto wrapper for the JWT/JWS validator. Decouples the JWT layer from the
 * platform crypto backend: callers pass the JWS signing input and signature
 * bytes plus a DER-encoded public key and algorithm selector; the wrapper
 * performs the verify via whatever backend the build provides.
 *
 * Spec grounding:
 *   RFC 7515 §5 -- JWS signature computation
 *   RFC 7518 §3.1 -- JWT signing algorithms (RS256/384/512, ES256/384/512)
 *   OPC-10000-7 CU 1697 -- User Token JWT Server Facet
 */
#ifndef MU_CRYPTO_JWT_H
#define MU_CRYPTO_JWT_H

#include "muc_opcua/authorization/jwt.h"
#include "muc_opcua/opcua_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Verify a JWS signature.
 *
 *   signing_input    - header.payload bytes (the ASCII content covered by the
 *                      JWS signature, RFC 7515 §5.1)
 *   signing_input_len
 *                     - byte length of signing_input
 *   signature        - the raw signature bytes
 *   signature_len    - byte length of signature
 *   public_key_der   - DER-encoded public key (RSA SubjectPublicKeyInfo or EC
 *                      SubjectPublicKeyInfo; the backend determines the type)
 *   public_key_len   - byte length of public_key_der
 *   alg              - JWS algorithm (determines the hash and signature form)
 *
 * Returns true if the signature is valid for the given key and input; false
 * otherwise (including unsupported algorithm, malformed key, or backend not
 * compiled in).
 */
opcua_boolean_t mu_crypto_jwt_verify(const opcua_byte_t *signing_input, size_t signing_input_len,
                                     const opcua_byte_t *signature, size_t signature_len,
                                     const opcua_byte_t *public_key_der, size_t public_key_len, mu_jwt_alg_t alg);

#ifdef __cplusplus
}
#endif

#endif /* MU_CRYPTO_JWT_H */
