/* src/cu/core_2022_server/authorization/jwt.c
 *
 * JWT/JWS compact-form validator for the OPC UA User Token JWT Server Facet
 * (spec 093, OPC-10000-7 CU 1697). Validates the three-segment compact JWS:
 *
 *   <base64url(header)>.<base64url(payload)>.<base64url(signature)>
 *
 * The validator is freestanding: no heap allocation, no POSIX dependency, no
 * external JWT library. The signature is delegated to mu_crypto_jwt_verify
 * (crypto_jwt.c) which wraps the platform crypto backend.
 *
 * Spec grounding:
 *   RFC 7515 §5 -- JWS signature input is ASCII bytes of header.payload
 *   RFC 7519 §4 -- JWT compact serialization, registered claims
 *   RFC 7519 §4.1.4 -- exp claim, §4.1.5 -- nbf claim
 *   RFC 8725 §3 -- JWT BCP: enforce exp/nbf, dispatch by alg, reject "none"
 *   OPC-10000-4 §5.7.3 -- ActivateSession UserIdentityToken dispatch
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_CU_USER_TOKEN_JWT

#include "muc_opcua/authorization/jwt.h"
#include "muc_opcua/opcua_types.h"

#include "base64url.h"
#include "claim_scanner.h"
#include "crypto_jwt.h"

#include <stddef.h>
#include <string.h>

/* Upper bounds on decoded segment sizes. These bound the on-stack footprint
   of the validator. Real JWT payloads are typically <1 KB; 2 KB is a generous
   margin without being wasteful on embedded targets. */
#define MU_JWT_HEADER_MAX 128
#define MU_JWT_PAYLOAD_MAX 2048
#define MU_JWT_SIGNATURE_MAX 2048 /* RSA-4096 = 512, ECDSA P-521 = 132 */

/* Locate the next '.' in jwt starting at offset `start`. Returns the index of
   the dot, or jwt_len if none is found. */
static size_t find_dot(const char *jwt, size_t jwt_len, size_t start) {
    for (size_t i = start; i < jwt_len; i++) {
        if (jwt[i] == '.') {
            return i;
        }
    }
    return jwt_len;
}

/* Compare a NUL-terminated literal against a length-bounded byte string. */
static int eq_lit(const char *lit, const char *buf, size_t buf_len) {
    size_t lit_len = strlen(lit);
    return lit_len == buf_len && memcmp(lit, buf, lit_len) == 0;
}

/* Parse the JWS header (a JSON object) for the `alg` field. Returns the JWS
   algorithm string length and writes its start into *out_str; returns -1 if
   the alg field is absent or malformed. */
static int header_alg(const char *header, size_t header_len, const char **out_str) {
    /* Find "alg" key. Header is a flat JSON object; a simple linear scan is
       sufficient. */
    for (size_t i = 0; i + 5 < header_len; i++) {
        if (header[i] == '"' && header[i + 1] == 'a' && header[i + 2] == 'l' && header[i + 3] == 'g' &&
            header[i + 4] == '"') {
            /* Skip "alg", then optional whitespace, then ':'. */
            size_t j = i + 5;
            while (j < header_len && (header[j] == ' ' || header[j] == '\t')) {
                j++;
            }
            if (j >= header_len || header[j] != ':') {
                return -1;
            }
            j++;
            while (j < header_len && (header[j] == ' ' || header[j] == '\t')) {
                j++;
            }
            if (j >= header_len || header[j] != '"') {
                return -1;
            }
            j++;
            size_t val_start = j;
            while (j < header_len && header[j] != '"') {
                if (header[j] == '\\' && j + 1 < header_len) {
                    j += 2;
                } else {
                    j++;
                }
            }
            if (j >= header_len) {
                return -1;
            }
            *out_str = header + val_start;
            return (int)(j - val_start);
        }
    }
    return -1;
}

/* Map a JWS alg string to mu_jwt_alg_t. Returns MU_JWT_ALG_NONE for unknown or
   unsupported algorithms (including "none", which is rejected per RFC 8725). */
static mu_jwt_alg_t alg_from_str(const char *s, size_t len) {
    if (eq_lit("RS256", s, len)) {
        return MU_JWT_ALG_RS256;
    }
    if (eq_lit("RS384", s, len)) {
        return MU_JWT_ALG_RS384;
    }
    if (eq_lit("RS512", s, len)) {
        return MU_JWT_ALG_RS512;
    }
    if (eq_lit("ES256", s, len)) {
        return MU_JWT_ALG_ES256;
    }
    if (eq_lit("ES384", s, len)) {
        return MU_JWT_ALG_ES384;
    }
    if (eq_lit("ES512", s, len)) {
        return MU_JWT_ALG_ES512;
    }
    return MU_JWT_ALG_NONE;
}

/* Compare a NUL-terminated expected string against the iss claim. Returns 1 on
   match, 0 otherwise. */
static int issuer_url_match(const char *expected, const char *actual) {
    if (expected == NULL || actual == NULL) {
        return 0;
    }
    return strcmp(expected, actual) == 0;
}

/* Compare a NUL-terminated expected audience against the aud claim. */
static int audience_match(const char *expected, const char *actual) {
    if (expected == NULL) {
        /* No audience configured means we don't enforce. */
        return 1;
    }
    if (actual == NULL || actual[0] == '\0') {
        return 0;
    }
    return strcmp(expected, actual) == 0;
}

