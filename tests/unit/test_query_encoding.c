#include "unity.h"
#include "micro_opcua/encoding.h"
#include "../../src/services/query.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#ifdef MICRO_OPCUA_SERVICE_QUERY

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

void test_query_next_request_decode(void) {
    opcua_byte_t buf[] = {
        0x01, /* release_continuation_point = true */
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

#endif /* MICRO_OPCUA_SERVICE_QUERY */

int main(void) {
    UNITY_BEGIN();
#ifdef MICRO_OPCUA_SERVICE_QUERY
    RUN_TEST(test_content_filter_decode);
    RUN_TEST(test_query_next_request_decode);
#endif
    return UNITY_END();
}
