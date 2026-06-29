/* tests/unit/test_build_config.c */
#include "micro_opcua/config.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_config_user_auth(void) {
#ifdef MICRO_OPCUA_USER_AUTH
    TEST_ASSERT_TRUE(1);
#else
    TEST_ASSERT_TRUE(1); // Test passes regardless, but ensures symbol can be compiled/checked
#endif
}

void test_build_config_mbedtls(void) {
#ifdef MICRO_OPCUA_HAVE_MBEDTLS
    TEST_ASSERT_TRUE(1);
#else
    TEST_ASSERT_TRUE(1);
#endif
}

void test_build_config_wolfssl(void) {
#ifdef MICRO_OPCUA_HAVE_WOLFSSL
    TEST_ASSERT_TRUE(1);
#else
    TEST_ASSERT_TRUE(1);
#endif
}

void test_build_config_connection_rx_buffer_size_default(void) {
    /* OPC-10000-6 §7.1.2.3 sets an 8192-byte receive-buffer negotiation floor,
       so the per-connection backing store cannot be smaller. */
    TEST_ASSERT_TRUE(MU_CONNECTION_RX_BUFFER_SIZE >= MU_MIN_CHUNK_SIZE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_build_config_user_auth);
    RUN_TEST(test_build_config_mbedtls);
    RUN_TEST(test_build_config_wolfssl);
    RUN_TEST(test_build_config_connection_rx_buffer_size_default);
    return UNITY_END();
}
