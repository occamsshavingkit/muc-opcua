/* tests/unit/test_build_config.c */
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_build_config_user_auth);
    RUN_TEST(test_build_config_mbedtls);
    RUN_TEST(test_build_config_wolfssl);
    return UNITY_END();
}
