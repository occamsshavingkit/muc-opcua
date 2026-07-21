#include "core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/alarms_conditions.h"
#include "unity.h"

static _Alignas(8) opcua_byte_t server_storage[MU_SERVER_STORAGE_BYTES];
static mu_server_t *server;
static opcua_byte_t rx_buffer[16384];
static opcua_byte_t tx_buffer[16384];

void setUp(void) {
#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
    mu_server_config_t config = {.endpoint_url = "opc.tcp://localhost:4840",
                                 .receive_buffer = rx_buffer,
                                 .receive_buffer_size = sizeof(rx_buffer),
                                 .send_buffer = tx_buffer,
                                 .send_buffer_size = sizeof(tx_buffer),
                                 .max_sessions = 1,
                                 .max_secure_channels = 1,
                                 .max_chunk_count = 1,
                                 .max_message_size = sizeof(rx_buffer)};

    fake_platform_init(&config.tcp_adapter, &config.time_adapter, &config.entropy_adapter);

    opcua_statuscode_t status = mu_server_init(server_storage, sizeof(server_storage), &config, &server);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
#endif
}

void tearDown(void) {}

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS

void test_mu_alarms_set_active_triggers_event(void) {
    mu_condition_id_t alarm_id;
    alarm_id.node_id.namespace_index = 1;
    alarm_id.node_id.identifier_type = MU_NODEID_NUMERIC;
    alarm_id.node_id.identifier.numeric = 1000;

    opcua_statuscode_t status = mu_alarms_set_active(server, &alarm_id, true);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    TEST_ASSERT_EQUAL(1, server->condition_count);
    TEST_ASSERT_TRUE(server->conditions[0].active_state);
    TEST_ASSERT_FALSE(server->conditions[0].acked_state);
    TEST_ASSERT_TRUE(server->conditions[0].retain);
    TEST_ASSERT_TRUE(mu_nodeid_equal(&alarm_id.node_id, &server->conditions[0].id.node_id));
}

void test_mu_alarms_acknowledge_method_call(void) {
    mu_condition_id_t alarm_id;
    alarm_id.node_id.namespace_index = 1;
    alarm_id.node_id.identifier_type = MU_NODEID_NUMERIC;
    alarm_id.node_id.identifier.numeric = 1000;

    mu_alarms_set_active(server, &alarm_id, true);

    mu_nodeid_t method_id;
    method_id.namespace_index = 0;
    method_id.identifier_type = MU_NODEID_NUMERIC;
    method_id.identifier.numeric = 9111;
    mu_variant_t output_args[2];
    size_t output_args_count = 0;
    opcua_statuscode_t status = mu_alarms_conditions_method_dispatch(server, &method_id, &alarm_id.node_id, 0, NULL,
                                                                     &output_args_count, output_args);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_TRUE(server->conditions[0].acked_state);
    TEST_ASSERT_TRUE(server->conditions[0].retain);
}

void test_mu_alarms_trigger_dialog(void) {
    mu_condition_id_t dialog_id;
    dialog_id.node_id.namespace_index = 1;
    dialog_id.node_id.identifier_type = MU_NODEID_NUMERIC;
    dialog_id.node_id.identifier.numeric = 2000;

    opcua_statuscode_t status = mu_alarms_trigger_dialog(server, &dialog_id, 0x03);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_TRUE(server->conditions[0].is_dialog);
    TEST_ASSERT_EQUAL(0x03, server->conditions[0].valid_responses_mask);
}

void test_mu_alarms_dialog_respond_method(void) {
    mu_condition_id_t dialog_id;
    dialog_id.node_id.namespace_index = 1;
    dialog_id.node_id.identifier_type = MU_NODEID_NUMERIC;
    dialog_id.node_id.identifier.numeric = 2000;
    mu_alarms_trigger_dialog(server, &dialog_id, 0x03);

    mu_nodeid_t method_id;
    method_id.namespace_index = 0;
    method_id.identifier_type = MU_NODEID_NUMERIC;
    method_id.identifier.numeric = 9069;
    mu_variant_t input_args[1];
    input_args[0].type = MU_TYPE_INT32;
    input_args[0].value.i32 = 1;

    mu_variant_t output_args[2];
    size_t output_args_count = 0;
    opcua_statuscode_t status = mu_alarms_conditions_method_dispatch(server, &method_id, &dialog_id.node_id, 1,
                                                                     input_args, &output_args_count, output_args);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_FALSE(server->conditions[0].active_state);
    TEST_ASSERT_EQUAL(1, server->conditions[0].expected_response);
}

#endif /* MUC_OPCUA_SERVICE_ALARMS_CONDITIONS */

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
    RUN_TEST(test_mu_alarms_set_active_triggers_event);
    RUN_TEST(test_mu_alarms_acknowledge_method_call);
    RUN_TEST(test_mu_alarms_trigger_dialog);
    RUN_TEST(test_mu_alarms_dialog_respond_method);
#endif
    return UNITY_END();
}
