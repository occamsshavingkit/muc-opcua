/* tests/unit/test_audit_events.c
 *
 * Audit event type validation and callback dispatch tests.
 * OPC-10000-5 §6.5 (Audit Event Types), §6.5.2 (EventType), §6.5.3 (AuditEventType).
 */
#include "muc_opcua/server.h"
#include "muc_opcua/services/audit.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

#include "../../src/core/server_internal.h"
#include "fake_platform.h"

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_AUDITING

static opcua_byte_t rx_buf[8192];
static opcua_byte_t tx_buf[8192];

static struct mu_server s_audit_server;
static mu_server_config_t s_audit_cfg;

static void setup_audit_server(bool auditing_enabled) {
    memset(&s_audit_server, 0, sizeof(s_audit_server));
    memset(&s_audit_cfg, 0, sizeof(s_audit_cfg));
    s_audit_cfg.endpoint_url = "opc.tcp://localhost:4840";
    s_audit_cfg.receive_buffer = rx_buf;
    s_audit_cfg.receive_buffer_size = sizeof(rx_buf);
    s_audit_cfg.send_buffer = tx_buf;
    s_audit_cfg.send_buffer_size = sizeof(tx_buf);
    s_audit_cfg.max_sessions = 1;
    s_audit_cfg.max_secure_channels = 1;
    s_audit_cfg.max_chunk_count = 1;
    s_audit_cfg.max_message_size = 8192;
    s_audit_cfg.auditing_enabled = auditing_enabled;
    fake_platform_init(&s_audit_cfg.tcp_adapter, &s_audit_cfg.time_adapter, &s_audit_cfg.entropy_adapter);
    s_audit_server.config = s_audit_cfg;
}

/* Callback tracking for test assertions */
static int g_callback_invocations = 0;
static int g_callback_order[4] = {0, 0, 0, 0};
static int g_callback_count = 0;
static mu_audit_event_t g_last_event_copy;
static int g_ordered_callback_ids[3] = {0, 0, 0};
static int g_ordered_callback_count = 0;

static void reset_callback_tracking(void) {
    g_callback_invocations = 0;
    g_callback_count = 0;
    g_ordered_callback_count = 0;
    memset(&g_last_event_copy, 0, sizeof(g_last_event_copy));
    memset(g_callback_order, 0, sizeof(g_callback_order));
    memset(g_ordered_callback_ids, 0, sizeof(g_ordered_callback_ids));
}

static void record_ordered_callback(int callback_id) {
    if (g_ordered_callback_count < 3) {
        g_ordered_callback_ids[g_ordered_callback_count++] = callback_id;
    }
}

static struct mu_server *g_test_server = NULL;

static opcua_datetime_t audit_test_time(void *context) {
    (void)context;
    return 123456789u;
}

static void audit_test_callback(struct mu_server *server, const mu_audit_event_t *event, void *context) {
    (void)context;
    g_test_server = server;
    g_callback_invocations++;
    if (g_callback_count < 4) {
        g_callback_order[g_callback_count++] = event->event_type;
    }
    memcpy(&g_last_event_copy, event, sizeof(g_last_event_copy));
}

static void audit_callback_a(struct mu_server *server, const mu_audit_event_t *event, void *context) {
    (void)context;
    record_ordered_callback(1);
    (void)server;
    (void)event;
}

static void audit_callback_b(struct mu_server *server, const mu_audit_event_t *event, void *context) {
    (void)context;
    record_ordered_callback(2);
    (void)server;
    (void)event;
}

static void audit_callback_c(struct mu_server *server, const mu_audit_event_t *event, void *context) {
    (void)context;
    record_ordered_callback(3);
    (void)server;
    (void)event;
}

void test_audit_event_type_constants_are_distinct(void) {
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL, MU_AUDIT_EVENT_CREATE_SESSION);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_CREATE_SESSION, MU_AUDIT_EVENT_ACTIVATE_SESSION);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_ACTIVATE_SESSION, MU_AUDIT_EVENT_WRITE_UPDATE);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_WRITE_UPDATE, MU_AUDIT_EVENT_NODE_MANAGEMENT);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_NODE_MANAGEMENT, MU_AUDIT_EVENT_METHOD);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_METHOD, MU_AUDIT_EVENT_CONDITION_ENABLE);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_CONDITION_ENABLE, MU_AUDIT_EVENT_CONDITION_ACKNOWLEDGE);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_CONDITION_ACKNOWLEDGE, MU_AUDIT_EVENT_CONDITION_CONFIRM);
    TEST_ASSERT_NOT_EQUAL(MU_AUDIT_EVENT_CONDITION_CONFIRM, MU_AUDIT_EVENT_CONDITION_RESPOND);
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

void test_audit_event_struct_has_method_fields(void) {
    mu_audit_event_t e;
    memset(&e, 0, sizeof(e));
    e.event_type = MU_AUDIT_EVENT_METHOD;
    e.specific.method.object_id.identifier.numeric = 2253u;
    e.specific.method.method_id.identifier.numeric = 11492u;
    TEST_ASSERT_EQUAL_UINT32(MU_AUDIT_EVENT_METHOD, e.event_type);
    TEST_ASSERT_EQUAL_UINT32(2253u, e.specific.method.object_id.identifier.numeric);
    TEST_ASSERT_EQUAL_UINT32(11492u, e.specific.method.method_id.identifier.numeric);
}

void test_audit_event_struct_has_condition_fields(void) {
    mu_audit_event_t e;
    memset(&e, 0, sizeof(e));
    e.event_type = MU_AUDIT_EVENT_CONDITION_ENABLE;
    e.specific.condition.condition_id.identifier.numeric = 6000u;
    TEST_ASSERT_EQUAL_UINT32(MU_AUDIT_EVENT_CONDITION_ENABLE, e.event_type);
    TEST_ASSERT_EQUAL_UINT32(6000u, e.specific.condition.condition_id.identifier.numeric);
}

