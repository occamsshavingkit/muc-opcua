/* src/cu/core_2022_server/authorization/claim_scanner.c
 *
 * Minimal streaming JSON key-value scanner for the JWT payload (RFC 7519 §4).
 * Freestanding C11, no heap. The scanner recognises a flat JSON object only;
 * nested values are skipped defensively.
 *
 * Algorithm:
 *   1. Walk the input looking for quoted keys followed (after whitespace) by ':'.
 *   2. After ':', the value is one of: string, integer, array, object, or
 *      literal (true/false/null).
 *   3. For string values, copy into a fixed-size buffer with NUL termination
 *      (truncate on overflow). Handle \" and \\ escapes; other escapes are
 *      passed through as-is.
 *   4. For integer values, parse with a leading '-' if present.
 *   5. For arrays, use the first string element (RFC 7519 §4.1.3 allows `aud`
 *      to be either a string or an array of strings).
 *
 * Unknown keys are skipped. Malformed JSON does not halt the scan; the missing
 * fields simply remain at their caller-initialised values.
 */
#include "claim_scanner.h"

#include <stddef.h>
#include <string.h>

/* Copy a quoted JSON string value (between the surrounding quotes) into dst,
   applying \" and \\ unescaping. Truncates to dst_size-1 with NUL terminator.
   Returns the number of input bytes consumed INCLUDING the closing quote, or
   -1 on malformed input (unterminated string). */
static int copy_json_string(const char *src, size_t src_len, size_t start, char *dst, size_t dst_size) {
    size_t i = start;
    size_t out = 0;
    while (i < src_len) {
        char c = src[i++];
        if (c == '"') {
            if (dst_size > 0) {
                dst[out < dst_size - 1 ? out : dst_size - 1] = '\0';
            }
            return (int)i;
        }
        if (c == '\\' && i < src_len) {
            char esc = src[i++];
            if (esc == '"') {
                c = '"';
            } else if (esc == '\\') {
                c = '\\';
            } else if (esc == '/') {
                c = '/';
            } else if (esc == 'n') {
                c = '\n';
            } else if (esc == 'r') {
                c = '\r';
            } else if (esc == 't') {
                c = '\t';
            } else {
                /* Keep the escaped character literally; not a JSON-compliant
                   escape but acceptable for our limited use case. */
                c = esc;
            }
        }
        if (dst != 0 && out + 1 < dst_size) {
            dst[out] = c;
        }
        out++;
    }
    return -1; /* unterminated string */
}

/* Parse a signed decimal integer. Returns the number of bytes consumed (>= 1)
   or -1 on missing digits. The result is written to *out. */
static int parse_json_int(const char *src, size_t src_len, size_t start, opcua_int64_t *out) {
    size_t i = start;
    int negative = 0;
    if (i < src_len && src[i] == '-') {
        negative = 1;
        i++;
    }
    opcua_int64_t value = 0;
    size_t digits = 0;
    while (i < src_len && src[i] >= '0' && src[i] <= '9') {
        value = value * 10 + (opcua_int64_t)(src[i] - '0');
        i++;
        digits++;
    }
    if (digits == 0) {
        return -1;
    }
    *out = negative ? -value : value;
    return (int)(i - start);
}

/* Skip whitespace starting at src[start]. Returns the new index. */
static size_t skip_ws(const char *src, size_t src_len, size_t start) {
    size_t i = start;
    while (i < src_len) {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
        } else {
            break;
        }
    }
    return i;
}

/* Skip a single JSON value (string, number, array, object, literal) starting
   at src[start]. Returns the index just past the value, or src_len if the
   value runs off the end. */
