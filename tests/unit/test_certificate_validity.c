/* tests/unit/test_certificate_validity.c
 *
 * Feature 028 US3 (T025/T029/T030): real-crypto certificate validation. Unlike
 * test_certificate.c (which stubs the validity hook), this generates REAL X.509
 * certificates with OpenSSL and drives them through the REAL host backend
 * (get_certificate_key_bits + verify_certificate_validity) via
 * mu_certificate_validate — closing the "mock hides real behaviour" gap for the
 * cert accept/reject paths.
 *
 * - Expired and not-yet-valid certs -> Bad_CertificateTimeInvalid.
 *   OPC-10000-4 §5.5 (certificate validation) / OPC-10000-6 §6.1.3.
 * - RSA key below the Basic256Sha256 lower bound -> Bad_SecurityChecksFailed.
 *   OPC-10000-7 Basic256Sha256 asymmetric key-size bounds.
 * - A valid in-window RSA-2048 cert -> Good (control).
 */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#if defined(MUC_OPCUA_HAVE_OPENSSL) && MUC_OPCUA_HAVE_OPENSSL

#include "platform/host_crypto_adapter.h"
#include "security/certificate.h"
#include "security/security_policy.h"

/* The static-analysis environment has no OpenSSL headers; the build does. */
/* cppcheck-suppress missingIncludeSystem */
#include <openssl/evp.h>
/* cppcheck-suppress missingIncludeSystem */
#include <openssl/x509.h>

static mu_crypto_adapter_t crypto;

void setUp(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&crypto));
}
void tearDown(void) {
    mu_host_crypto_adapter_cleanup(&crypto);
}

/* Generate an RSA key of the given size; NULL on failure. Portable across
   OpenSSL 1.1.1 and 3.x (EVP_PKEY_keygen, not EVP_RSA_gen). */
static EVP_PKEY *gen_rsa_key(int key_bits) {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (pctx != NULL) {
        if ((EVP_PKEY_keygen_init(pctx) != 1) || (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, key_bits) != 1) ||
            (EVP_PKEY_keygen(pctx, &pkey) != 1)) {
            pkey = NULL;
        }
        EVP_PKEY_CTX_free(pctx);
    }
    return pkey;
}

/* Build and self-sign an X.509 v3 cert with the given validity window (offsets in
   seconds relative to now); NULL on failure. */
static X509 *build_signed_cert(EVP_PKEY *pkey, long not_before_off, long not_after_off) {
    X509 *x = X509_new();
    if (x != NULL) {
        X509_set_version(x, 2); /* v3 */
        ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
        X509_gmtime_adj(X509_getm_notBefore(x), not_before_off);
        X509_gmtime_adj(X509_getm_notAfter(x), not_after_off);
        X509_set_pubkey(x, pkey);
        X509_NAME *name = X509_get_subject_name(x);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)"muc-opcua-test", -1, -1, 0);
        X509_set_issuer_name(x, name); /* self-signed */
        if (X509_sign(x, pkey, EVP_sha256()) == 0) {
            X509_free(x);
            x = NULL;
        }
    }
    return x;
}

/* Generate a self-signed DER certificate with the given RSA key size and validity
   window. Returns the DER length, 0 on error. */
static size_t make_der_cert(int key_bits, long not_before_off, long not_after_off, opcua_byte_t *out, size_t out_cap) {
    size_t written = 0;
    EVP_PKEY *pkey = gen_rsa_key(key_bits);
    if (pkey != NULL) {
        X509 *x = build_signed_cert(pkey, not_before_off, not_after_off);
        if (x != NULL) {
            int len = i2d_X509(x, NULL);
            if ((len > 0) && ((size_t)len <= out_cap)) {
                unsigned char *p = out;
                written = (size_t)i2d_X509(x, &p);
            }
            X509_free(x);
        }
        EVP_PKEY_free(pkey);
    }
    return written;
}

#define DAY (60L * 60L * 24L)

/* Control: a real in-window RSA-2048 cert validates Good through the real backend. */
void test_real_in_window_rsa2048_is_good(void) {
    opcua_byte_t der[4096];
    size_t len = make_der_cert(2048, -DAY, DAY, der, sizeof(der));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_certificate_validate(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, der, len));
}

/* An expired cert (notAfter in the past) is rejected with Bad_CertificateTimeInvalid. */
void test_real_expired_cert_is_time_invalid(void) {
    opcua_byte_t der[4096];
    size_t len = make_der_cert(2048, -2 * DAY, -DAY, der, sizeof(der));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_CERTIFICATETIMEINVALID,
                            mu_certificate_validate(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, der, len));
}

/* A not-yet-valid cert (notBefore in the future) is likewise time-invalid. */
void test_real_not_yet_valid_cert_is_time_invalid(void) {
    opcua_byte_t der[4096];
    size_t len = make_der_cert(2048, DAY, 2 * DAY, der, sizeof(der));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_CERTIFICATETIMEINVALID,
                            mu_certificate_validate(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, der, len));
}

/* An RSA-1024 key is below the Basic256Sha256 lower bound (2048) -> checks failed. */
void test_real_undersized_key_is_checks_failed(void) {
    opcua_byte_t der[4096];
    size_t len = make_der_cert(1024, -DAY, DAY, der, sizeof(der));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SECURITYCHECKSFAILED,
                            mu_certificate_validate(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, der, len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_real_in_window_rsa2048_is_good);
    RUN_TEST(test_real_expired_cert_is_time_invalid);
    RUN_TEST(test_real_not_yet_valid_cert_is_time_invalid);
    RUN_TEST(test_real_undersized_key_is_checks_failed);
    return UNITY_END();
}

#else /* no OpenSSL backend compiled */

void setUp(void) {}
void tearDown(void) {}
void test_cert_validity_skipped_without_openssl(void) {
    TEST_IGNORE_MESSAGE("no OpenSSL crypto backend: real-cert validity tests skipped");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cert_validity_skipped_without_openssl);
    return UNITY_END();
}

#endif
