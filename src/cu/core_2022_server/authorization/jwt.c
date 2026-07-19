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

#define MU_JWT_HEADER_MAX 128
#define MU_JWT_PAYLOAD_MAX 2048
#define MU_JWT_SIGNATURE_MAX 2048

/* Locate the next '.' in jwt starting at offset `start`. */
static size_t find_dot(const char *jwt, size_t jwt_len, size_t start) {
    for (size_t i = start; i < jwt_len; i++) {
        if (jwt[i] == '.') {
            return i;
        }
    }
    return jwt_len;
}

static int eq_lit(const char *lit, const char *buf, size_t buf_len) {
    size_t lit_len = strlen(lit);
    return lit_len == buf_len && memcmp(lit, buf, lit_len) == 0;
}

/* ---- Segment splitting (Stage 1) ---- */

typedef struct {
    size_t dot1;
    size_t dot2;
    size_t header_b64_len;
    size_t payload_b64_len;
    size_t sig_b64_len;
} jwt_segments_t;

static mu_jwt_result_t split_segments(const char *jwt, size_t jwt_len, jwt_segments_t *seg) {
    seg->dot1 = find_dot(jwt, jwt_len, 0);
    if (seg->dot1 == jwt_len) {
        return MU_JWT_ERR_MALFORMED;
    }
    seg->dot2 = find_dot(jwt, jwt_len, seg->dot1 + 1);
    if (seg->dot2 == jwt_len) {
        return MU_JWT_ERR_MALFORMED;
    }
    if (find_dot(jwt, jwt_len, seg->dot2 + 1) != jwt_len) {
        return MU_JWT_ERR_MALFORMED;
    }
    seg->header_b64_len = seg->dot1;
    seg->payload_b64_len = seg->dot2 - (seg->dot1 + 1);
    seg->sig_b64_len = jwt_len - (seg->dot2 + 1);
    if (seg->header_b64_len == 0 || seg->payload_b64_len == 0 || seg->sig_b64_len == 0) {
        return MU_JWT_ERR_MALFORMED;
    }
    return MU_JWT_OK;
}

/* ---- Header parsing (Stage 2) ---- */

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

static mu_jwt_result_t decode_and_check_alg(const char *jwt, size_t header_b64_len, mu_jwt_alg_t *out_alg) {
    unsigned char header_buf[MU_JWT_HEADER_MAX];
    int header_len = mu_base64url_decode(jwt, header_b64_len, header_buf, sizeof(header_buf) - 1);
    if (header_len < 0) {
        return MU_JWT_ERR_BASE64;
    }
    header_buf[header_len] = '\0';

    /* Find "alg":"XXX" in the flat header JSON. */
    const char *hdr = (const char *)header_buf;
    const char *alg_str = NULL;
    size_t alg_len = 0;
    for (size_t i = 0; i + 6 < (size_t)header_len; i++) {
        if (hdr[i] == '"' && hdr[i + 1] == 'a' && hdr[i + 2] == 'l' && hdr[i + 3] == 'g' && hdr[i + 4] == '"') {
            size_t j = i + 5;
            while (j < (size_t)header_len && (hdr[j] == ' ' || hdr[j] == '\t')) {
                j++;
            }
            if (j >= (size_t)header_len || hdr[j] != ':') {
                break;
            }
            j++;
            while (j < (size_t)header_len && (hdr[j] == ' ' || hdr[j] == '\t')) {
                j++;
            }
            if (j >= (size_t)header_len || hdr[j] != '"') {
                break;
            }
            j++;
            alg_str = hdr + j;
            while (j < (size_t)header_len && hdr[j] != '"') {
                j++;
            }
            alg_len = j - (size_t)(alg_str - hdr);
            break;
        }
    }
    if (alg_str == NULL || alg_len == 0) {
        return MU_JWT_ERR_UNSUPPORTED_ALG;
    }
    *out_alg = alg_from_str(alg_str, alg_len);
    if (*out_alg == MU_JWT_ALG_NONE) {
        return MU_JWT_ERR_UNSUPPORTED_ALG;
    }
    return MU_JWT_OK;
}

/* ---- Claim checks (Stage 3) ---- */

static int issuer_url_match(const char *expected, const char *actual) {
    if (expected == NULL || actual == NULL) {
        return 0;
    }
    return strcmp(expected, actual) == 0;
}

