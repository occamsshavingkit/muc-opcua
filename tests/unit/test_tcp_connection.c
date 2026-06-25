/* tests/unit/test_tcp_connection.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"

void setUp(void) {}
void tearDown(void) {}

void test_tcp_hello_acknowledge_negotiation(void) {
    TEST_IGNORE_MESSAGE("Implement TCP Hello/Acknowledge negotiation test");
}

void test_tcp_default_buffer_size(void) {
    TEST_IGNORE_MESSAGE("Implement TCP 8192-byte default buffers test");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tcp_hello_acknowledge_negotiation);
    RUN_TEST(test_tcp_default_buffer_size);
    return UNITY_END();
}
