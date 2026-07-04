/* tests/unit/test_sym_chunk.c
 * Round-trip protection of symmetric (MSG) chunks for Basic256Sha256 in both
 * Sign and SignAndEncrypt modes, plus key derivation and tamper detection. A
 * single derived key set wraps and unwraps (modelling one party's send keys,
 * which its peer also derives). */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#ifdef MUC_OPCUA_SECURITY
#include "security/security_policy.h"
#include "security/sym_chunk.h"
#include <string.h>

#ifdef MUC_OPCUA_HAVE_OPENSSL
#include "platform/host_crypto_adapter.h"
static mu_crypto_adapter_t crypto;
static mu_sym_keys_t keys;
static bool host_crypto_active;
#endif

void setUp(void) {}

void tearDown(void) {
#ifdef MUC_OPCUA_HAVE_OPENSSL
    if (host_crypto_active) {
        mu_host_crypto_adapter_cleanup(&crypto);
        host_crypto_active = false;
    }
#endif
}

#ifdef MUC_OPCUA_HAVE_OPENSSL
static void prepare_host_keys(void) {
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_host_crypto_adapter_init(&crypto));
    host_crypto_active = true;
    opcua_byte_t client_nonce[32], server_nonce[32];
    for (int i = 0; i < 32; i++) {
        client_nonce[i] = (opcua_byte_t)i;
        server_nonce[i] = (opcua_byte_t)(200 - i);
    }
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_keys_derive(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, server_nonce,
                                         sizeof(server_nonce), client_nonce, sizeof(client_nonce), &keys));
}
#endif

typedef struct {
    unsigned hmac_sha256_calls;
    unsigned stateless_encrypt_calls;
    unsigned stateless_decrypt_calls;
    unsigned cipher_ctx_init_calls;
    unsigned cipher_ctx_encrypt_calls;
    unsigned cipher_ctx_decrypt_calls;
    unsigned cipher_ctx_free_calls;
} counting_crypto_stub_t;

_Static_assert(MU_CIPHER_CTX_SIZE >= MU_B256S256_ENCRYPTION_KEY_LENGTH, "test cipher ctx must fit the AES-256 key");

static opcua_statuscode_t counting_hmac_sha256(void *context, const opcua_byte_t *key, size_t key_length,
                                               const opcua_byte_t *data, size_t data_length, opcua_byte_t *mac) {
    counting_crypto_stub_t *stub = (counting_crypto_stub_t *)context;
    if (!stub || !key || key_length == 0 || !data || !mac) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    stub->hmac_sha256_calls++;
    for (size_t i = 0; i < MU_B256S256_SIGNATURE_LENGTH; i++) {
        mac[i] = (opcua_byte_t)(0xA5u ^ (opcua_byte_t)i ^ key[i % key_length] ^ (opcua_byte_t)data_length);
    }
    for (size_t i = 0; i < data_length; i++) {
        size_t slot = i % MU_B256S256_SIGNATURE_LENGTH;
        mac[slot] = (opcua_byte_t)(mac[slot] + data[i] + key[(i + slot) % key_length] + (opcua_byte_t)i);
        mac[(slot + 7u) % MU_B256S256_SIGNATURE_LENGTH] ^= (opcua_byte_t)(data[i] + (opcua_byte_t)(i * 13u));
    }
    return MU_STATUS_GOOD;
}

static void counting_xor_crypt(const opcua_byte_t *key, const opcua_byte_t *iv, const opcua_byte_t *input,
                               size_t length, opcua_byte_t *output) {
    for (size_t i = 0; i < length; i++) {
        opcua_byte_t stream =
            (opcua_byte_t)(key[i % MU_B256S256_ENCRYPTION_KEY_LENGTH] ^ iv[i % MU_B256S256_ENCRYPTION_BLOCK_SIZE] ^
                           (opcua_byte_t)(0x5Au + (i * 29u)));
        output[i] = (opcua_byte_t)(input[i] ^ stream);
    }
}

