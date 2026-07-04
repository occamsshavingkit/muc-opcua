/* src/services/subscription.h
 *
 * No-heap data-change subscription engine for the Micro Embedded Device 2017 Server
 * Profile (the Embedded Data Change Subscription Server Facet). All state is fixed-size
 * and lives in the caller-owned struct mu_server; nothing here allocates. Compiled only
 * when MUC_OPCUA_SUBSCRIPTIONS is defined.
 *
 * Service mapping: Subscription Service Set OPC 10000-4 §5.14, MonitoredItem Service Set
 * §5.13, MonitoringParameters §7.21, DataChangeFilter §7.17, NotificationMessage /
 * DataChangeNotification §7.20.
 */
#ifndef MUC_OPCUA_SERVICES_SUBSCRIPTION_H
#define MUC_OPCUA_SERVICES_SUBSCRIPTION_H

#include "muc_opcua/address_space.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"
#include <stdbool.h>
#include <stddef.h>

#if MUC_OPCUA_SUBSCRIPTIONS

/*
 * The embedded build raises these maxima via -D overrides to meet the Standard
 * DataChange Subscription 2017 Server Facet minimums (OPC-10000-7 §6.6.17):
 * MU_MAX_MONITORED_ITEMS >= 100 (Monitor Items 100),
 * MU_MAX_SUBSCRIPTIONS >= 2 (Subscription Minimum 02),
 * MU_MAX_PUBLISH_REQUESTS >= 5 (Subscription Publish Min 05), and
 * MU_MONITORED_QUEUE_DEPTH >= 2 (Monitor MinQueueSize_02).
 */

/* OPC-10000-4 §7.21 MonitoringParameters queueSize, §5.13.2. */
#ifndef MU_MONITORED_QUEUE_DEPTH
/* Default queue size is 1 for micro; embedded overrides to >=2. */
#define MU_MONITORED_QUEUE_DEPTH 1
#endif

/* OPC-10000-4 §5.13.5 SetTriggering. */
#ifndef MU_MAX_TRIGGER_LINKS
#define MU_MAX_TRIGGER_LINKS 4
#endif

/* Fixed engine capacities. Integrator-overridable with -D (e.g. -DMU_MAX_MONITORED_ITEMS=16). */
#ifndef MU_MAX_SUBSCRIPTIONS
#define MU_MAX_SUBSCRIPTIONS 2
#endif
#ifndef MU_MAX_MONITORED_ITEMS
#define MU_MAX_MONITORED_ITEMS 8
#endif
/* Bitmap words for the per-tick reportable-items scan (T003).
 * Sized for the maximum MU_MAX_MONITORED_ITEMS across all profiles. */
#define MU_REPORTABLE_BITMAP_WORDS ((MU_MAX_MONITORED_ITEMS + 31u) / 32u)
#ifndef MU_MAX_PUBLISH_REQUESTS
#define MU_MAX_PUBLISH_REQUESTS 4
#endif
#ifndef MU_MAX_PUBLISH_ACKS
#define MU_MAX_PUBLISH_ACKS 4
#endif
#ifndef MU_MAX_MONITORED_STRING
#define MU_MAX_MONITORED_STRING 32
#endif
#ifndef MU_RETRANSMIT_BYTES
#define MU_RETRANSMIT_BYTES 256
#endif

/* Revised publishing-interval bounds in integer milliseconds. The wire carries a
   Duration (Double); the dispatch handler converts to/from integer ms at the boundary
   with integer-only bit math (the Cortex-M0+ target has no FPU), so the engine itself
   is floating-point-free. OPC 10000-4 §5.14.2.2. */
#ifndef MU_MIN_PUBLISHING_INTERVAL_MS
#define MU_MIN_PUBLISHING_INTERVAL_MS 50u
#endif
#ifndef MU_MAX_PUBLISHING_INTERVAL_MS
#define MU_MAX_PUBLISHING_INTERVAL_MS 60000u
#endif

/* MonitoringMode (OPC 10000-4 §5.13.1.2). */
typedef enum {
    MU_MONITORING_MODE_DISABLED = 0,
    MU_MONITORING_MODE_SAMPLING = 1,
    MU_MONITORING_MODE_REPORTING = 2
} mu_monitoring_mode_t;

