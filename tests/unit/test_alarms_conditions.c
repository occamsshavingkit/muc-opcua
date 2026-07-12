#include "core/server_internal.h"
#include "fake_platform.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/alarms_conditions.h"
#include "unity.h"

static mu_server_t server;
static opcua_byte_t rx_buffer[1024];
static opcua_byte_t tx_buffer[1024];

void setUp(void) {
#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
    fake_platform_init();

    mu_server_config_t config = {.endpoint_url = "opc.tcp://localhost:4840",
                                 .receive_buffer = rx_buffer,
                                 .receive_buffer_size = sizeof(rx_buffer),
                                 .send_buffer = tx_buffer,
                                 .send_buffer_size = sizeof(tx_buffer),
                                 .max_chunk_count = 1,
                                 .max_message_size = sizeof(rx_buffer)};

    mu_status_t status = mu_server_init(&server, &config);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
#endif
}

void tearDown(void) {}

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS

void test_mu_alarms_set_active_triggers_event(void) {
    mu_condition_id_t alarm_id;
    alarm_id.node_id = mu_nodeid_make_numeric(1, 1000);

    // Set active
    mu_status_t status = mu_alarms_set_active(&server, &alarm_id, true);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);

    // Verify tracked state
    TEST_ASSERT_EQUAL(1, server.condition_count);
    TEST_ASSERT_TRUE(server.conditions[0].active_state);
    TEST_ASSERT_FALSE(server.conditions[0].acked_state);
    TEST_ASSERT_TRUE(server.conditions[0].retain);
    TEST_ASSERT_TRUE(mu_nodeid_equal(&alarm_id.node_id, &server.conditions[0].id.node_id));
}

void test_mu_alarms_acknowledge_method_call(void) {
    mu_condition_id_t alarm_id;
    alarm_id.node_id = mu_nodeid_make_numeric(1, 1000);

    mu_alarms_set_active(&server, &alarm_id, true);

    mu_nodeid_t method_id = mu_nodeid_make_numeric(0, 9111);
    mu_variant_t output_args[2];
    size_t output_args_count = 0;
    opcua_statuscode_t status = mu_alarms_conditions_method_dispatch(&server, &method_id, &alarm_id.node_id, 0, NULL,
                                                                     &output_args_count, output_args);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_TRUE(server.conditions[0].acked_state);
    TEST_ASSERT_TRUE(server.conditions[0].retain);
}

void test_mu_alarms_trigger_dialog(void) {
    mu_condition_id_t dialog_id;
    dialog_id.node_id = mu_nodeid_make_numeric(1, 2000);

    mu_status_t status = mu_alarms_trigger_dialog(&server, &dialog_id, 0x03); // Responses 0 and 1
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_TRUE(server.conditions[1].is_dialog);
    TEST_ASSERT_EQUAL(0x03, server.conditions[1].valid_responses_mask);
}

void test_mu_alarms_dialog_respond_method(void) {
    mu_condition_id_t dialog_id;
    dialog_id.node_id = mu_nodeid_make_numeric(1, 2000);
    mu_alarms_trigger_dialog(&server, &dialog_id, 0x03);

    mu_nodeid_t method_id = mu_nodeid_make_numeric(0, 9069);
    mu_variant_t input_args[1];
    input_args[0].type = MU_VARIANT_TYPE_INT32;
    input_args[0].value.i32 = 1; // Valid response

    mu_variant_t output_args[2];
    size_t output_args_count = 0;
    opcua_statuscode_t status = mu_alarms_conditions_method_dispatch(&server, &method_id, &dialog_id.node_id, 1,
                                                                     input_args, &output_args_count, output_args);

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, status);
    TEST_ASSERT_FALSE(server.conditions[1].active_state);
    TEST_ASSERT_EQUAL(1, server.conditions[1].expected_response);
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