static opcua_statuscode_t counting_aes256_cbc_encrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                                      const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    counting_crypto_stub_t *stub = (counting_crypto_stub_t *)context;
    if (!stub || !key || !iv || !input || !output) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    stub->stateless_encrypt_calls++;
    counting_xor_crypt(key, iv, input, length, output);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t counting_aes256_cbc_decrypt(void *context, const opcua_byte_t *key, const opcua_byte_t *iv,
                                                      const opcua_byte_t *input, size_t length, opcua_byte_t *output) {
    counting_crypto_stub_t *stub = (counting_crypto_stub_t *)context;
    if (!stub || !key || !iv || !input || !output) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    stub->stateless_decrypt_calls++;
    counting_xor_crypt(key, iv, input, length, output);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t counting_cipher_ctx_init(void *context, const opcua_byte_t *key, opcua_byte_t *ctx_storage) {
    counting_crypto_stub_t *stub = (counting_crypto_stub_t *)context;
    if (!stub || !key || !ctx_storage) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    stub->cipher_ctx_init_calls++;
    (void)memcpy(ctx_storage, key, MU_B256S256_ENCRYPTION_KEY_LENGTH);
    (void)memset(ctx_storage + MU_B256S256_ENCRYPTION_KEY_LENGTH, 0, MU_CIPHER_CTX_SIZE - MU_B256S256_ENCRYPTION_KEY_LENGTH);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t counting_aes256_cbc_encrypt_ctx(void *context, opcua_byte_t *ctx_storage,
                                                          const opcua_byte_t *iv, const opcua_byte_t *input,
                                                          size_t length, opcua_byte_t *output) {
    counting_crypto_stub_t *stub = (counting_crypto_stub_t *)context;
    if (!stub || !ctx_storage || !iv || !input || !output) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    stub->cipher_ctx_encrypt_calls++;
    counting_xor_crypt(ctx_storage, iv, input, length, output);
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t counting_aes256_cbc_decrypt_ctx(void *context, opcua_byte_t *ctx_storage,
                                                          const opcua_byte_t *iv, const opcua_byte_t *input,
                                                          size_t length, opcua_byte_t *output) {
    counting_crypto_stub_t *stub = (counting_crypto_stub_t *)context;
    if (!stub || !ctx_storage || !iv || !input || !output) {
        return MU_STATUS_BAD_INTERNALERROR;
    }
    stub->cipher_ctx_decrypt_calls++;
    counting_xor_crypt(ctx_storage, iv, input, length, output);
    return MU_STATUS_GOOD;
}

static void counting_cipher_ctx_free(void *context, opcua_byte_t *ctx_storage) {
    counting_crypto_stub_t *stub = (counting_crypto_stub_t *)context;
    if (stub) {
        stub->cipher_ctx_free_calls++;
    }
    if (ctx_storage) {
        (void)memset(ctx_storage, 0, MU_CIPHER_CTX_SIZE);
    }
}

static void counting_stub_init(counting_crypto_stub_t *stub, mu_crypto_adapter_t *adapter, bool provide_ctx_init) {
    (void)memset(stub, 0, sizeof(*stub));
    (void)memset(adapter, 0, sizeof(*adapter));
    adapter->context = stub;
    adapter->hmac_sha256 = counting_hmac_sha256;
    adapter->aes256_cbc_encrypt = counting_aes256_cbc_encrypt;
    adapter->aes256_cbc_decrypt = counting_aes256_cbc_decrypt;
    adapter->cipher_ctx_init = provide_ctx_init ? counting_cipher_ctx_init : NULL;
    adapter->aes256_cbc_encrypt_ctx = counting_aes256_cbc_encrypt_ctx;
    adapter->aes256_cbc_decrypt_ctx = counting_aes256_cbc_decrypt_ctx;
    adapter->cipher_ctx_free = counting_cipher_ctx_free;
}

static void fill_counting_keys(mu_sym_keys_t *keys) {
    (void)memset(keys, 0, sizeof(*keys));
    for (size_t i = 0; i < MU_B256S256_SIGNATURE_KEY_LENGTH; i++) {
        keys->signing_key[i] = (opcua_byte_t)(0x10u + (i * 3u));
    }
    for (size_t i = 0; i < MU_B256S256_ENCRYPTION_KEY_LENGTH; i++) {
        keys->encrypting_key[i] = (opcua_byte_t)(0x80u ^ (i * 5u));
    }
    for (size_t i = 0; i < MU_B256S256_ENCRYPTION_BLOCK_SIZE; i++) {
        keys->iv[i] = (opcua_byte_t)(0x40u + (i * 7u));
    }
}

void test_cipher_context_prepared_once_and_reused_per_message(void) {
    counting_crypto_stub_t stub;
    mu_crypto_adapter_t adapter;
    mu_sym_keys_t local_keys;
    counting_stub_init(&stub, &adapter, true);
    fill_counting_keys(&local_keys);

    TEST_ASSERT_FALSE(local_keys.cipher_ctx_valid);
    mu_sym_keys_prepare_cipher(&local_keys, &adapter);
    TEST_ASSERT_TRUE(local_keys.cipher_ctx_valid);
    TEST_ASSERT_EQUAL_UINT(1, stub.cipher_ctx_init_calls);

    for (opcua_uint32_t message = 0; message < 3; message++) {
        opcua_byte_t body[41];
        for (size_t i = 0; i < sizeof(body); i++) {
            body[i] = (opcua_byte_t)(0x20u + message + (i * 11u));
        }

        opcua_byte_t chunk[512];
        size_t chunk_len = 0;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap(&adapter, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT,
                                                            &local_keys, "MSG", 7, 11, 100 + message, 200 + message,
                                                            body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
        TEST_ASSERT_EQUAL_UINT(1, stub.cipher_ctx_init_calls);
        TEST_ASSERT_EQUAL_UINT(message + 1u, stub.cipher_ctx_encrypt_calls);
        TEST_ASSERT_EQUAL_UINT(0, stub.stateless_encrypt_calls);

        const opcua_byte_t *recovered = NULL;
        size_t recovered_len = 0;
        mu_sym_chunk_info_t info;
        (void)memset(&info, 0, sizeof(info));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                          mu_sym_chunk_unwrap(&adapter, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &local_keys, chunk,
                                              chunk_len, &recovered, &recovered_len, &info));
        TEST_ASSERT_EQUAL(sizeof(body), recovered_len);
        TEST_ASSERT_EQUAL_MEMORY(body, recovered, sizeof(body));
        TEST_ASSERT_EQUAL_UINT(1, stub.cipher_ctx_init_calls);
        TEST_ASSERT_EQUAL_UINT(message + 1u, stub.cipher_ctx_decrypt_calls);
        TEST_ASSERT_EQUAL_UINT(0, stub.stateless_decrypt_calls);
    }

    mu_sym_keys_release_cipher(&local_keys);
    TEST_ASSERT_FALSE(local_keys.cipher_ctx_valid);
    TEST_ASSERT_EQUAL_UINT(1, stub.cipher_ctx_free_calls);
}

void test_cipher_context_falls_back_to_stateless_without_init_callback(void) {
    counting_crypto_stub_t stub;
    mu_crypto_adapter_t adapter;
    mu_sym_keys_t local_keys;
    counting_stub_init(&stub, &adapter, false);
    fill_counting_keys(&local_keys);

    TEST_ASSERT_NULL(adapter.cipher_ctx_init);
    mu_sym_keys_prepare_cipher(&local_keys, &adapter);
    TEST_ASSERT_FALSE(local_keys.cipher_ctx_valid);
    TEST_ASSERT_EQUAL_UINT(0, stub.cipher_ctx_init_calls);

    for (opcua_uint32_t message = 0; message < 2; message++) {
        opcua_byte_t body[37];
        for (size_t i = 0; i < sizeof(body); i++) {
            body[i] = (opcua_byte_t)(0x55u ^ (opcua_byte_t)(message + i));
        }

        opcua_byte_t chunk[512];
        size_t chunk_len = 0;
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap(&adapter, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT,
                                                            &local_keys, "MSG", 9, 13, 300 + message, 400 + message,
                                                            body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
        TEST_ASSERT_EQUAL_UINT(message + 1u, stub.stateless_encrypt_calls);
        TEST_ASSERT_EQUAL_UINT(0, stub.cipher_ctx_encrypt_calls);

        const opcua_byte_t *recovered = NULL;
        size_t recovered_len = 0;
        mu_sym_chunk_info_t info;
        memset(&info, 0, sizeof(info));
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                          mu_sym_chunk_unwrap(&adapter, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &local_keys, chunk,
                                              chunk_len, &recovered, &recovered_len, &info));
        TEST_ASSERT_EQUAL(sizeof(body), recovered_len);
        TEST_ASSERT_EQUAL_MEMORY(body, recovered, sizeof(body));
        TEST_ASSERT_FALSE(local_keys.cipher_ctx_valid);
        TEST_ASSERT_EQUAL_UINT(message + 1u, stub.stateless_decrypt_calls);
        TEST_ASSERT_EQUAL_UINT(0, stub.cipher_ctx_decrypt_calls);
    }

    mu_sym_keys_release_cipher(&local_keys);
    TEST_ASSERT_EQUAL_UINT(0, stub.cipher_ctx_free_calls);
}

#ifdef MUC_OPCUA_HAVE_OPENSSL
/* Derivation fills all key material and is deterministic; different nonces differ. */
void test_keys_derivation_deterministic(void) {
    prepare_host_keys();
    mu_sym_keys_t a, b;
    opcua_byte_t s1[32], s2[32];
    (void)memset(s1, 0x01, sizeof(s1));
    (void)memset(s2, 0x02, sizeof(s2));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_keys_derive(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, s1, 32, s2, 32, &a));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_keys_derive(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, s1, 32, s2, 32, &b));
    TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));
    /* Swapping secret/seed yields different material. */
    mu_sym_keys_t c;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_keys_derive(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, s2, 32, s1, 32, &c));
    TEST_ASSERT_NOT_EQUAL(0, memcmp(&a, &c, sizeof(a)));
}

