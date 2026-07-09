/* tests/unit/test_address_space_values.c */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define INDEX_LOOKUP_NUMERIC_NODE(ns, id)                                                                              \
    { {(ns), MU_NODEID_NUMERIC, {(id)}}, MU_NODECLASS_VARIABLE, {0, NULL}, {0, NULL}, NULL, 0, NULL }

static const opcua_byte_t s_index_lookup_string_id[] = "indexed-string-node";
static const opcua_byte_t s_missing_index_lookup_string_id[] = "missing-indexed-node";

static const mu_node_t s_index_lookup_nodes[] = {
    INDEX_LOOKUP_NUMERIC_NODE(1, 1403),
    INDEX_LOOKUP_NUMERIC_NODE(0, 85),
    INDEX_LOOKUP_NUMERIC_NODE(2, 60010),
    INDEX_LOOKUP_NUMERIC_NODE(1, 1000),
    INDEX_LOOKUP_NUMERIC_NODE(0, 2258),
    INDEX_LOOKUP_NUMERIC_NODE(3, 42),
    INDEX_LOOKUP_NUMERIC_NODE(2, 17),
    INDEX_LOOKUP_NUMERIC_NODE(1, 98765),
    INDEX_LOOKUP_NUMERIC_NODE(0, 35),
    INDEX_LOOKUP_NUMERIC_NODE(3, 900),
    INDEX_LOOKUP_NUMERIC_NODE(2, 4096),
    INDEX_LOOKUP_NUMERIC_NODE(1, 7),
    INDEX_LOOKUP_NUMERIC_NODE(0, 50000),
    INDEX_LOOKUP_NUMERIC_NODE(2, 123),
    INDEX_LOOKUP_NUMERIC_NODE(1, 2048),
    INDEX_LOOKUP_NUMERIC_NODE(3, 65535),
    INDEX_LOOKUP_NUMERIC_NODE(0, 100),
    INDEX_LOOKUP_NUMERIC_NODE(2, 999),
    INDEX_LOOKUP_NUMERIC_NODE(1, 3333),
    INDEX_LOOKUP_NUMERIC_NODE(0, 2),
    INDEX_LOOKUP_NUMERIC_NODE(3, 31415),
    INDEX_LOOKUP_NUMERIC_NODE(2, 27182),
    INDEX_LOOKUP_NUMERIC_NODE(1, 8080),
    INDEX_LOOKUP_NUMERIC_NODE(0, 12345),
    INDEX_LOOKUP_NUMERIC_NODE(2, 1),
    INDEX_LOOKUP_NUMERIC_NODE(3, 2049),
    INDEX_LOOKUP_NUMERIC_NODE(1, 65534),
    INDEX_LOOKUP_NUMERIC_NODE(0, 99999),
    INDEX_LOOKUP_NUMERIC_NODE(2, 54321),
    INDEX_LOOKUP_NUMERIC_NODE(3, 11),
    INDEX_LOOKUP_NUMERIC_NODE(1, 2468),
    INDEX_LOOKUP_NUMERIC_NODE(0, 2257),
    INDEX_LOOKUP_NUMERIC_NODE(2, 70000),
    INDEX_LOOKUP_NUMERIC_NODE(3, 123456),
    INDEX_LOOKUP_NUMERIC_NODE(1, 4321),
    INDEX_LOOKUP_NUMERIC_NODE(0, 777),
    INDEX_LOOKUP_NUMERIC_NODE(2, 2222),
    INDEX_LOOKUP_NUMERIC_NODE(3, 333),
    INDEX_LOOKUP_NUMERIC_NODE(1, 999),
    INDEX_LOOKUP_NUMERIC_NODE(0, 65536),
    {{1,
      MU_NODEID_STRING,
      {.string = {(opcua_int32_t)(sizeof(s_index_lookup_string_id) - 1u), s_index_lookup_string_id}}},
     MU_NODECLASS_VARIABLE,
     {0, NULL},
     {0, NULL},
     NULL,
     0,
     NULL}};

static const mu_address_space_t s_index_lookup_space = {s_index_lookup_nodes,
                                                        sizeof(s_index_lookup_nodes) / sizeof(s_index_lookup_nodes[0])};

#undef INDEX_LOOKUP_NUMERIC_NODE

