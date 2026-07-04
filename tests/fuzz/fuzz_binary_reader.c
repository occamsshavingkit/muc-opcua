/* tests/fuzz/fuzz_binary_reader.c */
#include "muc_opcua/muc_opcua.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define FUZZ_DECLARED_PAYLOAD_MAX 16u

opcua_statuscode_t mu_binary_read_array_length(mu_binary_reader_t *reader, opcua_int32_t *length);

static uint32_t read_le32_prefix(const uint8_t *data, size_t size) {
    uint32_t value = 0u;
    size_t i;
    size_t limit = size < 4u ? size : 4u;
    for (i = 0u; i < limit; ++i) {
        value |= (uint32_t)data[i] << (8u * i);
    }
    return value;
}

static void write_le32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xffu);
    buffer[1] = (uint8_t)((value >> 8u) & 0xffu);
    buffer[2] = (uint8_t)((value >> 16u) & 0xffu);
    buffer[3] = (uint8_t)((value >> 24u) & 0xffu);
}

static void require_bad_decoding_for_invalid_array_count(uint32_t encoded_count) {
    uint8_t buffer[4];
    mu_binary_reader_t reader;
    opcua_int32_t length = 0;
    opcua_statuscode_t status;

    write_le32(buffer, encoded_count);
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));
    status = mu_binary_read_array_length(&reader, &length);
    if (status != MU_STATUS_BAD_DECODINGERROR || reader.status != MU_STATUS_BAD_DECODINGERROR) {
        abort();
    }
}

static uint32_t overdeclared_length_from_input(const uint8_t *data, size_t size, size_t payload_length) {
    uint32_t remaining_range = (uint32_t)MU_MAX_ENCODED_STRING_LENGTH - (uint32_t)payload_length;
    return (uint32_t)payload_length + 1u + (read_le32_prefix(data, size) % remaining_range);
}

static void require_bad_decoding_for_overdeclared_length(uint32_t declared_length, const uint8_t *data, size_t size,
                                                         size_t payload_length, int read_bytestring) {
    uint8_t buffer[4u + FUZZ_DECLARED_PAYLOAD_MAX];
    mu_binary_reader_t reader;
    opcua_statuscode_t status;
    size_t i;

    write_le32(buffer, declared_length);
    for (i = 0u; i < payload_length; ++i) {
        buffer[4u + i] = i < size ? data[i] : (uint8_t)i;
    }

    mu_binary_reader_init(&reader, buffer, 4u + payload_length);
    if (read_bytestring) {
        mu_bytestring_t value;
        status = mu_binary_read_bytestring(&reader, &value);
    } else {
        mu_string_t value;
        status = mu_binary_read_string(&reader, &value);
    }

    if (status != MU_STATUS_BAD_DECODINGERROR || reader.status != MU_STATUS_BAD_DECODINGERROR) {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint32_t encoded_count = 0x80000000u | (read_le32_prefix(data, size) & 0x7fffffffu);
    size_t string_payload_length = size > 4u ? (size_t)(data[4] % FUZZ_DECLARED_PAYLOAD_MAX) : 0u;
    size_t bytestring_payload_length = size > 5u ? (size_t)(data[5] % FUZZ_DECLARED_PAYLOAD_MAX) : 0u;
    uint32_t string_declared_length = overdeclared_length_from_input(data, size, string_payload_length);
    uint32_t bytestring_declared_length = overdeclared_length_from_input(data, size, bytestring_payload_length);

    /* OPC-10000-6 section 5.2.5: -1 is the only null-array marker; lower counts are malformed. */
    if (encoded_count == 0xffffffffu) {
        encoded_count = 0xfffffffeu;
    }

    require_bad_decoding_for_invalid_array_count(encoded_count);

    /* OPC-10000-6 section 5.2: declared String/ByteString lengths must stay within the encoded bytes. */
    require_bad_decoding_for_overdeclared_length(string_declared_length, data, size, string_payload_length, 0);
    require_bad_decoding_for_overdeclared_length(bytestring_declared_length, data, size, bytestring_payload_length, 1);
    return 0;
}
