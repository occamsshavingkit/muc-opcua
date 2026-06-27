/* include/micro_opcua/config.h */
#ifndef MICRO_OPCUA_CONFIG_H
#define MICRO_OPCUA_CONFIG_H

#include <stddef.h>

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
#define MU_CIPHER_CTX_SIZE 512
#endif

/* E4: shared secure-path scratch owned by struct mu_server when
 * MICRO_OPCUA_SECURITY is enabled. Sized for the worst-case secure response /
 * OPN scratch, replacing respbody[5120] + opn_buf[1024] stack buffers. */
#ifndef MU_SECURE_SCRATCH_SIZE
#define MU_SECURE_SCRATCH_SIZE 6144
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

/* Maximum number of sessions/channels for Nano profile */
#define MU_MAX_SESSIONS 2
#define MU_MAX_SECURE_CHANNELS 1

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
 * The subscription engine (MICRO_OPCUA_SUBSCRIPTIONS, the Micro profile) adds the
 * fixed-size subscription / MonitoredItem / parked-Publish arrays to struct mu_server,
 * so the no-heap storage block is larger when it is compiled in. Security builds
 * also reserve server-owned scratch for large secure-channel transient buffers. */
#ifdef MICRO_OPCUA_SECURITY
/* secure_scratch + the two per-direction prepared cipher contexts that now live in
 * client_keys/server_keys (mu_sym_keys_t.cipher_ctx, 2 x MU_CIPHER_CTX_SIZE). */
#define MU_SERVER_SECURITY_STORAGE_BYTES (MU_SECURE_SCRATCH_SIZE + 2 * MU_CIPHER_CTX_SIZE)
#else
#define MU_SERVER_SECURITY_STORAGE_BYTES 0
#endif

#ifdef MICRO_OPCUA_SUBSCRIPTIONS
#if MICRO_OPCUA_SUBSCRIPTIONS_STANDARD
/* OPC-10000-7 §6.6.17 Standard DataChange Subscription storage; zero unless
 * the Standard facet is enabled. */
#define MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES \
    (MU_MAX_MONITORED_ITEMS * (MU_MONITORED_QUEUE_DEPTH * 64 + MU_MAX_TRIGGER_LINKS * 4 + 64))
#else
#define MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES 0
#endif

#define MU_SERVER_STORAGE_BYTES (3072 + MU_SUBSCRIPTIONS_STANDARD_STORAGE_BYTES + MU_SERVER_SECURITY_STORAGE_BYTES + MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES)
#else
#define MU_SERVER_STORAGE_BYTES (1024 + MU_SERVER_SECURITY_STORAGE_BYTES + MU_ADDRESS_SPACE_INDEX_STORAGE_BYTES)
#endif

#endif /* MICRO_OPCUA_CONFIG_H */
