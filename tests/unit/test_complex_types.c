/* tests/unit/test_complex_types.c
 *
 * ComplexType Server Facet: type registration + binary encode/decode round-trip tests.
 * OPC-10000-3 §5.6.4 (StructureDefinition), §5.6.5 (EnumDefinition),
 * OPC-10000-6 §5.2.2.12 (Structure), §5.2.2.9 (Enumeration), §5.4.1.
 */
#include "../../src/core/server_internal.h"
#include "muc_opcua/address_space/complex_types.h"
#include "muc_opcua/encoding.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_COMPLEX_TYPES

static mu_server_t s_server;
static opcua_byte_t encode_buf[4096];
static opcua_byte_t decode_buf[4096];

/* Round-trip test helpers */

static mu_nodeid_t make_numeric_nodeid(opcua_uint16_t ns, opcua_uint32_t id) {
    mu_nodeid_t nid;
    nid.namespace_index = ns;
    nid.identifier_type = MU_NODEID_NUMERIC;
    nid.identifier.numeric = id;
    return nid;
}

static mu_structure_field_t make_field(const char *name, opcua_uint32_t type_node_id, opcua_int32_t rank,
                                       opcua_boolean_t optional) {
    mu_structure_field_t f;
    memset(&f, 0, sizeof(f));
    f.name = name;
    f.data_type = make_numeric_nodeid(0, type_node_id);
    f.value_rank = rank;
    f.is_optional = optional;
    return f;
}

/* T023: Round-trip test for scalar fields (OPC-10000-6 §5.2.2.12) */
void test_struct_roundtrip_scalars(void) {
    typedef struct {
        opcua_int64_t i64;
        opcua_uint64_t u64;
        opcua_double_t d;
        mu_string_t s;
        opcua_int32_t i32;
        opcua_uint32_t u32;
        opcua_float_t f;
        opcua_int16_t i16;
        opcua_uint16_t u16;
        opcua_boolean_t b;
        opcua_sbyte_t sb;
        opcua_byte_t by;
    } test_fields_t;

    mu_structure_field_t fields[12];
    fields[0] = make_field("Int64", 8, -1, false);
    fields[1] = make_field("UInt64", 9, -1, false);
    fields[2] = make_field("Double", 11, -1, false);
    fields[3] = make_field("String", 12, -1, false);
    fields[4] = make_field("Int32", 6, -1, false);
    fields[5] = make_field("UInt32", 7, -1, false);
    fields[6] = make_field("Float", 10, -1, false);
    fields[7] = make_field("Int16", 4, -1, false);
    fields[8] = make_field("UInt16", 5, -1, false);
    fields[9] = make_field("Bool", 1, -1, false);
    fields[10] = make_field("SByte", 2, -1, false);
    fields[11] = make_field("Byte", 3, -1, false);

    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = 12;
    def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    mu_nodeid_t type_id = make_numeric_nodeid(1, 4023);
    memset(&s_server, 0, sizeof(s_server));
    opcua_statuscode_t sc = mu_register_structure_type(&s_server, &type_id, &def);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    const mu_structure_definition_t *registered_def = mu_find_structure_definition(&s_server, &type_id);
    TEST_ASSERT_NOT_NULL(registered_def);

    test_fields_t in_val;
    memset(&in_val, 0, sizeof(in_val));
    in_val.i64 = -7000000000LL;
    in_val.u64 = 8000000000ULL;
    in_val.d = 2.5;
    in_val.s.length = 5;
    in_val.s.data = (opcua_byte_t *)"hello";
    in_val.i32 = -50000;
    in_val.u32 = 60000;
    in_val.f = 1.5f;
    in_val.i16 = -300;
    in_val.u16 = 400;
    in_val.b = true;
    in_val.sb = -42;
    in_val.by = 200;

    memset(encode_buf, 0, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));

    sc = mu_binary_encode_struct(&writer, registered_def, &in_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_TRUE(writer.position > 0);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);

    test_fields_t out_val;
    memset(&out_val, 0, sizeof(out_val));
    sc = mu_binary_decode_struct(&reader, registered_def, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);

    TEST_ASSERT_EQUAL_INT64(in_val.i64, out_val.i64);
    TEST_ASSERT_EQUAL_UINT64(in_val.u64, out_val.u64);
    TEST_ASSERT_TRUE(in_val.d == out_val.d);
    TEST_ASSERT_EQUAL_INT32(in_val.s.length, out_val.s.length);
    TEST_ASSERT_EQUAL_MEMORY(in_val.s.data, out_val.s.data, (size_t)in_val.s.length);
    TEST_ASSERT_EQUAL_INT32(in_val.i32, out_val.i32);
    TEST_ASSERT_EQUAL_UINT32(in_val.u32, out_val.u32);
    TEST_ASSERT_TRUE(in_val.f == out_val.f);
    TEST_ASSERT_EQUAL_INT16(in_val.i16, out_val.i16);
    TEST_ASSERT_EQUAL_UINT16(in_val.u16, out_val.u16);
    TEST_ASSERT_EQUAL_UINT8(1, (opcua_byte_t)out_val.b);
    TEST_ASSERT_EQUAL_INT8(in_val.sb, out_val.sb);
    TEST_ASSERT_EQUAL_UINT8(in_val.by, out_val.by);
}

