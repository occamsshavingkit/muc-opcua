/* tests/unit/test_sym_chunk.c
 * Round-trip protection of symmetric (MSG) chunks for Basic256Sha256 in both
 * Sign and SignAndEncrypt modes, plus key derivation and tamper detection. A
 * single derived key set wraps and unwraps (modelling one party's send keys,
 * which its peer also derives). */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "security/sym_chunk.h"
#include "security/security_policy.h"
#include <string.h>

#ifdef MICRO_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"

static mu_crypto_adapter_t crypto;
static mu_sym_keys_t keys;

void setUp(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&crypto));
    opcua_byte_t client_nonce[32], server_nonce[32];
    for (int i = 0; i < 32; i++) { client_nonce[i] = (opcua_byte_t)i; server_nonce[i] = (opcua_byte_t)(200 - i); }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_sym_keys_derive(&crypto, server_nonce, sizeof(server_nonce), client_nonce, sizeof(client_nonce), &keys));
}
void tearDown(void) {
    mu_host_crypto_adapter_cleanup(&crypto);
}

/* Derivation fills all key material and is deterministic; different nonces differ. */
void test_keys_derivation_deterministic(void) {
    mu_sym_keys_t a, b;
    opcua_byte_t s1[32], s2[32];
    memset(s1, 0x01, sizeof(s1));
    memset(s2, 0x02, sizeof(s2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&crypto, s1, 32, s2, 32, &a));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&crypto, s1, 32, s2, 32, &b));
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));
    /* Swapping secret/seed yields different material. */
    mu_sym_keys_t c;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&crypto, s2, 32, s1, 32, &c));
    TEST_ASSERT_NOT_EQUAL(0, memcmp(&a, &c, sizeof(a)));
}

void test_sign_and_encrypt_roundtrip(void) {
    opcua_byte_t body[100];
    for (size_t i = 0; i < sizeof(body); i++) body[i] = (opcua_byte_t)(i * 3 + 5);

    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys, "MSG",
                          5, 9, 3, 77, body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
    TEST_ASSERT_EQUAL('M', chunk[0]);
    /* Encrypted region must be a multiple of the AES block. */
    TEST_ASSERT_EQUAL(0, (chunk_len - 16) % MU_B256S256_ENCRYPTION_BLOCK_SIZE);

    opcua_byte_t recovered[1024];
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys,
                            chunk, chunk_len, recovered, sizeof(recovered), &recovered_len, &info));
    TEST_ASSERT_EQUAL(sizeof(body), recovered_len);
    TEST_ASSERT_EQUAL_MEMORY(body, recovered, sizeof(body));
    TEST_ASSERT_EQUAL(MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, info.mode);
    TEST_ASSERT_EQUAL_UINT32(5, info.secure_channel_id);
    TEST_ASSERT_EQUAL_UINT32(9, info.token_id);
    TEST_ASSERT_EQUAL_UINT32(3, info.sequence_number);
    TEST_ASSERT_EQUAL_UINT32(77, info.request_id);
    TEST_ASSERT_EQUAL_MEMORY("MSG", info.message_type, 3);
}

void test_sign_only_roundtrip(void) {
    opcua_byte_t body[55];
    for (size_t i = 0; i < sizeof(body); i++) body[i] = (opcua_byte_t)(i + 1);

    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &keys, "MSG",
                          1, 2, 4, 8, body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
    /* Sign-only: header(16) + SeqHeader(8) + body + HMAC(32), no padding. */
    TEST_ASSERT_EQUAL(16 + 8 + sizeof(body) + MU_B256S256_SIGNATURE_LENGTH, chunk_len);

    opcua_byte_t recovered[1024];
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &keys,
                            chunk, chunk_len, recovered, sizeof(recovered), &recovered_len, &info));
    TEST_ASSERT_EQUAL(sizeof(body), recovered_len);
    TEST_ASSERT_EQUAL_MEMORY(body, recovered, sizeof(body));
    TEST_ASSERT_EQUAL(MU_MESSAGE_SECURITY_MODE_SIGN, info.mode);
}

void test_tampered_body_rejected(void) {
    opcua_byte_t body[64];
    memset(body, 0x33, sizeof(body));
    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys, "MSG",
                          1, 1, 1, 1, body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
    chunk[20] ^= 0x01; /* corrupt inside the encrypted region */

    opcua_byte_t recovered[1024];
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys,
                            chunk, chunk_len, recovered, sizeof(recovered), &recovered_len, &info));
}

void test_wrong_keys_rejected(void) {
    opcua_byte_t body[40];
    memset(body, 0x77, sizeof(body));
    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &keys, "MSG",
                          1, 1, 1, 1, body, sizeof(body), chunk, sizeof(chunk), &chunk_len));

    mu_sym_keys_t other;
    opcua_byte_t a[32], b[32];
    memset(a, 0xAA, sizeof(a));
    memset(b, 0xBB, sizeof(b));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_keys_derive(&crypto, a, 32, b, 32, &other));

    opcua_byte_t recovered[1024];
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD,
        mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &other,
                            chunk, chunk_len, recovered, sizeof(recovered), &recovered_len, &info));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_keys_derivation_deterministic);
    RUN_TEST(test_sign_and_encrypt_roundtrip);
    RUN_TEST(test_sign_only_roundtrip);
    RUN_TEST(test_tampered_body_rejected);
    RUN_TEST(test_wrong_keys_rejected);
    return UNITY_END();
}

#else /* !MICRO_OPCUA_HAVE_OPENSSL */
void setUp(void) {}
void tearDown(void) {}
void test_sym_chunk_skipped_without_openssl(void) {
    TEST_IGNORE_MESSAGE("OpenSSL not available; symmetric chunk tests skipped");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sym_chunk_skipped_without_openssl);
    return UNITY_END();
}
#endif
