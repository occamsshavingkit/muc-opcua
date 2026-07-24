/* tests/unit/test_jwt_multi_issuer.c
 *
 * Spec 093 Task T021 -- User Story 2: Multi-issuer configuration.
 *
 * Configures two distinct trusted issuers (different RSA keypairs, different
 * issuer URLs, distinct audiences) and asserts:
 *   - A JWT signed by issuer A with A's audience is accepted.
 *   - A JWT signed by issuer B with B's audience is accepted.
 *   - A JWT signed by A but carrying B's audience is rejected with
 *     MU_JWT_ERR_AUDIENCE (cross-issuer audience mismatch, per spec.md US2
 *     Acceptance Scenario 2).
 *   - A JWT whose `iss` matches neither configured URL is rejected with
 *     MU_JWT_ERR_ISSUER.
 *
 * Spec grounding:
 *   spec.md US2 Acceptance Scenarios 1 & 2.
 *   OPC-10000-7 CU 1629 -- Authorization Service Server Facet.
 *   RFC 7519 §4.1.1 -- iss claim, §4.1.3 -- aud claim.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_CU_USER_TOKEN_JWT

#include "muc_opcua/authorization/jwt.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#if defined(MUC_OPCUA_HAVE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- Base64url encoder (no padding) -- duplicated from test_jwt.c so this
 *       test stays self-contained. --------------------------------------- */
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

/* ---- Fixture: two distinct RSA-2048 keypairs and their DER SPKI blobs. */
static EVP_PKEY *s_key_a = NULL;
static EVP_PKEY *s_key_b = NULL;
static unsigned char *s_der_a = NULL;
static size_t s_der_a_len = 0;
static unsigned char *s_der_b = NULL;
static size_t s_der_b_len = 0;

static void gen_rsa_key(EVP_PKEY **out) {
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    TEST_ASSERT_NOT_NULL(pctx);
    TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen_init(pctx));
    TEST_ASSERT_EQUAL(1, EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048));
    TEST_ASSERT_EQUAL(1, EVP_PKEY_keygen(pctx, out));
    EVP_PKEY_CTX_free(pctx);
}

static void derize_pubkey(EVP_PKEY *key, unsigned char **der_out, size_t *len_out) {
    unsigned char *tmp = NULL;
    int len = i2d_PUBKEY(key, &tmp);
    TEST_ASSERT_GREATER_THAN(0, len);
    *der_out = tmp;
    *len_out = (size_t)len;
}

#define ISSUER_A_URL "https://auth-a.example.com"
#define ISSUER_B_URL "https://auth-b.example.com"
#define AUDIENCE_A "opcua-server-a"
#define AUDIENCE_B "opcua-server-b"
#define TEST_NOW (1700000000LL)
#define TEST_EXP (TEST_NOW + 3600)

static const char s_header[] = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";

static void make_payload(char *buf, size_t buf_max, const char *iss, const char *aud, const char *sub) {
    int n = snprintf(buf, buf_max, "{\"iss\":\"%s\",\"sub\":\"%s\",\"aud\":\"%s\",\"exp\":%lld,\"iat\":%lld}", iss, sub,
                     aud, (long long)TEST_EXP, (long long)TEST_NOW);
    TEST_ASSERT_TRUE((size_t)n < buf_max);
    (void)n;
}

void setUp(void) {
    if (s_key_a == NULL) {
        gen_rsa_key(&s_key_a);
    }
    if (s_key_b == NULL) {
        gen_rsa_key(&s_key_b);
    }
    if (s_der_a == NULL) {
        derize_pubkey(s_key_a, &s_der_a, &s_der_a_len);
    }
    if (s_der_b == NULL) {
        derize_pubkey(s_key_b, &s_der_b, &s_der_b_len);
    }
}

void tearDown(void) {}

