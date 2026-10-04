/* tests/fuzz/fuzz_message_chunk.c */
#include "../../src/core/message_chunk.h"
#include "../../src/core/uasc.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define FUZZ_TCP_MESSAGE_HEADER_SIZE 8u
#define FUZZ_MESSAGE_CHUNK_HEADER_SIZE 12u
#define FUZZ_ABORT_CHUNK_SIZE (MU_UASC_SYMMETRIC_HEADER_SIZE + 12u)
#define FUZZ_NON_FINAL_CHUNK_SIZE (MU_UASC_SYMMETRIC_HEADER_SIZE + 8u)
#define FUZZ_NEGOTIATED_RECEIVE_LIMIT 32u
#define FUZZ_NEGOTIATED_OVERSIZE_SPAN 64u

static uint8_t read_input_byte(const uint8_t *data, size_t size, size_t offset, uint8_t fallback) {
    return offset < size ? data[offset] : fallback;
}

static void write_le32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xffu);
    buffer[1] = (uint8_t)((value >> 8u) & 0xffu);
    buffer[2] = (uint8_t)((value >> 16u) & 0xffu);
    buffer[3] = (uint8_t)((value >> 24u) & 0xffu);
}

static uint32_t read_input_u32(const uint8_t *data, size_t size, size_t offset, uint32_t fallback) {
    uint32_t value = 0u;
    size_t i;

    for (i = 0u; i < 4u; ++i) {
        uint8_t byte = read_input_byte(data, size, offset + i, (uint8_t)((fallback >> (8u * i)) & 0xffu));
        value |= (uint32_t)byte << (8u * i);
    }

    return value;
}

static int is_valid_message_type(const uint8_t message_type[3]) {
    static const uint8_t valid_types[][3] = {
        {'H', 'E', 'L'}, {'A', 'C', 'K'}, {'E', 'R', 'R'}, {'O', 'P', 'N'}, {'C', 'L', 'O'}, {'M', 'S', 'G'},
    };
    size_t i;

    for (i = 0u; i < sizeof(valid_types) / sizeof(valid_types[0]); ++i) {
        if (message_type[0] == valid_types[i][0] && message_type[1] == valid_types[i][1] &&
            message_type[2] == valid_types[i][2]) {
            return 1;
        }
    }

    return 0;
}

static void force_invalid_message_type(uint8_t message_type[3]) {
    if (is_valid_message_type(message_type)) {
        message_type[2] = (uint8_t)(message_type[2] ^ 0x20u);
    }
}

static opcua_statuscode_t select_abort_tcp_error(const uint8_t *data, size_t size, size_t offset) {
    static const opcua_statuscode_t errors[] = {
        MU_STATUS_BAD_TCPINTERNALERROR,
        MU_STATUS_BAD_TCPMESSAGETYPEINVALID,
        MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN,
        MU_STATUS_BAD_TCPMESSAGETOOLARGE,
    };

    return errors[read_input_byte(data, size, offset, 0u) % (sizeof(errors) / sizeof(errors[0]))];
}

static void require_bad_tcp_message_type_invalid(const uint8_t *data, size_t size) {
    uint8_t buffer[FUZZ_MESSAGE_CHUNK_HEADER_SIZE];
    mu_message_header_t header;

    buffer[0] = read_input_byte(data, size, 0u, (uint8_t)'X');
    buffer[1] = read_input_byte(data, size, 1u, (uint8_t)'Y');
    buffer[2] = read_input_byte(data, size, 2u, (uint8_t)'Z');
    force_invalid_message_type(buffer);

    buffer[3] = (uint8_t)'F';
    write_le32(&buffer[4], (uint32_t)sizeof(buffer));
    buffer[8] = read_input_byte(data, size, 3u, 0u);
    buffer[9] = read_input_byte(data, size, 4u, 0u);
    buffer[10] = read_input_byte(data, size, 5u, 0u);
    buffer[11] = read_input_byte(data, size, 6u, 0u);

    /* OPC-10000-6 section 7.1.2.2: MessageType must be one of the defined three-byte values. */
    if (mu_parse_message_header(buffer, sizeof(buffer), &header) != MU_STATUS_BAD_TCPMESSAGETYPEINVALID) {
        abort();
    }
}

