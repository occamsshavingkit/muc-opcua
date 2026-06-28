/* tests/unit/test_asym_chunk.c
 * Round-trip protection of asymmetric (OPN) chunks for Basic256Sha256, using two
 * independent keypairs: a "client" adapter wraps a request (encrypting to the
 * server's certificate, signing with the client's key); the "server" adapter
 * unwraps it (decrypting with its key, verifying against the embedded client
 * certificate). Also covers None passthrough and tamper detection. */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "security/asym_chunk.h"
#include "security/security_policy.h"
#include <string.h>

#ifdef MICRO_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"

static mu_crypto_adapter_t server_crypto;
static mu_crypto_adapter_t client_crypto;

void setUp(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&server_crypto));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&client_crypto));
}
void tearDown(void) {
    mu_host_crypto_adapter_cleanup(&server_crypto);
    mu_host_crypto_adapter_cleanup(&client_crypto);
}

static void get_cert(mu_crypto_adapter_t *c, const opcua_byte_t **cert, size_t *len) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, c->get_own_certificate(c->context, cert, len));
}

void test_basic256sha256_roundtrip(void) {
    const opcua_byte_t *server_cert = NULL, *client_cert = NULL;
    size_t server_cert_len = 0, client_cert_len = 0;
    get_cert(&server_crypto, &server_cert, &server_cert_len);
    get_cert(&client_crypto, &client_cert, &client_cert_len);

    /* A representative OPN request body (opaque bytes here). */
    opcua_byte_t body[120];
    for (size_t i = 0; i < sizeof(body); i++) body[i] = (opcua_byte_t)(i * 7 + 1);

    opcua_byte_t chunk[4096];
    size_t chunk_len = 0;
    /* Client wraps, encrypting to the server certificate. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_wrap(&client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                           0 /* new channel */, 1 /* seq */, 42 /* request id */,
                           server_cert, server_cert_len, body, sizeof(body),
                           chunk, sizeof(chunk), &chunk_len));
    TEST_ASSERT_GREATER_THAN(0, chunk_len);
    /* Header is cleartext "OPNF". */
    TEST_ASSERT_EQUAL('O', chunk[0]);
    TEST_ASSERT_EQUAL('P', chunk[1]);
    TEST_ASSERT_EQUAL('N', chunk[2]);

    /* Server unwraps with its private key, verifying the client signature. */
    opcua_byte_t recovered[2048];
    size_t recovered_len = 0;
    mu_asym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    opcua_byte_t scratch[6144];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_unwrap(&server_crypto, chunk, chunk_len,
                             recovered, sizeof(recovered), &recovered_len,
                             scratch, sizeof(scratch), &info));

    TEST_ASSERT_EQUAL(sizeof(body), recovered_len);
    TEST_ASSERT_EQUAL_MEMORY(body, recovered, sizeof(body));
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_BASIC256SHA256_ID, info.policy);
    TEST_ASSERT_EQUAL_UINT32(1, info.sequence_number);
    TEST_ASSERT_EQUAL_UINT32(42, info.request_id);
    /* The embedded SenderCertificate is the client's. */
    TEST_ASSERT_EQUAL(client_cert_len, info.sender_cert_len);
    TEST_ASSERT_EQUAL_MEMORY(client_cert, info.sender_cert, client_cert_len);
}

void test_none_passthrough(void) {
    opcua_byte_t body[64];
    for (size_t i = 0; i < sizeof(body); i++) body[i] = (opcua_byte_t)(255 - i);

    opcua_byte_t chunk[512];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_wrap(&client_crypto, MU_SECURITY_POLICY_NONE_ID,
                           7, 3, 99, NULL, 0, body, sizeof(body),
                           chunk, sizeof(chunk), &chunk_len));

    opcua_byte_t recovered[512];
    size_t recovered_len = 0;
    mu_asym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    opcua_byte_t scratch[6144];
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_unwrap(&server_crypto, chunk, chunk_len,
                             recovered, sizeof(recovered), &recovered_len,
                             scratch, sizeof(scratch), &info));
    TEST_ASSERT_EQUAL(sizeof(body), recovered_len);
    TEST_ASSERT_EQUAL_MEMORY(body, recovered, sizeof(body));
    TEST_ASSERT_EQUAL(MU_SECURITY_POLICY_NONE_ID, info.policy);
    TEST_ASSERT_EQUAL_UINT32(7, info.secure_channel_id);
    TEST_ASSERT_EQUAL_UINT32(99, info.request_id);
}

void test_tampered_signature_rejected(void) {
    const opcua_byte_t *server_cert = NULL;
    size_t server_cert_len = 0;
    get_cert(&server_crypto, &server_cert, &server_cert_len);

    opcua_byte_t body[80];
    memset(body, 0x5A, sizeof(body));

    opcua_byte_t chunk[4096];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_wrap(&client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                           0, 1, 5, server_cert, server_cert_len, body, sizeof(body),
                           chunk, sizeof(chunk), &chunk_len));

    /* Flip a byte in the encrypted region (after the cleartext header). */
    chunk[chunk_len - 1] ^= 0xFF;

    opcua_byte_t recovered[2048];
    size_t recovered_len = 0;
    mu_asym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    opcua_byte_t scratch[6144];
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_unwrap(&server_crypto, chunk, chunk_len,
                             recovered, sizeof(recovered), &recovered_len,
                             scratch, sizeof(scratch), &info));
}

void test_wrong_receiver_thumbprint_rejected(void) {
    /* Wrap encrypting to the CLIENT's own cert; the server (different key) must
       reject it because the ReceiverCertificateThumbprint won't match its cert. */
    const opcua_byte_t *client_cert = NULL;
    size_t client_cert_len = 0;
    get_cert(&client_crypto, &client_cert, &client_cert_len);

    opcua_byte_t body[48];
    memset(body, 0x11, sizeof(body));

    opcua_byte_t chunk[4096];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_wrap(&client_crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID,
                           0, 1, 5, client_cert, client_cert_len, body, sizeof(body),
                           chunk, sizeof(chunk), &chunk_len));

    opcua_byte_t recovered[2048];
    size_t recovered_len = 0;
    mu_asym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    opcua_byte_t scratch[6144];
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
        mu_asym_chunk_unwrap(&server_crypto, chunk, chunk_len,
                             recovered, sizeof(recovered), &recovered_len,
                             scratch, sizeof(scratch), &info));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basic256sha256_roundtrip);
    RUN_TEST(test_none_passthrough);
    RUN_TEST(test_tampered_signature_rejected);
    RUN_TEST(test_wrong_receiver_thumbprint_rejected);
    return UNITY_END();
}

#else /* !MICRO_OPCUA_HAVE_OPENSSL */
void setUp(void) {}
void tearDown(void) {}
void test_asym_chunk_skipped_without_openssl(void) {
    TEST_IGNORE_MESSAGE("OpenSSL not available; asymmetric chunk tests skipped");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_asym_chunk_skipped_without_openssl);
    return UNITY_END();
}
#endif