void test_audit_event_struct_has_node_mgmt_fields(void) {
    mu_audit_event_t e;
    memset(&e, 0, sizeof(e));
    e.event_type = MU_AUDIT_EVENT_NODE_MANAGEMENT;
    TEST_ASSERT_EQUAL_UINT32(MU_AUDIT_EVENT_NODE_MANAGEMENT, e.event_type);
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
    setup_audit_server(false);
    mu_server_t *server = &s_audit_server;

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

/* T010: OPC-10000-5 §6.5.3 — callback receives event with populated fields */
void test_callback_receives_event_fields(void) {
    reset_callback_tracking();
    setup_audit_server(true);
    mu_server_t *server = &s_audit_server;
    server->config.time_adapter.get_time = audit_test_time;
    mu_server_set_audit_callback(server, audit_test_callback, NULL);

    mu_audit_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = MU_AUDIT_EVENT_CREATE_SESSION;
    event.status = true;
    event.server_id.data = (opcua_byte_t *)"TestServer";
    event.server_id.length = 10;
    event.client_audit_entry_id.data = (opcua_byte_t *)"AuditEntry1";
    event.client_audit_entry_id.length = 11;
    event.client_user_id.data = (opcua_byte_t *)"TestUser";
    event.client_user_id.length = 8;
    event.specific.create_session.session_id.identifier.numeric = 42;

    mu_raise_audit_event(server, &event);

    TEST_ASSERT_EQUAL_INT(1, g_callback_invocations);
    TEST_ASSERT_EQUAL_PTR(server, g_test_server);
    TEST_ASSERT_EQUAL_UINT64(123456789u, g_last_event_copy.action_timestamp);
    TEST_ASSERT_EQUAL_UINT32(MU_AUDIT_EVENT_CREATE_SESSION, g_last_event_copy.event_type);
    TEST_ASSERT_EQUAL_INT32(10, g_last_event_copy.server_id.length);
    TEST_ASSERT_EQUAL_STRING_LEN("TestServer", g_last_event_copy.server_id.data, 10);
    TEST_ASSERT_EQUAL_INT32(11, g_last_event_copy.client_audit_entry_id.length);
    TEST_ASSERT_EQUAL_STRING_LEN("AuditEntry1", g_last_event_copy.client_audit_entry_id.data, 11);
    TEST_ASSERT_EQUAL_UINT32(42, g_last_event_copy.specific.create_session.session_id.identifier.numeric);
}

/* T011: multiple callbacks fire in registration order */
void test_multiple_callbacks_fire_in_order(void) {
    reset_callback_tracking();
    setup_audit_server(true);
    mu_server_t *server = &s_audit_server;

    mu_server_set_audit_callback(server, audit_callback_a, NULL);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_add_audit_callback(server, audit_callback_b, NULL));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_server_add_audit_callback(server, audit_callback_c, NULL));

    mu_audit_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = MU_AUDIT_EVENT_CREATE_SESSION;

    mu_raise_audit_event(server, &event);

    TEST_ASSERT_EQUAL_INT(3, g_ordered_callback_count);
    TEST_ASSERT_EQUAL_INT(1, g_ordered_callback_ids[0]);
    TEST_ASSERT_EQUAL_INT(2, g_ordered_callback_ids[1]);
    TEST_ASSERT_EQUAL_INT(3, g_ordered_callback_ids[2]);
}

/* T012: OPC-10000-5 §6.5 — callback NOT invoked when auditing disabled */
void test_auditing_disabled_no_dispatch(void) {
    reset_callback_tracking();
    setup_audit_server(false);
    mu_server_t *server = &s_audit_server;
    mu_server_set_audit_callback(server, audit_test_callback, NULL);

    mu_audit_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = MU_AUDIT_EVENT_CREATE_SESSION;

    mu_raise_audit_event(server, &event);

    TEST_ASSERT_EQUAL_INT(0, g_callback_invocations);
}

/* T013: mu_server_add_audit_callback overflow returns BAD_OUTOFMEMORY */
void test_add_audit_callback_overflow(void) {
    setup_audit_server(false);
    mu_server_t *server = &s_audit_server;

    int dummy = 0;
    mu_server_set_audit_callback(server, audit_callback_a, &dummy);
    mu_server_add_audit_callback(server, audit_callback_b, &dummy);
    mu_server_add_audit_callback(server, audit_callback_c, &dummy);
    mu_server_add_audit_callback(server, audit_test_callback, &dummy);
    /* 5th should overflow */
    opcua_statuscode_t sc = mu_server_add_audit_callback(server, audit_test_callback, &dummy);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_OUTOFMEMORY, sc);
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
    RUN_TEST(test_audit_event_struct_has_method_fields);
    RUN_TEST(test_audit_event_struct_has_condition_fields);
    RUN_TEST(test_audit_event_struct_has_node_mgmt_fields);
    RUN_TEST(test_audit_disabled_flag);
    RUN_TEST(test_raise_audit_event_valid_input);
    RUN_TEST(test_raise_audit_event_null_safety);
    RUN_TEST(test_callback_receives_event_fields);
    RUN_TEST(test_multiple_callbacks_fire_in_order);
    RUN_TEST(test_auditing_disabled_no_dispatch);
    RUN_TEST(test_add_audit_callback_overflow);
#else
    RUN_TEST(test_audit_events_require_auditing_build);
#endif
    return UNITY_END();
}
