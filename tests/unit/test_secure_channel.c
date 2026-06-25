/* tests/unit/test_secure_channel.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/secure_channel.h"

void test_secure_channel_open_none(void) {
    mu_secure_channel_t channel;
    mu_secure_channel_init(&channel);
    TEST_ASSERT_FALSE(channel.is_open);
    
    opcua_uint32_t revised_lifetime = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_secure_channel_open(&channel, 1000, &revised_lifetime));
    TEST_ASSERT_TRUE(channel.is_open);
    TEST_ASSERT_EQUAL(10000, revised_lifetime); /* Bounded to min */
    TEST_ASSERT_EQUAL(1, channel.channel_id);
    TEST_ASSERT_EQUAL(1, channel.token_id);
    
    /* Renew */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_secure_channel_open(&channel, 5000000, &revised_lifetime));
    TEST_ASSERT_EQUAL(3600000, revised_lifetime); /* Bounded to max */
    TEST_ASSERT_EQUAL(2, channel.token_id);
}

void test_secure_channel_close(void) {
    mu_secure_channel_t channel;
    mu_secure_channel_init(&channel);
    
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN, mu_secure_channel_close(&channel));
    
    opcua_uint32_t revised_lifetime = 0;
    mu_secure_channel_open(&channel, 3600000, &revised_lifetime);
    TEST_ASSERT_TRUE(channel.is_open);
    
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_secure_channel_close(&channel));
    TEST_ASSERT_FALSE(channel.is_open);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_secure_channel_open_none);
    RUN_TEST(test_secure_channel_close);
    return UNITY_END();
}
