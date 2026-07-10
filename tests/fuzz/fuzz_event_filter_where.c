/* tests/fuzz/fuzz_event_filter_where.c
 *
 * Fuzz target for the EventFilter WhereClause evaluator (spec 061). Builds an
 * arbitrary compact operand tree from the input and evaluates it — exercising the
 * element recursion, operand bounds, and Like matcher for crashes/OOB reads.
 */
#include "muc_opcua/config.h"

#if MUC_OPCUA_EVENT_FILTER_WHERE

#include "services/event_filter.h"
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) {
        return 0;
    }

    mu_event_fields_t fields;
    memset(&fields, 0, sizeof(fields));
    fields.severity = (opcua_uint16_t)(data[0] | (data[1] << 8));
    fields.event_type.identifier_type = MU_NODEID_NUMERIC;
    fields.event_type.identifier.numeric = data[2];
    if (size > 40) {
        fields.message.data = data + size - 32;
        fields.message.length = 32;
    }

    mu_where_clause_t wc;
    memset(&wc, 0, sizeof(wc));

    size_t n_el = (size_t)(data[3] % (MU_INTERN_MAX_WHERE_ELEMENTS + 1));
    size_t op_index = 0;
    size_t di = 4;
    for (size_t e = 0; e < n_el && di + 1 < size; ++e) {
        wc.elements[e].op = data[di++] % 18;
        size_t n_op = (size_t)(data[di++] % 4);
        wc.elements[e].operand_base = (opcua_byte_t)op_index;
        wc.elements[e].operand_count = 0;
        for (size_t o = 0; o < n_op && op_index < MU_INTERN_MAX_WHERE_OPERANDS && di + 2 < size; ++o) {
            mu_where_operand_t *op = &wc.operands[op_index];
            op->kind = data[di++] % 3;
            op->field = data[di] % 9;
            op->element_index = data[di] % (n_el ? n_el : 1);
            op->lit_type = data[di] % 26;
            op->num.i = (opcua_int64_t)(int8_t)data[di];
            op->blob_off = 0;
            op->blob_len = (opcua_uint16_t)(data[di] % (MU_INTERN_WHERE_BLOB_BYTES + 1));
            di++;
            op_index++;
            wc.elements[e].operand_count++;
        }
        wc.element_count++;
    }
    wc.operand_count = (opcua_byte_t)op_index;
    /* seed the blob so string/Like literals have something bounded to read */
    for (size_t i = 0; i < MU_INTERN_WHERE_BLOB_BYTES && i < size; ++i) {
        wc.blob[i] = data[i];
    }
    wc.blob_len = (opcua_uint16_t)(size < MU_INTERN_WHERE_BLOB_BYTES ? size : MU_INTERN_WHERE_BLOB_BYTES);

    (void)mu_where_clause_eval(&wc, &fields);
    return 0;
}

#endif /* MUC_OPCUA_EVENT_FILTER_WHERE */
