#ifndef MUC_OPCUA_SERVICES_ALARMS_CONDITIONS_H
#define MUC_OPCUA_SERVICES_ALARMS_CONDITIONS_H

#include "muc_opcua/config.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/types.h"

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Identifier for a Condition instance.
 */
typedef struct {
    mu_nodeid_t node_id;
} mu_condition_id_t;

/**
 * @brief State tracking for an AcknowledgeableCondition / AlarmCondition.
 */
typedef struct {
    mu_condition_id_t id;
    bool active_state;
    bool acked_state;
    bool confirmed_state;
    bool retain;
    bool is_dialog;
    int expected_response;
    uint32_t valid_responses_mask;
    bool is_used;
} mu_condition_t;

struct mu_server;

/**
 * @brief Sets a condition active or inactive and triggers corresponding events.
 */
opcua_statuscode_t mu_alarms_set_active(struct mu_server *server, const mu_condition_id_t *id, bool active);

/**
 * @brief Triggers a DialogCondition.
 */
opcua_statuscode_t mu_alarms_trigger_dialog(struct mu_server *server, const mu_condition_id_t *id,
                                     uint32_t valid_responses_mask);

/**
 * @brief Method dispatch handler for Alarms & Conditions Methods.
 */
opcua_statuscode_t mu_alarms_conditions_method_dispatch(struct mu_server *server, const mu_nodeid_t *method_id,
                                                 const mu_nodeid_t *object_id, size_t input_args_count,
                                                 const mu_variant_t *input_args, size_t *output_args_count,
                                                 mu_variant_t *output_args);

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICE_ALARMS_CONDITIONS */

#endif /* MUC_OPCUA_SERVICES_ALARMS_CONDITIONS_H */
