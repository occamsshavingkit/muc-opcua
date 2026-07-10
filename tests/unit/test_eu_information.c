/* tests/unit/test_eu_information.c
 * EUInformation binary codec (spec 060, OPC-10000-8 §5.6.4.3): round-trip the
 * four fields (namespaceUri String, unitId Int32, displayName/description
 * LocalizedText), plus the underlying LocalizedText codec (§5.2.2.14). */
#include "muc_opcua/encoding.h"
#include "muc_opcua/types.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_DATA_ACCESS

static mu_string_t s(const char *cstr) {
    mu_string_t v = {(opcua_int32_t)strlen(cstr), (const opcua_byte_t *)cstr};
    return v;
}

static void assert_string_eq(const char *expect, const mu_string_t *actual) {
    TEST_ASSERT_EQUAL((opcua_int32_t)strlen(expect), actual->length);
    TEST_ASSERT_EQUAL_MEMORY(expect, actual->data, (size_t)actual->length);
}

/* LocalizedText round-trip with both locale and text present. */
void test_localized_text_roundtrip_full(void) {
    opcua_byte_t buf[128];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    mu_localized_text_t in = {s("en-US"), s("meter per second")};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_localized_text(&w, &in));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    mu_localized_text_t out;
    (void)memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_localized_text(&r, &out));
    assert_string_eq("en-US", &out.locale);
    assert_string_eq("meter per second", &out.text);
    /* Mask(1) + locale(4+5) + text(4+16) = 30 bytes. */
    TEST_ASSERT_EQUAL(1u + 4u + 5u + 4u + 16u, w.position);
}

/* LocalizedText with text only (no locale) — the common case; mask bit 0 clear. */
void test_localized_text_roundtrip_text_only(void) {
    opcua_byte_t buf[64];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    mu_localized_text_t in;
    (void)memset(&in, 0, sizeof(in));
    in.locale.length = -1; /* null string => absent */
    in.text = s("hour");
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_localized_text(&w, &in));
    TEST_ASSERT_EQUAL(0x02, buf[0]); /* only Text-present bit set */

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    mu_localized_text_t out;
    (void)memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_localized_text(&r, &out));
    TEST_ASSERT_TRUE(out.locale.length <= 0); /* absent */
    assert_string_eq("hour", &out.text);
}

/* Full EUInformation round-trip. */
void test_eu_information_roundtrip(void) {
    opcua_byte_t buf[256];
    mu_binary_writer_t w;
    mu_binary_writer_init(&w, buf, sizeof(buf));
    mu_eu_information_t in;
    (void)memset(&in, 0, sizeof(in));
    in.namespace_uri = s("http://www.opcfoundation.org/UA/units/un/cefact");
    in.unit_id = 4405297; /* "m/s" UNECE code */
    in.display_name = (mu_localized_text_t){s("en"), s("m/s")};
    in.description = (mu_localized_text_t){s("en"), s("metre per second")};
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_write_eu_information(&w, &in));

    mu_binary_reader_t r;
    mu_binary_reader_init(&r, buf, w.position);
    mu_eu_information_t out;
    (void)memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_binary_read_eu_information(&r, &out));
    assert_string_eq("http://www.opcfoundation.org/UA/units/un/cefact", &out.namespace_uri);
    TEST_ASSERT_EQUAL_INT32(4405297, out.unit_id);
    assert_string_eq("m/s", &out.display_name.text);
    assert_string_eq("metre per second", &out.description.text);
    /* The whole buffer must be consumed (no trailing bytes). */
    TEST_ASSERT_EQUAL(w.position, r.position);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_localized_text_roundtrip_full);
    RUN_TEST(test_localized_text_roundtrip_text_only);
    RUN_TEST(test_eu_information_roundtrip);
    return UNITY_END();
}

#else /* !MUC_OPCUA_DATA_ACCESS */

void test_eu_information_skipped(void) {
    TEST_IGNORE_MESSAGE("Data Access feature not enabled");
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_eu_information_skipped);
    return UNITY_END();
}

#endif