/* DataChangeTrigger (OPC 10000-4 §7.17.2). */
typedef enum {
    MU_DATACHANGE_TRIGGER_STATUS = 0,
    MU_DATACHANGE_TRIGGER_STATUS_VALUE = 1,
    MU_DATACHANGE_TRIGGER_STATUS_VALUE_TIMESTAMP = 2
} mu_datachange_trigger_t;

typedef enum {
    MU_DEADBAND_TYPE_NONE = 0,
    MU_DEADBAND_TYPE_ABSOLUTE = 1,
    MU_DEADBAND_TYPE_PERCENT = 2
} mu_deadband_type_t;

typedef struct {
    opcua_double_t processing_interval; /* in milliseconds */
    opcua_datetime_t last_calculation;  /* timestamp of last aggregate calculation */
    union {
        struct {
            opcua_double_t sum;
        } avg;
        struct {
            mu_variant_t min_val;
        } min;
        struct {
            mu_variant_t max_val;
        } max;
    } accumulator;
    opcua_uint32_t aggregate_type; /* MU_ID_AGGREGATETYPE_AVERAGE, MINIMUM, MAXIMUM */
    opcua_uint32_t sample_count;   /* number of samples in the current interval */
} mu_aggregate_state_t;

/* A single data MonitoredItem (OPC 10000-4 §5.13, §7.21). */
typedef struct {
    /* 8-byte aligned fields */
    opcua_uint64_t next_sample_ms; /* monotonic tick of the next sample */
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    opcua_double_t deadband_value;
    opcua_double_t last_reported_numeric;
    mu_aggregate_state_t aggregate_state;
#endif
    const mu_node_t *resolved_node; /* cached static address-space resolution */

    /* Struct/Union fields (4-byte aligned on 32-bit) */
    mu_variant_t last_value;
    mu_nodeid_t node_id; /* monitored node (numeric, or string into the buffer) */

    /* 4-byte aligned fields */
    opcua_uint32_t monitored_item_id;    /* server-assigned IntegerId */
    opcua_uint32_t subscription_id;      /* owning subscription */
    opcua_uint32_t client_handle;        /* echoed in every notification */
    opcua_uint32_t attribute_id;         /* usually Value (13) */
    opcua_uint32_t sampling_interval_ms; /* revised */
    opcua_statuscode_t last_status;
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    opcua_uint32_t queue_size;
    opcua_uint32_t triggered_items[MU_MAX_TRIGGER_LINKS];

    struct {
        mu_variant_t value;
        opcua_statuscode_t status;
    } queue[MU_MONITORED_QUEUE_DEPTH];
#endif

    /* 1-byte aligned arrays */
    opcua_byte_t node_id_string[MU_MAX_MONITORED_STRING]; /* backing store for a string identifier */

    /* 1-byte fields */
    opcua_byte_t monitoring_mode; /* mu_monitoring_mode_t */
    opcua_byte_t trigger;         /* mu_datachange_trigger_t */
    bool in_use;
    bool has_value; /* a baseline sample has been taken */
    bool pending;   /* a change is queued, awaiting the next Publish */
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    opcua_byte_t deadband_type; /* mu_deadband_type_t */
    bool has_reported;
    bool has_aggregate;
    opcua_byte_t queue_head;
    opcua_byte_t queue_tail;
    opcua_byte_t queue_count;
    bool discard_oldest;
    bool queue_overflow;
    opcua_byte_t triggered_count;
#endif
#ifdef MUC_OPCUA_EVENTS
    opcua_byte_t select_clauses[8];
    opcua_byte_t select_clauses_count;
#endif
} mu_monitored_item_t;

/* Retransmission slot for Republish (OPC 10000-4 §5.14.6): the last NotificationMessage
   body, retained until acknowledged. */
typedef struct {
    bool valid;
    opcua_uint32_t sequence_number;
    opcua_int64_t publish_time;
    opcua_byte_t message[MU_RETRANSMIT_BYTES];
    size_t message_len;
} mu_retransmit_slot_t;

