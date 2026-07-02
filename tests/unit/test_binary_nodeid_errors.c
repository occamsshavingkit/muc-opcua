/* tests/unit/test_binary_nodeid_errors.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* OPC-10000-6 section 5.2.2.9: NodeId binary encoding masks and payloads. */
static void assert_buffer_equal(const opcua_byte_t *expected, const opcua_byte_t *actual, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) {
        TEST_ASSERT_EQUAL_HEX8(expected[i], actual[i]);
    }
}

static void assert_read_numeric_nodeid(const opcua_byte_t *buffer, size_t length, opcua_uint16_t namespace_index,
                                       opcua_uint32_t identifier) {
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, length);

    mu_nodeid_t node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &node_id));
    TEST_ASSERT_EQUAL(length, reader.position);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, node_id.identifier_type);
    TEST_ASSERT_EQUAL_UINT16(namespace_index, node_id.namespace_index);
    TEST_ASSERT_EQUAL_UINT32(identifier, node_id.identifier.numeric);
}

static void assert_truncated_nodeid_fails(const opcua_byte_t *buffer, size_t length) {
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, length);

    mu_nodeid_t node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_nodeid(&reader, &node_id));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, reader.status);
}

static void assert_expanded_nodeid_fails(const opcua_byte_t *buffer, size_t length) {
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, length);

    mu_expanded_nodeid_t node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_expanded_nodeid(&reader, &node_id));
}

void test_binary_nodeid_read_preserves_two_byte_variant(void) {
    const opcua_byte_t buffer[] = {0x00, 0x48};

    assert_read_numeric_nodeid(buffer, sizeof(buffer), 0u, 72u);
}

void test_binary_nodeid_read_preserves_four_byte_variant(void) {
    const opcua_byte_t buffer[] = {0x01, 0x05, 0x01, 0x04};

    assert_read_numeric_nodeid(buffer, sizeof(buffer), 5u, 1025u);
}

void test_binary_nodeid_read_preserves_numeric_variant(void) {
    const opcua_byte_t buffer[] = {0x02, 0x2c, 0x01, 0x04, 0x03, 0x02, 0x01};

    assert_read_numeric_nodeid(buffer, sizeof(buffer), 300u, 0x01020304u);
}

void test_binary_nodeid_read_preserves_string_variant(void) {
    const opcua_byte_t buffer[] = {0x03, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 't', 'e', 's', 't'};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_nodeid_t node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&reader, &node_id));
    TEST_ASSERT_EQUAL(sizeof(buffer), reader.position);
    TEST_ASSERT_EQUAL(MU_NODEID_STRING, node_id.identifier_type);
    TEST_ASSERT_EQUAL_UINT16(1u, node_id.namespace_index);
    TEST_ASSERT_EQUAL_INT32(4, node_id.identifier.string.length);
    TEST_ASSERT_EQUAL_MEMORY("test", node_id.identifier.string.data, 4);
}

void test_binary_nodeid_write_preserves_supported_compact_variants(void) {
    const mu_nodeid_t two_byte = {0u, MU_NODEID_NUMERIC, {.numeric = 72u}};
    const mu_nodeid_t four_byte = {5u, MU_NODEID_NUMERIC, {.numeric = 1025u}};
    const mu_nodeid_t numeric = {300u, MU_NODEID_NUMERIC, {.numeric = 0x01020304u}};
    opcua_byte_t buffer[16];
    mu_binary_writer_t writer;

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &two_byte));
    {
        const opcua_byte_t expected[] = {0x00, 0x48};
        TEST_ASSERT_EQUAL(sizeof(expected), writer.position);
        assert_buffer_equal(expected, buffer, sizeof(expected));
    }

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &four_byte));
    {
        const opcua_byte_t expected[] = {0x01, 0x05, 0x01, 0x04};
        TEST_ASSERT_EQUAL(sizeof(expected), writer.position);
        assert_buffer_equal(expected, buffer, sizeof(expected));
    }

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &numeric));
    {
        const opcua_byte_t expected[] = {0x02, 0x2c, 0x01, 0x04, 0x03, 0x02, 0x01};
        TEST_ASSERT_EQUAL(sizeof(expected), writer.position);
        assert_buffer_equal(expected, buffer, sizeof(expected));
    }
}

void test_binary_nodeid_write_preserves_string_variant(void) {
    const mu_nodeid_t string_id = {1u, MU_NODEID_STRING, {.string = {4, (const opcua_byte_t *)"test"}}};
    const opcua_byte_t expected[] = {0x03, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 't', 'e', 's', 't'};
    opcua_byte_t buffer[16];
    mu_binary_writer_t writer;

    mu_binary_writer_init(&writer, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&writer, &string_id));
    TEST_ASSERT_EQUAL(sizeof(expected), writer.position);
    assert_buffer_equal(expected, buffer, sizeof(expected));
}

