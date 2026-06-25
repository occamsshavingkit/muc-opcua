/* src/core/service_message.h */
#ifndef MICRO_OPCUA_SERVICE_MESSAGE_H
#define MICRO_OPCUA_SERVICE_MESSAGE_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include <stddef.h>

opcua_statuscode_t mu_parse_service_prefix(const opcua_byte_t *buffer, size_t length, size_t *offset, mu_nodeid_t *node_id);
opcua_statuscode_t mu_write_service_prefix(opcua_byte_t *buffer, size_t length, size_t *offset, const mu_nodeid_t *node_id);

#endif /* MICRO_OPCUA_SERVICE_MESSAGE_H */
