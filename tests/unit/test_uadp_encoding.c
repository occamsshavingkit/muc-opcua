#include "micro_opcua/encoding.h"
#include "micro_opcua/pubsub.h"
#include "micro_opcua/status.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static size_t build_sample_uadp_message(opcua_byte_t *buffer, size_t buffer_size) {
    static const mu_pubsub_field_t fields[] = {
        {.value = {MU_TYPE_UINT32, {.ui32 = 42u}}},
        {.value = {MU_TYPE_BOOLEAN, {.b = true}}},
    };
    mu_pubsub_connection_t conn = {.port = 4840, .enabled = true, .publisher_id = 0x12345678, .address = "239.0.0.1"};
    mu_pubsub_writer_group_t wg = {
        .writer_group_id = 1,
        .publishing_interval_ms = 1000,
        .dataset_writer = {.data_set_writer_id = 0x1002, .fields = fields, .field_count = 2}};
    size_t bytes_written = 0;
    opcua_statuscode_t status = mu_encode_uadp_network_message(&conn, &wg, buffer, buffer_size, &bytes_written);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    return bytes_written;
}

void test_uadp_network_message_encoding(void) {
    static const mu_pubsub_field_t fields[] = {
        {.value = {MU_TYPE_UINT32, {.ui32 = 42u}}},
        {.value = {MU_TYPE_BOOLEAN, {.b = true}}},
    };
    mu_pubsub_connection_t conn = {.port = 4840, .enabled = true, .publisher_id = 0x12345678, .address = "239.0.0.1"};
    mu_pubsub_writer_group_t wg = {
        .writer_group_id = 1,
        .publishing_interval_ms = 1000,
        .dataset_writer = {.data_set_writer_id = 0x1002, .fields = fields, .field_count = 2}};

    opcua_byte_t buffer[64];
    size_t bytes_written = 0;

    opcua_statuscode_t status = mu_encode_uadp_network_message(&conn, &wg, buffer, sizeof(buffer), &bytes_written);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);

    /* OPC-10000-14 sections 7.2.4.4.2, 7.2.4.5.2, 7.2.4.5.4,
       and 7.2.4.5.5: UADP NetworkMessage with UInt32 PublisherId,
       one DataSetWriterId, one sized Data Key Frame, and Variant fields. */
    TEST_ASSERT_EQUAL_UINT32(19, bytes_written);
    TEST_ASSERT_EQUAL_HEX8(0xD1, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x78, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x56, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x34, buffer[4]);
    TEST_ASSERT_EQUAL_HEX8(0x12, buffer[5]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[6]); /* PayloadHeader Count */
    TEST_ASSERT_EQUAL_HEX8(0x02, buffer[7]);
    TEST_ASSERT_EQUAL_HEX8(0x10, buffer[8]); /* DataSetWriterId 0x1002 */
    TEST_ASSERT_EQUAL_HEX8(0x08, buffer[9]); /* DataSetMessage size */
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[10]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[11]); /* DataSetFlags1: valid, Variant field encoding */

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, &buffer[12], bytes_written - 12);
    mu_variant_t out0;
    mu_variant_t out1;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &out0));
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, out0.type);
    TEST_ASSERT_EQUAL_UINT32(42u, out0.value.ui32);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_binary_read_variant(&reader, &out1));
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, out1.type);
    TEST_ASSERT_TRUE(out1.value.b);
    TEST_ASSERT_EQUAL_UINT32(bytes_written - 12, reader.position);
}

void test_uadp_network_message_decoder_reads_publisher_payload(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2];
    mu_pubsub_received_message_t message = {
        .fields = fields,
        .field_capacity = 2,
    };

    /* OPC-10000-14 sections 7.2.4.4.2, 7.2.4.5.2, 7.2.4.5.3,
       7.2.4.5.4, and 7.2.4.5.5: decode the current publisher's
       UInt32 PublisherId, PayloadHeader, sized DataSet payload, Data
       Key Frame, and scalar Variant fields (OPC-10000-6 section
       5.2.2.16). */
    opcua_statuscode_t status = mu_decode_uadp_network_message(buffer, bytes_written, &message);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_UINT32(0x12345678u, message.publisher_id);
    TEST_ASSERT_EQUAL_UINT16(0x1002u, message.data_set_writer_id);
    TEST_ASSERT_EQUAL_UINT32(2u, message.field_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, fields[0].type);
    TEST_ASSERT_EQUAL_UINT32(42u, fields[0].value.ui32);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, fields[1].type);
    TEST_ASSERT_TRUE(fields[1].value.b);
}

