/* src/services/subscription.h
 *
 * No-heap data-change subscription engine for the Micro Embedded Device 2017 Server
 * Profile (the Embedded Data Change Subscription Server Facet). All state is fixed-size
 * and lives in the caller-owned struct mu_server; nothing here allocates. Compiled only
 * when MICRO_OPCUA_SUBSCRIPTIONS is defined.
 *
 * Service mapping: Subscription Service Set OPC 10000-4 §5.14, MonitoredItem Service Set
 * §5.13, MonitoringParameters §7.21, DataChangeFilter §7.17, NotificationMessage /
 * DataChangeNotification §7.20.
 */
#ifndef MICRO_OPCUA_SERVICES_SUBSCRIPTION_H
#define MICRO_OPCUA_SERVICES_SUBSCRIPTION_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/types.h"
#include "micro_opcua/status.h"
#include <stddef.h>
#include <stdbool.h>

#if MICRO_OPCUA_SUBSCRIPTIONS

/* Fixed engine capacities. Integrator-overridable with -D (e.g. -DMU_MAX_MONITORED_ITEMS=16). */
#ifndef MU_MAX_SUBSCRIPTIONS
#define MU_MAX_SUBSCRIPTIONS 2
#endif
#ifndef MU_MAX_MONITORED_ITEMS
#define MU_MAX_MONITORED_ITEMS 8
#endif
#ifndef MU_MAX_PUBLISH_REQUESTS
#define MU_MAX_PUBLISH_REQUESTS 4
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

