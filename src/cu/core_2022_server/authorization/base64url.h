/* src/cu/core_2022_server/authorization/base64url.h
 *
 * URL-safe Base64 decoder (RFC 4648 §5, RFC 7515 §2) used by the JWT/JWS
 * validator. No heap allocation: the caller provides the output buffer.
 */
#ifndef MU_BASE64URL_H
#define MU_BASE64URL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decode URL-safe Base64 (alphabet A-Z a-z 0-9 - _ ; padding optional).
 *
 *   input      - base64url-encoded bytes; need not be NUL-terminated
 *   input_len  - byte count of input
 *   output     - caller-provided byte buffer
 *   output_max - capacity of output
 *
 * Returns the number of bytes written to output, or -1 on error (invalid
 * character, malformed length, or output buffer too small).
 */
int mu_base64url_decode(const char *input, size_t input_len, unsigned char *output, size_t output_max);

#ifdef __cplusplus
}
#endif

#endif /* MU_BASE64URL_H */
