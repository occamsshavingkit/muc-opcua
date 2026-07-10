#ifndef MUC_OPCUA_SERVICES_SUBSCRIPTION_PUBLISH_COMMON_H
#define MUC_OPCUA_SERVICES_SUBSCRIPTION_PUBLISH_COMMON_H

#if MUC_OPCUA_SUBSCRIPTIONS

#include "../subscription.h"
#include <string.h>

#include "../../core/ctz.h"
#include "../../core/server_internal.h"
#include "../../core/service_dispatch.h"
#include "../read.h"
#include "../service_header.h"
#include "muc_opcua/address_space.h"

#define MU_PUBLISH_BODY_BYTES 512u
#define MU_ID_DATACHANGENOTIFICATION_ENCODING_DEFAULTBINARY 811u
/* StatusChangeNotification_Encoding_DefaultBinary = 820 (OPC UA NodeIds.csv). Was
   813 (a non-existent node), which broke standard clients' decode of the Publish
   ExtensionObject TypeId → stream desync → spurious Bad_ServerNameMissing (#284). */
#define MU_ID_STATUSCHANGENOTIFICATION_ENCODING_DEFAULTBINARY 820u

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
#define MU_STATUSCODE_INFOTYPE_DATAVALUE_OVERFLOW 0x00000480u
#ifndef MU_STATUS_BAD_TOOMANYOPERATIONS
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS ((opcua_statuscode_t)0x80100000u)
#else
#define MU_LOCAL_STATUS_BAD_TOOMANYOPERATIONS MU_STATUS_BAD_TOOMANYOPERATIONS
#endif
#endif

/* --- internal helpers shared across sub-files --- */

/* deadband.c (filter evaluation) */
bool monitored_item_reportable(const mu_monitored_item_t *item, const mu_subscription_t *sub);
opcua_int32_t count_reportable_items(const struct mu_server *server, mu_subscription_t *sub);
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
void prepare_reportable_queues(struct mu_server *server, const mu_subscription_t *sub);
void enqueue_resend_data(struct mu_server *server, mu_subscription_t *sub);
void monitored_item_drain_reported_entries(mu_monitored_item_t *item, opcua_int32_t *remaining);
#endif
void clear_reported_items(struct mu_server *server, const mu_subscription_t *sub
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
                          ,
                          opcua_int32_t report_count
#endif
);

/* notification.c */
opcua_statuscode_t write_data_change_notification(mu_binary_writer_t *w, const struct mu_server *server,
                                                  const mu_subscription_t *sub, opcua_int32_t report_count);
opcua_statuscode_t write_status_change_notification(mu_binary_writer_t *w, opcua_statuscode_t status);
#ifdef MUC_OPCUA_EVENTS
opcua_statuscode_t write_event_notification_list(mu_binary_writer_t *w, struct mu_server *server,
                                                 const mu_subscription_t *sub);
#endif

/* publish.c internal */
opcua_statuscode_t write_publish_response_prefix(mu_binary_writer_t *w, opcua_uint32_t request_handle);
void backpatch_int32(opcua_byte_t *buffer, size_t position, opcua_int32_t value);
opcua_datetime_t publish_time_now(const struct mu_server *server);
bool publish_request_dequeue(mu_subscriptions_t *subs, opcua_uint32_t session_id, mu_publish_request_t *out_request);
bool publish_request_dequeue_valid(struct mu_server *server, opcua_uint32_t session_id, opcua_uint64_t now_ms,
                                   mu_publish_request_t *out_request);
opcua_uint32_t current_sequence_number(mu_subscription_t *sub);
void advance_sequence_number(mu_subscription_t *sub);
void advance_publish_timer(mu_subscription_t *sub, opcua_uint64_t now_ms);
void prepare_publish_notification_data(struct mu_server *server, mu_subscription_t *sub, opcua_int32_t *report_count,
                                       bool *has_data, bool *has_events);
void publish_response_encode_and_emit(struct mu_server *server, mu_subscription_t *sub, bool has_data,
                                      opcua_int32_t report_count, bool has_events, opcua_uint64_t now_ms);
void publish_due(struct mu_server *server, opcua_uint64_t now_ms);

/* retransmit.c */
void store_retransmit(mu_subscription_t *sub, opcua_uint32_t sequence_number, opcua_datetime_t publish_time,
                      const opcua_byte_t *body, size_t body_length);
bool publish_response_oversize_status(opcua_statuscode_t status);

#endif /* MUC_OPCUA_SUBSCRIPTIONS */

#endif /* MUC_OPCUA_SERVICES_SUBSCRIPTION_PUBLISH_COMMON_H */