/* T024: Round-trip test for optional fields with EncodingMask (OPC-10000-6 §5.2.2.12) */
void test_struct_roundtrip_optional_fields(void) {
    typedef struct {
        opcua_uint32_t mask;
        opcua_int32_t a;
        opcua_int32_t b;
        opcua_int32_t c;
    } test_opt_t;

    mu_structure_field_t fields[3];
    fields[0] = make_field("A", 6, -1, true);
    fields[1] = make_field("B", 6, -1, true);
    fields[2] = make_field("C", 6, -1, true);

    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = 3;
    def.structure_type = MU_STRUCTURE_TYPE_OPTIONAL;

    mu_nodeid_t type_id = make_numeric_nodeid(1, 4024);
    memset(&s_server, 0, sizeof(s_server));
    opcua_statuscode_t sc = mu_register_structure_type(&s_server, &type_id, &def);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    const mu_structure_definition_t *registered_def = mu_find_structure_definition(&s_server, &type_id);
    TEST_ASSERT_NOT_NULL(registered_def);

    test_opt_t in_val;
    memset(&in_val, 0, sizeof(in_val));
    in_val.mask = (1u << 0) | (1u << 2);
    in_val.a = 100;
    in_val.b = 0; /* not set in mask */
    in_val.c = 300;

    memset(encode_buf, 0, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));
    sc = mu_binary_encode_struct(&writer, registered_def, &in_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);

    test_opt_t out_val;
    memset(&out_val, 0xFF, sizeof(out_val));
    out_val.mask = in_val.mask;
    sc = mu_binary_decode_struct(&reader, registered_def, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);

    TEST_ASSERT_EQUAL_INT32(100, out_val.a);
    TEST_ASSERT_EQUAL_INT32(300, out_val.c);
    /* field B was not set in mask — decoder zeros it. */
    TEST_ASSERT_EQUAL_INT32(0, out_val.b);
}

/* T025: Empty structures encode a zero-field body and decode successfully. */
void test_struct_roundtrip_empty(void) {
    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.field_count = 0;
    def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    memset(encode_buf, 0, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));
    opcua_statuscode_t sc = mu_binary_encode_struct(&writer, &def, NULL);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_UINT(0, writer.position);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);
    sc = mu_binary_decode_struct(&reader, &def, NULL);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_UINT(0, reader.position);
}

