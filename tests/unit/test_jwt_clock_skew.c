/* tests/unit/test_jwt_clock_skew.c
 *
 * Spec 093 Task T022 -- User Story 2: per-issuer clock skew tolerance.
 *
 * Configures a single issuer with a 30-second clock-skew tolerance, then
 * probes the boundary:
 *   - token `exp` exactly `now`                       -> accepted (still valid)
 *   - token `exp` = now + skew (30 s past)            -> accepted (within skew)
 *   - token `exp` = now + skew + 1 (31 s past)        -> rejected MU_JWT_ERR_EXPIRED
 *   - token `nbf` = now + skew (30 s in future)       -> accepted (within skew)
 *   - token `nbf` = now + skew + 1 (31 s in future)   -> rejected MU_JWT_ERR_NOT_YET_VALID
 *   - issuer with skew=0 rejects a 1-second-past-exp  -> MU_JWT_ERR_EXPIRED
 *
 * Spec grounding:
 *   spec.md FR-005 -- per-issuer clock_skew_seconds.
 *   spec.md Assumptions (last bullet) -- integrator-owned clock sync.
 *   RFC 7519 §4.1.4 (exp), §4.1.5 (nbf).
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_CU_USER_TOKEN_JWT

#include "muc_opcua/authorization/jwt.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#if defined(MUC_OPCUA_HAVE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char s_b64url_tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void b64url_encode(const unsigned char *in, size_t in_len, char *out) {
    size_t i = 0;
    size_t o = 0;
    while (i + 2 < in_len) {
        unsigned int v = ((unsigned int)in[i] << 16) | ((unsigned int)in[i + 1] << 8) | (unsigned int)in[i + 2];
        out[o++] = s_b64url_tab[(v >> 18) & 0x3F];
        out[o++] = s_b64url_tab[(v >> 12) & 0x3F];
        out[o++] = s_b64url_tab[(v >> 6) & 0x3F];
        out[o++] = s_b64url_tab[v & 0x3F];
        i += 3;
    }
    if (i < in_len) {
        unsigned int v = (unsigned int)in[i] << 16;
        if (i + 1 < in_len) {
            v |= (unsigned int)in[i + 1] << 8;
        }
        out[o++] = s_b64url_tab[(v >> 18) & 0x3F];
        out[o++] = s_b64url_tab[(v >> 12) & 0x3F];
        if (i + 1 < in_len) {
            out[o++] = s_b64url_tab[(v >> 6) & 0x3F];
        }
    }
    out[o] = '\0';
}

static size_t build_jwt_rs256(const char *header_json, const char *payload_json, EVP_PKEY *signer, char *out,
                              size_t out_max) {
    char header_b64[256];
    char payload_b64[1024];
    b64url_encode((const unsigned char *)header_json, strlen(header_json), header_b64);
    b64url_encode((const unsigned char *)payload_json, strlen(payload_json), payload_b64);

    char signing_input[1280];
    int si_len = snprintf(signing_input, sizeof(signing_input), "%s.%s", header_b64, payload_b64);
    TEST_ASSERT_GREATER_THAN(0, si_len);
    TEST_ASSERT_TRUE((size_t)si_len < sizeof(signing_input));

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    TEST_ASSERT_NOT_NULL(mdctx);
    TEST_ASSERT_EQUAL(1, EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, signer));

    size_t sig_len = 0;
    TEST_ASSERT_EQUAL(1, EVP_DigestSign(mdctx, NULL, &sig_len, (const unsigned char *)signing_input, (size_t)si_len));
    unsigned char *sig = (unsigned char *)malloc(sig_len);
    TEST_ASSERT_NOT_NULL(sig);
    TEST_ASSERT_EQUAL(1, EVP_DigestSign(mdctx, sig, &sig_len, (const unsigned char *)signing_input, (size_t)si_len));
    EVP_MD_CTX_free(mdctx);

    char sig_b64[1024];
    b64url_encode(sig, sig_len, sig_b64);
    free(sig);

    int total = snprintf(out, out_max, "%s.%s", signing_input, sig_b64);
    TEST_ASSERT_TRUE((size_t)total < out_max);
    return (size_t)total;
}

static EVP_PKEY *s_key = NULL;
static unsigned char *s_der = NULL;
static size_t s_der_len = 0;

#define TEST_NOW (1700000000LL)
#define SKEW_SECONDS (30)

static const char s_header[] = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";

static void make_payload_exp_nbf(char *buf, size_t buf_max, opcua_int64_t exp, opcua_int64_t nbf) {
    int n;
    if (nbf > 0) {
        n = snprintf(buf, buf_max,
                     "{\"iss\":\"https://auth.example.com\",\"sub\":\"operator1\",\"aud\":\"opcua-server\","
                     "\"exp\":%lld,\"iat\":%lld,\"nbf\":%lld}",
                     (long long)exp, (long long)TEST_NOW, (long long)nbf);
    } else {
        n = snprintf(buf, buf_max,
                     "{\"iss\":\"https://auth.example.com\",\"sub\":\"operator1\",\"aud\":\"opcua-server\","
                     "\"exp\":%lld,\"iat\":%lld}",
                     (long long)exp, (long long)TEST_NOW);
    }
    TEST_ASSERT_TRUE((size_t)n < buf_max);
    (void)n;
}

static void make_issuer_with_skew(mu_jwt_issuer_t *out, opcua_uint32_t skew) {
    memset(out, 0, sizeof(*out));
    out->issuer_url = "https://auth.example.com";
    out->public_key = s_der;
    out->public_key_len = s_der_len;
    out->expected_audience = "opcua-server";
    out->clock_skew_seconds = skew;
    out->alg = MU_JWT_ALG_RS256;
}

void setUp(void) {
    if (s_key == NULL) {
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        TEST_ASSERT_NOT_NULL(pctx);
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen_init(pctx));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen(pctx, &s_key));
        EVP_PKEY_CTX_free(pctx);
    }
    if (s_der == NULL) {
        unsigned char *tmp = NULL;
        int len = i2d_PUBKEY(s_key, &tmp);
        TEST_ASSERT_GREATER_THAN(0, len);
        s_der = tmp;
        s_der_len = (size_t)len;
    }
}

void tearDown(void) {}

/* exp == now: token is still valid (within bound). */
void test_skew_exp_equal_now_is_accepted(void) {
    char payload[512];
    make_payload_exp_nbf(payload, sizeof(payload), TEST_NOW, 0);

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer_with_skew(&issuer, SKEW_SECONDS);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* exp = now - skew: exactly at the skew boundary, still accepted. */
void test_skew_exp_at_boundary_is_accepted(void) {
    char payload[512];
    make_payload_exp_nbf(payload, sizeof(payload), TEST_NOW - SKEW_SECONDS, 0);

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer_with_skew(&issuer, SKEW_SECONDS);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* exp = now - skew - 1: 1 second past the skew window, rejected. */
void test_skew_exp_one_second_past_boundary_is_rejected(void) {
    char payload[512];
    make_payload_exp_nbf(payload, sizeof(payload), TEST_NOW - SKEW_SECONDS - 1, 0);

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer_with_skew(&issuer, SKEW_SECONDS);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_EXPIRED, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* nbf = now + skew: exactly at the skew boundary, accepted. */
void test_skew_nbf_at_boundary_is_accepted(void) {
    char payload[512];
    make_payload_exp_nbf(payload, sizeof(payload), TEST_NOW + 3600, TEST_NOW + SKEW_SECONDS);

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer_with_skew(&issuer, SKEW_SECONDS);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* nbf = now + skew + 1: 1 second beyond the skew window, rejected. */
void test_skew_nbf_one_second_past_boundary_is_rejected(void) {
    char payload[512];
    make_payload_exp_nbf(payload, sizeof(payload), TEST_NOW + 3600, TEST_NOW + SKEW_SECONDS + 1);

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer_with_skew(&issuer, SKEW_SECONDS);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_NOT_YET_VALID, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* skew = 0 + 1-second-past-exp -> rejected: proves skew is per-issuer, not a
 * global default. */
void test_skew_zero_skew_rejects_one_second_past_exp(void) {
    char payload[512];
    make_payload_exp_nbf(payload, sizeof(payload), TEST_NOW - 1, 0);

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer_with_skew(&issuer, 0);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_EXPIRED, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_skew_exp_equal_now_is_accepted);
    RUN_TEST(test_skew_exp_at_boundary_is_accepted);
    RUN_TEST(test_skew_exp_one_second_past_boundary_is_rejected);
    RUN_TEST(test_skew_nbf_at_boundary_is_accepted);
    RUN_TEST(test_skew_nbf_one_second_past_boundary_is_rejected);
    RUN_TEST(test_skew_zero_skew_rejects_one_second_past_exp);
    return UNITY_END();
}

#else /* !MUC_OPCUA_HAVE_OPENSSL */

int main(void) {
    return 0;
}

#endif /* MUC_OPCUA_HAVE_OPENSSL */

#else /* !MUC_OPCUA_CU_USER_TOKEN_JWT */

int main(void) {
    return 0;
}

#endif /* MUC_OPCUA_CU_USER_TOKEN_JWT */
