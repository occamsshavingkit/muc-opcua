/* tests/unit/test_jwt.c
 *
 * Unit tests for the JWT/JWS validator (spec 093, tasks T012-T017).
 *
 * Generates an RSA-2048 keypair at setUp time with OpenSSL EVP, then constructs
 * signed JWTs in compact JWS form and asserts mu_jwt_validate() outcomes for the
 * spec.md acceptance scenarios (SC-001..SC-005) and the Edge Cases list.
 *
 * Spec grounding:
 *   OPC-10000-4 §5.7.3 -- ActivateSession user token dispatch
 *   OPC-10000-7 CU 1697 -- User Token JWT Server Facet
 *   RFC 7515 §5 -- JWS signature input is ASCII of header.payload
 *   RFC 7519 §4 -- JWT compact serialization, registered claims
 *   RFC 8725 §3 -- JWT BCP: enforce exp/nbf, dispatch by alg, reject "none"
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_CU_USER_TOKEN_JWT

#include "muc_opcua/authorization/jwt.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#if defined(MUC_OPCUA_HAVE_OPENSSL)
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Test fixture: a single RSA-2048 keypair shared across all tests.    */
/* ------------------------------------------------------------------ */

static EVP_PKEY *s_signing_key = NULL;      /* key the validator trusts (via DER) */
static EVP_PKEY *s_wrong_key = NULL;        /* a different key for sig-fail tests */
static unsigned char *s_trusted_der = NULL; /* DER SubjectPublicKeyInfo */
static size_t s_trusted_der_len = 0;

/* Base64url alphabet (RFC 4648 §5). No padding -- JWS uses no padding. */
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

/* Build the compact JWS form: base64url(header).base64url(payload).base64url(sig)
 * and write it (NUL-terminated) into out. Returns the length written. */
static size_t build_jwt(const char *header_json, const char *payload_json, EVP_PKEY *signer, char *out,
                        size_t out_max) {
    char header_b64[256];
    char payload_b64[1024];
    b64url_encode((const unsigned char *)header_json, strlen(header_json), header_b64);
    b64url_encode((const unsigned char *)payload_json, strlen(payload_json), payload_b64);

    /* signing input is the ASCII bytes of "header.payload" (RFC 7515 §5.1). */
    char signing_input[1280];
    int si_len = snprintf(signing_input, sizeof(signing_input), "%s.%s", header_b64, payload_b64);
    TEST_ASSERT_GREATER_THAN(0, si_len);
    TEST_ASSERT_TRUE((size_t)si_len < sizeof(signing_input));

    /* sign with RSA PKCS#1 v1.5 + SHA-256 (RS256). */
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    TEST_ASSERT_NOT_NULL(mdctx);
    TEST_ASSERT_EQUAL(1, EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, signer));

    size_t sig_len = 0;
    TEST_ASSERT_EQUAL(1, EVP_DigestSign(mdctx, NULL, &sig_len, (const unsigned char *)signing_input, (size_t)si_len));
    TEST_ASSERT_GREATER_THAN(0, sig_len);

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

/* Default payload claims for the "valid" JWT. */
#define TEST_ISSUER "https://auth.example.com"
#define TEST_AUDIENCE "opcua-server"
#define TEST_SUBJECT "operator1"
#define TEST_NOW (1700000000LL)    /* fixed test clock: Tue 2023-11-14 */
#define TEST_EXP (TEST_NOW + 3600) /* +1h */
#define TEST_IAT (TEST_NOW)
#define TEST_NBF (TEST_NOW)

/* Build the standard valid issuer record (RS256). */
static void make_trusted_issuer(mu_jwt_issuer_t *out) {
    memset(out, 0, sizeof(*out));
    out->issuer_url = TEST_ISSUER;
    out->public_key = s_trusted_der;
    out->public_key_len = s_trusted_der_len;
    out->expected_audience = TEST_AUDIENCE;
    out->clock_skew_seconds = 0;
    out->alg = MU_JWT_ALG_RS256;
}

static char s_valid_header[] = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";

/* Build a "default valid" payload using the test clock. */
static void make_payload_exp_aud_sub(char *buf, size_t buf_max, opcua_int64_t exp, opcua_int64_t nbf, const char *iss,
                                     const char *aud, const char *sub) {
    int n;
    if (nbf > 0) {
        n = snprintf(buf, buf_max,
                     "{\"iss\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"exp\":%lld,\"iat\":%lld,\"nbf\":%lld}", iss, sub,
                     aud, (long long)exp, (long long)TEST_IAT, (long long)nbf);
    } else {
        n = snprintf(buf, buf_max, "{\"iss\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"exp\":%lld,\"iat\":%lld}", iss, sub,
                     aud, (long long)exp, (long long)TEST_IAT);
    }
    TEST_ASSERT_TRUE((size_t)n < buf_max);
    (void)n;
}

