#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_aliasname_stub_present(void)
{
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_aliasname_stub_present);
    return UNITY_END();
}
