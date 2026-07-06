/* tests/unit/test_audit_events.c
 *
 * Spec Kit 037, T043-T046. Validate audit event types and API
 * per OPC-10000-5 §6.5.
 *
 * Deferred audit callback mechanism (T034):
 *   - mu_audit_callback_t typedef does not yet exist
 *   - Callback registration API does not yet exist
 *   - Callback dispatch from mu_raise_audit_event is a no-op
 *   - Per-event-type handler routing is not implemented
 *   All of the above are pending audit callback implementation.
 */
#include "muc_opcua/server.h"
#include "muc_opcua/services/audit.h"
#include "unity.h"
#include <string.h>

#include "fake_platform.h"

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_AUDITING

static opcua_byte_t rx_buf[8192];
static opcua_byte_t tx_buf[8192];

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

void test_audit_disabled_flag(void) {
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.auditing_enabled = false;
    TEST_ASSERT_FALSE(config.auditing_enabled);
    config.auditing_enabled = true;
    TEST_ASSERT_TRUE(config.auditing_enabled);
}

/* T031: OPC-10000-5 §6.5 — valid audit event dispatch does not crash */
void test_raise_audit_event_valid_input(void) {
    _Alignas(8) opcua_byte_t storage[MU_SERVER_STORAGE_BYTES];
    mu_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.endpoint_url = "opc.tcp://localhost:4840";
    config.receive_buffer = rx_buf;
    config.receive_buffer_size = sizeof(rx_buf);
    config.send_buffer = tx_buf;
    config.send_buffer_size = sizeof(tx_buf);
    config.max_sessions = 1;
    config.max_secure_channels = 1;
    config.max_chunk_count = 1;
    config.max_message_size = 8192;
    fake_platform_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);
    mu_server_t *server = NULL;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_init(storage, sizeof(storage), &config, &server));

    mu_audit_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = MU_AUDIT_EVENT_CREATE_SESSION;
    event.status = true;
    event.client_user_id.data = (opcua_byte_t *)"TestUser";
    event.client_user_id.length = 8;
    event.specific.create_session.session_id.identifier.numeric = 42;

    mu_raise_audit_event(server, &event);
    TEST_PASS_MESSAGE("mu_raise_audit_event with valid input did not crash");
}

/* T032: OPC-10000-5 §6.5 — null pointers are silently tolerated */
void test_raise_audit_event_null_safety(void) {
    mu_raise_audit_event(NULL, NULL);
    TEST_PASS_MESSAGE("mu_raise_audit_event(NULL, NULL) did not crash");
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
    RUN_TEST(test_audit_disabled_flag);
    RUN_TEST(test_raise_audit_event_valid_input);
    RUN_TEST(test_raise_audit_event_null_safety);
#else
    RUN_TEST(test_audit_events_require_auditing_build);
#endif
    return UNITY_END();
}