void test_sign_and_encrypt_roundtrip(void) {
    prepare_host_keys();
    opcua_byte_t body[100];
    for (size_t i = 0; i < sizeof(body); i++) {
        body[i] = (opcua_byte_t)(i * 3 + 5);
    }

    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys, "MSG", 5, 9, 3, 77,
                                        body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
    TEST_ASSERT_EQUAL('M', chunk[0]);
    /* Encrypted region must be a multiple of the AES block. */
    TEST_ASSERT_EQUAL(0, (chunk_len - 16) % MU_B256S256_ENCRYPTION_BLOCK_SIZE);

    const opcua_byte_t *recovered = NULL;
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys,
                                                          chunk, chunk_len, &recovered, &recovered_len, &info));
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
    prepare_host_keys();
    opcua_byte_t body[55];
    for (size_t i = 0; i < sizeof(body); i++) {
        body[i] = (opcua_byte_t)(i + 1);
    }

    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &keys, "MSG", 1, 2, 4,
                                                        8, body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
    /* Sign-only: header(16) + SeqHeader(8) + body + HMAC(32), no padding. */
    TEST_ASSERT_EQUAL(16 + 8 + sizeof(body) + MU_B256S256_SIGNATURE_LENGTH, chunk_len);

    const opcua_byte_t *recovered = NULL;
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &keys, chunk,
                                                          chunk_len, &recovered, &recovered_len, &info));
    TEST_ASSERT_EQUAL(sizeof(body), recovered_len);
    TEST_ASSERT_EQUAL_MEMORY(body, recovered, sizeof(body));
    TEST_ASSERT_EQUAL(MU_MESSAGE_SECURITY_MODE_SIGN, info.mode);
}