/* A single data MonitoredItem (OPC 10000-4 §5.13, §7.21). */
typedef struct {
    bool in_use;
    opcua_uint32_t monitored_item_id;       /* server-assigned IntegerId */
    opcua_uint32_t subscription_id;         /* owning subscription */
    opcua_uint32_t client_handle;           /* echoed in every notification */
    mu_nodeid_t node_id;                     /* monitored node (numeric, or string into the buffer) */
    opcua_byte_t node_id_string[MU_MAX_MONITORED_STRING]; /* backing store for a string identifier */
    opcua_uint32_t attribute_id;            /* usually Value (13) */
    opcua_uint32_t sampling_interval_ms;    /* revised */
    mu_monitoring_mode_t monitoring_mode;
    mu_datachange_trigger_t trigger;
    /* Change-detection state. last_value holds scalar built-in types only, which covers
       the monitorable base nodes (ServerStatus.CurrentTime, .State, ServiceLevel). */
    mu_variant_t last_value;
    opcua_statuscode_t last_status;
    bool has_value;                         /* a baseline sample has been taken */
    bool pending;                           /* a change is queued, awaiting the next Publish */
    opcua_uint64_t next_sample_ms;          /* monotonic tick of the next sample */
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

/* A Subscription (OPC 10000-4 §5.14.1.3 state variables). */
typedef struct {
    bool in_use;
    opcua_uint32_t subscription_id;         /* server-assigned IntegerId */
    opcua_uint32_t session_id;              /* owning session */
    opcua_uint32_t publishing_interval_ms;  /* revised, integer ms */
    opcua_uint32_t max_keep_alive_count;    /* revised */
    opcua_uint32_t lifetime_count;          /* revised */
    opcua_uint32_t max_notifications_per_publish;
    opcua_byte_t priority;
    bool publishing_enabled;
    /* Runtime counters / timers. */
    opcua_uint32_t sequence_number;         /* next NotificationMessage sequence number */
    opcua_uint32_t keep_alive_counter;
    opcua_uint32_t lifetime_counter;
    opcua_uint64_t next_publish_ms;         /* monotonic tick when the publishing timer fires */
    bool more_notifications;
    mu_retransmit_slot_t retransmit;
} mu_subscription_t;

/* A parked Publish request (OPC 10000-4 §5.14.5): held until the publishing timer fires
   with notifications or a keep-alive due. Routed back to the owning session/connection. */
typedef struct {
    bool in_use;
    opcua_uint32_t session_id;
    opcua_uint32_t request_handle;
    opcua_uint32_t request_id;              /* secure-channel request id for the async response */
    opcua_uint64_t enqueued_ms;
} mu_publish_request_t;

/* The whole engine: fixed-size arrays, embedded in struct mu_server. */
typedef struct {
    mu_subscription_t subscriptions[MU_MAX_SUBSCRIPTIONS];
    mu_monitored_item_t monitored_items[MU_MAX_MONITORED_ITEMS];
    mu_publish_request_t publish_queue[MU_MAX_PUBLISH_REQUESTS];
    opcua_uint32_t next_subscription_id;    /* monotonic id allocator */
    opcua_uint32_t next_monitored_item_id;
} mu_subscriptions_t;

struct mu_server; /* forward declaration; the tick reads the address space + send path */

void mu_subscriptions_init(mu_subscriptions_t *subs);

/* Create a Subscription (OPC 10000-4 §5.14.2). The publishing interval is already
   converted to integer ms and clamped by the caller; lifetime/keep-alive counts are
   revised here to server bounds. Returns Bad_TooManySubscriptions when full. */
opcua_statuscode_t mu_subscription_create(mu_subscriptions_t *subs,
                                          opcua_uint32_t session_id,
                                          opcua_uint32_t publishing_interval_ms,
                                          opcua_uint32_t requested_lifetime_count,
                                          opcua_uint32_t requested_max_keep_alive_count,
                                          opcua_uint32_t max_notifications_per_publish,
                                          opcua_byte_t priority,
                                          bool publishing_enabled,
                                          opcua_uint64_t now_ms,
                                          mu_subscription_t **out_sub);

/* Delete a Subscription and all its MonitoredItems (OPC 10000-4 §5.14.8). The session_id
   must own the subscription. Returns Bad_SubscriptionIdInvalid for an unknown id. */
opcua_statuscode_t mu_subscription_delete(mu_subscriptions_t *subs,
                                          opcua_uint32_t session_id,
                                          opcua_uint32_t subscription_id);

/* Look up a Subscription by id owned by the given session (NULL if absent). */
mu_subscription_t *mu_subscription_find(mu_subscriptions_t *subs,
                                        opcua_uint32_t session_id,
                                        opcua_uint32_t subscription_id);

/* Allocate a MonitoredItem slot under a subscription (OPC 10000-4 §5.13.2). Assigns a
   unique non-zero monitored_item_id and records the owning subscription; returns
   Bad_TooManyMonitoredItems when the array is full. The caller fills the node/attribute/
   parameters and the initial sample into the returned slot. */
opcua_statuscode_t mu_monitored_item_alloc(mu_subscriptions_t *subs,
                                           opcua_uint32_t subscription_id,
                                           mu_monitored_item_t **out_item);

/* Delete a MonitoredItem under a subscription (OPC 10000-4 §5.13.6). Returns
   Bad_MonitoredItemIdInvalid for an unknown id. */
opcua_statuscode_t mu_monitored_item_delete(mu_subscriptions_t *subs,
                                            opcua_uint32_t subscription_id,
                                            opcua_uint32_t monitored_item_id);

/* Park a Publish request for asynchronous completion by the publishing timer
   (OPC 10000-4 §5.14.5). Returns Bad_TooManyPublishRequests when the queue is full. */
opcua_statuscode_t mu_publish_request_enqueue(mu_subscriptions_t *subs,
                                              opcua_uint32_t session_id,
                                              opcua_uint32_t request_id,
                                              opcua_uint32_t request_handle,
                                              opcua_uint64_t now_ms);

/* Poll-driven sampling + publishing-timer advance. Called once per mu_server_poll with
   the current monotonic tick. Samples due MonitoredItems, fires due publishing timers,
   and emits parked Publish responses (US2/US3). A no-op until those land. */
void mu_subscriptions_tick(struct mu_server *server, opcua_uint64_t now_ms);

#endif /* MICRO_OPCUA_SUBSCRIPTIONS */

#endif /* MICRO_OPCUA_SERVICES_SUBSCRIPTION_H */
