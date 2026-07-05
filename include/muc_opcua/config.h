/* include/muc_opcua/config.h */
#ifndef MUC_OPCUA_CONFIG_H
#define MUC_OPCUA_CONFIG_H

#include <stddef.h>

/* Feature 025 (F9): reject illegal feature-gate combinations at compile time. */
#include "muc_opcua/features.h"

/* Optimization-remediation compile-time knobs.
 * Defaults are intentionally overridable with -D for embedded profiles. */

/* E1: maximum nodes in the address-space index before falling back to a
 * linear scan. The index RAM scales by 2 bytes per node. */
#ifndef MU_MAX_ADDRESS_SPACE_NODES
#define MU_MAX_ADDRESS_SPACE_NODES 64
#endif

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
#define MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES (MU_MAX_ADDRESS_SPACE_NODES * 2 + 128)
#endif

/* D9: mu_status_name() strings are OFF for embedded builds by default; this
 * macro is intentionally left undefined unless supplied with -D. */

/* Default chunk size limits (OPC 10000-6 7.1.2.3, 7.1.2.4) */
#define MU_MIN_CHUNK_SIZE 8192
#define MU_DEFAULT_MAX_CHUNK_COUNT 1
#define MU_DEFAULT_MAX_MESSAGE_SIZE 8192

/**
 * @brief Maximum concurrent TCP connections. Each connection owns exactly one
 * SecureChannel (OPC 10000-6 7.1.2), so this is also the structural ceiling on
 * concurrent secure channels -- see MU_MAX_SECURE_CHANNELS below. Overridable
 * with -D independent of MUC_OPCUA_MULTIPLE_CONNECTIONS.
 */
#ifndef MU_MAX_CONNECTIONS
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
#define MU_MAX_CONNECTIONS 4
#else
#define MU_MAX_CONNECTIONS 1
#endif
#endif

/**
 * @brief Default secure channel limits. Kept as its own overridable macro for
 * API stability, but structurally always equal to MU_MAX_CONNECTIONS -- one
 * secure channel per connection, never more (see mu_connection_t in
 * server_internal.h). Previously hardcoded to 2 independent of connection
 * count, which did not match either the 1:1 structural reality or this
 * project's own documentation (docs/architecture.md, docs/api-reference.md,
 * docs/integration-guide.md all documented this as 1).
 */
#ifndef MU_MAX_SECURE_CHANNELS
#define MU_MAX_SECURE_CHANNELS MU_MAX_CONNECTIONS
#endif

/**
 * @brief Dynamic address space limits
 */
#ifndef MU_MAX_DYNAMIC_NODES
#define MU_MAX_DYNAMIC_NODES 32
#endif

#ifndef MU_MAX_DYNAMIC_REFERENCES
#define MU_MAX_DYNAMIC_REFERENCES 64
#endif

#ifndef MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH
#define MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH 64
#endif

#ifndef MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH
#define MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH 64
#endif

#ifndef MU_MAX_DYNAMIC_STRING_NODEID_LENGTH
#define MU_MAX_DYNAMIC_STRING_NODEID_LENGTH 64
#endif

#ifndef MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH
#define MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH MU_MAX_DYNAMIC_STRING_NODEID_LENGTH
#endif

/**
 * @brief Maximum concurrent Sessions. The Micro profile requires >=2 sessions
 * (docs/conformance/profile-micro.md), multiplexed over as few as a single
 * SecureChannel/connection -- so this floor is independent of
 * MU_MAX_CONNECTIONS. Previously had no #ifndef guard, so it could not be
 * raised with -D at all, unlike every other limit in this file.
 */
#ifndef MU_MAX_SESSIONS
#define MU_MAX_SESSIONS 2
#endif

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
#define MU_SERVER_STORAGE_BASE_BYTES 3072
#else
#define MU_SERVER_STORAGE_BASE_BYTES 1024
#endif