void test_struct_roundtrip_nested(void) {
    typedef struct {
        opcua_int32_t x;
        opcua_uint32_t y;
    } inner_t;
    typedef struct {
        inner_t inner;
        opcua_uint32_t tail;
    } outer_t;

    mu_structure_field_t inner_fields[2];
    inner_fields[0] = make_field("X", 6, -1, false);
    inner_fields[1] = make_field("Y", 7, -1, false);

    mu_structure_definition_t inner_def;
    memset(&inner_def, 0, sizeof(inner_def));
    inner_def.fields = inner_fields;
    inner_def.field_count = 2;
    inner_def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    mu_nodeid_t inner_type_id = make_numeric_nodeid(1, 5025);
    memset(&s_server, 0, sizeof(s_server));
    opcua_statuscode_t sc = mu_register_structure_type(&s_server, &inner_type_id, &inner_def);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    const mu_structure_definition_t *registered_inner_def = mu_find_structure_definition(&s_server, &inner_type_id);
    TEST_ASSERT_NOT_NULL(registered_inner_def);

    mu_structure_field_t outer_fields[2];
    outer_fields[0] = make_field("Inner", 5025, -1, false);
    outer_fields[0].data_type.namespace_index = 1;
    outer_fields[0].nested_structure = registered_inner_def;
    outer_fields[1] = make_field("Tail", 7, -1, false);

    mu_structure_definition_t outer_def;
    memset(&outer_def, 0, sizeof(outer_def));
    outer_def.fields = outer_fields;
    outer_def.field_count = 2;
    outer_def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    mu_nodeid_t outer_type_id = make_numeric_nodeid(1, 6025);
    sc = mu_register_structure_type(&s_server, &outer_type_id, &outer_def);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    const mu_structure_definition_t *registered_outer_def = mu_find_structure_definition(&s_server, &outer_type_id);
    TEST_ASSERT_NOT_NULL(registered_outer_def);

    outer_t in_val;
    memset(&in_val, 0, sizeof(in_val));
    in_val.inner.x = -12345;
    in_val.inner.y = 54321u;
    in_val.tail = 90000u;

    memset(encode_buf, 0, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));
    sc = mu_binary_encode_struct(&writer, registered_outer_def, &in_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);

    outer_t out_val;
    memset(&out_val, 0, sizeof(out_val));
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);
    sc = mu_binary_decode_struct(&reader, registered_outer_def, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_INT32(in_val.inner.x, out_val.inner.x);
    TEST_ASSERT_EQUAL_UINT32(in_val.inner.y, out_val.inner.y);
    TEST_ASSERT_EQUAL_UINT32(in_val.tail, out_val.tail);
}

void test_struct_roundtrip_array_fields(void) {
    typedef struct {
        opcua_int32_t value_count;
        opcua_int32_t values[3];
        opcua_uint32_t tail;
    } array_holder_t;

    mu_structure_field_t fields[2];
    fields[0] = make_field("Values", 6, 1, false);
    fields[1] = make_field("Tail", 7, -1, false);

    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = 2;
    def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    array_holder_t in_val;
    memset(&in_val, 0, sizeof(in_val));
    in_val.value_count = 3;
    in_val.values[0] = -1;
    in_val.values[1] = 0;
    in_val.values[2] = 42;
    in_val.tail = 77u;

    memset(encode_buf, 0, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));
    opcua_statuscode_t sc = mu_binary_encode_struct(&writer, &def, &in_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);

    array_holder_t out_val;
    memset(&out_val, 0, sizeof(out_val));
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);
    sc = mu_binary_decode_struct(&reader, &def, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_INT32(3, out_val.value_count);
    TEST_ASSERT_EQUAL_INT32(-1, out_val.values[0]);
    TEST_ASSERT_EQUAL_INT32(0, out_val.values[1]);
    TEST_ASSERT_EQUAL_INT32(42, out_val.values[2]);
    TEST_ASSERT_EQUAL_UINT32(77u, out_val.tail);
}

/* T026: Int32 array fields preserve OPC UA length prefix + elements (OPC-10000-6 §5.2.2.1). */
void test_struct_roundtrip_registered_int32_array_field(void) {
    typedef struct {
        opcua_int32_t value_count;
        opcua_int32_t values[3];
    } int32_array_holder_t;

    mu_structure_field_t fields[1];
    fields[0] = make_field("Values", 6, 1, false);

    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = 1;
    def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    mu_nodeid_t type_id = make_numeric_nodeid(1, 4026);
    memset(&s_server, 0, sizeof(s_server));
    opcua_statuscode_t sc = mu_register_structure_type(&s_server, &type_id, &def);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    const mu_structure_definition_t *registered_def = mu_find_structure_definition(&s_server, &type_id);
    TEST_ASSERT_NOT_NULL(registered_def);

    int32_array_holder_t in_val;
    memset(&in_val, 0, sizeof(in_val));
    in_val.value_count = 3;
    in_val.values[0] = 1;
    in_val.values[1] = 2;
    in_val.values[2] = 3;

    memset(encode_buf, 0, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));
    sc = mu_binary_encode_struct(&writer, registered_def, &in_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);

    int32_array_holder_t out_val;
    memset(&out_val, 0, sizeof(out_val));
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);
    sc = mu_binary_decode_struct(&reader, registered_def, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_INT32(3, out_val.value_count);
    TEST_ASSERT_EQUAL_INT32(1, out_val.values[0]);
    TEST_ASSERT_EQUAL_INT32(2, out_val.values[1]);
    TEST_ASSERT_EQUAL_INT32(3, out_val.values[2]);
}

