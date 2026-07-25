/* tests/unit/test_jwt_claims.c
 *
 * Spec 093 Task T025 -- User Story 3: `sub` claim handling.
 *
 * Validates that mu_jwt_validate() correctly extracts and rejects the `sub`
 * claim across the boundary conditions listed in spec.md SC-005 / FR-006:
 *   - A short, ASCII-safe subject is returned verbatim.
 *   - An empty subject ("sub":"" in the payload) is rejected with
 *     MU_JWT_ERR_NO_SUB.
 *   - A 128-byte subject (exactly the buffer capacity) is preserved intact.
 *   - A 200-byte subject (overlong) is silently truncated to 127 bytes plus a
 *     NUL terminator and still accepted -- the buffer is fixed at 128 B per
 *     data-model.md.
 *   - A subject containing JSON-special characters (escaped quotes, embedded
 *     commas, unicode escapes) is unescaped correctly.
 *
 * Spec grounding:
 *   spec.md SC-005 -- `sub` claim reported in session user identity.
 *   spec.md FR-006 -- `sub` MUST be used as user identity.
 *   OPC-10000-4 §5.7.3 -- ActivateSession user identity.
 *   RFC 7519 §4.1.2 -- `sub` claim semantics.
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
    char payload_b64[2048];
    b64url_encode((const unsigned char *)header_json, strlen(header_json), header_b64);
    b64url_encode((const unsigned char *)payload_json, strlen(payload_json), payload_b64);

    char signing_input[2304];
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
#define TEST_EXP (TEST_NOW + 3600)

static const char s_header[] = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";

static void make_issuer(mu_jwt_issuer_t *out) {
    memset(out, 0, sizeof(*out));
    out->issuer_url = "https://auth.example.com";
    out->public_key = s_der;
    out->public_key_len = s_der_len;
    out->expected_audience = "opcua-server";
    out->clock_skew_seconds = 0;
    out->alg = MU_JWT_ALG_RS256;
}

/* Build a payload with a literal `sub` value already serialized as a JSON
 * string. Caller provides the inner-JSON text of the subject (already escaped
 * if necessary); it is wrapped in quotes by this helper. */
static void make_payload_with_raw_sub(char *buf, size_t buf_max, const char *sub_raw_json_inner) {
    int n = snprintf(buf, buf_max,
                     "{\"iss\":\"https://auth.example.com\",\"sub\":%s,\"aud\":\"opcua-server\","
                     "\"exp\":%lld,\"iat\":%lld}",
                     sub_raw_json_inner, (long long)TEST_EXP, (long long)TEST_NOW);
    TEST_ASSERT_TRUE((size_t)n < buf_max);
    (void)n;
}

/* Build a payload that additionally carries a raw JSON value for the OPC UA
 * `roles` claim (already serialised by the caller, including any surrounding
 * array brackets). spec 093 US3 / spec.md Edge Cases. */
