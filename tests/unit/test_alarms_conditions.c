#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
#include "../../src/core/server_internal.h" // IWYU pragma: keep
#endif
#include "fake_platform.h"
#include "muc_opcua/server.h"
#include "muc_opcua/services/alarms_conditions.h"
#include "unity.h"
#include <string.h>

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

static const mu_node_t *find_base_node(opcua_uint32_t id) {
    mu_nodeid_t node_id = {0, MU_NODEID_NUMERIC, {.numeric = id}};
    return mu_resolve_node(NULL, NULL, NULL, &node_id);
}

static void assert_type_node(opcua_uint32_t id, mu_node_class_t node_class, const char *browse_name) {
    const mu_node_t *node = find_base_node(id);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(node_class, node->node_class);
    TEST_ASSERT_EQUAL_INT((int)strlen(browse_name), node->browse_name.length);
    TEST_ASSERT_EQUAL_MEMORY(browse_name, node->browse_name.data, strlen(browse_name));
}

/* T221-T224: OPC-10000-9 §§5.5, 5.7, 5.8 and 5.8.17. */
void test_gauntlet_alarm_type_nodes_resolve(void) {
    assert_type_node(2782, MU_NODECLASS_OBJECTTYPE, "ConditionType");
    assert_type_node(2881, MU_NODECLASS_OBJECTTYPE, "AcknowledgeableConditionType");
    assert_type_node(2915, MU_NODECLASS_OBJECTTYPE, "AlarmConditionType");
    assert_type_node(2929, MU_NODECLASS_OBJECTTYPE, "ShelvedStateMachineType");
}

/* T226: the OPC-10000-9 lifecycle model uses the Part 16 state-machine
 * ObjectTypes and the Part 9 two-state/condition VariableTypes. */
void test_gauntlet_alarm_lifecycle_type_nodes_resolve(void) {
    assert_type_node(2307, MU_NODECLASS_OBJECTTYPE, "StateType");
    assert_type_node(2310, MU_NODECLASS_OBJECTTYPE, "TransitionType");
    assert_type_node(2771, MU_NODECLASS_OBJECTTYPE, "FiniteStateMachineType");
    assert_type_node(8995, MU_NODECLASS_VARIABLETYPE, "TwoStateVariableType");
    assert_type_node(9002, MU_NODECLASS_VARIABLETYPE, "ConditionVariableType");
}

/* T228/T230/T231: OPC-10000-10 §5.2. ProgramDiagnosticDataType(894)
 * is retained for compatibility; ProgramDiagnostic2DataType(24033) is its
 * corrected replacement. NodeId 2378 is ProgramTransitionEventType, not a
 * distinct "ProgramType". */
void test_gauntlet_program_type_nodes_resolve(void) {
    assert_type_node(894, MU_NODECLASS_DATATYPE, "ProgramDiagnosticDataType");
    assert_type_node(2378, MU_NODECLASS_OBJECTTYPE, "ProgramTransitionEventType");
    assert_type_node(2391, MU_NODECLASS_OBJECTTYPE, "ProgramStateMachineType");
    assert_type_node(24033, MU_NODECLASS_DATATYPE, "ProgramDiagnostic2DataType");
}

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
    RUN_TEST(test_gauntlet_alarm_type_nodes_resolve);
    RUN_TEST(test_gauntlet_alarm_lifecycle_type_nodes_resolve);
    RUN_TEST(test_gauntlet_program_type_nodes_resolve);
    RUN_TEST(test_mu_alarms_set_active_triggers_event);
    RUN_TEST(test_mu_alarms_acknowledge_method_call);
    RUN_TEST(test_mu_alarms_trigger_dialog);
    RUN_TEST(test_mu_alarms_dialog_respond_method);
#endif
    return UNITY_END();
}
