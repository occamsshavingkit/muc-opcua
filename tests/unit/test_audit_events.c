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
#include "../../src/services/subscription_publish/common.h"
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

#if MUC_OPCUA_CU_EVENTS
void test_audit_event_select_fields_resolve_from_payload(void) {
    setup_audit_server(true);

    mu_audit_payload_t payload;
    memset(&payload, 0, sizeof(payload));
    memcpy(payload.event_id, "AUD1", 4u);
    payload.event_id_length = 4u;
    payload.source_node = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {.numeric = 2253u}};
    payload.status = false;
    payload.action_timestamp = 123456789;
    payload.session_id = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {.numeric = 42u}};
    payload.attribute_id = MU_ATTRIBUTEID_VALUE;
    payload.old_value.type = MU_TYPE_INT32;
    payload.old_value.value.i32 = 10;
    payload.new_value.type = MU_TYPE_INT32;
    payload.new_value.value.i32 = 20;
    memcpy(payload.server_id.data, "server", 6u);
    payload.server_id.len = 6;
    memcpy(payload.client_audit_entry_id.data, "entry", 5u);
    payload.client_audit_entry_id.len = 5;
    memcpy(payload.client_user_id.data, "user", 4u);
    payload.client_user_id.len = 4;
    memcpy(payload.secure_channel_id.data, "7", 1u);
    payload.secure_channel_id.len = 1;

    mu_event_notification_t event;
    memset(&event, 0, sizeof(event));
    event.audit_ref = mu_audit_pool_store(&s_audit_server, &payload);
    event.event_type = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {.numeric = 2100u}};
    event.time = payload.action_timestamp;
    event.message = (mu_string_t){16, (const opcua_byte_t *)"Attribute Write"};
    event.severity = 1u;

    mu_variant_t value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_EVENTID);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTESTRING, value.type);
    TEST_ASSERT_EQUAL_INT32(4, value.value.bytestr.length);
    TEST_ASSERT_EQUAL_MEMORY("AUD1", value.value.bytestr.data, 4u);

    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_SOURCENODE);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, value.type);
    TEST_ASSERT_EQUAL_UINT32(2253u, value.value.nodeid.identifier.numeric);
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_SOURCENAME);
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, value.type);
    TEST_ASSERT_EQUAL_STRING_LEN("Server", value.value.str.data, 6u);
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_RECEIVETIME);
    TEST_ASSERT_EQUAL(MU_TYPE_DATETIME, value.type);
    TEST_ASSERT_EQUAL_INT64(123456789, value.value.dt);
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_STATUS);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, value.type);
    TEST_ASSERT_FALSE(value.value.b);
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_ATTRIBUTEID);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, value.type);
    TEST_ASSERT_EQUAL_UINT32(MU_ATTRIBUTEID_VALUE, value.value.ui32);
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_OLDVALUE);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value.type);
    TEST_ASSERT_EQUAL_INT32(10, value.value.i32);
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_NEWVALUE);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value.type);
    TEST_ASSERT_EQUAL_INT32(20, value.value.i32);
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_SESSIONID);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, value.type);
    TEST_ASSERT_EQUAL_UINT32(42u, value.value.nodeid.identifier.numeric);

    s_audit_server.audit_pool[event.audit_ref.index].sequence++;
    value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_STATUS);
    TEST_ASSERT_EQUAL(MU_TYPE_NULL, value.type);
}

void test_non_audit_event_resolves_audit_fields_to_null(void) {
    setup_audit_server(true);
    mu_event_notification_t event;
    memset(&event, 0, sizeof(event));
    mu_variant_t value = mu_event_notification_resolve_field(&s_audit_server, &event, MU_EVENT_FIELD_STATUS);
    TEST_ASSERT_EQUAL(MU_TYPE_NULL, value.type);
}

static mu_event_notification_t raise_and_capture_notification(const mu_audit_event_t *audit_event) {
    setup_audit_server(true);
    s_audit_server.config.time_adapter.get_time = audit_test_time;
    s_audit_server.subs.subscriptions[0].in_use = true;
    mu_raise_audit_event(&s_audit_server, audit_event);
    TEST_ASSERT_EQUAL_UINT32(1u, s_audit_server.subs.subscriptions[0].event_queue.count);
    return s_audit_server.subs.subscriptions[0].event_queue.queue[0];
}

static void assert_audit_base_fields(const mu_event_notification_t *notification, opcua_uint32_t event_type,
                                     bool status) {
    TEST_ASSERT_EQUAL_UINT32(event_type, notification->event_type.identifier.numeric);
    TEST_ASSERT_EQUAL_INT64(123456789, notification->time);
    mu_variant_t value = mu_event_notification_resolve_field(&s_audit_server, notification, MU_EVENT_FIELD_EVENTID);
    TEST_ASSERT_EQUAL(MU_TYPE_BYTESTRING, value.type);
    TEST_ASSERT_TRUE(value.value.bytestr.length > 0);
    value = mu_event_notification_resolve_field(&s_audit_server, notification, MU_EVENT_FIELD_SOURCENODE);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, value.type);
    TEST_ASSERT_EQUAL_UINT32(2253u, value.value.nodeid.identifier.numeric);
    value = mu_event_notification_resolve_field(&s_audit_server, notification, MU_EVENT_FIELD_STATUS);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, value.type);
    TEST_ASSERT_EQUAL(status, value.value.b);
}

