#ifndef MICRO_OPCUA_PUBSUB_H
#define MICRO_OPCUA_PUBSUB_H

#include "micro_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Represents a UDP connection for publishing
typedef struct {
    uint16_t port;
    bool enabled;
    uint32_t publisher_id;
} mu_pubsub_connection_t;

// Contains variables to publish
typedef struct {
    uint16_t data_set_writer_id;
    // In a real implementation this would point to a PublishedDataSet
    // For this MVP, we just use dummy values or hardcoded values
} mu_pubsub_dataset_writer_t;

// Groups dataset writers and handles timing
typedef struct {
    uint16_t writer_group_id;
    uint32_t publishing_interval_ms;
    uint32_t last_publish_time_ms; // Internal state
    mu_pubsub_dataset_writer_t dataset_writer;
} mu_pubsub_writer_group_t;

struct mu_server;
opcua_statuscode_t mu_server_add_writer_group(struct mu_server *server, const mu_pubsub_writer_group_t *wg);
opcua_statuscode_t mu_pubsub_poll(struct mu_server *server);
opcua_statuscode_t mu_encode_uadp_network_message(const mu_pubsub_connection_t *conn, 
                                                  const mu_pubsub_writer_group_t *wg,
                                                  opcua_byte_t *buffer, size_t buffer_size, 
                                                  size_t *bytes_written);

#ifdef __cplusplus
}
#endif

#endif // MICRO_OPCUA_PUBSUB_H
