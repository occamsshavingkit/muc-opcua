#ifndef MICRO_OPCUA_PUBSUB_H
#define MICRO_OPCUA_PUBSUB_H

#include "micro_opcua/types.h"

#ifndef MU_PUBSUB_MAX_FIELDS
#define MU_PUBSUB_MAX_FIELDS 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Represents one UADP/UDP publisher connection (OPC-10000-14 sections 7.2 and 7.3). */
typedef struct {
    uint16_t port;
    bool enabled;
    uint32_t publisher_id;
    const char *address; /* NULL defaults to IPv4 broadcast for host adapters. */
} mu_pubsub_connection_t;

/* One published field encoded with UADP Variant field encoding
 * (OPC-10000-14 sections 7.2.4.5.4 and 7.2.4.5.5). */
typedef struct {
    mu_variant_t value;
} mu_pubsub_field_t;

/* Contains caller-owned fields to publish. The field array must outlive any
 * writer group registered with mu_server_add_writer_group(). */
typedef struct {
    uint16_t data_set_writer_id;
    const mu_pubsub_field_t *fields;
    size_t field_count;
} mu_pubsub_dataset_writer_t;

/* Groups one DataSetWriter and handles cooperative publishing timing. */
typedef struct {
    uint16_t writer_group_id;
    uint32_t publishing_interval_ms;
    uint32_t last_publish_time_ms; /* Internal state */
    mu_pubsub_dataset_writer_t dataset_writer;
} mu_pubsub_writer_group_t;

/* Caller-owned decode output for the scoped UADP subscriber subset
 * (OPC-10000-14 sections 5.4.2.1, 5.4.2.2, and 7.2.4.4.2).
 * The subset exposes one UInt32 PublisherId, one DataSetWriterId, and
 * scalar Variant fields decoded per OPC-10000-6 section 5.2.2.16.
 * fields points to caller-provided storage for field_capacity entries and
 * may be NULL only when field_capacity is 0; the decoder does not allocate
 * memory or retain buffer pointers after return. */
typedef struct {
    uint32_t publisher_id;
    uint16_t data_set_writer_id;
    mu_variant_t *fields;
    size_t field_capacity;
    size_t field_count;
} mu_pubsub_received_message_t;

struct mu_server;
opcua_statuscode_t mu_server_add_writer_group(struct mu_server *server, const mu_pubsub_writer_group_t *wg);
opcua_statuscode_t mu_pubsub_poll(struct mu_server *server);
opcua_statuscode_t mu_encode_uadp_network_message(const mu_pubsub_connection_t *conn,
                                                  const mu_pubsub_writer_group_t *wg, opcua_byte_t *buffer,
                                                  size_t buffer_size, size_t *bytes_written);
/* Decode one scoped UADP NetworkMessage payload into caller-owned output.
 * message->fields provides the only Variant storage; field_count is reset
 * before decoding and set to the decoded scalar field count on success. */
opcua_statuscode_t mu_decode_uadp_network_message(const opcua_byte_t *buffer, size_t buffer_size,
                                                  mu_pubsub_received_message_t *message);

#ifdef __cplusplus
}
#endif

#endif // MICRO_OPCUA_PUBSUB_H