void test_uadp_network_message_decoder_rejects_null_input_buffer(void) {
    mu_variant_t fields[1];
    mu_pubsub_received_message_t message = {
        .fields = fields,
        .field_capacity = 1,
        .field_count = 99,
    };

    /* OPC-10000-4 section 7.38.2: invalid decoder arguments return
       Bad_InvalidArgument without leaving a stale field count. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_INVALIDARGUMENT, mu_decode_uadp_network_message(NULL, 1u, &message));
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
}

void test_uadp_network_message_decoder_rejects_null_decoded_message_pointer(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));

    /* OPC-10000-4 section 7.38.2: the caller-provided decoded-message
       output is required for deterministic decoder failure reporting. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_INVALIDARGUMENT, mu_decode_uadp_network_message(buffer, bytes_written, NULL));
}

void test_uadp_network_message_decoder_rejects_null_field_output_with_capacity(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_pubsub_received_message_t missing_fields = {
        .fields = NULL,
        .field_capacity = 1,
        .field_count = 99,
    };

    /* OPC-10000-4 section 7.38.2: a nonzero field capacity requires
       caller-owned field storage. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_INVALIDARGUMENT,
                            mu_decode_uadp_network_message(buffer, bytes_written, &missing_fields));
    TEST_ASSERT_EQUAL_UINT32(0u, missing_fields.field_count);
}

void test_uadp_network_message_decoder_rejects_truncated_network_header(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2];
    mu_pubsub_received_message_t message = {
        .fields = fields,
        .field_capacity = 2,
        .field_count = 99,
    };

    (void)bytes_written;

    /* OPC-10000-14 section 7.2.4.4.2: a truncated UADP
       NetworkMessage header is malformed and maps deterministically to
       Bad_DecodingError per OPC-10000-4 section 7.38.2. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_decode_uadp_network_message(buffer, 1u, &message));
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
}

void test_uadp_network_message_decoder_rejects_truncated_payload_header(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2];
    mu_pubsub_received_message_t message = {
        .fields = fields,
        .field_capacity = 2,
        .field_count = 99,
    };

    (void)bytes_written;

    /* OPC-10000-14 section 7.2.4.5.2: a PayloadHeader that advertises a
       DataSetMessage but truncates its DataSetWriterId is malformed and
       returns Bad_DecodingError per OPC-10000-4 section 7.38.2. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, mu_decode_uadp_network_message(buffer, 8u, &message));
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
}

void test_uadp_network_message_decoder_rejects_dataset_message_size_mismatch(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2];
    mu_pubsub_received_message_t message = {
        .fields = fields,
        .field_capacity = 2,
        .field_count = 99,
    };
    opcua_byte_t size_mismatch[64];

    memcpy(size_mismatch, buffer, bytes_written);
    size_mismatch[9] = (opcua_byte_t)(size_mismatch[9] + 1u);

    /* OPC-10000-14 section 7.2.4.5.3: the sized DataSetMessage must fit
       the received NetworkMessage bytes; a length mismatch is a decoding
       error per OPC-10000-4 section 7.38.2. */
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            mu_decode_uadp_network_message(size_mismatch, bytes_written, &message));
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
}

void test_uadp_network_message_decoder_rejects_output_capacity_overflow(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2] = {
        {MU_TYPE_NULL, {.ui32 = 0u}},
        {MU_TYPE_UINT32, {.ui32 = 0xA5A5A5A5u}},
    };
    mu_pubsub_received_message_t message = {
        .publisher_id = 0xDEADBEEFu,
        .data_set_writer_id = 0xBEEFu,
        .fields = fields,
        .field_capacity = 1,
        .field_count = 99,
    };

    /* OPC-10000-14 section 7.2.4.5.5: this Data Key Frame decodes two
       scalar Variant fields, but the caller provides one output slot.
       OPC-10000-4 section 7.38.2 maps the capacity exhaustion to
       Bad_TooManyOperations. fields[1] is a canary outside the declared
       field_capacity and must not be written. */
    opcua_statuscode_t status = mu_decode_uadp_network_message(buffer, bytes_written, &message);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS, status);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, message.publisher_id);
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, message.data_set_writer_id);
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, fields[1].type);
    TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5u, fields[1].value.ui32);
}

