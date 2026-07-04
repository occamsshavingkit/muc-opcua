/* tests/unit/test_user_token_decoder.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_decode_username_token_direct(void) {
    /* Hardcoded binary representation of UserNameIdentityToken body:
       - policyId: "username_policy" (len 15) -> 0f 00 00 00 + "username_policy"
       - userName: "admin" (len 5) -> 05 00 00 00 + "admin"
       - password: "admin" (len 5) -> 05 00 00 00 + "admin"
       - encryptionAlgorithm: null (0xffffffff)
    */
    opcua_byte_t buf[] = {0x0f, 0x00, 0x00, 0x00, 'u', 's',  'e',  'r',  'n',  'a',  'm',  'e',  '_', 'p',
                          'o',  'l',  'i',  'c',  'y', 0x05, 0x00, 0x00, 0x00, 'a',  'd',  'm',  'i', 'n',
                          0x05, 0x00, 0x00, 0x00, 'a', 'd',  'm',  'i',  'n',  0xff, 0xff, 0xff, 0xff};

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buf, sizeof(buf));

    mu_username_identity_token_t token;
    (void)memset(&token, 0, sizeof(token));

    opcua_statuscode_t status = mu_binary_read_username_identity_token(&reader, &token);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(15, token.policy_id.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.policy_id.data, "username_policy", 15));

    TEST_ASSERT_EQUAL(5, token.username.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.username.data, "admin", 5));

    TEST_ASSERT_EQUAL(5, token.password.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.password.data, "admin", 5));

    TEST_ASSERT_EQUAL(-1, token.encryption_algorithm.length);
    TEST_ASSERT_NULL(token.encryption_algorithm.data);
}

void test_decode_username_from_file_fixture(void) {
    FILE *f = fopen(PROJECT_ROOT_DIR "/tests/fixtures/activate_session_user_plaintext_req.bin", "rb");
    TEST_ASSERT_NOT_NULL(f);

    opcua_byte_t buf[512];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    TEST_ASSERT_TRUE(n > 0);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buf, n);

    /* Skip RequestHeader */
    // authenticationToken
    mu_nodeid_t token_nodeid;
    mu_binary_read_nodeid(&reader, &token_nodeid);
    // timestamp
    reader.position += 8;
    // requestHandle, returnDiagnostics, auditEntryId, timeoutHint
    reader.position += 4 + 4;
    mu_string_t audit;
    mu_binary_read_string(&reader, &audit);
    reader.position += 4;
    // additionalHeader
    mu_nodeid_t add_type;
    size_t add_len;
    mu_binary_read_extension_object_header(&reader, &add_type, &add_len);

    /* Skip signatures, software certs, locales */
    mu_string_t sig_alg;
    mu_binary_read_string(&reader, &sig_alg);
    mu_bytestring_t client_sig;
    mu_binary_read_bytestring(&reader, &client_sig);

    // software certs (array of SignedSoftwareCertificate)
    opcua_int32_t cert_count;
    mu_binary_read_int32(&reader, &cert_count);
    TEST_ASSERT_EQUAL(0, cert_count);

    // localeIds (array of String)
    opcua_int32_t locale_count;
    mu_binary_read_int32(&reader, &locale_count);
    TEST_ASSERT_EQUAL(0, locale_count);

    /* Decode ExtensionObject header for userIdentityToken */
    mu_nodeid_t token_type;
    size_t token_body_len;
    opcua_statuscode_t status = mu_binary_read_extension_object_header(&reader, &token_type, &token_body_len);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL(324, token_type.identifier.numeric);

    /* Read body */
    mu_username_identity_token_t token;
    memset(&token, 0, sizeof(token));
    status = mu_binary_read_username_identity_token(&reader, &token);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    TEST_ASSERT_EQUAL(15, token.policy_id.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.policy_id.data, "username_policy", 15));
    TEST_ASSERT_EQUAL(5, token.username.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.username.data, "admin", 5));
    TEST_ASSERT_EQUAL(5, token.password.length);
    TEST_ASSERT_EQUAL_INT(0, memcmp(token.password.data, "admin", 5));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_username_token_direct);
    RUN_TEST(test_decode_username_from_file_fixture);
    return UNITY_END();
}