static size_t skip_value(const char *src, size_t src_len, size_t start) {
    size_t i = start;
    if (i >= src_len) {
        return i;
    }
    char c = src[i];
    if (c == '"') {
        i++;
        while (i < src_len) {
            char x = src[i++];
            if (x == '\\' && i < src_len) {
                i++; /* skip escaped char */
                continue;
            }
            if (x == '"') {
                break;
            }
        }
        return i;
    }
    if (c == '[' || c == '{') {
        char open_ch = c;
        char close_ch = (c == '[') ? ']' : '}';
        int depth = 1;
        i++;
        while (i < src_len && depth > 0) {
            char x = src[i++];
            if (x == '"') {
                /* skip nested string */
                while (i < src_len) {
                    char s = src[i++];
                    if (s == '\\' && i < src_len) {
                        i++;
                        continue;
                    }
                    if (s == '"') {
                        break;
                    }
                }
                continue;
            }
            if (x == open_ch) {
                depth++;
            } else if (x == close_ch) {
                depth--;
            }
        }
        return i;
    }
    /* number, true, false, null -- read until delimiter */
    while (i < src_len) {
        char x = src[i];
        if (x == ',' || x == '}' || x == ']' || x == ' ' || x == '\t' || x == '\n' || x == '\r') {
            break;
        }
        i++;
    }
    return i;
}

void mu_claim_scan(const char *json, size_t json_len, mu_jwt_claims_t *out) {
    if (json == 0 || out == 0) {
        return;
    }
    size_t i = 0;
    while (i < json_len) {
        /* Find next '"' that opens a key. */
        if (json[i] != '"') {
            i++;
            continue;
        }
        size_t key_start = i + 1;
        size_t key_end = key_start;
        while (key_end < json_len && json[key_end] != '"') {
            if (json[key_end] == '\\' && key_end + 1 < json_len) {
                key_end += 2;
            } else {
                key_end++;
            }
        }
        if (key_end >= json_len) {
            return; /* unterminated key */
        }
        size_t key_len = key_end - key_start;
        size_t after_key = key_end + 1;

        size_t colon = skip_ws(json, json_len, after_key);
        if (colon >= json_len || json[colon] != ':') {
            i = after_key;
            continue;
        }
        size_t value_start = skip_ws(json, json_len, colon + 1);
        if (value_start >= json_len) {
            return;
        }

        /* Match against the six registered claim names (RFC 7519 §4.1).
           copy_json_string returns the absolute index just past the closing
           quote (it scans from `start` but returns the new `i`, not a length),
           so we assign its result to `i` directly. */
        const char *key = json + key_start;
        if (key_len == 3 && memcmp(key, "iss", 3) == 0) {
            if (json[value_start] == '"') {
                int n = copy_json_string(json, json_len, value_start + 1, out->iss, sizeof(out->iss));
                if (n < 0) {
                    return;
                }
                i = (size_t)n;
                continue;
            }
        } else if (key_len == 3 && memcmp(key, "sub", 3) == 0) {
            if (json[value_start] == '"') {
                int n = copy_json_string(json, json_len, value_start + 1, out->sub, sizeof(out->sub));
                if (n < 0) {
                    return;
                }
                i = (size_t)n;
                continue;
            }
        } else if (key_len == 3 && memcmp(key, "aud", 3) == 0) {
            if (json[value_start] == '"') {
                int n = copy_json_string(json, json_len, value_start + 1, out->aud, sizeof(out->aud));
                if (n < 0) {
                    return;
                }
                i = (size_t)n;
                continue;
            }
            if (json[value_start] == '[') {
                /* aud as array of strings: use first element if it is a string. */
                size_t arr = skip_ws(json, json_len, value_start + 1);
                if (arr < json_len && json[arr] == '"') {
                    int n = copy_json_string(json, json_len, arr + 1, out->aud, sizeof(out->aud));
                    if (n < 0) {
                        return;
                    }
                }
            }
        } else if (key_len == 3 && memcmp(key, "exp", 3) == 0) {
            opcua_int64_t v = 0;
            int n = parse_json_int(json, json_len, value_start, &v);
            if (n > 0) {
                out->exp = v;
                i = value_start + (size_t)n;
                continue;
            }
        } else if (key_len == 3 && memcmp(key, "nbf", 3) == 0) {
            opcua_int64_t v = 0;
            int n = parse_json_int(json, json_len, value_start, &v);
            if (n > 0) {
                out->nbf = v;
                i = value_start + (size_t)n;
                continue;
            }
        } else if (key_len == 3 && memcmp(key, "iat", 3) == 0) {
            opcua_int64_t v = 0;
            int n = parse_json_int(json, json_len, value_start, &v);
            if (n > 0) {
                out->iat = v;
                i = value_start + (size_t)n;
                continue;
            }
        }

        /* Unknown key or value type we don't consume -- skip the value. */
        i = skip_value(json, json_len, value_start);
    }
}
