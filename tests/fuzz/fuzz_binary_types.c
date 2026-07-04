/* tests/fuzz/fuzz_binary_types.c */
#include "muc_opcua/muc_opcua.h"
#include <stddef.h>
#include <stdint.h>

static void decode_all_from(const uint8_t *data, size_t size) {
    mu_binary_reader_t reader;
    mu_nodeid_t node_id;
    mu_string_t string_value;
    mu_bytestring_t bytes_value;
    mu_nodeid_t extension_type;
    size_t extension_length = 0u;
    mu_variant_t variant;
    mu_datavalue_t data_value;

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_nodeid(&reader, &node_id);

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_string(&reader, &string_value);

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_bytestring(&reader, &bytes_value);

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_extension_object_header(&reader, &extension_type, &extension_length);
    if (reader.status == MU_STATUS_GOOD && reader.position <= reader.length &&
        extension_length <= reader.length - reader.position) {
        (void)mu_binary_skip_extension_object(&reader);
    }

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_variant(&reader, &variant);

    mu_binary_reader_init(&reader, data, size);
    (void)mu_binary_read_datavalue(&reader, &data_value);
}

static size_t build_structured_frame(const uint8_t *data, size_t size, uint8_t *frame, size_t capacity) {
    size_t n = 0u;
    size_t copy_len;

    if (capacity < 16u) {
        return 0u;
    }

    /* OPC-10000-6 section 5.2 binary type frames: a valid TwoByte NodeId,
       a bounded String payload, an empty ByteString, and room for fuzz bytes. */
    frame[n++] = 0x00u;
    frame[n++] = size == 0u ? 0u : data[0];
    frame[n++] = 0x01u;
    frame[n++] = 0x00u;
    frame[n++] = 0x00u;
    frame[n++] = 0x00u;
    frame[n++] = size > 1u ? data[1] : 0u;
    frame[n++] = 0x00u;
    frame[n++] = 0x00u;
    frame[n++] = 0x00u;
    frame[n++] = 0xffu;
    frame[n++] = 0xffu;
    frame[n++] = 0xffu;
    frame[n++] = 0xffu;

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
    static const uint8_t empty = 0u;
    uint8_t structured[64];
    size_t structured_size;

    if (data == NULL) {
        data = &empty;
        size = 0u;
    }

    decode_all_from(data, size);

    structured_size = build_structured_frame(data, size, structured, sizeof(structured));
    if (structured_size != 0u) {
        decode_all_from(structured, structured_size);
    }

    return 0;
}