/* Build the two-issuer trust table used by every test in this file. */
static void make_two_issuer_table(mu_jwt_issuer_t issuers[2]) {
    memset(&issuers[0], 0, sizeof(issuers[0]));
    issuers[0].issuer_url = ISSUER_A_URL;
    issuers[0].public_key = s_der_a;
    issuers[0].public_key_len = s_der_a_len;
    issuers[0].expected_audience = AUDIENCE_A;
    issuers[0].alg = MU_JWT_ALG_RS256;

    memset(&issuers[1], 0, sizeof(issuers[1]));
    issuers[1].issuer_url = ISSUER_B_URL;
    issuers[1].public_key = s_der_b;
    issuers[1].public_key_len = s_der_b_len;
    issuers[1].expected_audience = AUDIENCE_B;
    issuers[1].alg = MU_JWT_ALG_RS256;
}

/* US2 Scenario 1: JWT signed by A with A's audience -> accepted. */
void test_multi_issuer_jwt_from_a_is_accepted(void) {
    char payload[512];
    make_payload(payload, sizeof(payload), ISSUER_A_URL, AUDIENCE_A, "user-a");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key_a, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuers[2];
    make_two_issuer_table(issuers);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, issuers, 2, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_OK, r);
    TEST_ASSERT_EQUAL_STRING("user-a", claims.sub);
    TEST_ASSERT_EQUAL_STRING(ISSUER_A_URL, claims.iss);
    TEST_ASSERT_EQUAL_STRING(AUDIENCE_A, claims.aud);
}

/* US2 Scenario 1 (cont.): JWT signed by B with B's audience -> accepted. */
void test_multi_issuer_jwt_from_b_is_accepted(void) {
    char payload[512];
    make_payload(payload, sizeof(payload), ISSUER_B_URL, AUDIENCE_B, "user-b");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key_b, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuers[2];
    make_two_issuer_table(issuers);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, issuers, 2, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_OK, r);
    TEST_ASSERT_EQUAL_STRING("user-b", claims.sub);
}

/* US2 Scenario 2: JWT signed by A but carrying B's audience -> audience
 * mismatch against issuer A (the only one whose URL matches) is rejected
 * with MU_JWT_ERR_AUDIENCE. */
void test_multi_issuer_cross_audience_is_rejected(void) {
    char payload[512];
    /* iss matches A, but aud is B's audience. */
    make_payload(payload, sizeof(payload), ISSUER_A_URL, AUDIENCE_B, "user-a");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key_a, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuers[2];
    make_two_issuer_table(issuers);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, issuers, 2, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_AUDIENCE, r);
}

/* JWT whose iss matches neither configured URL -> MU_JWT_ERR_ISSUER. */
void test_multi_issuer_unknown_iss_is_rejected(void) {
    char payload[512];
    make_payload(payload, sizeof(payload), "https://unknown.example.com", AUDIENCE_A, "user-x");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key_a, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuers[2];
    make_two_issuer_table(issuers);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, issuers, 2, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_ISSUER, r);
}

/* Cross-key signature attack: iss claims A (so audience A is checked), but
 * the token is signed by B's key. The validator must not fall back to the
 * other issuer row -- it returns MU_JWT_ERR_SIGNATURE. */
void test_multi_issuer_wrong_key_for_matched_issuer_is_rejected(void) {
    char payload[512];
    make_payload(payload, sizeof(payload), ISSUER_A_URL, AUDIENCE_A, "user-a");

    /* Sign with B's key, but iss says A. */
    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key_b, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuers[2];
    make_two_issuer_table(issuers);

    mu_jwt_claims_t claims;
    mu_jwt_result_t r = mu_jwt_validate(jwt, jwt_len, issuers, 2, TEST_NOW, &claims);
    TEST_ASSERT_EQUAL(MU_JWT_ERR_SIGNATURE, r);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_multi_issuer_jwt_from_a_is_accepted);
    RUN_TEST(test_multi_issuer_jwt_from_b_is_accepted);
    RUN_TEST(test_multi_issuer_cross_audience_is_rejected);
    RUN_TEST(test_multi_issuer_unknown_iss_is_rejected);
    RUN_TEST(test_multi_issuer_wrong_key_for_matched_issuer_is_rejected);
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
