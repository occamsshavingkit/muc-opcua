/* tests/unit/test_diagnostics.c
 *
 * Spec Kit 037, T066-T069 + spec 064 (Base Server Behaviour facet). Validate the
 * Server Diagnostics counters (OPC-10000-5 §6.3.3/§6.3.5) AND their address-space
 * exposure: ServerDiagnosticsSummary (i=2275) serves a live
 * ServerDiagnosticsSummaryDataType ExtensionObject (i=861, §12.9).
 */
#include "../../src/address_space/base_nodes.h"
#include "../../src/core/server_internal.h"
#include "muc_opcua/encoding.h"
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

/* A security-related session rejection bumps BOTH the security-specific and the
   overall rejected-session counter (OPC-10000-5 §12.9). */
void test_session_security_rejected_bumps_both_counters(void) {
    memset(&s_server, 0, sizeof(s_server));
    mu_diagnostics_session_security_rejected(&s_server);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.security_rejected_session_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.rejected_session_count);
}

/* A rejected request bumps rejectedRequestsCount; a security rejection also bumps
   securityRejectedRequestsCount. */
void test_request_rejected_counters(void) {
    memset(&s_server, 0, sizeof(s_server));
    mu_diagnostics_request_rejected(&s_server, false);
    mu_diagnostics_request_rejected(&s_server, true);
    TEST_ASSERT_EQUAL_UINT32(2, s_server.diag.rejected_requests_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_server.diag.security_rejected_requests_count);
}

#if MUC_OPCUA_BASE_NODES
/* Base Server Behaviour facet (spec 064): the ServerDiagnosticsSummary node (i=2275)
   serves the LIVE counter struct as a ServerDiagnosticsSummaryDataType ExtensionObject
   (Encoding DefaultBinary ns0 i=861, 12 x UInt32 in §12.9 order). This proves the
   counters are both wired (above) AND exposed (here) — the facet is observable, not the
   previous inert state. */
void test_server_diagnostics_summary_node_encodes_live_counters(void) {
    mu_diagnostics_summary_t diag;
    memset(&diag, 0, sizeof(diag));
    diag.current_session_count = 3u;
    diag.cumulated_session_count = 7u;
    diag.current_subscription_count = 2u;
    diag.rejected_requests_count = 5u;

    mu_base_runtime_nodes_t rn;
    memset(&rn, 0, sizeof(rn));
    mu_base_runtime_init(&rn, NULL, 0, &diag);

    const mu_node_t *node = NULL;
    for (size_t i = 0; i < rn.space.node_count; ++i) {
        if (rn.space.nodes[i].node_id.identifier.numeric == 2275u) {
            node = &rn.space.nodes[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "ServerDiagnosticsSummary (2275) must be resolvable");
    TEST_ASSERT_NOT_NULL(node->value);

    mu_variant_t v;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_value_source_read(node->value, &node->node_id, &v));
    TEST_ASSERT_EQUAL(MU_TYPE_EXTENSIONOBJECT, v.type);
    TEST_ASSERT_FALSE(v.is_array);

    opcua_byte_t buf[128];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_write_variant(&w, &v));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    opcua_byte_t enc_mask = 0;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_byte(&r, &enc_mask));
    TEST_ASSERT_EQUAL_UINT8(MU_TYPE_EXTENSIONOBJECT, enc_mask); /* 22, no array bit */

    mu_nodeid_t type_id;
    size_t body_len = 0;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&r, &type_id, &body_len));
    TEST_ASSERT_EQUAL_UINT32(861u, type_id.identifier.numeric);
    TEST_ASSERT_EQUAL_UINT32(48u, (opcua_uint32_t)body_len); /* 12 x UInt32 */

    opcua_uint32_t f[12];
    for (int i = 0; i < 12; ++i) {
        TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &f[i]));
    }
    TEST_ASSERT_EQUAL_UINT32(0u, f[0]);  /* serverViewCount */
    TEST_ASSERT_EQUAL_UINT32(3u, f[1]);  /* currentSessionCount */
    TEST_ASSERT_EQUAL_UINT32(7u, f[2]);  /* cumulatedSessionCount */
    TEST_ASSERT_EQUAL_UINT32(2u, f[7]);  /* currentSubscriptionCount */
    TEST_ASSERT_EQUAL_UINT32(5u, f[11]); /* rejectedRequestsCount */
}

/* EnabledFlag (i=2294) reads Boolean true. */
void test_server_diagnostics_enabled_flag_is_true(void) {
    mu_diagnostics_summary_t diag;
    memset(&diag, 0, sizeof(diag));
    mu_base_runtime_nodes_t rn;
    memset(&rn, 0, sizeof(rn));
    mu_base_runtime_init(&rn, NULL, 0, &diag);

    const mu_node_t *node = NULL;
    for (size_t i = 0; i < rn.space.node_count; ++i) {
        if (rn.space.nodes[i].node_id.identifier.numeric == 2294u) {
            node = &rn.space.nodes[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "EnabledFlag (2294) must be resolvable");
    mu_variant_t v;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_value_source_read(node->value, &node->node_id, &v));
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, v.type);
    TEST_ASSERT_TRUE(v.value.b);
}
#endif /* MUC_OPCUA_BASE_NODES */

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
    RUN_TEST(test_session_security_rejected_bumps_both_counters);
    RUN_TEST(test_request_rejected_counters);
#if MUC_OPCUA_BASE_NODES
    RUN_TEST(test_server_diagnostics_summary_node_encodes_live_counters);
    RUN_TEST(test_server_diagnostics_enabled_flag_is_true);
#endif
#else
    RUN_TEST(test_diagnostics_require_build_flag);
#endif
    return UNITY_END();
}