void test_binary_nodeid_invalid_encoding_mask(void) {
    opcua_byte_t buffer[2] = {0x06, 0x00}; /* 0x06 is invalid encoding format */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_nodeid_t node_id;
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_nodeid(&reader, &node_id));
}

void test_binary_nodeid_truncated_two_byte_variant_fails(void) {
    const opcua_byte_t buffer[] = {0x00};

    assert_truncated_nodeid_fails(buffer, sizeof(buffer));
}

void test_binary_nodeid_truncated_four_byte_variant_fails(void) {
    const opcua_byte_t buffer[] = {0x01, 0x05, 0x01};

    assert_truncated_nodeid_fails(buffer, sizeof(buffer));
}

void test_binary_nodeid_truncated_numeric_variant_fails(void) {
    const opcua_byte_t buffer[] = {0x02, 0x2c, 0x01, 0x04, 0x03, 0x02};

    assert_truncated_nodeid_fails(buffer, sizeof(buffer));
}

void test_binary_nodeid_truncated_string_variant_fails(void) {
    const opcua_byte_t buffer[] = {0x03, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 't', 'e', 's'};

    assert_truncated_nodeid_fails(buffer, sizeof(buffer));
}

void test_binary_expanded_nodeid_truncated_namespace_uri_fails(void) {
    /* OPC-10000-6 section 5.2.2.10: NamespaceUriFlag requires a complete
       String after the embedded NodeId. */
    const opcua_byte_t buffer[] = {0x80u, 0x48u, 0x04u, 0x00u, 0x00u, 0x00u, 't', 'e'};

    assert_expanded_nodeid_fails(buffer, sizeof(buffer));
}

void test_binary_expanded_nodeid_truncated_server_index_fails(void) {
    /* OPC-10000-6 section 5.2.2.10: ServerIndexFlag requires a complete UInt32
       after the embedded NodeId and optional NamespaceUri. */
    const opcua_byte_t buffer[] = {0x40u, 0x48u, 0x34u, 0x12u};

    assert_expanded_nodeid_fails(buffer, sizeof(buffer));
}

void test_binary_nodeid_guid_identifier_is_rejected_cleanly(void) {
    const opcua_byte_t buffer[] = {0x04u, 0x01u, 0x00u, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u,
                                   0x07u, 0x08u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_nodeid_t node_id;
    /* OPC-10000-6 section 5.2.2.9 Guid NodeIds are either supported or rejected
       deterministically; this profile explicitly rejects them. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_nodeid(&reader, &node_id));
}

void test_binary_nodeid_opaque_identifier_is_rejected_cleanly(void) {
    const opcua_byte_t buffer[] = {0x05u, 0x01u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0xaau, 0xbbu, 0xccu};
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buffer, sizeof(buffer));

    mu_nodeid_t node_id;
    /* OPC-10000-6 section 5.2.2.9 Opaque NodeIds are either supported or
       rejected deterministically; this profile explicitly rejects them. */
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_DECODINGERROR, mu_binary_read_nodeid(&reader, &node_id));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_nodeid_read_preserves_two_byte_variant);
    RUN_TEST(test_binary_nodeid_read_preserves_four_byte_variant);
    RUN_TEST(test_binary_nodeid_read_preserves_numeric_variant);
    RUN_TEST(test_binary_nodeid_read_preserves_string_variant);
    RUN_TEST(test_binary_nodeid_write_preserves_supported_compact_variants);
    RUN_TEST(test_binary_nodeid_write_preserves_string_variant);
    RUN_TEST(test_binary_nodeid_invalid_encoding_mask);
    RUN_TEST(test_binary_nodeid_truncated_two_byte_variant_fails);
    RUN_TEST(test_binary_nodeid_truncated_four_byte_variant_fails);
    RUN_TEST(test_binary_nodeid_truncated_numeric_variant_fails);
    RUN_TEST(test_binary_nodeid_truncated_string_variant_fails);
    RUN_TEST(test_binary_expanded_nodeid_truncated_namespace_uri_fails);
    RUN_TEST(test_binary_expanded_nodeid_truncated_server_index_fails);
    RUN_TEST(test_binary_nodeid_guid_identifier_is_rejected_cleanly);
    RUN_TEST(test_binary_nodeid_opaque_identifier_is_rejected_cleanly);
    return UNITY_END();
}
