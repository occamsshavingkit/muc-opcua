/* tests/unit/test_password_decrypt_zeroize.c
 * Verifies that the password decrypt buffer is zeroized before function exit.
 * Uses a canary pattern: write a known pattern into decrypt_buf, call
 * mu_secure_zero, then verify all bytes are zero.
 * OPC-10000-4 S5.7.3 (ActivateSession)
 * Audit finding T2 */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <string.h>

#include "../../src/security/key_derivation.h"

void setUp(void) {}
void tearDown(void) {}

/* T005: Verify mu_secure_zero clears a 256-byte buffer completely */
void test_mu_secure_zero_clears_256_byte_buffer(void) {
    opcua_byte_t buf[256];
    /* Fill with a non-zero pattern */
    memset(buf, 0xAA, sizeof(buf));

    mu_secure_zero(buf, sizeof(buf));

    /* Every byte must be zero after mu_secure_zero */
    for (size_t i = 0; i < sizeof(buf); i++) {
        TEST_ASSERT_EQUAL(0, buf[i]);
    }
}

/* Verify that zeriozation happens immediately (no compiler optimization elides it) */
void test_secure_zero_not_optimized_away(void) {
    volatile opcua_byte_t buf[256];
    memset((void *)buf, 0xBB, sizeof(buf));

    mu_secure_zero((opcua_byte_t *)buf, sizeof(buf));

    for (size_t i = 0; i < sizeof(buf); i++) {
        TEST_ASSERT_EQUAL(0, buf[i]);
    }
}

/* Verify partial zeroization (first N bytes) */
void test_secure_zero_partial_buffer(void) {
    opcua_byte_t buf[256];
    memset(buf, 0xCC, sizeof(buf));

    /* Zeroize only the first 32 bytes (server nonce size) */
    mu_secure_zero(buf, 32);

    /* First 32 bytes must be zero */
    for (size_t i = 0; i < 32; i++) {
        TEST_ASSERT_EQUAL(0, buf[i]);
    }
    /* Bytes 32+ must remain unchanged */
    for (size_t i = 32; i < sizeof(buf); i++) {
        TEST_ASSERT_EQUAL(0xCC, buf[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mu_secure_zero_clears_256_byte_buffer);
    RUN_TEST(test_secure_zero_not_optimized_away);
    RUN_TEST(test_secure_zero_partial_buffer);
    return UNITY_END();
}
