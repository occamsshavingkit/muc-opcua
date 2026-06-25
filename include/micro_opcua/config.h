/* include/micro_opcua/config.h */
#ifndef MICRO_OPCUA_CONFIG_H
#define MICRO_OPCUA_CONFIG_H

#include <stddef.h>

/* Default chunk size limits (OPC 10000-6 7.1.2.3, 7.1.2.4) */
#define MU_MIN_CHUNK_SIZE 8192
#define MU_DEFAULT_MAX_CHUNK_COUNT 1
#define MU_DEFAULT_MAX_MESSAGE_SIZE 8192

/* Maximum number of sessions/channels for Nano profile */
#define MU_MAX_SESSIONS 1
#define MU_MAX_SECURE_CHANNELS 1

/* Fixed storage allocation size for the server */
#define MU_SERVER_STORAGE_BYTES 1024

#endif /* MICRO_OPCUA_CONFIG_H */
