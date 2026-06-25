/* src/core/sequence.h */
#ifndef MICRO_OPCUA_SEQUENCE_H
#define MICRO_OPCUA_SEQUENCE_H

#include "micro_opcua/opcua_types.h"
#include "micro_opcua/status.h"
#include <stdbool.h>

typedef struct {
    opcua_uint32_t last_sequence_number;
    bool is_initialized;
} mu_sequence_validator_t;

void mu_sequence_validator_init(mu_sequence_validator_t *validator);
opcua_statuscode_t mu_sequence_validate(mu_sequence_validator_t *validator, opcua_uint32_t sequence_number);

#endif /* MICRO_OPCUA_SEQUENCE_H */