static const mu_node_t *find_node_linear_for_test(const mu_address_space_t *space, const mu_nodeid_t *node_id) {
    size_t i;

    for (i = 0; i < space->node_count; i++) {
        if (mu_nodeid_equal(&space->nodes[i].node_id, node_id)) {
            return &space->nodes[i];
        }
    }

    return NULL;
}

void test_static_value_source_boolean(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};

    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_BOOLEAN;
    source.data.static_value.value.b = true;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_BOOLEAN, value.type);
    TEST_ASSERT_TRUE(value.value.b);
}

void test_static_value_source_int32(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};

    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_INT32;
    source.data.static_value.value.i32 = -42;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_INT32, value.type);
    TEST_ASSERT_EQUAL_INT32(-42, value.value.i32);
}

void test_static_value_source_uint32(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};

    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_UINT32;
    source.data.static_value.value.ui32 = 42;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_UINT32, value.type);
    TEST_ASSERT_EQUAL_UINT32(42, value.value.ui32);
}

void test_static_value_source_float(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};

    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_FLOAT;
    source.data.static_value.value.f = 3.14f;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_FLOAT, value.type);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, value.value.f);
}

void test_static_value_source_string_empty(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};

    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_STRING;
    source.data.static_value.value.str.length = 0;
    source.data.static_value.value.str.data = NULL;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, value.type);
    TEST_ASSERT_EQUAL_INT32(0, value.value.str.length);
}

void test_static_value_source_string_64_bytes(void) {
    mu_value_source_t source;
    mu_variant_t value;
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {1000}};
    opcua_byte_t str_data[64] = {0};

    source.type = MU_VALUESOURCE_STATIC;
    source.data.static_value.type = MU_TYPE_STRING;
    source.data.static_value.value.str.length = 64;
    source.data.static_value.value.str.data = str_data;

    TEST_ASSERT_EQUAL(MU_STATUS_GOOD, mu_value_source_read(&source, &id, &value));
    TEST_ASSERT_EQUAL(MU_TYPE_STRING, value.type);
    TEST_ASSERT_EQUAL_INT32(64, value.value.str.length);
}

void test_address_space_find_node_index_matches_linear_scan(void) {
    static const mu_nodeid_t missing_node_ids[] = {
        {1, MU_NODEID_NUMERIC, {1404}},
        {0, MU_NODEID_NUMERIC, {2259}},
        {2, MU_NODEID_NUMERIC, {60011}},
        {9, MU_NODEID_NUMERIC, {1403}},
        {1, MU_NODEID_NUMERIC, {85}},
        {1,
         MU_NODEID_STRING,
         {.string = {(opcua_int32_t)(sizeof(s_missing_index_lookup_string_id) - 1u),
                     s_missing_index_lookup_string_id}}}};
    size_t i;

    TEST_ASSERT_TRUE(s_index_lookup_space.node_count > 40u);
    TEST_ASSERT_TRUE(s_index_lookup_space.node_count <= MU_INTERN_MAX_ADDRESS_SPACE_NODES);

    for (i = 0; i < s_index_lookup_space.node_count; i++) {
        const mu_nodeid_t *node_id = &s_index_lookup_nodes[i].node_id;
        const mu_node_t *expected = find_node_linear_for_test(&s_index_lookup_space, node_id);
        const mu_node_t *actual = mu_address_space_find_node(&s_index_lookup_space, node_id);

        TEST_ASSERT_EQUAL_PTR(&s_index_lookup_nodes[i], expected);
        TEST_ASSERT_EQUAL_PTR(expected, actual);
    }

    for (i = 0; i < sizeof(missing_node_ids) / sizeof(missing_node_ids[0]); i++) {
        const mu_nodeid_t *node_id = &missing_node_ids[i];

        TEST_ASSERT_NULL(find_node_linear_for_test(&s_index_lookup_space, node_id));
        TEST_ASSERT_NULL(mu_address_space_find_node(&s_index_lookup_space, node_id));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_static_value_source_boolean);
    RUN_TEST(test_static_value_source_int32);
    RUN_TEST(test_static_value_source_uint32);
    RUN_TEST(test_static_value_source_float);
    RUN_TEST(test_static_value_source_string_empty);
    RUN_TEST(test_static_value_source_string_64_bytes);
    RUN_TEST(test_address_space_find_node_index_matches_linear_scan);
    return UNITY_END();
}
