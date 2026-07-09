/* include/muc_opcua/config.h */
#ifndef MUC_OPCUA_CONFIG_H
#define MUC_OPCUA_CONFIG_H

#include <stddef.h>

/* Feature 025 (F9): reject illegal feature-gate combinations at compile time. */
#include "muc_opcua/features.h"

/* Spec 056: the single source of truth for every tunable capacity dimension.
 * Resolves each MU_INTERN_MAX_* via the default<profile<user cascade. ALL code
 * compiles off the MU_INTERN_* macros; the public MU_MAX_* knobs are inputs only
 * (read in the cascade). The *_STORAGE_BYTES math below consumes MU_INTERN_*. */
#include "muc_opcua/capacities.h"

/* UB7 / FR-026: gate dynamic (heap) allocation. When undefined or 0 the
 * library compiles with zero calloc / malloc calls. Overridable with -D. */
#ifndef MUC_OPCUA_ALLOW_HEAP
#define MUC_OPCUA_ALLOW_HEAP 1
#endif

/* Optimization-remediation compile-time knobs.
 * Defaults are intentionally overridable with -D for embedded profiles. */

/* E1: address-space index capacity is MU_INTERN_MAX_ADDRESS_SPACE_NODES
 * (capacities.h). The index RAM scales by 2 bytes per node. */

/* E2: opaque per-channel cipher-context storage. 512 bytes gives headroom
 * for an AES-256 key schedule; backend size asserts are added with adapter use. */
#ifndef MU_CIPHER_CTX_SIZE
#ifdef MUC_OPCUA_HAVE_OPENSSL
#define MU_CIPHER_CTX_SIZE 32
#else
#define MU_CIPHER_CTX_SIZE 512
#endif
#endif

/* E4: shared secure-path scratch owned by struct mu_server when
 * MUC_OPCUA_SECURITY is enabled. Sized for the worst-case secure response /
 * OPN scratch + session-handshake buffers (CreateSession/ActivateSession). */
#ifndef MU_SECURE_SCRATCH_SIZE
#define MU_SECURE_SCRATCH_SIZE 14336
#endif

/* Feature 025 (F2/F5): persistent copy of the current SecureChannel's client
 * application-instance certificate, retained for the channel lifetime so the
 * ActivateSession ClientSignature can be verified (OPC-10000-4 §5.7.3). The OPN
 * sender certificate otherwise points into transient receive-buffer memory that
 * is gone by ActivateSession time. Sized for an RSA-4096 app-instance cert, and
 * only present in MUC_OPCUA_SECURITY builds. */
#ifndef MU_MAX_CLIENT_CERT_SIZE
#define MU_MAX_CLIENT_CERT_SIZE 1600
#endif

/* Issue #197: the address-space lookup index now lives in struct mu_server
 * (mu_address_space_index_t user_address_space_index) instead of a file-static.
 * Reserve room for it in the caller storage block: the order[] array
 * (2 bytes/node) plus bookkeeping/padding headroom. */
#ifndef MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES
#define MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES (MU_INTERN_MAX_ADDRESS_SPACE_NODES * 2 + 128)
#endif

/* D9: mu_status_name() strings are OFF for embedded builds by default; this
 * macro is intentionally left undefined unless supplied with -D. */

/* Default chunk size limits (OPC 10000-6 7.1.2.3, 7.1.2.4) */
#define MU_MIN_CHUNK_SIZE 8192
#define MU_DEFAULT_MAX_CHUNK_COUNT 1
#define MU_DEFAULT_MAX_MESSAGE_SIZE 8192

#ifndef MU_CLIENT_MAX_SESSIONS
#define MU_CLIENT_MAX_SESSIONS 1
#endif

#ifndef MU_CLIENT_DEFAULT_TIMEOUT_MS
#define MU_CLIENT_DEFAULT_TIMEOUT_MS 5000
#endif

/* Concurrent TCP connections, secure channels, dynamic address-space limits,
 * and concurrent Sessions are all resolved in capacities.h as
 * MU_INTERN_MAX_CONNECTIONS / MU_INTERN_MAX_SECURE_CHANNELS (1:1 with
 * connections) / MU_INTERN_MAX_DYNAMIC_* / MU_INTERN_MAX_SESSIONS. Each is
 * overridable with the public MU_MAX_* knob per the cascade. */

