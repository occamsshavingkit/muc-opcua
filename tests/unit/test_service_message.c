/* tests/unit/test_service_message.c
 *
 * Unit tests for mu_parse_service_prefix and mu_write_service_prefix
 * (src/core/service_message.c). These are on the hot dispatch path. */

#include "muc_opcua/encoding.h"
#include "unity.h"

/* Internal function under test — declared in service_message.h */
opcua_statuscode_t mu_parse_service_prefix(const opcua_byte_t *buffer, size_t length, size_t *offset,
                                           mu_nodeid_t *node_id);
opcua_statuscode_t mu_write_service_prefix(opcua_byte_t *buffer, size_t length, size_t *offset,
                                           const mu_nodeid_t *node_id);

void setUp(void) {}
void tearDown(void) {}

/* Parse a valid NodeId from a buffer and verify the result. */
void test_parse_service_prefix_valid_nodeid(void) {
    opcua_byte_t buf[8];
    mu_nodeid_t node_id;
    size_t offset = 0;

    /* Encode a ns=0 numeric NodeId with value 42 using TwoByte encoding */
    buf[0] = 0x00; /* encoding mask: ns=0, TwoByte */
    buf[1] = 42;   /* numeric value */
    opcua_statuscode_t s = mu_parse_service_prefix(buf, 2, &offset, &node_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, node_id.identifier_type);
    TEST_ASSERT_EQUAL(0u, node_id.namespace_index);
    TEST_ASSERT_EQUAL(42u, node_id.identifier.numeric);
    TEST_ASSERT_EQUAL(2u, offset);
}

/* Parse from NULL buffer returns Bad_InternalError. */
void test_parse_service_prefix_null_buffer(void) {
    mu_nodeid_t node_id;
    size_t offset = 0;
    opcua_statuscode_t s = mu_parse_service_prefix(NULL, 10, &offset, &node_id);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, s);
}

/* Write-then-read round trip: written NodeId is parsed back identically. */
void test_write_then_parse_service_prefix_roundtrip(void) {
    opcua_byte_t buf[16];
    size_t offset = 0;

    mu_nodeid_t write_id = {0u, MU_NODEID_NUMERIC, {99u}};
    opcua_statuscode_t s = mu_write_service_prefix(buf, sizeof(buf), &offset, &write_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_TRUE(offset > 0);

    size_t read_offset = 0;
    mu_nodeid_t read_id;
    s = mu_parse_service_prefix(buf, offset, &read_offset, &read_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(write_id.namespace_index, read_id.namespace_index);
    TEST_ASSERT_EQUAL(write_id.identifier_type, read_id.identifier_type);
    TEST_ASSERT_EQUAL(write_id.identifier.numeric, read_id.identifier.numeric);
    TEST_ASSERT_EQUAL(offset, read_offset);
}

/* Write with NULL buffer returns Bad_InternalError. */
void test_write_service_prefix_null_buffer(void) {
    mu_nodeid_t node_id = {0u, MU_NODEID_NUMERIC, {1u}};
    size_t offset = 0;
    opcua_statuscode_t s = mu_write_service_prefix(NULL, 10, &offset, &node_id);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, s);
}

/* Round-trip with boundary NodeId value: ns=0 numeric 0. */
void test_write_then_parse_boundary_zero(void) {
    opcua_byte_t buf[16];
    size_t offset = 0;

    mu_nodeid_t write_id = {0u, MU_NODEID_NUMERIC, {0u}};
    opcua_statuscode_t s = mu_write_service_prefix(buf, sizeof(buf), &offset, &write_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_TRUE(offset > 0);

    size_t read_offset = 0;
    mu_nodeid_t read_id;
    s = mu_parse_service_prefix(buf, offset, &read_offset, &read_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(0u, read_id.namespace_index);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, read_id.identifier_type);
    TEST_ASSERT_EQUAL(0u, read_id.identifier.numeric);
}

/* Round-trip with boundary NodeId value: ns=0 numeric max uint32. */
void test_write_then_parse_boundary_max_uint32(void) {
    opcua_byte_t buf[16];
    size_t offset = 0;

    mu_nodeid_t write_id = {0u, MU_NODEID_NUMERIC, {0xFFFFFFFFu}};
    opcua_statuscode_t s = mu_write_service_prefix(buf, sizeof(buf), &offset, &write_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_TRUE(offset > 0);

    size_t read_offset = 0;
    mu_nodeid_t read_id;
    s = mu_parse_service_prefix(buf, offset, &read_offset, &read_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, read_id.identifier_type);
    TEST_ASSERT_EQUAL(0xFFFFFFFFu, read_id.identifier.numeric);
}

/* Round-trip with boundary NodeId value: ns=65535 numeric 0. */
void test_write_then_parse_boundary_max_namespace(void) {
    opcua_byte_t buf[16];
    size_t offset = 0;

    mu_nodeid_t write_id = {65535u, MU_NODEID_NUMERIC, {0u}};
    opcua_statuscode_t s = mu_write_service_prefix(buf, sizeof(buf), &offset, &write_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_TRUE(offset > 0);

    size_t read_offset = 0;
    mu_nodeid_t read_id;
    s = mu_parse_service_prefix(buf, offset, &read_offset, &read_id);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
    TEST_ASSERT_EQUAL(65535u, read_id.namespace_index);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, read_id.identifier_type);
    TEST_ASSERT_EQUAL(0u, read_id.identifier.numeric);
}

/* Parse with truncated/invalid input returns error. */
void test_parse_service_prefix_truncated(void) {
    opcua_byte_t buf[1] = {0xFF}; /* invalid encoding mask, only 1 byte */
    mu_nodeid_t node_id;
    size_t offset = 0;
    opcua_statuscode_t s = mu_parse_service_prefix(buf, 1, &offset, &node_id);
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, s);
}

/* Parse with empty buffer returns error. */
void test_parse_service_prefix_empty(void) {
    opcua_byte_t buf[1] = {0x00};
    mu_nodeid_t node_id;
    size_t offset = 0;
    opcua_statuscode_t s = mu_parse_service_prefix(buf, 0, &offset, &node_id);
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_GOOD, s);
}

/* Write with NULL node_id returns error. */
void test_write_service_prefix_null_nodeid(void) {
    opcua_byte_t buf[16];
    size_t offset = 0;
    opcua_statuscode_t s = mu_write_service_prefix(buf, sizeof(buf), &offset, NULL);
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_INTERNALERROR, s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_service_prefix_valid_nodeid);
    RUN_TEST(test_parse_service_prefix_null_buffer);
    RUN_TEST(test_write_then_parse_service_prefix_roundtrip);
    RUN_TEST(test_write_service_prefix_null_buffer);
    RUN_TEST(test_write_then_parse_boundary_zero);
    RUN_TEST(test_write_then_parse_boundary_max_uint32);
    RUN_TEST(test_write_then_parse_boundary_max_namespace);
    RUN_TEST(test_parse_service_prefix_truncated);
    RUN_TEST(test_parse_service_prefix_empty);
    RUN_TEST(test_write_service_prefix_null_nodeid);
    return UNITY_END();
}