void test_open_secure_channel_audit_maps_to_event(void) {
    mu_audit_event_t audit_event;
    memset(&audit_event, 0, sizeof(audit_event));
    audit_event.event_type = MU_AUDIT_EVENT_OPEN_SECURE_CHANNEL;
    audit_event.status = false;
    audit_event.specific.open_channel.secure_channel_id = (mu_string_t){2, (const opcua_byte_t *)"17"};
    mu_event_notification_t notification = raise_and_capture_notification(&audit_event);
    assert_audit_base_fields(&notification, 2060u, false);
    mu_variant_t value =
        mu_event_notification_resolve_field(&s_audit_server, &notification, MU_EVENT_FIELD_SECURECHANNELID);
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, value.type);
    TEST_ASSERT_EQUAL_STRING_LEN("17", value.value.str.data, 2u);
}

void test_create_session_audit_maps_to_event(void) {
    mu_audit_event_t audit_event;
    memset(&audit_event, 0, sizeof(audit_event));
    audit_event.event_type = MU_AUDIT_EVENT_CREATE_SESSION;
    audit_event.status = true;
    audit_event.specific.create_session.session_id = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {.numeric = 42u}};
    mu_event_notification_t notification = raise_and_capture_notification(&audit_event);
    assert_audit_base_fields(&notification, 2071u, true);
    mu_variant_t value = mu_event_notification_resolve_field(&s_audit_server, &notification, MU_EVENT_FIELD_SESSIONID);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, value.type);
    TEST_ASSERT_EQUAL_UINT32(42u, value.value.nodeid.identifier.numeric);
}

void test_activate_session_audit_maps_to_event(void) {
    mu_audit_event_t audit_event;
    memset(&audit_event, 0, sizeof(audit_event));
    audit_event.event_type = MU_AUDIT_EVENT_ACTIVATE_SESSION;
    audit_event.status = false;
    audit_event.specific.activate_session.session_id = (mu_nodeid_t){0, MU_NODEID_NUMERIC, {.numeric = 43u}};
    audit_event.specific.activate_session.user_name = (mu_string_t){5, (const opcua_byte_t *)"alice"};
    mu_event_notification_t notification = raise_and_capture_notification(&audit_event);
    assert_audit_base_fields(&notification, 2075u, false);
    mu_variant_t value = mu_event_notification_resolve_field(&s_audit_server, &notification, MU_EVENT_FIELD_SESSIONID);
    TEST_ASSERT_EQUAL(MU_TYPE_NODEID, value.type);
    TEST_ASSERT_EQUAL_UINT32(43u, value.value.nodeid.identifier.numeric);
    value = mu_event_notification_resolve_field(&s_audit_server, &notification, MU_EVENT_FIELD_CLIENTUSERID);
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, value.type);
    TEST_ASSERT_EQUAL_STRING_LEN("alice", value.value.str.data, 5u);
}

void test_write_update_audit_maps_to_event(void) {
    mu_audit_event_t audit_event;
    memset(&audit_event, 0, sizeof(audit_event));
    audit_event.event_type = MU_AUDIT_EVENT_WRITE_UPDATE;
    audit_event.status = true;
    audit_event.specific.write_update.attribute_id = MU_ATTRIBUTEID_VALUE;
    audit_event.specific.write_update.old_value.type = MU_TYPE_INT32;
    audit_event.specific.write_update.old_value.value.i32 = 10;
    audit_event.specific.write_update.new_value.type = MU_TYPE_INT32;
    audit_event.specific.write_update.new_value.value.i32 = 20;
    mu_event_notification_t notification = raise_and_capture_notification(&audit_event);
    assert_audit_base_fields(&notification, 2100u, true);
    mu_variant_t value =
        mu_event_notification_resolve_field(&s_audit_server, &notification, MU_EVENT_FIELD_ATTRIBUTEID);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, value.type);
    TEST_ASSERT_EQUAL_UINT32(MU_ATTRIBUTEID_VALUE, value.value.ui32);
    value = mu_event_notification_resolve_field(&s_audit_server, &notification, MU_EVENT_FIELD_OLDVALUE);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value.type);
    TEST_ASSERT_EQUAL_INT32(10, value.value.i32);
    value = mu_event_notification_resolve_field(&s_audit_server, &notification, MU_EVENT_FIELD_NEWVALUE);
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value.type);
    TEST_ASSERT_EQUAL_INT32(20, value.value.i32);
}
#endif

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
#if MUC_OPCUA_CU_EVENTS
    RUN_TEST(test_audit_event_select_fields_resolve_from_payload);
    RUN_TEST(test_non_audit_event_resolves_audit_fields_to_null);
    RUN_TEST(test_open_secure_channel_audit_maps_to_event);
    RUN_TEST(test_create_session_audit_maps_to_event);
    RUN_TEST(test_activate_session_audit_maps_to_event);
    RUN_TEST(test_write_update_audit_maps_to_event);
#endif
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