/* Per-connection stream reassembly buffer used when MUC_OPCUA_MULTIPLE_CONNECTIONS
 * is enabled. OPC-10000-6 §7.1.2.3 defines an 8192-byte minimum receive buffer
 * negotiation floor, so the backing storage must not be smaller than that floor. */
#ifndef MU_CONNECTION_RX_BUFFER_SIZE
#define MU_CONNECTION_RX_BUFFER_SIZE MU_MIN_CHUNK_SIZE
#elif MU_CONNECTION_RX_BUFFER_SIZE < MU_MIN_CHUNK_SIZE
#error "MU_CONNECTION_RX_BUFFER_SIZE must be at least MU_MIN_CHUNK_SIZE"
#endif

#ifndef MU_CONNECTION_BASE_STORAGE_BYTES
/* client handle, stream bookkeeping, TCP negotiation state, and secure-channel
 * state outside the per-connection RX buffer. The host ABI is currently the
 * larger bound (1328 B; ARM Cortex-M0+ is 1288 B) when security is enabled. */
#define MU_CONNECTION_BASE_STORAGE_BYTES 1328
#endif

/* String length limits.
 * MU_MAX_ENCODED_STRING_LENGTH bounds any String/ByteString on the wire. It must be
 * large enough for the Hello EndpointUrl, which "shall be less than 4096 bytes"
 * (OPC 10000-6 7.1.2.3), and for endpoint/application URIs in responses.
 * MU_MAX_STRING_VALUE_LENGTH is the separate, tighter limit for a bounded String
 * *Variable value* (64 UTF-8 bytes per the feature spec); it is enforced at the
 * value-source layer, not on every wire string. */
#define MU_MAX_ENCODED_STRING_LENGTH 4096
#define MU_MAX_STRING_VALUE_LENGTH 64

/* Fixed storage allocation size for the server.
 * The subscription engine (MUC_OPCUA_SUBSCRIPTIONS, the Micro profile) adds the
 * fixed-size subscription / MonitoredItem / parked-Publish arrays to struct mu_server,
 * so the no-heap storage block is larger when it is compiled in. Security builds
 * also reserve server-owned scratch for large secure-channel transient buffers. */
#ifdef MUC_OPCUA_SECURITY
/* secure_scratch + the two per-direction prepared cipher contexts that now live in
 * client_keys/server_keys (mu_sym_keys_t.cipher_ctx, 2 x MU_CIPHER_CTX_SIZE). */
#define MU_SERVER_SECURITY_STORAGE_BYTES (MU_SECURE_SCRATCH_SIZE + 2 * MU_CIPHER_CTX_SIZE + MU_MAX_CLIENT_CERT_SIZE)
#else
#define MU_SERVER_SECURITY_STORAGE_BYTES 0
#endif

/* Feature 025 (T038): single definition of every storage sub-total. Each macro
 * already evaluates to 0 when its feature is disabled, so MU_SERVER_STORAGE_BYTES
 * needs only one form. The base constant is the sole thing that differs between a
 * subscription-capable server (3072, adds the fixed subscription/MonitoredItem/
 * parked-Publish arrays) and a non-subscription server (1024). Previously the two
 * arms duplicated every sub-total macro, which invited drift. */
#ifdef MUC_OPCUA_SUBSCRIPTIONS
#define MU_SERVER_STORAGE_BASE_BYTES 3200
#else
#define MU_SERVER_STORAGE_BASE_BYTES 1152
#endif

/* Multi-chunk reassembly buffer size. OPC-10000-6 §6.7.2 */
#ifdef MUC_OPCUA_MULTI_CHUNK
#ifndef MU_CHUNK_ASSEMBLY_BUFFER_SIZE
#define MU_CHUNK_ASSEMBLY_BUFFER_SIZE 8192
#endif
#endif

/* Multi-chunk reassembly storage. Sized for the default max message plus
   assembler bookkeeping. */
#ifdef MUC_OPCUA_MULTI_CHUNK
#define MU_CHUNK_ASSEMBLY_STORAGE_BYTES (MU_CHUNK_ASSEMBLY_BUFFER_SIZE + 32)
#else
#define MU_CHUNK_ASSEMBLY_STORAGE_BYTES 0
#endif

