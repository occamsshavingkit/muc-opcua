/* tests/fuzz/fuzz_expanded_nodeid.c */
#include "muc_opcua/muc_opcua.h"
/* The static-analysis environment may not provide system headers; the build does. */
/* cppcheck-suppress missingIncludeSystem */
#include <stddef.h>
/* cppcheck-suppress missingIncludeSystem */
#include <stdint.h>

static const uint8_t g_empty_input = 0u;

static void decode_expanded_nodeid_from(const uint8_t *data, size_t size) {
    mu_binary_reader_t reader;
    mu_expanded_nodeid_t node_id;

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_expanded_nodeid(&reader, &node_id);
}

static uint8_t fuzz_byte_or_zero(const uint8_t *data, size_t size, size_t index) {
    uint8_t value = 0u;

    if (index < size) {
        value = data[index];
    }
    return value;
}

static void write_flagged_prefix(const uint8_t *data, size_t size, uint8_t *frame) {
    frame[0] = (uint8_t)(0xC0u | (fuzz_byte_or_zero(data, size, 0u) & 0x0Fu));
    frame[1] = fuzz_byte_or_zero(data, size, 1u);
    frame[2] = fuzz_byte_or_zero(data, size, 2u);
    frame[3] = fuzz_byte_or_zero(data, size, 3u);
    frame[4] = fuzz_byte_or_zero(data, size, 4u);
    frame[5] = fuzz_byte_or_zero(data, size, 5u);
}

static size_t build_flagged_expanded_nodeid_frame(const uint8_t *data, size_t size, uint8_t *frame, size_t capacity) {
    size_t used = 0u;

    /* OPC-10000-6 section 5.2.2.10: combine a valid TwoByte NodeId with
       NamespaceUri/ServerIndex flag coverage and trailing fuzz bytes. */
    if (capacity >= 8u) {
        size_t copy_len = size;

        write_flagged_prefix(data, size, frame);
        used = 6u;
        if (copy_len > (capacity - used)) {
            copy_len = capacity - used;
        }
        for (size_t i = 0u; i < copy_len; ++i) {
            frame[used + i] = data[i];
        }
        used += copy_len;
    }
    return used;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t structured[64];
    size_t structured_size;
    const uint8_t *input_data = data;
    size_t input_size = size;

    if (input_data == NULL) {
        input_data = &g_empty_input;
        input_size = 0u;
    }

    decode_expanded_nodeid_from(input_data, input_size);

    structured_size = build_flagged_expanded_nodeid_frame(input_data, input_size, structured, sizeof(structured));
    if (structured_size != 0u) {
        decode_expanded_nodeid_from(structured, structured_size);
    }

    return 0;
}