void test_uadp_network_message_decoder_rejects_unsupported_publisher_id_type(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2] = {
        {MU_TYPE_UINT32, {.ui32 = 0xA5A5A5A5u}},
        {MU_TYPE_BOOLEAN, {.b = true}},
    };
    mu_pubsub_received_message_t message = {
        .publisher_id = 0xDEADBEEFu,
        .data_set_writer_id = 0xBEEFu,
        .fields = fields,
        .field_capacity = 2,
        .field_count = 99,
    };

    /* OPC-10000-14 section 7.2.4.4.2 permits multiple PublisherId
       layouts; this subscriber slice only supports UInt32. Per
       OPC-10000-4 section 7.38.2, an unsupported layout returns
       Bad_NotSupported without exposing stale decoded fields. */
    buffer[1] = 0x01u; /* ExtendedFlags1 PublisherId type UInt16. */

    opcua_statuscode_t status = mu_decode_uadp_network_message(buffer, bytes_written, &message);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTSUPPORTED, status);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, message.publisher_id);
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, message.data_set_writer_id);
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, fields[0].type);
    TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5u, fields[0].value.ui32);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, fields[1].type);
    TEST_ASSERT_TRUE(fields[1].value.b);
}

void test_uadp_network_message_decoder_rejects_security_header(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2] = {
        {MU_TYPE_UINT32, {.ui32 = 0xA5A5A5A5u}},
        {MU_TYPE_BOOLEAN, {.b = true}},
    };
    mu_pubsub_received_message_t message = {
        .publisher_id = 0xDEADBEEFu,
        .data_set_writer_id = 0xBEEFu,
        .fields = fields,
        .field_capacity = 2,
        .field_count = 99,
    };

    /* OPC-10000-14 sections 7.2.4.4.2 and 7.2.4.4.3.1 define the
       SecurityHeader layout behind ExtendedFlags1. This subscriber
       slice does not implement UADP security headers, so OPC-10000-4
       section 7.38.2 requires a deterministic Bad_NotSupported result
       without exposing stale decoded fields. */
    buffer[1] |= 0x10u; /* ExtendedFlags1 bit 4: SecurityHeader enabled. */

    opcua_statuscode_t status = mu_decode_uadp_network_message(buffer, bytes_written, &message);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTSUPPORTED, status);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, message.publisher_id);
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, message.data_set_writer_id);
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, fields[0].type);
    TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5u, fields[0].value.ui32);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, fields[1].type);
    TEST_ASSERT_TRUE(fields[1].value.b);
}

void test_uadp_network_message_decoder_rejects_multiple_dataset_messages(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2] = {
        {MU_TYPE_UINT32, {.ui32 = 0xA5A5A5A5u}},
        {MU_TYPE_BOOLEAN, {.b = true}},
    };
    mu_pubsub_received_message_t message = {
        .publisher_id = 0xDEADBEEFu,
        .data_set_writer_id = 0xBEEFu,
        .fields = fields,
        .field_capacity = 2,
        .field_count = 99,
    };

    buffer[6] = 2u; /* PayloadHeader Count */

    /* OPC-10000-14 section 7.2.4.5.2 PayloadHeader permits multiple
       DataSetMessages, but this subscriber slice decodes one
       DataSetMessage. Per OPC-10000-4 section 7.38.2, the unsupported
       layout returns Bad_NotSupported without exposing decoded fields. */
    opcua_statuscode_t status = mu_decode_uadp_network_message(buffer, bytes_written, &message);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTSUPPORTED, status);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, message.publisher_id);
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, message.data_set_writer_id);
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, fields[0].type);
    TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5u, fields[0].value.ui32);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, fields[1].type);
    TEST_ASSERT_TRUE(fields[1].value.b);
}