void setUp(void) {
    /* Generate the RSA-2048 keypairs once per test run (setUp fires per-test,
     * but generation is fast enough; cache the trusted DER globally). */
    if (s_signing_key == NULL) {
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        TEST_ASSERT_NOT_NULL(pctx);
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen_init(pctx));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen(pctx, &s_signing_key));
        EVP_PKEY_CTX_free(pctx);
    }
    if (s_wrong_key == NULL) {
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        TEST_ASSERT_NOT_NULL(pctx);
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen_init(pctx));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 512));
        TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen(pctx, &s_wrong_key));
        EVP_PKEY_CTX_free(pctx);
    }
    if (s_trusted_der == NULL) {
        unsigned char *tmp = NULL;
        int len = i2d_PUBKEY(s_signing_key, &tmp);
        TEST_ASSERT_GREATER_THAN(0, len);
        s_trusted_der = tmp; /* OPENSSL_malloc-allocated; leaked: process exits */
        s_trusted_der_len = (size_t)len;
    }
}

void tearDown(void) {
    /* keys + DER persist across tests; no per-test teardown needed. */
}

/* ================================================================== */
/* T012 -- Valid RS256 token -> MU_JWT_OK, claims correctly extracted. */
/* ================================================================== */
void test_jwt_valid_rs256_token_is_accepted_and_claims_extracted(void) {
    char payload[512];
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_EXP, 0, TEST_ISSUER, TEST_AUDIENCE, TEST_SUBJECT);

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_OK, r);
    TEST_ASSERT_EQUAL_STRING(TEST_SUBJECT, claims.sub);
    TEST_ASSERT_EQUAL_STRING(TEST_ISSUER, claims.iss);
    TEST_ASSERT_EQUAL_STRING(TEST_AUDIENCE, claims.aud);
    TEST_ASSERT_EQUAL(TEST_EXP, claims.exp);
    TEST_ASSERT_EQUAL(TEST_IAT, claims.iat);
}

/* ================================================================== */
/* T013 -- Expired token -> MU_JWT_ERR_EXPIRED.                       */
/* ================================================================== */
void test_jwt_expired_token_is_rejected(void) {
    char payload[512];
    /* exp 1 hour BEFORE the test clock. */
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_NOW - 3600, 0, TEST_ISSUER, TEST_AUDIENCE, TEST_SUBJECT);

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_EXPIRED, r);
}

/* ================================================================== */
/* T014 -- Wrong signing key -> MU_JWT_ERR_SIGNATURE.                 */
/* ================================================================== */
void test_jwt_signed_with_wrong_key_is_rejected(void) {
    char payload[512];
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_EXP, 0, TEST_ISSUER, TEST_AUDIENCE, TEST_SUBJECT);

    /* Sign with s_wrong_key, but validator trusts s_signing_key. */
    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_wrong_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_SIGNATURE, r);
}

/* ================================================================== */
/* T015 -- Wrong issuer / wrong audience / missing sub.               */
/* ================================================================== */
void test_jwt_wrong_issuer_is_rejected(void) {
    char payload[512];
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_EXP, 0, "https://wrong.example.com", TEST_AUDIENCE,
                             TEST_SUBJECT);

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_ISSUER, r);
}

void test_jwt_wrong_audience_is_rejected(void) {
    char payload[512];
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_EXP, 0, TEST_ISSUER, "wrong-audience", TEST_SUBJECT);

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_AUDIENCE, r);
}

void test_jwt_missing_sub_is_rejected(void) {
    char payload[512];
    /* Payload has no "sub" field. */
    int n = snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"aud\":\"%s\",\"exp\":%lld,\"iat\":%lld}", TEST_ISSUER,
                     TEST_AUDIENCE, (long long)TEST_EXP, (long long)TEST_IAT);
    TEST_ASSERT_TRUE((size_t)n < sizeof(payload));
    (void)n;

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_NO_SUB, r);
}

/* ================================================================== */
/* T016 -- Malformed / bad Base64 / unsupported alg.                  */
/* ================================================================== */
void test_jwt_not_three_segments_is_rejected(void) {
    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    /* Two segments only. */
    const char *two = "aaa.bbb";
    TEST_ASSERT_EQUAL(MU_JWT_ERR_MALFORMED, mu_jwt_validate(two, strlen(two), &issuer, 1, TEST_NOW, &claims));
    /* Four segments. */
    const char *four = "aaa.bbb.ccc.ddd";
    TEST_ASSERT_EQUAL(MU_JWT_ERR_MALFORMED, mu_jwt_validate(four, strlen(four), &issuer, 1, TEST_NOW, &claims));
    /* Empty. */
    TEST_ASSERT_EQUAL(MU_JWT_ERR_MALFORMED, mu_jwt_validate("", 0, &issuer, 1, TEST_NOW, &claims));
}

