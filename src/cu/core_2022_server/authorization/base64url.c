/* src/cu/core_2022_server/authorization/base64url.c
 *
 * URL-safe Base64 decoder (RFC 4648 §5, RFC 7515 §2). Freestanding C11, no
 * heap, no POSIX. The caller provides the output buffer; the function returns
 * the number of bytes written or -1 on error.
 *
 * JWT/JWS uses the URL-safe alphabet without padding, so this decoder
 * tolerates inputs whose length is not a multiple of 4 by implicit padding
 * (2-byte tail -> 1 byte out, 3-byte tail -> 2 bytes out, 1-byte tail is
 * illegal per RFC 4648).
 */
#include "base64url.h"

#include <stddef.h>

/* Decode-table sentinel: -1 means "invalid character". Padding '=' maps to
   the sentinel 64 (handled by the caller, not the table merge). */
static int b64url_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') {
        return (int)(c - 'A'); /* 0-25 */
    }
    if (c >= 'a' && c <= 'z') {
        return (int)(c - 'a') + 26; /* 26-51 */
    }
    if (c >= '0' && c <= '9') {
        return (int)(c - '0') + 52; /* 52-61 */
    }
    if (c == '-') {
        return 62;
    }
    if (c == '_') {
        return 63;
    }
    if (c == '=') {
        return 64; /* padding */
    }
    return -1;
}

int mu_base64url_decode(const char *input, size_t input_len, unsigned char *output, size_t output_max) {
    if (input == 0 || output == 0) {
        return -1;
    }
    if (input_len == 0) {
        return 0;
    }

    size_t out_pos = 0;
    size_t in_pos = 0;
    int quad[4];
    int quad_len = 0;

    while (in_pos < input_len) {
        unsigned char c = (unsigned char)input[in_pos++];
        int v = b64url_value(c);
        if (v < 0) {
            return -1; /* invalid character */
        }
        if (v == 64) {
            /* Padding terminates the input. The remainder (if any) must be
               padding too. */
            break;
        }
        quad[quad_len++] = v;
        if (quad_len == 4) {
            if (out_pos + 3 > output_max) {
                return -1;
            }
            output[out_pos++] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
            output[out_pos++] = (unsigned char)((quad[1] << 4) | (quad[2] >> 2));
            output[out_pos++] = (unsigned char)((quad[2] << 6) | quad[3]);
            quad_len = 0;
        }
    }

    /* Flush a partial quad (1, 2, or 3 trailing base64 characters). */
    if (quad_len == 1) {
        return -1; /* illegal: a single trailing char carries no data */
    }
    if (quad_len == 2) {
        if (out_pos + 1 > output_max) {
            return -1;
        }
        output[out_pos++] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
    } else if (quad_len == 3) {
        if (out_pos + 2 > output_max) {
            return -1;
        }
        output[out_pos++] = (unsigned char)((quad[0] << 2) | (quad[1] >> 4));
        output[out_pos++] = (unsigned char)((quad[1] << 4) | (quad[2] >> 2));
    }

    return (int)out_pos;
}
