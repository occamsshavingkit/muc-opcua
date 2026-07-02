/* tests/unit/test_certificate.c
 * Certificate thumbprint computation and peer-certificate validation for
 * Basic256Sha256 (OPC 10000-6 §6.7.4 / 10000-4 §7.2). Uses the host crypto backend. */
#include "muc_opcua/muc_opcua.h"
#include "security/certificate.h"
#include "security/security_policy.h"
#include "unity.h"
#include <string.h>

#ifdef MUC_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"
#include <openssl/evp.h>

static mu_crypto_adapter_t crypto;

void setUp(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&crypto));
}
void tearDown(void) {
    mu_host_crypto_adapter_cleanup(&crypto);
}

/* The thumbprint is the SHA-1 of the DER certificate (the standard X.509
   thumbprint), 20 bytes, and is deterministic. */
void test_thumbprint_is_sha1_of_der(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));

    opcua_byte_t thumb[MU_THUMBPRINT_LENGTH];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_certificate_thumbprint(&crypto, cert, cert_len, thumb));

    /* Independently compute SHA-1 of the DER and compare. */
    opcua_byte_t expected[MU_THUMBPRINT_LENGTH];
    unsigned int elen = 0;
    TEST_ASSERT_EQUAL(1, EVP_Digest(cert, cert_len, expected, &elen, EVP_sha1(), NULL));
    TEST_ASSERT_EQUAL(MU_THUMBPRINT_LENGTH, elen);
    TEST_ASSERT_EQUAL_MEMORY(expected, thumb, MU_THUMBPRINT_LENGTH);

    /* Deterministic. */
    opcua_byte_t thumb2[MU_THUMBPRINT_LENGTH];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_certificate_thumbprint(&crypto, cert, cert_len, thumb2));
    TEST_ASSERT_EQUAL_MEMORY(thumb, thumb2, MU_THUMBPRINT_LENGTH);
}

/* The self-signed RSA-2048 cert is a valid peer cert for Basic256Sha256. */
void test_validate_accepts_rsa2048(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_certificate_validate(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, cert, cert_len));
}

/* Unparseable bytes are rejected. */
void test_validate_rejects_garbage(void) {
    opcua_byte_t junk[64];
    memset(junk, 0xAB, sizeof(junk));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
                          mu_certificate_validate(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, junk, sizeof(junk)));
}

/* Basic256Sha256 requires a certificate; a missing one is rejected. */
void test_validate_rejects_missing_cert_for_secure_policy(void) {
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
                          mu_certificate_validate(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, NULL, 0));
}

/* SecurityPolicy None uses no certificate, so validation is a no-op success. */
void test_validate_none_policy_needs_no_cert(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_certificate_validate(&crypto, MU_SECURITY_POLICY_NONE_ID, NULL, 0));
}

/* OPC-10000-4 §5.5 (feature 025 F3): a secured policy MUST fail closed when the
   backend cannot check certificate validity (no verify_certificate_validity hook)
   rather than silently accepting an unchecked certificate. */
void test_validate_fails_closed_without_validity_hook(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));
    mu_crypto_adapter_t no_validity = crypto;
    no_validity.verify_certificate_validity = NULL;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_CERTIFICATEINVALID,
                      mu_certificate_validate(&no_validity, MU_SECURITY_POLICY_BASIC256SHA256_ID, cert, cert_len));
}

/* A validity-hook result other than Good is propagated (e.g. an expired cert
   surfaces as Bad_CertificateTimeInvalid). */
static opcua_statuscode_t stub_validity_time_invalid(void *c, const opcua_byte_t *cert, size_t len) {
    (void)c;
    (void)cert;
    (void)len;
    return MU_STATUS_BAD_CERTIFICATETIMEINVALID;
}

void test_validate_propagates_expired_cert(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));
    mu_crypto_adapter_t expired = crypto;
    expired.verify_certificate_validity = stub_validity_time_invalid;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_CERTIFICATETIMEINVALID,
                      mu_certificate_validate(&expired, MU_SECURITY_POLICY_BASIC256SHA256_ID, cert, cert_len));
}

/* The real host backend reports its freshly generated self-signed cert as valid. */
void test_validate_accepts_in_window_cert(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.verify_certificate_validity(crypto.context, cert, cert_len));
}

/* Feature 027: rsa_sha256_verify's contract is
   (certificate, cert_len, data, data_len, signature, sig_len) — the public key
   comes from the FIRST argument. The x509 user-identity-token verify path used to
   pass the signed data as the certificate; that only "worked" against a mock and
   was rejected by a real backend. This exercises the real host adapter to pin the
   argument order down so the bug cannot silently return. */
void test_rsa_verify_argument_order_is_certificate_first(void) {
    const opcua_byte_t *cert = NULL;
    size_t cert_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.get_own_certificate(crypto.context, &cert, &cert_len));

    /* Payload standing in for (serverCert || serverNonce). */
    opcua_byte_t data[64];
    memset(data, 0xAB, sizeof(data));
    opcua_byte_t sig[512];
    size_t sig_len = sizeof(sig);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, crypto.rsa_sha256_sign(crypto.context, data, sizeof(data), sig, &sig_len));

    /* Correct contract order verifies. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      crypto.rsa_sha256_verify(crypto.context, cert, cert_len, data, sizeof(data), sig, sig_len));

    /* The previously-shipped mis-ordering (signed data passed as the certificate)
       must NOT verify on a real backend. */
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
                          crypto.rsa_sha256_verify(crypto.context, data, sizeof(data), sig, sig_len, cert, cert_len));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_thumbprint_is_sha1_of_der);
    RUN_TEST(test_validate_accepts_rsa2048);
    RUN_TEST(test_validate_rejects_garbage);
    RUN_TEST(test_validate_rejects_missing_cert_for_secure_policy);
    RUN_TEST(test_validate_none_policy_needs_no_cert);
    RUN_TEST(test_validate_fails_closed_without_validity_hook);
    RUN_TEST(test_validate_propagates_expired_cert);
    RUN_TEST(test_validate_accepts_in_window_cert);
    RUN_TEST(test_rsa_verify_argument_order_is_certificate_first);
    return UNITY_END();
}

#else /* !MUC_OPCUA_HAVE_OPENSSL */
void setUp(void) {}
void tearDown(void) {}
void test_certificate_skipped_without_openssl(void) {
    TEST_IGNORE_MESSAGE("OpenSSL not available; certificate tests skipped");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_certificate_skipped_without_openssl);
    return UNITY_END();
}
#endif