static void require_message_size_boundary_tcp_errors(const uint8_t *data, size_t size) {
    uint8_t below_tcp_header[FUZZ_MESSAGE_CHUNK_HEADER_SIZE];
    uint8_t below_uasc_header[FUZZ_MESSAGE_CHUNK_HEADER_SIZE];
    uint8_t over_negotiated[FUZZ_NEGOTIATED_RECEIVE_LIMIT];
    mu_message_header_t header;
    uint32_t declared_too_large =
        (uint32_t)sizeof(over_negotiated) + 1u + (read_input_u32(data, size, 11u, 0u) % FUZZ_NEGOTIATED_OVERSIZE_SPAN);
    size_t i;

    below_tcp_header[0] = (uint8_t)'H';
    below_tcp_header[1] = (uint8_t)'E';
    below_tcp_header[2] = (uint8_t)'L';
    below_tcp_header[3] = (uint8_t)'F';
    write_le32(&below_tcp_header[4], (uint32_t)(read_input_byte(data, size, 7u, 0u) % FUZZ_TCP_MESSAGE_HEADER_SIZE));
    below_tcp_header[8] = read_input_byte(data, size, 8u, 0u);
    below_tcp_header[9] = read_input_byte(data, size, 9u, 0u);
    below_tcp_header[10] = read_input_byte(data, size, 10u, 0u);
    below_tcp_header[11] = read_input_byte(data, size, 11u, 0u);

    /* OPC-10000-6 section 7.1.2.2: MessageSize includes the TCP MessageHeader bytes. */
    if (mu_parse_message_header(below_tcp_header, sizeof(below_tcp_header), &header) !=
        MU_STATUS_BAD_TCPINTERNALERROR) {
        abort();
    }

    below_uasc_header[0] = (uint8_t)'M';
    below_uasc_header[1] = (uint8_t)'S';
    below_uasc_header[2] = (uint8_t)'G';
    below_uasc_header[3] = (uint8_t)'F';
    write_le32(&below_uasc_header[4], FUZZ_TCP_MESSAGE_HEADER_SIZE +
                                          (uint32_t)(read_input_byte(data, size, 12u, 0u) %
                                                     (FUZZ_MESSAGE_CHUNK_HEADER_SIZE - FUZZ_TCP_MESSAGE_HEADER_SIZE)));
    write_le32(&below_uasc_header[8], read_input_u32(data, size, 13u, 1u));

    if (mu_parse_message_header(below_uasc_header, sizeof(below_uasc_header), &header) !=
        MU_STATUS_BAD_TCPINTERNALERROR) {
        abort();
    }

    over_negotiated[0] = (uint8_t)'M';
    over_negotiated[1] = (uint8_t)'S';
    over_negotiated[2] = (uint8_t)'G';
    over_negotiated[3] = (uint8_t)'F';
    write_le32(&over_negotiated[4], declared_too_large);
    write_le32(&over_negotiated[8], read_input_u32(data, size, 17u, 1u));
    for (i = FUZZ_MESSAGE_CHUNK_HEADER_SIZE; i < sizeof(over_negotiated); ++i) {
        over_negotiated[i] =
            read_input_byte(data, size, 21u + (i - FUZZ_MESSAGE_CHUNK_HEADER_SIZE), (uint8_t)(0x30u + (uint8_t)i));
    }

    /* OPC-10000-6 section 7.1.2.2: MessageSize must fit the negotiated/available input bytes. */
    if (mu_parse_message_header(over_negotiated, sizeof(over_negotiated), &header) !=
        MU_STATUS_BAD_TCPMESSAGETOOLARGE) {
        abort();
    }
}

