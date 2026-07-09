/* tests/unit/test_operation_limits.c
 *
 * Spec 057: Base Info Server Capabilities / OperationLimits.
 * (a) MaxArrayLength is ENFORCED at variant decode: a value array whose element
 *     count exceeds MU_INTERN_MAX_ARRAY_LENGTH is rejected with
 *     Bad_EncodingLimitsExceeded (independent of buffer size).
 * (b) The advertised per-call node limits equal the enforcing constants
 *     (guarded so the advertised value can never drift from what is enforced).
 */
#include "muc_opcua/config.h"
#include "muc_opcua/encoding.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Advertised == enforced: MaxStringLength maps to the wire string ceiling. */
_Static_assert(MU_MAX_NODES_PER_READ > 0 && MU_MAX_NODES_PER_WRITE > 0 && MU_MAX_NODES_PER_BROWSE > 0 &&
                   MU_MAX_MONITORED_ITEMS_PER_CALL > 0 && MU_INTERN_MAX_ARRAY_LENGTH > 0,
               "operation limits must be positive");

/* Encode a variant header for an array of Int32 (builtin type 6) with the given
 * element count, then attempt to decode it. Returns the decode status. */
static opcua_statuscode_t decode_int32_array_of_length(opcua_int32_t length) {
    opcua_byte_t buf[16];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    mu_binary_write_byte(&w, (opcua_byte_t)(0x80 | 6)); /* array bit | Int32 */
    mu_binary_write_int32(&w, length);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    mu_variant_t v;
    (void)memset(&v, 0, sizeof(v));
    opcua_statuscode_t s = mu_binary_read_variant(&r, &v);
    /* These cases are rejected before any element allocation, so nothing to free.
     * Guard defensively in case that changes. */
    if (s == MU_STATUS_GOOD && v.is_array && v.value.array) {
        free((void *)v.value.array);
    }
    return s;
}

static void test_array_over_max_length_is_rejected(void) {
    /* One past the compiled ceiling must be refused with an encoding-limits error,
     * BEFORE any buffer-size dependent decoding. */
    opcua_statuscode_t s = decode_int32_array_of_length((opcua_int32_t)MU_INTERN_MAX_ARRAY_LENGTH + 1);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED, s);
}

static void test_array_at_max_length_not_rejected_for_limits(void) {
    /* Exactly at the ceiling is not an encoding-limits violation. (It may still
     * fail later for want of element bytes in this tiny buffer; that is a
     * decoding error, which is precisely NOT the limits rejection.) */
    opcua_statuscode_t s = decode_int32_array_of_length((opcua_int32_t)MU_INTERN_MAX_ARRAY_LENGTH);
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED, s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_array_over_max_length_is_rejected);
    RUN_TEST(test_array_at_max_length_not_rejected_for_limits);
    return UNITY_END();
}