static int audience_match(const char *expected, const char *actual) {
    if (expected == NULL || expected[0] == '\0') {
        return 0;
    }
    if (actual == NULL || actual[0] == '\0') {
        return 0;
    }
    return strcmp(expected, actual) == 0;
}

/* Validate claims + signature against one issuer. Returns MU_JWT_OK on
   success, or an error code. Does not check issuer URL (caller pre-filters). */
static mu_jwt_result_t validate_against_issuer(const mu_jwt_issuer_t *issuer, const mu_jwt_claims_t *claims,
                                               mu_jwt_alg_t alg, opcua_int64_t server_time_unix, const char *jwt,
                                               const jwt_segments_t *seg, mu_jwt_claims_t *out_claims) {
    opcua_int64_t skew = (opcua_int64_t)issuer->clock_skew_seconds;

    if (server_time_unix > claims->exp + skew) {
        return MU_JWT_ERR_EXPIRED;
    }
    if (claims->nbf != 0 && (server_time_unix + skew) < claims->nbf) {
        return MU_JWT_ERR_NOT_YET_VALID;
    }
    if (!audience_match(issuer->expected_audience, claims->aud)) {
        return MU_JWT_ERR_AUDIENCE;
    }
    if (claims->sub[0] == '\0') {
        return MU_JWT_ERR_NO_SUB;
    }
    if (issuer->alg != alg) {
        return MU_JWT_ERR_UNSUPPORTED_ALG;
    }

    unsigned char sig_buf[MU_JWT_SIGNATURE_MAX];
    int sig_len = mu_base64url_decode(jwt + seg->dot2 + 1, seg->sig_b64_len, sig_buf, sizeof(sig_buf));
    if (sig_len <= 0) {
        return MU_JWT_ERR_SIGNATURE;
    }

    if (!mu_crypto_jwt_verify((const opcua_byte_t *)jwt, seg->dot2, sig_buf, (size_t)sig_len, issuer->public_key,
                              issuer->public_key_len, alg)) {
        return MU_JWT_ERR_SIGNATURE;
    }

    if (out_claims != NULL) {
        *out_claims = *claims;
    }
    return MU_JWT_OK;
}

/* ---- Main entry point (thin orchestrator) ---- */

mu_jwt_result_t mu_jwt_validate(const char *jwt, size_t jwt_len, const mu_jwt_issuer_t *issuers,
                                opcua_byte_t issuer_count, opcua_int64_t server_time_unix,
                                mu_jwt_claims_t *out_claims) {
    if (jwt == NULL) {
        return MU_JWT_ERR_MALFORMED;
    }
    if (issuer_count == 0 || issuers == NULL) {
        return MU_JWT_ERR_NO_CONFIGURED_ISSUERS;
    }

    jwt_segments_t seg;
    mu_jwt_result_t r = split_segments(jwt, jwt_len, &seg);
    if (r != MU_JWT_OK) {
        return r;
    }

    mu_jwt_alg_t alg;
    r = decode_and_check_alg(jwt, seg.header_b64_len, &alg);
    if (r != MU_JWT_OK) {
        return r;
    }

    unsigned char payload_buf[MU_JWT_PAYLOAD_MAX];
    int payload_len =
        mu_base64url_decode(jwt + seg.dot1 + 1, seg.payload_b64_len, payload_buf, sizeof(payload_buf) - 1);
    if (payload_len < 0) {
        return MU_JWT_ERR_BASE64;
    }
    payload_buf[payload_len] = '\0';

    mu_jwt_claims_t claims;
    memset(&claims, 0, sizeof(claims));
    mu_claim_scan((const char *)payload_buf, (size_t)payload_len, &claims);

    if (claims.exp == 0) {
        return MU_JWT_ERR_EXPIRED;
    }

    bool any_issuer_matched = false;
    for (opcua_byte_t k = 0; k < issuer_count; k++) {
        if (!issuer_url_match(issuers[k].issuer_url, claims.iss)) {
            continue;
        }
        any_issuer_matched = true;
        r = validate_against_issuer(&issuers[k], &claims, alg, server_time_unix, jwt, &seg, out_claims);
        if (r == MU_JWT_OK) {
            return MU_JWT_OK;
        }
        /* Non-signature errors are definitive for this issuer URL. */
        if (r != MU_JWT_ERR_SIGNATURE) {
            return r;
        }
    }

    if (!any_issuer_matched) {
        return MU_JWT_ERR_ISSUER;
    }
    return MU_JWT_ERR_SIGNATURE;
}

#endif /* MUC_OPCUA_CU_USER_TOKEN_JWT */
