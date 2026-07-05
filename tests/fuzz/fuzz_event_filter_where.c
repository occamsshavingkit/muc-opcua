/* tests/fuzz/fuzz_event_filter_where.c
 *
 * Spec Kit 037, T035. Fuzz target for EventFilter where-clause evaluator.
 * Exercises mu_evaluate_event_filter_where with arbitrary inputs.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_EVENT_FILTER_WHERE

#include "muc_opcua/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4)
        return 0;

    mu_event_fields_t fields;
    memset(&fields, 0, sizeof(fields));
    fields.severity = (opcua_uint16_t)(size & 0xFFFF);
    fields.message.length = (opcua_int32_t)(size > 32 ? 32 : size);
    if (size > 32) {
        fields.message.data = (opcua_byte_t *)(data + size - 32);
    }

    opcua_uint32_t element_count = (opcua_uint32_t)(data[0] % 8);
    if (element_count == 0)
        element_count = 1;

    opcua_uint32_t operators[8];
    opcua_uint32_t field_indices[8];
    opcua_int64_t filter_values[8];

    for (opcua_uint32_t i = 0; i < element_count && i < 8; ++i) {
        operators[i] = (opcua_uint32_t)(data[i + 1] % 13);
        field_indices[i] = (opcua_uint32_t)(data[0] % 9);
        filter_values[i] = (opcua_int64_t)(int64_t)data[i + 1];
    }

    mu_evaluate_event_filter_where(&fields, operators, field_indices, filter_values, element_count);
    return 0;
}

#endif /* MUC_OPCUA_EVENT_FILTER_WHERE */
