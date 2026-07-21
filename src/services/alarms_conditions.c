#include "muc_opcua/services/alarms_conditions.h"
#include "../core/server_internal.h"
#include "muc_opcua/services/audit.h"
#include <string.h>

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS

#define MU_NODEID_ACKNOWLEDGEABLECONDITIONTYPE_ACKNOWLEDGE 9111
#define MU_NODEID_ACKNOWLEDGEABLECONDITIONTYPE_CONFIRM 9113
#define MU_NODEID_DIALOGCONDITIONTYPE_RESPOND 9069

static mu_condition_t *find_or_allocate_condition(mu_server_t *server, const mu_condition_id_t *id) {
    for (size_t i = 0; i < server->condition_count; ++i) {
        if (server->conditions[i].is_used && mu_nodeid_equal(&server->conditions[i].id.node_id, &id->node_id)) {
            return &server->conditions[i];
        }
    }

    if (server->condition_count < MU_INTERN_MAX_CONDITIONS) {
        mu_condition_t *cond = &server->conditions[server->condition_count++];
        (void)memset(cond, 0, sizeof(*cond));
        cond->is_used = true;
        cond->id = *id;
        return cond;
    }

    return NULL;
}

static mu_condition_t *find_condition(mu_server_t *server, const mu_condition_id_t *id) {
    for (size_t i = 0; i < server->condition_count; ++i) {
        if (server->conditions[i].is_used && mu_nodeid_equal(&server->conditions[i].id.node_id, &id->node_id)) {
            return &server->conditions[i];
        }
    }
    return NULL;
}

opcua_statuscode_t mu_alarms_set_active(struct mu_server *server, const mu_condition_id_t *id, bool active) {
    if (!server || !id) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    mu_condition_t *cond = find_or_allocate_condition(server, id);
    if (!cond) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    if (cond->active_state != active) {
        cond->active_state = active;
        if (active) {
            cond->acked_state = false;
            cond->retain = true;
        } else if (cond->acked_state) {
            cond->retain = false;
        }

#if MUC_OPCUA_CU_AUDITING
        {
            mu_audit_event_t audit_ev;
            (void)memset(&audit_ev, 0, sizeof(audit_ev));
            audit_ev.event_type = MU_AUDIT_EVENT_CONDITION_ENABLE;
            audit_ev.status = true;
            audit_ev.specific.condition.condition_id = id->node_id;
            mu_raise_audit_event(server, &audit_ev);
        }
#endif
    }

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_alarms_trigger_dialog(struct mu_server *server, const mu_condition_id_t *id,
                                            uint32_t valid_responses_mask) {
    if (!server || !id) {
        return MU_STATUS_BAD_INVALIDARGUMENT;
    }

    mu_condition_t *cond = find_or_allocate_condition(server, id);
    if (!cond) {
        return MU_STATUS_BAD_OUTOFMEMORY;
    }

    cond->is_dialog = true;
    cond->valid_responses_mask = valid_responses_mask;
    cond->active_state = true;
    cond->retain = true;

#if MUC_OPCUA_CU_AUDITING
    {
        mu_audit_event_t audit_ev;
        (void)memset(&audit_ev, 0, sizeof(audit_ev));
        audit_ev.event_type = MU_AUDIT_EVENT_CONDITION_RESPOND;
        audit_ev.status = true;
        audit_ev.specific.condition.condition_id = id->node_id;
        mu_raise_audit_event(server, &audit_ev);
    }
#endif

    return MU_STATUS_GOOD;
}

opcua_statuscode_t mu_alarms_conditions_method_dispatch(mu_server_t *server, const mu_nodeid_t *method_id,
                                                        const mu_nodeid_t *object_id, size_t input_args_count,
                                                        const mu_variant_t *input_args, size_t *output_args_count,
                                                        mu_variant_t *output_args) {
    (void)output_args;
    if (method_id->identifier_type != MU_NODEID_NUMERIC || method_id->namespace_index != 0) {
        return MU_STATUS_BAD_METHODINVALID;
    }

    mu_condition_id_t cid;
    cid.node_id = *object_id;

    if (method_id->identifier.numeric == MU_NODEID_ACKNOWLEDGEABLECONDITIONTYPE_ACKNOWLEDGE) {
        mu_condition_t *cond = find_condition(server, &cid);
        if (!cond) {
            return MU_STATUS_BAD_NODEIDUNKNOWN; // Condition doesn't exist
        }
        if (cond->acked_state) {
            return MU_STATUS_BAD_CONDITIONBRANCHALREADYACKED; // Map to correct OPC UA status
        }

        // Mark acked
        cond->acked_state = true;
        if (!cond->active_state) {
            cond->retain = false;
        }

#if MUC_OPCUA_CU_AUDITING
        {
            mu_audit_event_t audit_ev;
            (void)memset(&audit_ev, 0, sizeof(audit_ev));
            audit_ev.event_type = MU_AUDIT_EVENT_CONDITION_ACKNOWLEDGE;
            audit_ev.status = true;
            audit_ev.specific.condition.condition_id = cid.node_id;
            mu_raise_audit_event(server, &audit_ev);
        }
#endif

        *output_args_count = 0;
        return MU_STATUS_GOOD;
    }

    if (method_id->identifier.numeric == MU_NODEID_DIALOGCONDITIONTYPE_RESPOND) {
        mu_condition_t *cond = find_condition(server, &cid);
        if (!cond || !cond->is_dialog) {
            return MU_STATUS_BAD_NODEIDUNKNOWN;
        }
        if (!cond->active_state) {
            return MU_STATUS_BAD_DIALOGNOTACTIVE;
        }
        if (input_args_count < 1 || input_args[0].type != MU_TYPE_INT32) {
            return MU_STATUS_BAD_INVALIDARGUMENT;
        }

        int response = input_args[0].value.i32;
        if ((cond->valid_responses_mask & (1 << response)) == 0) {
            return MU_STATUS_BAD_DIALOGRESPONSEINVALID;
        }

        cond->expected_response = response;
        cond->active_state = false;
        cond->retain = false;

#if MUC_OPCUA_CU_AUDITING
        {
            mu_audit_event_t audit_ev;
            (void)memset(&audit_ev, 0, sizeof(audit_ev));
            audit_ev.event_type = MU_AUDIT_EVENT_CONDITION_RESPOND;
            audit_ev.status = true;
            audit_ev.specific.condition.condition_id = cid.node_id;
            mu_raise_audit_event(server, &audit_ev);
        }
#endif

        *output_args_count = 0;
        return MU_STATUS_GOOD;
    }

    if (method_id->identifier.numeric == MU_NODEID_ACKNOWLEDGEABLECONDITIONTYPE_CONFIRM) {
        mu_condition_t *cond = find_condition(server, &cid);
        if (!cond) {
            return MU_STATUS_BAD_NODEIDUNKNOWN;
        }
        if (cond->confirmed_state) {
            return MU_STATUS_BAD_CONDITIONBRANCHALREADYCONFIRMED;
        }

        cond->confirmed_state = true;

#if MUC_OPCUA_CU_AUDITING
        {
            mu_audit_event_t audit_ev;
            (void)memset(&audit_ev, 0, sizeof(audit_ev));
            audit_ev.event_type = MU_AUDIT_EVENT_CONDITION_CONFIRM;
            audit_ev.status = true;
            audit_ev.specific.condition.condition_id = cid.node_id;
            mu_raise_audit_event(server, &audit_ev);
        }
#endif

        *output_args_count = 0;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_METHODINVALID;
}

#endif /* MUC_OPCUA_SERVICE_ALARMS_CONDITIONS */
