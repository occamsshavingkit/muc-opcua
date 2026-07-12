/* tests/unit/test_write_response.c
 * Binary fixture-driven tests for WriteResponse encoding/decoding.
 * FIxture: tests/fixtures/write_response.bin
 *   - ResponseHeader: timestamp=0, requestHandle=1, serviceResult=GOOD,
 *     serviceDiagnostics=empty, stringTable=null, additionalHeader=null
 *   - Results[1]: [GOOD]
 *   - DiagnosticInfos: null
 * OPC 10000-4 §5.11.4 Write Service, OPC 10000-6 §5.2 */

#include "../../src/services/service_header.h"
#include "../../src/services/write.h"
#include "muc_opcua/muc_opcua.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

#define FIXTURE_PATH PROJECT_ROOT_DIR "/tests/fixtures/write_response.bin"

/* Offsets into the fixture for decode field verification:
 *   0..7:   timestamp (Int64) = 0
 *   8..11:  requestHandle (UInt32) = 1
 *  12..15:  serviceResult (StatusCode) = GOOD
 *  16:     serviceDiagnostics (empty mask byte) = 0x00
 *  17..20: stringTable (Int32) = -1 (null)
 *  21..23: additionalHeader (null ExtensionObject) = 0x00 0x00 0x00
 *  24..27: Results[] count (Int32) = 1
 *  28..31: Results[0] (StatusCode) = GOOD
 *  32..35: DiagnosticInfos (Int32) = -1 (null) */

static const opcua_byte_t EXPECTED_FIXTURE[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* timestamp = 0 */
    0x01, 0x00, 0x00, 0x00,                         /* requestHandle = 1 */
    0x00, 0x00, 0x00, 0x00,                         /* serviceResult = GOOD */
    0x00,                                           /* serviceDiagnostics: empty */
    0xFF, 0xFF, 0xFF, 0xFF,                         /* stringTable: null */
    0x00, 0x00, 0x00,                               /* additionalHeader: null */
    0x01, 0x00, 0x00, 0x00,                         /* Results[] count = 1 */
    0x00, 0x00, 0x00, 0x00,                         /* Results[0] = GOOD */
    0xFF, 0xFF, 0xFF, 0xFF,                         /* DiagnosticInfos: null */
};

#define FIXTURE_SIZE sizeof(EXPECTED_FIXTURE)
#define RESPONSE_HEADER_SIZE 24
#define RESULTS_COUNT_OFFSET 24
#define RESULTS_OFFSET 28
#define DIAG_INFOS_OFFSET 32

void setUp(void) {}
void tearDown(void) {}

void test_write_response_encode_matches_fixture(void) {
#ifdef MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
    opcua_byte_t buf[FIXTURE_SIZE];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    mu_response_header_t header;
    header.timestamp = 0;
    header.request_handle = 1;
    header.service_result = MU_STATUS_GOOD;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_response_header_encode(&w, &header));
    TEST_ASSERT_EQUAL(RESPONSE_HEADER_SIZE, w.position);

    opcua_statuscode_t results[] = {MU_STATUS_GOOD};
    mu_write_response_t resp;
    resp.num_results = 1;
    resp.results = results;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_write_response_encode(&w, &resp));
    TEST_ASSERT_EQUAL(FIXTURE_SIZE, w.position);

    TEST_ASSERT_EQUAL_MEMORY(EXPECTED_FIXTURE, buf, FIXTURE_SIZE);
#endif
}

void test_write_response_fixture_decode_verify_fields(void) {
#ifdef MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
    FILE *f = fopen(FIXTURE_PATH, "rb");
    TEST_ASSERT_NOT_NULL(f);
    opcua_byte_t file_buf[FIXTURE_SIZE];
    size_t n = fread(file_buf, 1, sizeof(file_buf), f);
    (void)fclose(f);
    TEST_ASSERT_EQUAL(FIXTURE_SIZE, n);

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, file_buf, n);

    /* Verify ResponseHeader fields */
    opcua_int64_t timestamp;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int64(&r, &timestamp));
    TEST_ASSERT_EQUAL(0, timestamp);

    opcua_uint32_t request_handle;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_uint32(&r, &request_handle));
    TEST_ASSERT_EQUAL(1, request_handle);

    opcua_statuscode_t service_result;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &service_result));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, service_result);

    /* serviceDiagnostics: empty DiagnosticInfo = masking byte 0x00 */
    opcua_byte_t diag_mask;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_byte(&r, &diag_mask));
    TEST_ASSERT_EQUAL(0x00, diag_mask);

    /* stringTable: null array (Int32 = -1) */
    opcua_int32_t string_table_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &string_table_len));
    TEST_ASSERT_EQUAL(-1, string_table_len);

    /* additionalHeader: null ExtensionObject (NodeId ns=0,i=0 then encoding=0) */
    mu_nodeid_t add_type;
    size_t add_body_len;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_extension_object_header(&r, &add_type, &add_body_len));
    TEST_ASSERT_EQUAL(0, add_type.namespace_index);
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, add_type.identifier_type);
    TEST_ASSERT_EQUAL(0, add_type.identifier.numeric);
    TEST_ASSERT_EQUAL(0, add_body_len);

    TEST_ASSERT_EQUAL(RESPONSE_HEADER_SIZE, r.position);

    /* WriteResponse body: Results[] */
    opcua_int32_t results_count;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &results_count));
    TEST_ASSERT_EQUAL(1, results_count);

    opcua_statuscode_t result0;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &result0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, result0);

    /* DiagnosticInfos[]: null */
    opcua_int32_t diag_infos_count;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_infos_count));
    TEST_ASSERT_EQUAL(-1, diag_infos_count);

    TEST_ASSERT_EQUAL(FIXTURE_SIZE, r.position);
#endif
}

void test_write_response_encode_with_two_results(void) {
#ifdef MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
    opcua_byte_t buf[128];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    opcua_statuscode_t results[] = {MU_STATUS_GOOD, MU_STATUS_BAD_NOTWRITABLE};
    mu_write_response_t resp;
    resp.num_results = 2;
    resp.results = results;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_write_response_encode(&w, &resp));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    opcua_int32_t count;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &count));
    TEST_ASSERT_EQUAL(2, count);

    opcua_statuscode_t r0, r1;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &r0));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, r0);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_statuscode(&r, &r1));
    TEST_ASSERT_EQUAL(MU_STATUS_BAD_NOTWRITABLE, r1);

    opcua_int32_t diag_count;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));
    TEST_ASSERT_EQUAL(-1, diag_count);
#endif
}

void test_write_response_decode_null_results(void) {
#ifdef MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
    opcua_byte_t buf[32];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));

    mu_write_response_t resp;
    resp.num_results = 0;
    resp.results = NULL;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_write_response_encode(&w, &resp));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);

    opcua_int32_t count;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &count));
    TEST_ASSERT_EQUAL(0, count);

    opcua_int32_t diag_count;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_int32(&r, &diag_count));
    TEST_ASSERT_EQUAL(-1, diag_count);
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_response_encode_matches_fixture);
    RUN_TEST(test_write_response_fixture_decode_verify_fields);
    RUN_TEST(test_write_response_encode_with_two_results);
    RUN_TEST(test_write_response_decode_null_results);
    return UNITY_END();
}
