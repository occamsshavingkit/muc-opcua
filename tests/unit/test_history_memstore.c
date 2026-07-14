#include "muc_opcua/services/history_memstore.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static mu_historical_data_point_t point(opcua_datetime_t timestamp, opcua_int32_t value) {
    mu_historical_data_point_t p;
    (void)memset(&p, 0, sizeof(p));
    p.source_timestamp = timestamp;
    p.server_timestamp = timestamp;
    p.status = MU_STATUS_GOOD;
    p.value.type = MU_TYPE_INT32;
    p.value.value.i32 = value;
    return p;
}

void test_read_continuation_resumes_after_filtered_scan(void) {
    mu_history_memstore_t store;
    mu_historical_data_point_t out[1];
    opcua_byte_t cp[sizeof(size_t)];
    size_t cp_len = 0;
    size_t actual = 0;
    size_t next_index = 0;

    (void)memset(&store, 0, sizeof(store));
    store.points[store.count++] = point(10, 10);
    store.points[store.count++] = point(20, 20);
    store.points[store.count++] = point(30, 30);
    store.points[store.count++] = point(40, 40);

    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_memstore_read_raw_modified(&store, NULL, false, 25, 100, 1, false, NULL,
                                                                          0, cp, &cp_len, out, 1, &actual));
    TEST_ASSERT_EQUAL(1, actual);
    TEST_ASSERT_EQUAL_INT64(30, out[0].source_timestamp);
    TEST_ASSERT_EQUAL(sizeof(size_t), cp_len);
    (void)memcpy(&next_index, cp, sizeof(next_index));
    TEST_ASSERT_EQUAL(3, next_index);

    cp_len = 0;
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD,
                            mu_memstore_read_raw_modified(&store, NULL, false, 25, 100, 1, false, cp,
                                                          sizeof(next_index), cp, &cp_len, out, 1, &actual));
    TEST_ASSERT_EQUAL(1, actual);
    TEST_ASSERT_EQUAL_INT64(40, out[0].source_timestamp);
}

void test_insert_rejects_existing_timestamp_and_replace_requires_existing_timestamp(void) {
    mu_history_memstore_t store;
    mu_historical_data_point_t update;
    opcua_statuscode_t result = 0;

    (void)memset(&store, 0, sizeof(store));
    store.points[store.count++] = point(10, 10);

    update = point(10, 99);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_memstore_update_data(&store, NULL, 1, &update, 1, &result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_ENTRYEXISTS, result);
    TEST_ASSERT_EQUAL(1, store.count);
    TEST_ASSERT_EQUAL_INT32(10, store.points[0].value.value.i32);

    update = point(20, 20);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_memstore_update_data(&store, NULL, 1, &update, 1, &result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, result);
    TEST_ASSERT_EQUAL(2, store.count);

    update = point(30, 30);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_memstore_update_data(&store, NULL, 2, &update, 1, &result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_NOENTRYEXISTS, result);
    TEST_ASSERT_EQUAL(2, store.count);

    update = point(20, 200);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, mu_memstore_update_data(&store, NULL, 2, &update, 1, &result));
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, result);
    TEST_ASSERT_EQUAL(2, store.count);
    TEST_ASSERT_EQUAL_INT32(200, store.points[1].value.value.i32);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_continuation_resumes_after_filtered_scan);
    RUN_TEST(test_insert_rejects_existing_timestamp_and_replace_requires_existing_timestamp);
    return UNITY_END();
}
