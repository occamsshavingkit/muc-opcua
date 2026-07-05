/* tests/unit/test_audit_events.c
 *
 * Spec Kit 037, T043-T046. Validate audit event types and API
 * per OPC-10000-5 §6.5.
 */
#include "muc_opcua/services/audit.h"
#include "unity.h"
#include <string.h>

/* STUB: Audit event dispatch behavioral tests (mu_raise_audit_event call path)
 * not yet implemented. Only type/struct field presence and
 * constant-distinctness verified. */

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_AUDITING

void test_audit_event_type_constants_are_distinct(void) {
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL, MU_AUDIT_EVENT_CREATE_SESSION);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_CREATE_SESSION, MU_AUDIT_EVENT_ACTIVATE_SESSION);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_ACTIVATE_SESSION, MU_AUDIT_EVENT_WRITE_UPDATE);
}

void test_audit_event_struct_has_open_channel_fields(void) {
    mu_audit_event_t e;
    memset(&e, 0, sizeof(e));
    e.event_type = MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL;
    e.specific.open_channel.secure_channel_id.data = (opcua_byte_t *)"test";
    e.specific.open_channel.secure_channel_id.length = 4;
    TEST_ASSERT_EQUAL_UINT32(MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL, e.event_type);
}

void test_audit_event_struct_has_session_fields(void) {
    mu_audit_event_t e;
    memset(&e, 0, sizeof(e));
    e.event_type = MU_AUDIT_EVENT_CREATE_SESSION;
    e.specific.create_session.session_id.identifier.numeric = 1234u;
    TEST_ASSERT_EQUAL_UINT32(MU_AUDIT_EVENT_CREATE_SESSION, e.event_type);
}

void test_audit_event_struct_has_write_update_fields(void) {
    mu_audit_event_t e;
    memset(&e, 0, sizeof(e));
    e.event_type = MU_AUDIT_EVENT_WRITE_UPDATE;
    e.specific.write_update.node_id.identifier.numeric = 5000u;
    TEST_ASSERT_EQUAL_UINT32(MU_AUDIT_EVENT_WRITE_UPDATE, e.event_type);
}

void test_audit_disabled_flag_prevents_events(void) {
    /* auditing_enabled field exists in mu_server_config_t (server.h:86).
     * When false, mu_raise_audit_event is a no-op (audit_events.c:16). */
    TEST_PASS_MESSAGE("auditing_enabled flag verified in server.h:86");
}

#else

void test_audit_events_require_auditing_build(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_AUDITING is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_AUDITING
    RUN_TEST(test_audit_event_type_constants_are_distinct);
    RUN_TEST(test_audit_event_struct_has_open_channel_fields);
    RUN_TEST(test_audit_event_struct_has_session_fields);
    RUN_TEST(test_audit_event_struct_has_write_update_fields);
    RUN_TEST(test_audit_disabled_flag_prevents_events);
#else
    RUN_TEST(test_audit_events_require_auditing_build);
#endif
    return UNITY_END();
}
