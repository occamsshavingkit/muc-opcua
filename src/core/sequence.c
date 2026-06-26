/* src/core/sequence.c */
#include "sequence.h"

void mu_sequence_validator_init(mu_sequence_validator_t *validator) {
    if (validator) {
        validator->last_sequence_number = 0;
        validator->is_initialized = false;
    }
}

opcua_statuscode_t mu_sequence_validate(mu_sequence_validator_t *validator, opcua_uint32_t sequence_number) {
    if (!validator) return MU_STATUS_BAD_INTERNALERROR;

    if (!validator->is_initialized) {
        validator->last_sequence_number = sequence_number;
        validator->is_initialized = true;
        return MU_STATUS_GOOD;
    }

    /* OPC 10000-6 6.7.2.4: 
       Any gap in the SequenceNumber indicates a missing MessageChunk and the receiver shall abort.
       Wrap around shall not occur until greater than UInt32.MaxValue - 1024 (4294966271).
       The first number after wrap around shall be 1. */
       
    opcua_uint32_t expected = validator->last_sequence_number + 1;
    
    if (sequence_number == expected) {
        validator->last_sequence_number = sequence_number;
        return MU_STATUS_GOOD;
    }
    
    /* Check wrap around. OPC 10000-6 6.7.2.4: SequenceNumber shall not wrap until it is
       greater than 4 294 966 271 (UInt32.MaxValue - 1024); the first number after the wrap
       around shall be less than 1024. */
    if (validator->last_sequence_number > 4294966271U && sequence_number < 1024) {
        validator->last_sequence_number = sequence_number;
        return MU_STATUS_GOOD;
    }

    return MU_STATUS_BAD_SECURITYCHECKSFAILED;
}