static void require_abort_chunk_tcp_error_no_service_payload(const uint8_t *data, size_t size) {
    uint8_t buffer[FUZZ_ABORT_CHUNK_SIZE];
    mu_message_header_t header;
    opcua_statuscode_t expected_error = select_abort_tcp_error(data, size, 7u);
    size_t i;

    buffer[0] = (uint8_t)'M';
    buffer[1] = (uint8_t)'S';
    buffer[2] = (uint8_t)'G';
    buffer[3] = (uint8_t)'A';
    write_le32(&buffer[4], (uint32_t)sizeof(buffer));
    write_le32(&buffer[8], read_input_u32(data, size, 8u, 1u));
    write_le32(&buffer[12], read_input_u32(data, size, 12u, 1u));
    write_le32(&buffer[16], read_input_u32(data, size, 16u, 1u));
    write_le32(&buffer[20], read_input_u32(data, size, 20u, 1u));
    write_le32(&buffer[MU_UASC_SYMMETRIC_HEADER_SIZE], expected_error);
    write_le32(&buffer[MU_UASC_SYMMETRIC_HEADER_SIZE + 4u], 0xffffffffu);

    for (i = MU_UASC_SYMMETRIC_HEADER_SIZE + 8u; i < sizeof(buffer); ++i) {
        buffer[i] = read_input_byte(data, size, i, (uint8_t)(0xa0u + (uint8_t)i));
    }

    /* OPC-10000-6 section 6.7.2: abort chunks carry Error/Reason, not service MessageBody bytes. */
    if (mu_parse_message_header(buffer, sizeof(buffer), &header) != expected_error) {
        abort();
    }
}

static void require_non_final_chunk_header_status(const uint8_t *data, size_t size) {
    uint8_t buffer[FUZZ_NON_FINAL_CHUNK_SIZE];
    mu_message_header_t header;
#ifdef MUC_OPCUA_CU_MULTI_CHUNK
    const opcua_statuscode_t expected_status = MU_STATUS_GOOD;
#else
    const opcua_statuscode_t expected_status = MU_STATUS_BAD_TCPMESSAGETYPEINVALID;
#endif
    size_t i;

    buffer[0] = (uint8_t)'M';
    buffer[1] = (uint8_t)'S';
    buffer[2] = (uint8_t)'G';
    buffer[3] = (uint8_t)'C';
    write_le32(&buffer[4], (uint32_t)sizeof(buffer));
    write_le32(&buffer[8], read_input_u32(data, size, 24u, 1u));
    write_le32(&buffer[12], read_input_u32(data, size, 28u, 1u));
    write_le32(&buffer[16], read_input_u32(data, size, 32u, 1u));
    write_le32(&buffer[20], read_input_u32(data, size, 36u, 1u));

    buffer[MU_UASC_SYMMETRIC_HEADER_SIZE] = 0x01u;
    buffer[MU_UASC_SYMMETRIC_HEADER_SIZE + 1u] = 0u;
    buffer[MU_UASC_SYMMETRIC_HEADER_SIZE + 2u] = read_input_byte(data, size, 40u, 0xffu);
    buffer[MU_UASC_SYMMETRIC_HEADER_SIZE + 3u] = read_input_byte(data, size, 41u, 0xffu);
    for (i = MU_UASC_SYMMETRIC_HEADER_SIZE + 4u; i < sizeof(buffer); ++i) {
        buffer[i] = read_input_byte(data, size, 42u + (i - (MU_UASC_SYMMETRIC_HEADER_SIZE + 4u)),
                                    (uint8_t)(0x55u ^ (uint8_t)i));
    }

    /* OPC-10000-6 section 6.7.2: continuation chunks are valid only when multi-chunk support is compiled in. */
    if (mu_parse_message_header(buffer, sizeof(buffer), &header) != expected_status) {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static const uint8_t empty_input = 0u;

    if (data == NULL) {
        data = &empty_input;
        size = 0u;
    }

    require_bad_tcp_message_type_invalid(data, size);
    require_message_size_boundary_tcp_errors(data, size);
    require_abort_chunk_tcp_error_no_service_payload(data, size);
    require_non_final_chunk_header_status(data, size);
    return 0;
}
