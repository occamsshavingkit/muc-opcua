/* tests/unit/test_complex_types.c
 *
 * Spec Kit 037, T054-T057. Validate ComplexType Server Facet types
 * and registration API per OPC-10000-3 §5.6.4, §5.6.5.
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/address_space/complex_types.h"
#include "unity.h"
#include <string.h>

/* STUB: Complex type round-trip encode/decode behavioral tests not yet implemented. Only type creation and registration API tested. */

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_COMPLEX_TYPES

static mu_server_t s_server;

void test_structure_field_type_exists(void) {
    mu_structure_field_t f;
    memset(&f, 0, sizeof(f));
    f.name = "X";
    f.value_rank = -1;
    TEST_ASSERT_EQUAL_STRING("X", f.name);
    TEST_ASSERT_EQUAL_INT(-1, f.value_rank);
}

void test_structure_definition_type_exists(void) {
    mu_structure_definition_t d;
    memset(&d, 0, sizeof(d));
    d.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;
    d.field_count = 3;
    TEST_ASSERT_EQUAL_UINT32(0, d.structure_type);
    TEST_ASSERT_EQUAL_UINT16(3, d.field_count);
}

void test_enum_field_type_exists(void) {
    mu_enum_field_t f;
    memset(&f, 0, sizeof(f));
    f.value = 42;
    f.name = "OK";
    TEST_ASSERT_EQUAL_INT64(42, f.value);
    TEST_ASSERT_EQUAL_STRING("OK", f.name);
}

void test_register_structure_type_succeeds(void) {
    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;
    mu_nodeid_t type_id = {1u, MU_NODEID_NUMERIC, {2000u}};
    memset(&s_server, 0, sizeof(s_server));
    opcua_statuscode_t s = mu_register_structure_type(&s_server, &type_id, &def);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
}

void test_register_enumeration_type_succeeds(void) {
    mu_enum_definition_t def;
    memset(&def, 0, sizeof(def));
    mu_nodeid_t type_id = {1u, MU_NODEID_NUMERIC, {3000u}};
    memset(&s_server, 0, sizeof(s_server));
    opcua_statuscode_t s = mu_register_enumeration_type(&s_server, &type_id, &def);
    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, s);
}

#else

void test_complex_types_require_build_flag(void) {
    TEST_PASS_MESSAGE("MUC_OPCUA_COMPLEX_TYPES is disabled in this build");
}

#endif

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_COMPLEX_TYPES
    RUN_TEST(test_structure_field_type_exists);
    RUN_TEST(test_structure_definition_type_exists);
    RUN_TEST(test_enum_field_type_exists);
    RUN_TEST(test_register_structure_type_succeeds);
    RUN_TEST(test_register_enumeration_type_succeeds);
#else
    RUN_TEST(test_complex_types_require_build_flag);
#endif
    return UNITY_END();
}
