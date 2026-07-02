/* tests/fuzz/fuzz_expanded_nodeid.c */
#include "muc_opcua/muc_opcua.h"
#include <stddef.h>
#include <stdint.h>

static const uint8_t g_empty_input = 0u;

static void decode_expanded_nodeid_from(const uint8_t *data, size_t size) {
    mu_binary_reader_t reader;
    mu_expanded_nodeid_t node_id;

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_expanded_nodeid(&reader, &node_id);
}

static size_t build_flagged_expanded_nodeid_frame(const uint8_t *data, size_t size, uint8_t *frame, size_t capacity) {
    size_t n = 0u;
    size_t copy_len;

    if (capacity < 8u) {
        return 0u;
    }

    /* OPC-10000-6 section 5.2.2.10: combine a valid TwoByte NodeId with
       NamespaceUri/ServerIndex flag coverage and trailing fuzz bytes. */
    frame[n++] = (uint8_t)(0xC0u | (size == 0u ? 0u : (data[0] & 0x0Fu)));
    frame[n++] = size > 1u ? data[1] : 0u;
    frame[n++] = size > 2u ? data[2] : 0u;
    frame[n++] = size > 3u ? data[3] : 0u;
    frame[n++] = size > 4u ? data[4] : 0u;
    frame[n++] = size > 5u ? data[5] : 0u;

    copy_len = size;
    if (copy_len > capacity - n) {
        copy_len = capacity - n;
    }
    for (size_t i = 0u; i < copy_len; ++i) {
        frame[n + i] = data[i];
    }
    return n + copy_len;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t structured[64];
    size_t structured_size;

    if (data == NULL) {
        data = &g_empty_input;
        size = 0u;
    }

    decode_expanded_nodeid_from(data, size);

    structured_size = build_flagged_expanded_nodeid_frame(data, size, structured, sizeof(structured));
    if (structured_size != 0u) {
        decode_expanded_nodeid_from(structured, structured_size);
    }

    return 0;
}