#ifdef MUC_OPCUA_EVENTS
#define MU_MAX_EVENT_QUEUE_SIZE 8
typedef struct {
    mu_event_notification_t queue[MU_MAX_EVENT_QUEUE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} mu_event_queue_t;
#endif

/* A Subscription (OPC 10000-4 §5.14.1.3 state variables). */
typedef struct {
    /* 8-byte aligned fields */
    opcua_uint64_t next_publish_ms; /* monotonic tick when the publishing timer fires */

    /* Nested struct (aligned to 4 bytes on 32-bit) */
    mu_retransmit_slot_t retransmit;

    /* 4-byte fields */
    opcua_uint32_t subscription_id;        /* server-assigned IntegerId */
    opcua_uint32_t session_id;             /* owning session */
    opcua_uint32_t publishing_interval_ms; /* revised, integer ms */
    opcua_uint32_t max_keep_alive_count;   /* revised */
    opcua_uint32_t lifetime_count;         /* revised */
    opcua_uint32_t max_notifications_per_publish;
    opcua_uint32_t sequence_number; /* next NotificationMessage sequence number */
    opcua_uint32_t keep_alive_counter;
    opcua_uint32_t lifetime_counter;

    /* 1-byte fields */
    opcua_byte_t priority;
    bool in_use;
    bool publishing_enabled;
    bool more_notifications;
#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    bool resend_data_pending; /* OPC-10000-5 §9.2 ResendData method latch */
#endif
#ifdef MUC_OPCUA_EVENTS
    mu_event_queue_t event_queue;
#endif
} mu_subscription_t;

/* A parked Publish request (OPC 10000-4 §5.14.5): held until the publishing timer fires
   with notifications or a keep-alive due. Routed back to the owning session/connection. */
typedef struct {
    bool in_use;
    opcua_uint32_t session_id;
    opcua_uint32_t request_handle;
    opcua_uint32_t request_id; /* secure-channel request id for the async response */
    opcua_uint64_t enqueued_ms;
    /* Acknowledgement results for the acks carried by this request, echoed in the
       PublishResponse results[] when it is answered (OPC 10000-4 §5.14.5.2). */
    opcua_uint32_t ack_count;
    opcua_statuscode_t ack_results[MU_MAX_PUBLISH_ACKS];
} mu_publish_request_t;

/* The whole engine: fixed-size arrays, embedded in struct mu_server. */
typedef struct {
    mu_subscription_t subscriptions[MU_MAX_SUBSCRIPTIONS];
    mu_monitored_item_t monitored_items[MU_MAX_MONITORED_ITEMS];
    mu_publish_request_t publish_queue[MU_MAX_PUBLISH_REQUESTS];
    opcua_uint32_t next_subscription_id; /* monotonic id allocator */
    opcua_uint32_t next_monitored_item_id;
    opcua_uint32_t active_monitored_items_count;
    /* Per-tick bitmap of items with pending/queued data (T003, OPC-10000-4
     * §5.13.1.2). Populated during the sampling loop; the count, encode and
     * clear passes iterate only set bits instead of scanning the full array. */
    opcua_uint32_t reportable_bitmap[MU_REPORTABLE_BITMAP_WORDS];
} mu_subscriptions_t;

struct mu_server; /* forward declaration; the tick reads the address space + send path */

void mu_subscriptions_init(mu_subscriptions_t *subs);

/* Create a Subscription (OPC 10000-4 §5.14.2). The publishing interval is already
   converted to integer ms and clamped by the caller; lifetime/keep-alive counts are
   revised here to server bounds. Returns Bad_TooManySubscriptions when full. */
opcua_statuscode_t mu_subscription_create(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                          opcua_uint32_t publishing_interval_ms,
                                          opcua_uint32_t requested_lifetime_count,
                                          opcua_uint32_t requested_max_keep_alive_count,
                                          opcua_uint32_t max_notifications_per_publish, opcua_byte_t priority,
                                          bool publishing_enabled, opcua_uint64_t now_ms, mu_subscription_t **out_sub);

/* Apply revised Subscription parameters (OPC 10000-4 §5.14.3). The interval is already
   converted to integer ms by dispatch; count revision remains integer-only here. */
void mu_subscription_apply_parameters(mu_subscription_t *sub, opcua_uint32_t publishing_interval_ms,
                                      opcua_uint32_t requested_lifetime_count,
                                      opcua_uint32_t requested_max_keep_alive_count,
                                      opcua_uint32_t max_notifications_per_publish, opcua_byte_t priority);

/* Delete a Subscription and all its MonitoredItems (OPC 10000-4 §5.14.8). The session_id
   must own the subscription. Returns Bad_SubscriptionIdInvalid for an unknown id. */
opcua_statuscode_t mu_subscription_delete(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                          opcua_uint32_t subscription_id);

/* Look up a Subscription by id owned by the given session (NULL if absent). */
mu_subscription_t *mu_subscription_find(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                        opcua_uint32_t subscription_id);

/* Allocate a MonitoredItem slot under a subscription (OPC 10000-4 §5.13.2). Assigns a
   unique non-zero monitored_item_id and records the owning subscription; returns
   Bad_TooManyMonitoredItems when the array is full. The caller fills the node/attribute/
   parameters and the initial sample into the returned slot. */
opcua_statuscode_t mu_monitored_item_alloc(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                           mu_monitored_item_t **out_item);

/* Delete a MonitoredItem under a subscription (OPC 10000-4 §5.13.6). Returns
   Bad_MonitoredItemIdInvalid for an unknown id. */
opcua_statuscode_t mu_monitored_item_delete(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                            opcua_uint32_t monitored_item_id);

#if MUC_OPCUA_SUBSCRIPTIONS_STANDARD
/* SetTriggering link storage (OPC-10000-4 §5.13.5). Both MonitoredItems must
   belong to the named Subscription. */
opcua_statuscode_t mu_monitored_item_add_trigger_link(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                                      opcua_uint32_t triggering_item_id, opcua_uint32_t linked_item_id);

opcua_statuscode_t mu_monitored_item_remove_trigger_link(mu_subscriptions_t *subs, opcua_uint32_t subscription_id,
                                                         opcua_uint32_t triggering_item_id,
                                                         opcua_uint32_t linked_item_id);

/* OPC-10000-5 §9.1 GetMonitoredItems: enumerate server-assigned handles and the
   client handles for MonitoredItems owned by the session's Subscription. */
opcua_statuscode_t mu_subscription_get_monitored_items(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                                       opcua_uint32_t subscription_id, opcua_uint32_t *server_handles,
                                                       opcua_uint32_t *client_handles, size_t max_handles,
                                                       size_t *out_count);

/* OPC-10000-5 §9.2 ResendData: request that the next Publish re-reports current
   values for all reporting data MonitoredItems in the Subscription. */
opcua_statuscode_t mu_subscription_request_resend_data(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                                       opcua_uint32_t subscription_id);
#endif

/* Park a Publish request for asynchronous completion by the publishing timer
   (OPC 10000-4 §5.14.5). Returns Bad_TooManyPublishRequests when the queue is full; on
   success, *out_req (if non-NULL) points at the parked slot so the caller can record the
   acknowledgement results to echo when the request is answered. */
opcua_statuscode_t mu_publish_request_enqueue(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                              opcua_uint32_t request_id, opcua_uint32_t request_handle,
                                              opcua_uint64_t now_ms, mu_publish_request_t **out_req);

/* Acknowledge a retained NotificationMessage (OPC 10000-4 §5.14.5): if the subscription's
   retransmit slot holds the given sequence number, purge it. Returns GOOD,
   Bad_SubscriptionIdInvalid (unknown sub for the session), or Bad_SequenceNumberUnknown. */
opcua_statuscode_t mu_subscription_acknowledge(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                               opcua_uint32_t subscription_id, opcua_uint32_t sequence_number);

/* Fetch a retained NotificationMessage body for Republish (OPC 10000-4 §5.14.6). On a
   match, returns GOOD and points *out_msg / *out_len at the retained NotificationMessage
   bytes; otherwise Bad_SubscriptionIdInvalid or Bad_MessageNotAvailable. */
opcua_statuscode_t mu_subscription_republish(mu_subscriptions_t *subs, opcua_uint32_t session_id,
                                             opcua_uint32_t subscription_id, opcua_uint32_t sequence_number,
                                             const opcua_byte_t **out_msg, size_t *out_len);

/* Poll-driven sampling + publishing-timer advance. Called once per mu_server_poll with
   the current monotonic tick. Samples due MonitoredItems, fires due publishing timers,
   and emits parked Publish responses (US2/US3). A no-op until those land. */
void mu_subscriptions_tick(struct mu_server *server, opcua_uint64_t now_ms);

#endif /* MUC_OPCUA_SUBSCRIPTIONS */

#endif /* MUC_OPCUA_SERVICES_SUBSCRIPTION_H */