void test_uadp_network_message_decoder_rejects_unsupported_dataset_message_type(void) {
    opcua_byte_t buffer[64];
    size_t bytes_written = build_sample_uadp_message(buffer, sizeof(buffer));
    mu_variant_t fields[2] = {
        {MU_TYPE_UINT32, {.ui32 = 0xA5A5A5A5u}},
        {MU_TYPE_BOOLEAN, {.b = true}},
    };
    mu_pubsub_received_message_t message = {
        .publisher_id = 0xDEADBEEFu,
        .data_set_writer_id = 0xBEEFu,
        .fields = fields,
        .field_capacity = 2,
        .field_count = 99,
    };

    /* OPC-10000-14 section 7.2.4.5.4 defines this slice's supported
       Data Key Frame layout. Delta/Event DataSetMessage alternatives
       use layouts described by sections 7.2.4.5.6 and 7.2.4.5.7, so
       OPC-10000-4 section 7.38.2 requires a deterministic
       Bad_NotSupported result without exposing decoded fields. */
    buffer[11] = 0x21u; /* Valid Variant encoding + Event DataSetMessage type. */

    opcua_statuscode_t status = mu_decode_uadp_network_message(buffer, bytes_written, &message);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOTSUPPORTED, status);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, message.publisher_id);
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, message.data_set_writer_id);
    TEST_ASSERT_EQUAL_UINT32(0u, message.field_count);
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, fields[0].type);
    TEST_ASSERT_EQUAL_UINT32(0xA5A5A5A5u, fields[0].value.ui32);
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, fields[1].type);
    TEST_ASSERT_TRUE(fields[1].value.b);
}

void test_uadp_network_message_rejects_too_many_fields(void) {
    mu_pubsub_field_t fields[MU_PUBSUB_MAX_FIELDS + 1];
    memset(fields, 0, sizeof(fields));
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        fields[i].value.type = MU_TYPE_UINT32;
        fields[i].value.value.ui32 = (opcua_uint32_t)i;
    }

    mu_pubsub_connection_t conn = {.port = 4840, .enabled = true, .publisher_id = 1u};
    mu_pubsub_writer_group_t wg = {
        .writer_group_id = 1,
        .publishing_interval_ms = 1000,
        .dataset_writer = {.data_set_writer_id = 1, .fields = fields, .field_count = MU_PUBSUB_MAX_FIELDS + 1}};

    opcua_byte_t buffer[128];
    size_t bytes_written = 123u;
    opcua_statuscode_t status = mu_encode_uadp_network_message(&conn, &wg, buffer, sizeof(buffer), &bytes_written);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_TOOMANYOPERATIONS, status);
    TEST_ASSERT_EQUAL_UINT32(0, bytes_written);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uadp_network_message_encoding);
    RUN_TEST(test_uadp_network_message_decoder_reads_publisher_payload);
    RUN_TEST(test_uadp_network_message_decoder_rejects_null_input_buffer);
    RUN_TEST(test_uadp_network_message_decoder_rejects_null_decoded_message_pointer);
    RUN_TEST(test_uadp_network_message_decoder_rejects_null_field_output_with_capacity);
    RUN_TEST(test_uadp_network_message_decoder_rejects_truncated_network_header);
    RUN_TEST(test_uadp_network_message_decoder_rejects_truncated_payload_header);
    RUN_TEST(test_uadp_network_message_decoder_rejects_dataset_message_size_mismatch);
    RUN_TEST(test_uadp_network_message_decoder_rejects_output_capacity_overflow);
    RUN_TEST(test_uadp_network_message_decoder_rejects_unsupported_publisher_id_type);
    RUN_TEST(test_uadp_network_message_decoder_rejects_security_header);
    RUN_TEST(test_uadp_network_message_decoder_rejects_multiple_dataset_messages);
    RUN_TEST(test_uadp_network_message_decoder_rejects_unsupported_dataset_message_type);
    RUN_TEST(test_uadp_network_message_rejects_too_many_fields);
    return UNITY_END();
}
