/* tests/unit/test_user_token_encrypted.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static opcua_statuscode_t mock_rsa_oaep_decrypt(void *context, const opcua_byte_t *input, size_t length,
                                                opcua_byte_t *output, size_t *output_length) {
    (void)context;
    if (length > *output_length) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }
    /* Decryption mock: XOR with 0x5A to simulate decryption */
    for (size_t i = 0; i < length; ++i) {
        output[i] = input[i] ^ 0x5A;
    }
    *output_length = length;
    return MU_STATUS_GOOD;
}

static opcua_statuscode_t test_auth_handler(void *handle, const mu_string_t *username, const mu_bytestring_t *password,
                                            const mu_string_t *policy_id) {
    (void)handle;
    (void)policy_id;

    if (username->length == 5 && memcmp(username->data, "admin", 5) == 0 && password->length == 5 &&
        memcmp(password->data, "admin", 5) == 0) {
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_IDENTITYTOKENREJECTED;
}

void test_encrypted_password_decryption(void) {
    mu_crypto_adapter_t crypto;
    memset(&crypto, 0, sizeof(crypto));
    crypto.rsa_oaep_decrypt = mock_rsa_oaep_decrypt;

    /* "admin" XOR 0x5A is:
       'a' (0x61) ^ 0x5A = 0x3B
       'd' (0x64) ^ 0x5A = 0x3E
       'm' (0x6D) ^ 0x5A = 0x37
       'i' (0x69) ^ 0x5A = 0x33
       'n' (0x6E) ^ 0x5A = 0x34
    */
    opcua_byte_t encrypted_pwd[] = {0x3B, 0x3E, 0x37, 0x33, 0x34};

    opcua_byte_t decrypt_buf[256];
    size_t out_len = sizeof(decrypt_buf);

    opcua_statuscode_t status =
        crypto.rsa_oaep_decrypt(NULL, encrypted_pwd, sizeof(encrypted_pwd), decrypt_buf, &out_len);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(5, out_len);
    TEST_ASSERT_EQUAL_MEMORY("admin", decrypt_buf, 5);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_encrypted_password_decryption);
    return UNITY_END();
}
