/* tests/unit/test_method_call_arbitrary.c
 *
 * Method Server Facet (spec 062): custom method registration KATs — success,
 * duplicate-overwrite, capacity-full, and that the registered context/signature/
 * executable flag are stored. OPC-10000-4 §5.11 (Call service).
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/method.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_METHOD_SERVER

static mu_server_t s_server;

static opcua_statuscode_t echo_callback(mu_server_t *server, void *context, const mu_nodeid_t *object_id,
                                        const mu_nodeid_t *method_id, const mu_variant_t *input_args,
                                        size_t input_count, mu_variant_t *output_args, size_t *output_count) {
    (void)server;
    (void)context;
    (void)object_id;
    (void)method_id;
    if (input_count > 0 && output_count != NULL && *output_count > 0) {
        output_args[0] = input_args[0];
        *output_count = 1;
    }
    return MU_STATUS_GOOD;
}

void test_register_custom_method_stores_context_and_executable(void) {
    (void)memset(&s_server, 0, sizeof(s_server));
    int ctx = 42;
    mu_nodeid_t method_id = {1u, MU_NODEID_NUMERIC, {1001u}};
    opcua_statuscode_t s =
        mu_server_register_method_callback(&s_server, &method_id, echo_callback, &ctx, NULL, 0, NULL, 0, true);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL_size_t(1, s_server.registered_method_count);
    TEST_ASSERT_TRUE(mu_nodeid_equal(&s_server.registered_methods[0].method_id, &method_id));
    TEST_ASSERT_EQUAL_PTR(&ctx, s_server.registered_methods[0].context);
    TEST_ASSERT_TRUE(s_server.registered_methods[0].executable);
}

void test_register_duplicate_method_overwrites(void) {
    (void)memset(&s_server, 0, sizeof(s_server));
    mu_nodeid_t method_id = {1u, MU_NODEID_NUMERIC, {2001u}};
    mu_server_register_method_callback(&s_server, &method_id, echo_callback, NULL, NULL, 0, NULL, 0, true);
    opcua_statuscode_t s =
        mu_server_register_method_callback(&s_server, &method_id, echo_callback, NULL, NULL, 0, NULL, 0, false);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL_size_t(1, s_server.registered_method_count);
    TEST_ASSERT_FALSE(s_server.registered_methods[0].executable); /* overwrote in place */
}

void test_register_capacity_full_returns_bad_outofmemory(void) {
    (void)memset(&s_server, 0, sizeof(s_server));
    for (opcua_uint32_t i = 0; i < MU_MAX_REGISTERED_METHODS; ++i) {
        mu_nodeid_t id = {1u, MU_NODEID_NUMERIC, {3000u + i}};
        TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_server_register_method_callback(&s_server, &id, echo_callback, NULL, NULL,
                                                                             0, NULL, 0, true));
    }
    mu_nodeid_t overflow = {1u, MU_NODEID_NUMERIC, {9999u}};
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_OUTOFMEMORY, mu_server_register_method_callback(&s_server, &overflow, echo_callback,
                                                                                    NULL, NULL, 0, NULL, 0, true));
}

void test_register_rejects_null_args(void) {
    (void)memset(&s_server, 0, sizeof(s_server));
    mu_nodeid_t method_id = {1u, MU_NODEID_NUMERIC, {1u}};
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ARGUMENTSMISSING,
                      mu_server_register_method_callback(&s_server, &method_id, NULL, NULL, NULL, 0, NULL, 0, true));
}

#else

void test_method_server_requires_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_METHOD_SERVER is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_METHOD_SERVER
    RUN_TEST(test_register_custom_method_stores_context_and_executable);
    RUN_TEST(test_register_duplicate_method_overwrites);
    RUN_TEST(test_register_capacity_full_returns_bad_outofmemory);
    RUN_TEST(test_register_rejects_null_args);
#else
    RUN_TEST(test_method_server_requires_build);
#endif
    return UNITY_END();
}
