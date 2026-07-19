/* src/cu/core_2022_server/authorization/claim_scanner.c
 *
 * Minimal streaming JSON key-value scanner for the JWT payload (RFC 7519 §4).
 * Freestanding C11, no heap. The scanner recognises a flat JSON object only;
 * nested values are skipped defensively.
 */
#include "claim_scanner.h"

#include <stddef.h>
#include <string.h>

/* Copy a quoted JSON string value into dst, applying \" and \\ unescaping.
   Returns bytes consumed including closing quote, or -1 on malformed input. */
static int copy_json_string(const char *src, size_t src_len, size_t start, char *dst, size_t dst_size, int *overflow) {
    size_t i = start;
    size_t out = 0;
    if (overflow != NULL) {
        *overflow = 0;
    }
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
            } else if (esc == 'n') {
                c = '\n';
            } else if (esc == 'r') {
                c = '\r';
            } else if (esc == 't') {
                c = '\t';
            } else {
                c = esc;
            }
        }
        if (dst != NULL && out + 1 < dst_size) {
            dst[out] = c;
        } else if (overflow != NULL) {
            *overflow = 1;
        }
        out++;
    }
    return -1;
}

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
        if (value > 9999999999LL) {
            while (i < src_len && src[i] >= '0' && src[i] <= '9') {
                i++;
            }
            break;
        }
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

static size_t skip_ws(const char *src, size_t src_len, size_t start) {
    size_t i = start;
    while (i < src_len && (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' || src[i] == '\r')) {
        i++;
    }
    return i;
}

static size_t skip_quoted(const char *src, size_t src_len, size_t i) {
    i++;
    while (i < src_len) {
        char x = src[i++];
        if (x == '\\' && i < src_len) {
            i++;
        } else if (x == '"') {
            break;
        }
    }
    return i;
}

static size_t skip_value(const char *src, size_t src_len, size_t start) {
    size_t i = start;
    if (i >= src_len) {
        return i;
    }
    char c = src[i];
    if (c == '"') {
        return skip_quoted(src, src_len, i);
    }
    if (c == '[' || c == '{') {
        char close_ch = (c == '[') ? ']' : '}';
        int depth = 1;
        i++;
        while (i < src_len && depth > 0) {
            char x = src[i++];
            if (x == '"') {
                i = skip_quoted(src, src_len, i - 1);
            } else if (x == c) {
                depth++;
            } else if (x == close_ch) {
                depth--;
            }
        }
        return i;
    }
    while (i < src_len && src[i] != ',' && src[i] != '}' && src[i] != ']' && src[i] != ' ' && src[i] != '\t' &&
           src[i] != '\n' && src[i] != '\r') {
        i++;
    }
    return i;
}

/* Extract a string claim value. Returns new stream position, or src_len on error. */
static size_t handle_string_claim(const char *json, size_t json_len, size_t value_start, char *dst, size_t dst_size,
                                  int reject_overflow) {
    if (json[value_start] != '"') {
        return skip_value(json, json_len, value_start);
    }
    int overflow = 0;
    int n = copy_json_string(json, json_len, value_start + 1, dst, dst_size, &overflow);
    if (n < 0) {
        return json_len;
    }
    if (reject_overflow && overflow) {
        dst[0] = '\0';
    }
    return (size_t)n;
}

/* Extract an integer claim value. Returns new stream position. */
static size_t handle_int_claim(const char *json, size_t json_len, size_t value_start, opcua_int64_t *out) {
    opcua_int64_t v = 0;
    int n = parse_json_int(json, json_len, value_start, &v);
    if (n > 0) {
        *out = v;
        return value_start + (size_t)n;
    }
    return skip_value(json, json_len, value_start);
}

/* Dispatch a recognized 3-char claim key to the right handler. Returns true if
   the key was recognized (regardless of whether the value was consumed). */
static int dispatch_claim(const char *key, const char *json, size_t json_len, size_t value_start, mu_jwt_claims_t *out,
                          size_t *new_pos) {
    if (key[0] == 'i' && key[1] == 's' && key[2] == 's') {
        *new_pos = handle_string_claim(json, json_len, value_start, out->iss, sizeof(out->iss), 0);
        return 1;
    }
    if (key[0] == 's' && key[1] == 'u' && key[2] == 'b') {
        *new_pos = handle_string_claim(json, json_len, value_start, out->sub, sizeof(out->sub), 1);
        return 1;
    }
    if (key[0] == 'a' && key[1] == 'u' && key[2] == 'd') {
        *new_pos = handle_string_claim(json, json_len, value_start, out->aud, sizeof(out->aud), 0);
        if (json[value_start] == '[') {
            size_t arr = skip_ws(json, json_len, value_start + 1);
            if (arr < json_len && json[arr] == '"') {
                (void)copy_json_string(json, json_len, arr + 1, out->aud, sizeof(out->aud), NULL);
            }
            *new_pos = skip_value(json, json_len, value_start);
        }
        return 1;
    }
    if (key[0] == 'e' && key[1] == 'x' && key[2] == 'p') {
        *new_pos = handle_int_claim(json, json_len, value_start, &out->exp);
        return 1;
    }
    if (key[0] == 'n' && key[1] == 'b' && key[2] == 'f') {
        *new_pos = handle_int_claim(json, json_len, value_start, &out->nbf);
        return 1;
    }
    if (key[0] == 'i' && key[1] == 'a' && key[2] == 't') {
        *new_pos = handle_int_claim(json, json_len, value_start, &out->iat);
        return 1;
    }
    return 0;
}

void mu_claim_scan(const char *json, size_t json_len, mu_jwt_claims_t *out) {
    if (json == NULL || out == NULL) {
        return;
    }
    int depth = 0;
    size_t i = 0;
    while (i < json_len) {
        if (json[i] == '{') {
            depth++;
            i++;
            continue;
        }
        if (json[i] == '}') {
            depth--;
            i++;
            continue;
        }
        if (json[i] != '"') {
            i++;
            continue;
        }

        /* Read the key */
        size_t key_start = i + 1;
        size_t key_end = skip_quoted(json, json_len, i);
        if (key_end > json_len) {
            return;
        }
        size_t key_len = key_end - 1 - key_start;

        size_t colon = skip_ws(json, json_len, key_end);
        if (colon >= json_len || json[colon] != ':') {
            i = key_end;
            continue;
        }
        size_t value_start = skip_ws(json, json_len, colon + 1);
        if (value_start >= json_len) {
            return;
        }

        if (depth != 1) {
            i = skip_value(json, json_len, value_start);
            continue;
        }

        if (key_len == 3) {
            size_t new_pos = value_start;
            if (dispatch_claim(json + key_start, json, json_len, value_start, out, &new_pos)) {
                i = new_pos;
                continue;
            }
        }

        i = skip_value(json, json_len, value_start);
    }
}