/* Multi-chunk reassembly buffer size. OPC-10000-6 §6.7.2 */
#ifndef MU_CHUNK_ASSEMBLY_BUFFER_SIZE
#define MU_CHUNK_ASSEMBLY_BUFFER_SIZE 8192
#endif

/* Multi-chunk reassembly storage. Sized for the default max message plus
   assembler bookkeeping. */
#define MU_CHUNK_ASSEMBLY_STORAGE_BYTES (MU_CHUNK_ASSEMBLY_BUFFER_SIZE + 32)

#if defined(MUC_OPCUA_SUBSCRIPTIONS) && MUC_OPCUA_SUBSCRIPTIONS_STANDARD
/* src/services/subscription.h is the canonical definer for these capacities.
 * These fallbacks must match its defaults so caller-storage sizing is correct
 * even when subscription.h is not included first. Because both files use
 * #ifndef guards, -D overrides and first-included definitions win consistently. */
#ifndef MU_MAX_MONITORED_ITEMS
#define MU_MAX_MONITORED_ITEMS 8
#endif
#ifndef MU_MONITORED_QUEUE_DEPTH
#define MU_MONITORED_QUEUE_DEPTH 1
#endif
#ifndef MU_MAX_TRIGGER_LINKS
#define MU_MAX_TRIGGER_LINKS 4
#endif
/* OPC-10000-7 §6.6.17 Standard DataChange Subscription storage; zero unless
 * the Standard facet is enabled. */
#define MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES                                                                        \
    (MU_MAX_MONITORED_ITEMS * (MU_MONITORED_QUEUE_DEPTH * 96 + MU_MAX_TRIGGER_LINKS * 8 + 200))
#else
#define MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
#define MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES                                                                          \
    (MU_MAX_CONNECTIONS * (MU_CONNECTION_RX_BUFFER_SIZE + MU_CONNECTION_BASE_STORAGE_BYTES))
#else
#define MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_EVENTS
#ifndef MU_MAX_SUBSCRIPTIONS
#define MU_MAX_SUBSCRIPTIONS 2
#endif
#define MU_EVENTS_STORAGE_BYTES (MU_MAX_SUBSCRIPTIONS * 700)
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
    (MU_MAX_DYNAMIC_NODES * (96 + MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH + MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH +             \
                             MU_MAX_DYNAMIC_STRING_NODEID_LENGTH) +                                                    \
     MU_MAX_DYNAMIC_REFERENCES * (88 + 3 * MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH) + 128)
#else
#define MU_NODEMANAGEMENT_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_SERVICE_QUERY
#ifndef MU_MAX_QUERY_CONTINUATION_POINTS
#define MU_MAX_QUERY_CONTINUATION_POINTS 2
#endif
#define MU_QUERY_STORAGE_BYTES (MU_MAX_QUERY_CONTINUATION_POINTS * 16 + 64)
#else
#define MU_QUERY_STORAGE_BYTES 0
#endif

#ifdef MUC_OPCUA_SERVICE_ALARMS_CONDITIONS
#ifndef MU_MAX_CONDITIONS
#define MU_MAX_CONDITIONS 10
#endif
#define MU_ALARMS_CONDITIONS_STORAGE_BYTES (MU_MAX_CONDITIONS * 32)
#else
#define MU_ALARMS_CONDITIONS_STORAGE_BYTES 0
#endif

#define MU_SERVER_STORAGE_BYTES                                                                                        \
    (MU_SERVER_STORAGE_BASE_BYTES + MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES + MU_SERVER_SECURITY_STORAGE_BYTES +       \
     MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES + MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES + MU_EVENTS_STORAGE_BYTES +          \
     MU_PUBSUB_STORAGE_BYTES + MU_NODEMANAGEMENT_STORAGE_BYTES + MU_QUERY_STORAGE_BYTES +                              \
     MU_ALARMS_CONDITIONS_STORAGE_BYTES + MU_CHUNK_ASSEMBLY_STORAGE_BYTES)

#endif /* MUC_OPCUA_CONFIG_H */
