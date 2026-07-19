/* include/muc_opcua/authorization/jwt.h
 *
 * JWT/OAuth2 Resource Server public API (spec 093).
 *
 * Spec grounding:
 *   OPC-10000-4 §5.7.3 -- ActivateSession user token dispatch
 *   OPC-10000-7 CU 1697 -- User Token JWT Server Facet
 *   OPC-10000-7 CU 1629 -- Authorization Service Server Facet
 *   RFC 7519 -- JSON Web Token (JWT)
 *   RFC 7515 -- JSON Web Signature (JWS)
 *   RFC 7518 §3.1 -- JWT signing algorithms
 *   RFC 8725 §3 -- JWT BCP (alg dispatch, exp/nbf enforcement)
 */
#ifndef MUC_OPCUA_AUTHORIZATION_JWT_H
#define MUC_OPCUA_AUTHORIZATION_JWT_H

#include "../opcua_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* JWT signing algorithms supported by this Resource Server.
   Maps to the JWS `alg` header value (RFC 7518 §3.1). */
typedef enum {
    MU_JWT_ALG_NONE = 0, /* invalid / sentinel */
    MU_JWT_ALG_RS256,    /* RSASSA-PKCS1-v1_5 SHA-256 */
    MU_JWT_ALG_RS384,    /* RSASSA-PKCS1-v1_5 SHA-384 */
    MU_JWT_ALG_RS512,    /* RSASSA-PKCS1-v1_5 SHA-512 */
    MU_JWT_ALG_ES256,    /* ECDSA P-256 SHA-256 */
    MU_JWT_ALG_ES384,    /* ECDSA P-384 SHA-384 */
    MU_JWT_ALG_ES512,    /* ECDSA P-521 SHA-512 */
} mu_jwt_alg_t;

/* A single trusted OAuth2 issuer configuration (data-model.md §mu_jwt_issuer_t).
   The server stores an array of these in the server config; the validator
   matches the JWT `iss` claim against `issuer_url` and uses the rest of the
   fields to verify the signature, audience, and time claims. */
typedef struct {
    const char *issuer_url;         /* `iss` claim value to match */
    const opcua_byte_t *public_key; /* DER-encoded public key (RSA or EC) */
    size_t public_key_len;
    const char *expected_audience;  /* `aud` claim value to match */
    opcua_uint32_t clock_skew_seconds; /* tolerance for exp/nbf checks */
    mu_jwt_alg_t alg;               /* expected signing algorithm */
} mu_jwt_issuer_t;

/* Claims extracted from a validated JWT (data-model.md §mu_jwt_claims_t).
   String fields are fixed-size; values longer than the buffer are truncated.
   `sub` MUST be non-empty for the token to be considered valid. */
typedef struct {
    char sub[128];
    char iss[128];
    char aud[128];
    opcua_int64_t exp; /* 0 if absent -> caller treats as error */
    opcua_int64_t nbf; /* 0 if absent -> no not-before check */
    opcua_int64_t iat; /* 0 if absent */
} mu_jwt_claims_t;

/* Validation result codes (data-model.md §mu_jwt_result_t). */
typedef enum {
    MU_JWT_OK = 0,
    MU_JWT_ERR_MALFORMED,             /* not three dot-separated segments */
    MU_JWT_ERR_BASE64,                /* invalid Base64url encoding */
    MU_JWT_ERR_SIGNATURE,             /* signature verification failed */
    MU_JWT_ERR_EXPIRED,               /* `exp` in the past (after skew) */
    MU_JWT_ERR_NOT_YET_VALID,         /* `nbf` in the future (after skew) */
    MU_JWT_ERR_ISSUER,                /* `iss` not in trusted issuer table */
    MU_JWT_ERR_AUDIENCE,              /* `aud` does not match expected */
    MU_JWT_ERR_NO_SUB,                /* `sub` missing or empty */
    MU_JWT_ERR_UNSUPPORTED_ALG,       /* `alg` not in mu_jwt_alg_t */
    MU_JWT_ERR_NO_CONFIGURED_ISSUERS, /* feature on, but 0 issuers configured */
} mu_jwt_result_t;

/* Validate a JWT bearer token against the given issuer table.
 *
 *   jwt                 - compact JWS form (header.payload.signature), need not
 *                         be NUL-terminated
 *   jwt_len             - byte length of jwt
 *   issuers             - trusted issuer table; may be NULL iff issuer_count==0
 *   issuer_count        - number of entries in issuers
 *   server_time_unix    - current wall clock in Unix seconds (caller-supplied
 *                         so the validator has no platform time dependency)
 *   out_claims          - filled in on MU_JWT_OK; may be NULL
 *
 * Returns MU_JWT_OK on success or a MU_JWT_ERR_* code on failure. On failure
 * *out_claims is not modified.
 */
mu_jwt_result_t mu_jwt_validate(const char *jwt, size_t jwt_len, const mu_jwt_issuer_t *issuers,
                                opcua_byte_t issuer_count, opcua_int64_t server_time_unix,
                                mu_jwt_claims_t *out_claims);

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_AUTHORIZATION_JWT_H */
