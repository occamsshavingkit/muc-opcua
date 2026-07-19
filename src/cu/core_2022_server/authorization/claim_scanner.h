/* src/cu/core_2022_server/authorization/claim_scanner.h
 *
 * Minimal streaming JSON key-value scanner for JWT payload (RFC 7519 §4).
 * Freestanding C11, no heap; output values land in fixed-size buffers owned by
 * the caller.
 */
#ifndef MU_CLAIM_SCANNER_H
#define MU_CLAIM_SCANNER_H

#include "muc_opcua/authorization/jwt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extract JWT claims from a flat JSON payload.
 *
 *   json     - the decoded JWT payload (a JSON object); need not be NUL-terminated
 *   json_len - byte length of json
 *   out      - zero-initialised claims struct to fill
 *
 * Recognises the registered JWT claims (RFC 7519 §4.1): iss, sub, aud, exp,
 * nbf, iat. Unknown keys are ignored. String values longer than the buffer
 * minus one are truncated; numeric values are parsed as signed 64-bit integers
 * (a leading '-' is honoured). Missing fields are left as-is (caller should
 * pre-zero the struct).
 *
 * The scanner handles a flat JSON object only. Nested objects/arrays (e.g.
 * `aud` as an array) are handled defensively: a JSON array of strings sets the
 * output to the first element.
 */
void mu_claim_scan(const char *json, size_t json_len, mu_jwt_claims_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MU_CLAIM_SCANNER_H */