#if defined(MUC_OPCUA_SUBSCRIPTIONS) && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
/* OPC-10000-7 §6.6.17 Standard DataChange Subscription storage; zero unless
 * the Standard facet is enabled. Covers monitored-item arrays, subscription
 * arrays, parked publish requests, and event infrastructure. Capacities come
 * from capacities.h (MU_INTERN_*). */
#define MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES                                                                        \
    (MU_INTERN_MAX_MONITORED_ITEMS * (MU_INTERN_MONITORED_QUEUE_DEPTH * 96 + MU_INTERN_MAX_TRIGGER_LINKS * 8 + 368) +  \
     MU_INTERN_MAX_SUBSCRIPTIONS * 336 + MU_INTERN_MAX_PUBLISH_REQUESTS * 48)
#else
#define MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
#define MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES                                                                          \
    (MU_INTERN_MAX_CONNECTIONS * (MU_CONNECTION_RX_BUFFER_SIZE + MU_CONNECTION_BASE_STORAGE_BYTES))
#else
#define MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_EVENTS
#define MU_EVENTS_STORAGE_BYTES (MU_INTERN_MAX_SUBSCRIPTIONS * 700)
#else
#define MU_EVENTS_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_PUBSUB
#define MU_PUBSUB_STORAGE_BYTES 100
#else
#define MU_PUBSUB_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_SERVICE_NODEMANAGEMENT
#define MU_NODEMANAGEMENT_STORAGE_BYTES                                                                                \
    (MU_INTERN_MAX_DYNAMIC_NODES *                                                                                     \
         (96 + MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH + MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH +                  \
          MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH) +                                                                \
     MU_INTERN_MAX_DYNAMIC_REFERENCES * (88 + 3 * MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH) + 128)
#else
#define MU_NODEMANAGEMENT_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_SERVICE_QUERY
#define MU_QUERY_STORAGE_BYTES (MU_INTERN_MAX_QUERY_CONTINUATION_POINTS * 16 + 64)
#else
#define MU_QUERY_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
#define MU_ALARMS_CONDITIONS_STORAGE_BYTES (MU_INTERN_MAX_CONDITIONS * 32)
#else
#define MU_ALARMS_CONDITIONS_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_AUDITING
/* MU_MAX_AUDIT_CALLBACKS(4) * (callback_ptr + context_ptr) + count */
#define MU_AUDITING_STORAGE_BYTES (4 * 2 * sizeof(void *) + sizeof(size_t))
#else
#define MU_AUDITING_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_CUSTOM_METHODS
/* MU_MAX_REGISTERED_METHODS(8) * (method nodeid + callback + context) + count. */
#define MU_CUSTOM_METHODS_STORAGE_BYTES (8 * 48 + sizeof(size_t))
#else
#define MU_CUSTOM_METHODS_STORAGE_BYTES 0
#endif

#if MUC_OPCUA_SERVER_DIAGNOSTICS
/* mu_diagnostics_summary_t: 11 uint32 counters. */
#define MU_SERVER_DIAGNOSTICS_STORAGE_BYTES (11 * sizeof(unsigned int))
#else
#define MU_SERVER_DIAGNOSTICS_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_COMPLEX_TYPES
/* 8 structures * (def_ptr + nodeid) + 8 enums * (def_ptr + nodeid) + 2 * uint16 */
#define MU_COMPLEX_TYPES_STORAGE_BYTES (8 * (sizeof(void *) + 24) + 8 * (sizeof(void *) + 24) + 4)
#else
#define MU_COMPLEX_TYPES_STORAGE_BYTES 0
#endif

#define MU_SERVER_STORAGE_BYTES                                                                                        \
    (MU_SERVER_STORAGE_BASE_BYTES + MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES + MU_SERVER_SECURITY_STORAGE_BYTES +       \
     MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES + MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES + MU_EVENTS_STORAGE_BYTES +          \
     MU_PUBSUB_STORAGE_BYTES + MU_NODEMANAGEMENT_STORAGE_BYTES + MU_ALARMS_CONDITIONS_STORAGE_BYTES +                  \
     MU_CHUNK_ASSEMBLY_STORAGE_BYTES + MU_AUDITING_STORAGE_BYTES + MU_CUSTOM_METHODS_STORAGE_BYTES +                   \
     MU_SERVER_DIAGNOSTICS_STORAGE_BYTES + MU_COMPLEX_TYPES_STORAGE_BYTES + MU_QUERY_STORAGE_BYTES)

#endif /* MUC_OPCUA_CONFIG_H */
