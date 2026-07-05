/* src/services/read/common.h — internal declarations shared across read/ sub-files */
#ifndef MUC_OPCUA_SERVICES_READ_COMMON_H
#define MUC_OPCUA_SERVICES_READ_COMMON_H

#include "muc_opcua/address_space.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/opcua_types.h"
#include "muc_opcua/status.h"

#include "../read.h"

opcua_statuscode_t read_attribute(const mu_address_space_t *address_space, const mu_node_t *node,
                                  opcua_uint32_t attribute_id, mu_variant_t *value);

opcua_boolean_t mu_read_cache_lookup(const mu_nodeid_t *node_id, opcua_double_t max_age_ms, opcua_datetime_t now,
                                     mu_variant_t *out);

void mu_read_cache_store(const mu_nodeid_t *node_id, const mu_variant_t *val, opcua_datetime_t read_time);

#ifdef MUC_OPCUA_MULTI_CHUNK
opcua_statuscode_t apply_index_range(const mu_string_t *index_range, mu_variant_t *value);
#endif

#endif