void test_tampered_body_rejected(void) {
    prepare_host_keys();
    opcua_byte_t body[64];
    (void)memset(body, 0x33, sizeof(body));
    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys, "MSG", 1, 1, 1, 1,
                                        body, sizeof(body), chunk, sizeof(chunk), &chunk_len));
    chunk[20] ^= 0x01; /* corrupt inside the encrypted region */

    const opcua_byte_t *recovered = NULL;
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN_AND_ENCRYPT, &keys,
                                                              chunk, chunk_len, &recovered, &recovered_len, &info));
}

void test_wrong_keys_rejected(void) {
    prepare_host_keys();
    opcua_byte_t body[40];
    (void)memset(body, 0x77, sizeof(body));
    opcua_byte_t chunk[1024];
    size_t chunk_len = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_wrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &keys, "MSG", 1, 1, 1,
                                                        1, body, sizeof(body), chunk, sizeof(chunk), &chunk_len));

    mu_sym_keys_t other;
    opcua_byte_t a[32], b[32];
    (void)memset(a, 0xAA, sizeof(a));
    (void)memset(b, 0xBB, sizeof(b));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
                      mu_sym_keys_derive(&crypto, MU_SECURITY_POLICY_BASIC256SHA256_ID, a, 32, b, 32, &other));

    const opcua_byte_t *recovered = NULL;
    size_t recovered_len = 0;
    mu_sym_chunk_info_t info;
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, mu_sym_chunk_unwrap(&crypto, MU_MESSAGE_SECURITY_MODE_SIGN, &other, chunk,
                                                              chunk_len, &recovered, &recovered_len, &info));
}
#endif /* MUC_OPCUA_HAVE_OPENSSL */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cipher_context_prepared_once_and_reused_per_message);
    RUN_TEST(test_cipher_context_falls_back_to_stateless_without_init_callback);
#ifdef MUC_OPCUA_HAVE_OPENSSL
    RUN_TEST(test_keys_derivation_deterministic);
    RUN_TEST(test_sign_and_encrypt_roundtrip);
    RUN_TEST(test_sign_only_roundtrip);
    RUN_TEST(test_tampered_body_rejected);
    RUN_TEST(test_wrong_keys_rejected);
#endif
    return UNITY_END();
}

#else /* !MUC_OPCUA_SECURITY */
void setUp(void) {}
void tearDown(void) {}
void test_sym_chunk_skipped_without_security(void) {
    TEST_IGNORE_MESSAGE("Security support not enabled; symmetric chunk tests skipped");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sym_chunk_skipped_without_security);
    return UNITY_END();
}
#endif
