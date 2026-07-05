/* tests/unit/test_diagnostics.c
 *
 * Spec Kit 037, T066-T069. Validate Server Diagnostics counters
 * per OPC-10000-5 §6.3.3 and §6.3.5.
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/services/diagnostics.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_SERVER_DIAGNOSTICS

static mu_server_t s_server;

void test_session_create_increments_counters(void) {
    memset(&s_server, 0, sizeof(s_server));
    mu_diagnostics_session_created(&s_server);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.cumulated_session_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.current_session_count);
}

void test_session_close_decrements_count(void) {
    memset(&s_server, 0, sizeof(s_server));
    mu_diagnostics_session_created(&s_server);
    mu_diagnostics_session_closed(&s_server);
    TEST_ASSERT_EQUAL_UINT32(0, s_server.diag.current_session_count);
}

void test_subscription_create_increments(void) {
    memset(&s_server, 0, sizeof(s_server));
    mu_diagnostics_subscription_created(&s_server);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.current_subscription_count);
}

void test_rejected_and_timeout_counters_compile(void) {
    memset(&s_server, 0, sizeof(s_server));
    mu_diagnostics_session_rejected(&s_server);
    mu_diagnostics_session_timeout(&s_server);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.rejected_session_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.session_timeout_count);
}

#else

void test_diagnostics_require_build_flag(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_SERVER_DIAGNOSTICS is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_SERVER_DIAGNOSTICS
    RUN_TEST(test_session_create_increments_counters);
    RUN_TEST(test_session_close_decrements_count);
    RUN_TEST(test_subscription_create_increments);
    RUN_TEST(test_rejected_and_timeout_counters_compile);
#else
    RUN_TEST(test_diagnostics_require_build_flag);
#endif
    return UNITY_END();
}