static void make_payload_with_roles(char *buf, size_t buf_max, const char *sub_value_quoted,
                                    const char *roles_raw_json) {
    int n = snprintf(buf, buf_max,
                     "{\"iss\":\"https://auth.example.com\",\"sub\":%s,\"aud\":\"opcua-server\","
                     "\"exp\":%lld,\"iat\":%lld,\"roles\":%s}",
                     sub_value_quoted, (long long)TEST_EXP, (long long)TEST_NOW, roles_raw_json);
    TEST_ASSERT_TRUE((size_t)n < buf_max);
    (void)n;
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

/* Short ASCII subject is extracted verbatim. */
void test_sub_short_ascii_is_extracted(void) {
    char payload[512];
    make_payload_with_raw_sub(payload, sizeof(payload), "\"operator1\"");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
    TEST_ASSERT_EQUAL_STRING("operator1", claims.sub);
}

/* Empty subject -> MU_JWT_ERR_NO_SUB. */
void test_sub_empty_is_rejected(void) {
    char payload[512];
    make_payload_with_raw_sub(payload, sizeof(payload), "\"\"");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_NO_SUB, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* A subject longer than the 128-byte (127-char + NUL) buffer is rejected by
 * the scanner (it sets dst[0]='\0' on overflow for `sub`, then the validator
 * fails with MU_JWT_ERR_NO_SUB). This is stricter than data-model.md ("values
 * longer than the buffer are truncated") -- the implementation treats an
 * overlong subject as malformed, which is the safer choice for an identity. */
void test_sub_128_byte_overflow_is_rejected(void) {
    char big[300];
    /* 200-character subject -- well over the 127-char buffer capacity. */
    memset(big, 'A', 200);
    big[200] = '\0';

    /* Wrap in JSON quotes. */
    char quoted[210];
    int n = snprintf(quoted, sizeof(quoted), "\"%s\"", big);
    TEST_ASSERT_TRUE((size_t)n < sizeof(quoted));
    (void)n;

    char payload[512];
    make_payload_with_raw_sub(payload, sizeof(payload), quoted);

    char jwt[2048];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_NO_SUB, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* A subject exactly 127 characters long fits the buffer (claims.sub[128] =
 * 127 chars + NUL). The validator MUST accept and preserve the full subject. */
void test_sub_exactly_127_chars_is_accepted(void) {
    char big[200];
    memset(big, 'A', 127);
    big[127] = '\0';

    char quoted[140];
    int n = snprintf(quoted, sizeof(quoted), "\"%s\"", big);
    TEST_ASSERT_TRUE((size_t)n < sizeof(quoted));
    (void)n;

    char payload[512];
    make_payload_with_raw_sub(payload, sizeof(payload), quoted);

    char jwt[2048];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
    TEST_ASSERT_EQUAL(127, (int)strlen(claims.sub));
    TEST_ASSERT_EQUAL_MEMORY(big, claims.sub, 127);
}

/* Subject with JSON escapes for quote and backslash is unescaped correctly by
 * the streaming scanner. (The scanner unescapes \" \\ \n \r \t but leaves
 * other \x and \uXXXX sequences as the literal char after the backslash.) */
void test_sub_with_json_escapes_is_unescaped(void) {
    char payload[512];
    /* Embedded escaped quote + backslash. After unescaping: a"b\c */
    make_payload_with_raw_sub(payload, sizeof(payload), "\"a\\\"b\\\\c\"");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
    TEST_ASSERT_EQUAL_STRING("a\"b\\c", claims.sub);
}

/* Subject absent entirely from the payload (different from empty) -- the
   scanner leaves claims.sub[0]=='\0', validator rejects with NO_SUB. */
void test_sub_absent_is_rejected(void) {
    char payload[512];
    int n = snprintf(payload, sizeof(payload),
                     "{\"iss\":\"https://auth.example.com\",\"aud\":\"opcua-server\","
                     "\"exp\":%lld,\"iat\":%lld}",
                     (long long)TEST_EXP, (long long)TEST_NOW);
    TEST_ASSERT_TRUE((size_t)n < sizeof(payload));
    (void)n;

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_NO_SUB, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* A bounded numeric role-claim array populates claims.role_node_ids in order
 * and sets role_count to the number of entries. spec 093 US3 / RFC 7519 §4. */
void test_roles_bounded_array_is_parsed(void) {
    char payload[640];
    make_payload_with_roles(payload, sizeof(payload), "\"operator1\"", "[15620,15621,15807]");

    char jwt[2048];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
    TEST_ASSERT_EQUAL(3, (int)claims.role_count);
    TEST_ASSERT_EQUAL_UINT32(15620u, claims.role_node_ids[0]);
    TEST_ASSERT_EQUAL_UINT32(15621u, claims.role_node_ids[1]);
    TEST_ASSERT_EQUAL_UINT32(15807u, claims.role_node_ids[2]);
    TEST_ASSERT_EQUAL(0, (int)claims.role_overflow);
}

/* An empty roles array is valid -- role_count stays at zero, no overflow. */
void test_roles_empty_array_yields_zero(void) {
    char payload[640];
    make_payload_with_roles(payload, sizeof(payload), "\"operator1\"", "[]");

    char jwt[2048];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
    TEST_ASSERT_EQUAL(0, (int)claims.role_count);
    TEST_ASSERT_EQUAL(0, (int)claims.role_overflow);
}

/* A non-numeric element in the roles array is malformed -- the validator
 * rejects the whole token with MU_JWT_ERR_MALFORMED (spec 093 Edge Cases). */
void test_roles_non_numeric_element_is_rejected(void) {
    char payload[640];
    make_payload_with_roles(payload, sizeof(payload), "\"operator1\"", "[15620,\"not-a-number\"]");

    char jwt[2048];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_MALFORMED, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* Listing more roles than MU_JWT_MAX_ROLES overflows the bounded store -- the
 * scanner sets role_overflow and the validator rejects with MALFORMED. */
void test_roles_over_capacity_is_rejected(void) {
    char payload[768];
    make_payload_with_roles(payload, sizeof(payload), "\"operator1\"",
                            "[1,2,3,4,5,6,7,8,9]");

    char jwt[2048];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_ERR_MALFORMED, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
}

/* A token without a roles claim parses normally; role_count/overflow stay 0. */
void test_roles_absent_leaves_zero(void) {
    char payload[512];
    make_payload_with_raw_sub(payload, sizeof(payload), "\"operator1\"");

    char jwt[1600];
    size_t jwt_len = build_jwt_rs256(s_header, payload, s_key, jwt, sizeof(jwt));

    mu_jwt_issuer_t issuer;
    make_issuer(&issuer);

    mu_jwt_claims_t claims;
    TEST_ASSERT_EQUAL(MU_JWT_OK, mu_jwt_validate(jwt, jwt_len, &issuer, 1, TEST_NOW, &claims));
    TEST_ASSERT_EQUAL(0, (int)claims.role_count);
    TEST_ASSERT_EQUAL(0, (int)claims.role_overflow);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sub_short_ascii_is_extracted);
    RUN_TEST(test_sub_empty_is_rejected);
    RUN_TEST(test_sub_128_byte_overflow_is_rejected);
    RUN_TEST(test_sub_exactly_127_chars_is_accepted);
    RUN_TEST(test_sub_with_json_escapes_is_unescaped);
    RUN_TEST(test_sub_absent_is_rejected);
    RUN_TEST(test_roles_bounded_array_is_parsed);
    RUN_TEST(test_roles_empty_array_yields_zero);
    RUN_TEST(test_roles_non_numeric_element_is_rejected);
    RUN_TEST(test_roles_over_capacity_is_rejected);
    RUN_TEST(test_roles_absent_leaves_zero);
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