mu_jwt_result_t mu_jwt_validate(const char *jwt, size_t jwt_len, const mu_jwt_issuer_t *issuers,
                                opcua_byte_t issuer_count, opcua_int64_t server_time_unix,
                                mu_jwt_claims_t *out_claims) {
    if (jwt == NULL) {
        return MU_JWT_ERR_MALFORMED;
    }
    if (issuer_count == 0 || issuers == NULL) {
        return MU_JWT_ERR_NO_CONFIGURED_ISSUERS;
    }

    /* Split header.payload.signature on '.' */
    size_t dot1 = find_dot(jwt, jwt_len, 0);
    if (dot1 == jwt_len) {
        return MU_JWT_ERR_MALFORMED;
    }
    size_t dot2 = find_dot(jwt, jwt_len, dot1 + 1);
    if (dot2 == jwt_len) {
        return MU_JWT_ERR_MALFORMED;
    }
    /* No third dot allowed */
    if (find_dot(jwt, jwt_len, dot2 + 1) != jwt_len) {
        return MU_JWT_ERR_MALFORMED;
    }

    size_t header_b64_len = dot1;
    size_t payload_b64_len = dot2 - (dot1 + 1);
    size_t sig_b64_len = jwt_len - (dot2 + 1);
    if (header_b64_len == 0 || payload_b64_len == 0 || sig_b64_len == 0) {
        return MU_JWT_ERR_MALFORMED;
    }

    /* Decode header. */
    unsigned char header_buf[MU_JWT_HEADER_MAX];
    int header_len = mu_base64url_decode(jwt, header_b64_len, header_buf, sizeof(header_buf) - 1);
    if (header_len < 0) {
        return MU_JWT_ERR_BASE64;
    }
    header_buf[header_len] = '\0';
    const char *header_json = (const char *)header_buf;

    const char *alg_str = NULL;
    int alg_str_len = header_alg(header_json, (size_t)header_len, &alg_str);
    if (alg_str_len <= 0) {
        return MU_JWT_ERR_UNSUPPORTED_ALG;
    }
    mu_jwt_alg_t alg = alg_from_str(alg_str, (size_t)alg_str_len);
    if (alg == MU_JWT_ALG_NONE) {
        return MU_JWT_ERR_UNSUPPORTED_ALG;
    }

    /* Decode payload and extract claims. */
    unsigned char payload_buf[MU_JWT_PAYLOAD_MAX];
    int payload_len = mu_base64url_decode(jwt + dot1 + 1, payload_b64_len, payload_buf, sizeof(payload_buf) - 1);
    if (payload_len < 0) {
        return MU_JWT_ERR_BASE64;
    }
    payload_buf[payload_len] = '\0';

    mu_jwt_claims_t claims;
    memset(&claims, 0, sizeof(claims));
    mu_claim_scan((const char *)payload_buf, (size_t)payload_len, &claims);

    /* exp must be present and in the future (after clock skew). RFC 7519 §4.1.4
       and the spec edge cases: a missing exp is rejected. */
    if (claims.exp == 0) {
        return MU_JWT_ERR_EXPIRED;
    }

    /* Match issuer. */
    const mu_jwt_issuer_t *issuer = NULL;
    for (opcua_byte_t k = 0; k < issuer_count; k++) {
        if (issuer_url_match(issuers[k].issuer_url, claims.iss)) {
            issuer = &issuers[k];
            break;
        }
    }
    if (issuer == NULL) {
        return MU_JWT_ERR_ISSUER;
    }

    opcua_int64_t skew = (opcua_int64_t)issuer->clock_skew_seconds;

    /* exp check (RFC 7519 §4.1.4). */
    if (server_time_unix > claims.exp + skew) {
        return MU_JWT_ERR_EXPIRED;
    }

    /* nbf check (RFC 7519 §4.1.5). 0 means not present. */
    if (claims.nbf != 0 && (server_time_unix + skew) < claims.nbf) {
        return MU_JWT_ERR_NOT_YET_VALID;
    }

    /* Audience must match the configured value. */
    if (!audience_match(issuer->expected_audience, claims.aud)) {
        return MU_JWT_ERR_AUDIENCE;
    }

    /* Subject must be present and non-empty. */
    if (claims.sub[0] == '\0') {
        return MU_JWT_ERR_NO_SUB;
    }

    /* Algorithm must match the configured expectation for this issuer. */
    if (issuer->alg != alg) {
        return MU_JWT_ERR_UNSUPPORTED_ALG;
    }

    /* Decode signature and verify. The signing input is the ASCII bytes of
       header.payload (RFC 7515 §5.1) -- that is, the original base64url text
       before decoding, including the '.'. */
    unsigned char sig_buf[MU_JWT_SIGNATURE_MAX];
    int sig_len = mu_base64url_decode(jwt + dot2 + 1, sig_b64_len, sig_buf, sizeof(sig_buf));
    if (sig_len <= 0) {
        return MU_JWT_ERR_SIGNATURE;
    }

    opcua_boolean_t sig_ok = mu_crypto_jwt_verify((const opcua_byte_t *)jwt, dot2, sig_buf, (size_t)sig_len,
                                                  issuer->public_key, issuer->public_key_len, alg);
    if (!sig_ok) {
        return MU_JWT_ERR_SIGNATURE;
    }

    if (out_claims != NULL) {
        *out_claims = claims;
    }
    return MU_JWT_OK;
}

#endif /* MUC_OPCUA_CU_USER_TOKEN_JWT */
