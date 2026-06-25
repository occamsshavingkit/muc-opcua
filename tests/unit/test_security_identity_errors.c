/* tests/unit/test_security_identity_errors.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

#include "../../src/services/secure_channel.h"

void test_unsupported_security_policy(void) {
    mu_secure_channel_t channel;
    mu_secure_channel_init(&channel);
    
    opcua_uint32_t revised_lifetime = 0;
    
    mu_string_t invalid_policy;
    invalid_policy.data = (opcua_byte_t*)"http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256";
    invalid_policy.length = 57;
    
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_SECURITYPOLICYREJECTED, 
                      mu_secure_channel_open(&channel, &invalid_policy, 1000, &revised_lifetime));
}

void test_unsupported_identity_token(void) {
    TEST_IGNORE_MESSAGE("To be implemented in T144");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unsupported_security_policy);
    RUN_TEST(test_unsupported_identity_token);
    return UNITY_END();
}
