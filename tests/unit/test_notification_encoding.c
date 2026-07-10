/* tests/unit/test_notification_encoding.c
 * Regression for #284: the NotificationData ExtensionObject TypeIds a Publish
 * response carries must be the correct OPC UA *_Encoding_DefaultBinary NodeIds
 * (from NodeIds.csv), else standard clients cannot decode the body and the
 * Publish stream desyncs. Verified on the wire by calling the writers directly. */
#include "muc_opcua/encoding.h"
#include "muc_opcua/opcua_types.h"
#include "unity.h"

#if MUC_OPCUA_SUBSCRIPTIONS
#include "services/subscription_publish/common.h"

void setUp(void) {}
void tearDown(void) {}

/* StatusChangeNotification_Encoding_DefaultBinary = 820 (NodeIds.csv). The bug:
   813 (a non-existent node) → clients fail to decode → spurious ServiceFault. */
void test_status_change_notification_typeid_is_820(void) {
    opcua_byte_t buf[64];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, write_status_change_notification(&w, MU_STATUS_BAD_TIMEOUT));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    mu_nodeid_t type_id;
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_nodeid(&r, &type_id));
    TEST_ASSERT_EQUAL(MU_NODEID_NUMERIC, type_id.identifier_type);
    TEST_ASSERT_EQUAL(0, type_id.namespace_index);
    TEST_ASSERT_EQUAL_UINT32(820u, type_id.identifier.numeric);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_status_change_notification_typeid_is_820);
    return UNITY_END();
}

#else
void setUp(void) {}
void tearDown(void) {}
void test_subscriptions_disabled(void) {
    TEST_IGNORE_MESSAGE("Subscriptions not enabled");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_subscriptions_disabled);
    return UNITY_END();
}
#endif
