/* tests/unit/test_uasc_framing.c */
#include "unity.h"
#include "micro_opcua/micro_opcua.h"
#include "../../src/core/uasc.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* A symmetric (MSG) response chunk is:
   MessageHeader(8) + SecureChannelId(4) + SymmetricSecurityHeader/TokenId(4)
   + SequenceHeader(8) + MessageBody. Body starts at MU_UASC_SYMMETRIC_HEADER_SIZE.
   (OPC 10000-6 6.7.2 MessageChunk, 6.7.3 SymmetricSecurityHeader, 6.7.7 SequenceHeader) */
void test_uasc_finalize_symmetric(void) {
    opcua_byte_t buf[64];
    memset(buf, 0, sizeof(buf));

    TEST_ASSERT_EQUAL(24, MU_UASC_SYMMETRIC_HEADER_SIZE);

    /* caller has already written a 4-byte body at the header offset */
    buf[24] = 0xDE; buf[25] = 0xAD; buf[26] = 0xBE; buf[27] = 0xEF;

    size_t total = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_uasc_finalize_symmetric(buf, sizeof(buf),
                                   /*channel*/ 1, /*token*/ 2,
                                   /*seq*/ 3, /*reqId*/ 4,
                                   /*body_length*/ 4, &total));

    TEST_ASSERT_EQUAL(28, total);
    TEST_ASSERT_EQUAL('M', buf[0]);
    TEST_ASSERT_EQUAL('S', buf[1]);
    TEST_ASSERT_EQUAL('G', buf[2]);
    TEST_ASSERT_EQUAL('F', buf[3]);          /* IsFinal */
    TEST_ASSERT_EQUAL(28, buf[4]);           /* MessageSize (low byte) */
    TEST_ASSERT_EQUAL(1, buf[8]);            /* SecureChannelId */
    TEST_ASSERT_EQUAL(2, buf[12]);           /* TokenId */
    TEST_ASSERT_EQUAL(3, buf[16]);           /* SequenceNumber */
    TEST_ASSERT_EQUAL(4, buf[20]);           /* RequestId */
    TEST_ASSERT_EQUAL(0xDE, buf[24]);        /* body preserved */
    TEST_ASSERT_EQUAL(0xEF, buf[27]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uasc_finalize_symmetric);
    return UNITY_END();
}