void test_jwt_bad_base64_is_rejected(void) {
    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    /* header.payload.signature shape, but invalid base64 chars in payload. */
    const char *bad = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.****badbase64****.aaa";
    TEST_ASSERT_EQUAL(MU_JWT_ERR_BASE64, mu_jwt_validate(bad, strlen(bad), &issuer, 1, TEST_NOW, &claims));
}

void test_jwt_unsupported_alg_is_rejected(void) {
    /* Sign an HS256-shaped token -- we can't actually HS256-sign it with this
     * fixture (no shared HMAC key plumbing), but mu_jwt_validate should reject
     * "HS256" in the header before reaching signature verification. */
    const char *header_hs256 = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    char payload[512];
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_EXP, 0, TEST_ISSUER, TEST_AUDIENCE, TEST_SUBJECT);

    char header_b64[256];
    char payload_b64[1024];
    b64url_encode((const unsigned char *)header_hs256, strlen(header_hs256), header_b64);
    b64url_encode((const unsigned char *)payload, strlen(payload), payload_b64);

    /* Append a dummy signature -- validator should reject on alg, not sig. */
    char jwt[1600];
    int n = snprintf(jwt, sizeof(jwt), "%s.%s.%s", header_b64, payload_b64, "AAAA");
    TEST_ASSERT_TRUE((size_t)n < sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, (size_t)n, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_UNSUPPORTED_ALG, r);
}

/* ================================================================== */
/* T017 -- nbf in future / no exp / no configured issuers.            */
/* ================================================================== */
void test_jwt_nbf_in_future_is_rejected(void) {
    char payload[512];
    /* nbf 1 hour AFTER the test clock -- still pending. */
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_EXP, TEST_NOW + 3600, TEST_ISSUER, TEST_AUDIENCE,
                             TEST_SUBJECT);

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_NOT_YET_VALID, r);
}

void test_jwt_no_exp_is_rejected(void) {
    char payload[512];
    /* exp missing entirely. */
    int n = snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"iat\":%lld}",
                     TEST_ISSUER, TEST_SUBJECT, TEST_AUDIENCE, (long long)TEST_IAT);
    TEST_ASSERT_TRUE((size_t)n < sizeof(payload));
    (void)n;

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_trusted_issuer(&issuer);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_EXPIRED, r);
}

void test_jwt_no_configured_issuers_is_rejected(void) {
    /* issuer_count == 0 must short-circuit regardless of token shape, even if
     * the token is otherwise valid. */
    char payload[512];
    make_payload_exp_aud_sub(payload, sizeof(payload), TEST_EXP, 0, TEST_ISSUER, TEST_AUDIENCE, TEST_SUBJECT);

    char jwt[1600];
    size_t jwt_len = build_jwt(s_valid_header, payload, s_signing_key, jwt, sizeof(jwt));

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, NULL, 0, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_NO_CONFIGURED_ISSUERS, r);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_jwt_valid_rs256_token_is_accepted_and_claims_extracted);
    RUN_TEST(test_jwt_expired_token_is_rejected);
    RUN_TEST(test_jwt_signed_with_wrong_key_is_rejected);
    RUN_TEST(test_jwt_wrong_issuer_is_rejected);
    RUN_TEST(test_jwt_wrong_audience_is_rejected);
    RUN_TEST(test_jwt_missing_sub_is_rejected);
    RUN_TEST(test_jwt_not_three_segments_is_rejected);
    RUN_TEST(test_jwt_bad_base64_is_rejected);
    RUN_TEST(test_jwt_unsupported_alg_is_rejected);
    RUN_TEST(test_jwt_nbf_in_future_is_rejected);
    RUN_TEST(test_jwt_no_exp_is_rejected);
    RUN_TEST(test_jwt_no_configured_issuers_is_rejected);
    return UNITY_END();
}

#else /* !MUC_OPCUA_HAVE_OPENSSL */

/* Stub main so the test target links on backends without OpenSSL -- the
 * encoder/decoder contract is exercised by the OpenSSL build. */
int main(void) {
    return 0;
}

#endif /* MUC_OPCUA_HAVE_OPENSSL */

#else /* !MUC_OPCUA_CU_USER_TOKEN_JWT */

int main(void) {
    return 0;
}

#endif /* MUC_OPCUA_CU_USER_TOKEN_JWT */
