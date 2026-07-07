/* include/muc_opcua/client.h
 *
 * Nano Embedded UA Client Profile surface.
 * Spec grounding: OPC-10000-4 §5.4.2, §5.5.2-3, §5.6.2-4, §5.8.2-4, §5.10.2;
 * OPC-10000-6 §7.1.2.3/§7.1.2.4.
 */
#ifndef MUC_OPCUA_CLIENT_H
#define MUC_OPCUA_CLIENT_H

#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MU_CLIENT_DISCONNECTED = 0,
    MU_CLIENT_RESOLVING,
    MU_CLIENT_CONNECTING,
    MU_CLIENT_CHANNEL_OPENING,
    MU_CLIENT_CHANNEL_OPEN,
    MU_CLIENT_SESSION_CREATING,
    MU_CLIENT_ACTIVATING,
    MU_CLIENT_ACTIVE,
    MU_CLIENT_CLOSING,
    MU_CLIENT_ERROR
} mu_client_state_t;

typedef enum { MU_CLIENT_SECURITY_NONE = 0, MU_CLIENT_SECURITY_BASIC256SHA256 = 1 } mu_client_security_policy_t;

typedef enum { MU_IDENTITY_ANONYMOUS = 0 } mu_identity_token_type_t;

typedef struct {
    mu_identity_token_type_t type;
    mu_string_t policy_id;
} mu_identity_token_t;

typedef struct {
    void *context;
    opcua_statuscode_t (*connect)(void *context, const char *endpoint_url, opcua_uint32_t timeout_ms);
    opcua_statuscode_t (*send)(void *context, const opcua_byte_t *buffer, size_t length, size_t *bytes_sent,
                               opcua_uint32_t timeout_ms);
    opcua_statuscode_t (*recv)(void *context, opcua_byte_t *buffer, size_t capacity, size_t *bytes_read,
                               opcua_uint32_t timeout_ms);
    void (*close)(void *context);
} mu_client_transport_t;

typedef struct {
    const char *endpoint_url;
    mu_client_security_policy_t security_policy;
    opcua_uint32_t timeout_ms;
    opcua_byte_t *send_buffer;
    size_t send_buffer_size;
    opcua_byte_t *receive_buffer;
    size_t receive_buffer_size;
} mu_client_config_t;

typedef struct {
    const char *endpoint_url;
    const char *security_policy_uri;
    mu_client_security_policy_t security_policy;
} mu_endpoint_description_t;

typedef struct {
    mu_nodeid_t session_id;
    mu_nodeid_t authentication_token;
    opcua_boolean_t active;
} mu_client_session_t;

typedef struct {
    opcua_uint32_t channel_id;
    opcua_uint32_t token_id;
    opcua_uint32_t sequence_number;
    opcua_boolean_t open;
} mu_client_secure_channel_t;

typedef struct {
    const mu_nodeid_t *node_ids;
    size_t num_nodes;
    opcua_double_t max_age;
    opcua_uint32_t timestamps_to_return;
} mu_read_params_t;

typedef struct {
    mu_variant_t value;
    opcua_statuscode_t status;
    opcua_datetime_t source_timestamp;
} mu_read_value_t;

typedef struct {
    mu_nodeid_t node_id;
    opcua_uint32_t browse_direction;
    opcua_uint32_t node_class_mask;
    opcua_uint32_t result_mask;
} mu_browse_params_t;

typedef struct {
    mu_nodeid_t node_id;
    mu_qualified_name_t browse_name;
    opcua_uint32_t node_class;
} mu_reference_description_t;

typedef struct {
    opcua_statuscode_t status;
    mu_reference_description_t references[4];
    size_t reference_count;
    opcua_boolean_t has_continuation_point;
} mu_browse_result_t;

typedef struct {
    mu_nodeid_t starting_node;
    mu_qualified_name_t relative_path[4];
    size_t relative_path_length;
} mu_browse_path_t;

typedef struct {
    mu_client_config_t config;
    mu_client_transport_t *transport;
    mu_client_state_t state;
    mu_client_secure_channel_t channel;
    mu_client_session_t session;
    opcua_statuscode_t last_error;
    opcua_uint64_t state_deadline_ms;
} mu_client_t;

opcua_statuscode_t mu_client_init(mu_client_t *client, const mu_client_config_t *config,
                                  mu_client_transport_t *transport);
opcua_statuscode_t mu_client_connect(mu_client_t *client);
opcua_statuscode_t mu_client_disconnect(mu_client_t *client);
opcua_statuscode_t mu_client_get_endpoints(mu_client_t *client, mu_endpoint_description_t *endpoints,
                                           size_t *num_endpoints);
opcua_statuscode_t mu_client_create_session(mu_client_t *client);
opcua_statuscode_t mu_client_activate_session(mu_client_t *client, const mu_identity_token_t *identity);
opcua_statuscode_t mu_client_close_session(mu_client_t *client);
opcua_statuscode_t mu_client_read(mu_client_t *client, const mu_read_params_t *params, mu_read_value_t *results,
                                  size_t *num_results);
opcua_statuscode_t mu_client_browse(mu_client_t *client, const mu_browse_params_t *params, mu_browse_result_t *results,
                                    size_t *num_results);
opcua_statuscode_t mu_client_browse_next(mu_client_t *client, mu_browse_result_t *results, size_t *num_results);
opcua_statuscode_t mu_client_translate_browse_paths_to_node_ids(mu_client_t *client, const mu_browse_path_t *paths,
                                                                size_t num_paths, mu_nodeid_t *targets);

opcua_statuscode_t mu_client_state_transition(mu_client_t *client, mu_client_state_t next_state);
opcua_boolean_t mu_client_state_is_connected(mu_client_state_t state);
const char *mu_client_state_name(mu_client_state_t state);

#ifdef MUC_OPCUA_IS_HOST
typedef struct {
    int fd;
} mu_client_posix_transport_t;
opcua_statuscode_t mu_client_posix_transport_init(mu_client_transport_t *transport,
                                                  mu_client_posix_transport_t *storage);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_CLIENT_H */
