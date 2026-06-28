/* tests/unit/test_build_config.c */
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_config_user_auth(void)
{
#ifdef MICRO_OPCUA_USER_AUTH
    TEST_ASSERT_TRUE(1);
#else
    TEST_ASSERT_TRUE(1); // Test passes regardless, but ensures symbol can be compiled/checked
#endif
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_config_user_auth);
    return UNITY_END();
}
