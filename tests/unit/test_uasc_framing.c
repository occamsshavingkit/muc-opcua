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

/* An OpenSecureChannel response chunk uses an AsymmetricSecurityHeader. For
   SecurityPolicy None it carries the None policy URI and null sender certificate
   / receiver thumbprint, so the body offset is fixed at
   MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE. (OPC 10000-6 6.7.4) */
void test_uasc_finalize_asymmetric_none(void) {
    opcua_byte_t buf[128];
    memset(buf, 0, sizeof(buf));

    /* 8 (MsgHeader) + 4 (ChannelId) + [4 + 47] (SecurityPolicyUri)
       + 4 (SenderCert null) + 4 (ReceiverThumbprint null) + 8 (SequenceHeader) = 79 */
    TEST_ASSERT_EQUAL(79, MU_UASC_ASYMMETRIC_NONE_HEADER_SIZE);

    buf[79] = 0xAB; /* 1-byte body */

    size_t total = 0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD,
        mu_uasc_finalize_asymmetric_none(buf, sizeof(buf),
                                         /*channel*/ 7, /*seq*/ 1, /*reqId*/ 1,
                                         /*body_length*/ 1, &total));

    TEST_ASSERT_EQUAL(80, total);
    TEST_ASSERT_EQUAL('O', buf[0]);
    TEST_ASSERT_EQUAL('P', buf[1]);
    TEST_ASSERT_EQUAL('N', buf[2]);
    TEST_ASSERT_EQUAL('F', buf[3]);
    TEST_ASSERT_EQUAL(80, buf[4]);           /* MessageSize */
    TEST_ASSERT_EQUAL(7, buf[8]);            /* SecureChannelId */
    TEST_ASSERT_EQUAL(47, buf[12]);          /* SecurityPolicyUri length */
    TEST_ASSERT_EQUAL_MEMORY("http://opcfoundation.org/UA/SecurityPolicy#None", &buf[16], 47);
    TEST_ASSERT_EQUAL(0xFF, buf[63]);        /* SenderCertificate null (-1) */
    TEST_ASSERT_EQUAL(0xFF, buf[67]);        /* ReceiverCertificateThumbprint null (-1) */
    TEST_ASSERT_EQUAL(1, buf[71]);           /* SequenceNumber */
    TEST_ASSERT_EQUAL(1, buf[75]);           /* RequestId */
    TEST_ASSERT_EQUAL(0xAB, buf[79]);        /* body preserved */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uasc_finalize_symmetric);
    RUN_TEST(test_uasc_finalize_asymmetric_none);
    return UNITY_END();
}

