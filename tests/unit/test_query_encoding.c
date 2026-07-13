#include "../../src/services/query.h"
#include "muc_opcua/encoding.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MUC_OPCUA_CU_QUERY

void test_content_filter_decode(void) {
    /* Create a binary buffer containing a ContentFilter with 1 element: ElementOperand */
    /* ContentFilter = UInt32 size, [ ContentFilterElement ] */
    /* ContentFilterElement = FilterOperator operator, UInt32 operandSize, [ FilterOperand ] */
    /* FilterOperand = ExtensionObject { TypeId, Encoding, Length, ByteString... } */
    /* Wait, for simplicity let's just make an empty ContentFilter */
    opcua_byte_t buf[] = {
        0x00, 0x00, 0x00, 0x00 /* 0 elements */
    };

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buf, sizeof(buf));

    mu_content_filter_t filter;
    mu_content_filter_element_t elements[1];
    mu_filter_operand_t operands[1];

    opcua_statuscode_t status = mu_content_filter_decode(&reader, &filter, elements, 1, operands, 1);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_EQUAL_UINT32(0, filter.elements_count);
}

/* Feature 025 (F11): an unsupported filter operand whose declared ExtensionObject
   body length exceeds the bytes actually remaining must be rejected, not skipped
   past the end of the buffer. OPC-10000-6 §5.2.2.15. */
void test_content_filter_rejects_unsupported_operand_oversized_length(void) {
    opcua_byte_t buf[64];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1)); /* 1 element */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 0)); /* filter operator */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_uint32(&w, 1)); /* 1 operand */
    /* Operand ExtensionObject: unsupported typeId (not 594/597), ByteString body
       claiming far more bytes than remain in the buffer. */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_nodeid(&w, &(mu_nodeid_t){.namespace_index = 0,
                                                                                .identifier_type = MU_NODEID_NUMERIC,
                                                                                .identifier.numeric = 999}));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_byte(&w, 0x01));  /* encoding = ByteString */
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_int32(&w, 1000)); /* body length >> remaining */

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buf, w.position);

    mu_content_filter_t filter;
    mu_content_filter_element_t elements[1];
    mu_filter_operand_t operands[1];
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR,
                            mu_content_filter_decode(&reader, &filter, elements, 1, operands, 1));
}

void test_query_next_request_decode(void) {
    opcua_byte_t buf[] = {
        0x01,                                      /* release_continuation_point = true */
        0x04, 0x00, 0x00, 0x00, 'T', 'e', 's', 't' /* continuation_point = "Test" */
    };
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, buf, sizeof(buf));

    mu_query_next_request_t req;
    opcua_statuscode_t status = mu_query_next_request_decode(&reader, &req);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, status);
    TEST_ASSERT_TRUE(req.release_continuation_point);
    TEST_ASSERT_EQUAL_INT32(4, req.continuation_point.length);
    TEST_ASSERT_EQUAL_STRING_LEN("Test", req.continuation_point.data, 4);
}

#endif /* MUC_OPCUA_CU_QUERY */

int main(void) {
    UNITY_BEGIN();
#ifdef MUC_OPCUA_CU_QUERY
    RUN_TEST(test_content_filter_decode);
    RUN_TEST(test_content_filter_rejects_unsupported_operand_oversized_length);
    RUN_TEST(test_query_next_request_decode);
#endif
    return UNITY_END();
}