/* T027: Registered empty structures encode a zero-field body and decode successfully (OPC-10000-6 §5.2.2.12). */
void test_struct_roundtrip_registered_empty(void) {
    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.field_count = 0;
    def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    mu_nodeid_t type_id = make_numeric_nodeid(1, 4027);
    memset(&s_server, 0, sizeof(s_server));
    opcua_statuscode_t sc = mu_register_structure_type(&s_server, &type_id, &def);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    const mu_structure_definition_t *registered_def = mu_find_structure_definition(&s_server, &type_id);
    TEST_ASSERT_NOT_NULL(registered_def);
    TEST_ASSERT_EQUAL_UINT16(0, registered_def->field_count);

    memset(encode_buf, 0xA5, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));
    sc = mu_binary_encode_struct(&writer, registered_def, NULL);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_UINT(0, writer.position);

    memset(decode_buf, 0x5A, sizeof(decode_buf));
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);
    sc = mu_binary_decode_struct(&reader, registered_def, decode_buf);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_UINT(0, reader.position);
    TEST_ASSERT_EQUAL_UINT8(0x5A, decode_buf[0]);
}

/* T028: Malformed input → Bad_DecodingError (OPC-10000-6 §5.4.1) */
void test_struct_decode_truncated(void) {
    mu_structure_field_t fields[1];
    fields[0] = make_field("X", 6, -1, false);

    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = 1;
    def.structure_type = MU_STRUCTURE_TYPE_STRUCTURE;

    opcua_int32_t out_val = 0;

    /* empty buffer = not enough bytes for Int32 */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, 2);
    opcua_statuscode_t sc = mu_binary_decode_struct(&reader, &def, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, sc);
}

void test_struct_decode_optional_mask_set_but_field_absent(void) {
    typedef struct {
        opcua_uint32_t mask;
        opcua_int32_t x;
    } test_opt_t;

    mu_structure_field_t fields[1];
    fields[0] = make_field("X", 6, -1, true);

    mu_structure_definition_t def;
    memset(&def, 0, sizeof(def));
    def.fields = fields;
    def.field_count = 1;
    def.structure_type = MU_STRUCTURE_TYPE_OPTIONAL;

    test_opt_t out_val;
    memset(&out_val, 0, sizeof(out_val));
    out_val.mask = 1u;

    memset(encode_buf, 0, sizeof(encode_buf));
    encode_buf[0] = 1u; /* EncodingMask bit 0 set, but no Int32 field bytes follow. */
    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, 4);
    opcua_statuscode_t sc = mu_binary_decode_struct(&reader, &def, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_DECODINGERROR, sc);
}

/* Enum round-trip (FR-008 / T038) */
void test_enum_roundtrip(void) {
    memset(encode_buf, 0, sizeof(encode_buf));
    mu_binary_writer_t writer;
    mu_binary_writer_init(&writer, encode_buf, sizeof(encode_buf));

    opcua_statuscode_t sc = mu_binary_encode_enum(&writer, 42);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);

    mu_binary_reader_t reader;
    mu_binary_reader_init(&reader, encode_buf, writer.position);
    opcua_int32_t out_val = 0;
    sc = mu_binary_decode_enum(&reader, &out_val);
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_GOOD, sc);
    TEST_ASSERT_EQUAL_INT32(42, out_val);
}

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
    RUN_TEST(test_struct_roundtrip_scalars);
    RUN_TEST(test_struct_roundtrip_optional_fields);
    RUN_TEST(test_struct_roundtrip_empty);
    RUN_TEST(test_struct_roundtrip_nested);
    RUN_TEST(test_struct_roundtrip_array_fields);
    RUN_TEST(test_struct_roundtrip_registered_int32_array_field);
    RUN_TEST(test_struct_roundtrip_registered_empty);
    RUN_TEST(test_struct_decode_truncated);
    RUN_TEST(test_struct_decode_optional_mask_set_but_field_absent);
    RUN_TEST(test_enum_roundtrip);
#else
    RUN_TEST(test_complex_types_require_build_flag);
#endif
    return UNITY_END();
}
