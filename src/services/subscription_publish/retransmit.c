#include "common.h"

#if MUC_OPCUA_SUBSCRIPTIONS

bool publish_response_oversize_status(opcua_statuscode_t status) {
    return status == MU_STATUS_BAD_ENCODINGERROR || status == MU_STATUS_BAD_OUTOFMEMORY ||
           status == MU_STATUS_BAD_RESPONSETOOLARGE;
}

void store_retransmit(mu_subscription_t *sub, opcua_uint32_t sequence_number, opcua_datetime_t publish_time,
                      const opcua_byte_t *body, size_t body_length) {
    sub->retransmit.valid = false;
    sub->retransmit.message_len = 0u;

    if (body_length > MU_RETRANSMIT_BYTES) {
        return;
    }

    sub->retransmit.sequence_number = sequence_number;
    sub->retransmit.publish_time = publish_time;
    (void)memcpy(sub->retransmit.message, body, body_length);
    sub->retransmit.message_len = body_length;
    sub->retransmit.valid = true;
}

opcua_statuscode_t mu_subscription_acknowledge(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                               opcua_uint32_t subscription_id, opcua_uint32_t sequence_number) {
    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    if (sub->retransmit.valid && sub->retransmit.sequence_number == sequence_number) {
        sub->retransmit.valid = false;
        sub->retransmit.message_len = 0u;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_SEQUENCENUMBERUNKNOWN;
}

opcua_statuscode_t mu_subscription_republish(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                             opcua_uint32_t subscription_id, opcua_uint32_t sequence_number,
                                             const opcua_byte_t **out_msg, size_t *out_len) {
    if (out_msg == NULL || out_len == NULL) {
        return MU_STATUS_BAD_INTERNALERROR;
    }

    *out_msg = NULL;
    *out_len = 0u;

    mu_subscription_t *sub = mu_subscription_find(subs, session_id, subscription_id);
    if (sub == NULL) {
        return MU_STATUS_BAD_SUBSCRIPTIONIDINVALID;
    }

    if (sub->retransmit.valid && sub->retransmit.sequence_number == sequence_number) {
        *out_msg = sub->retransmit.message;
        *out_len = sub->retransmit.message_len;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_MESSAGENOTAVAILABLE;
}

#endif /* MUC_OPCUA_SUBSCRIPTIONS */
