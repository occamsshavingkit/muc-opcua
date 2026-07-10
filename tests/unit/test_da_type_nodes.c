/* tests/unit/test_da_type_nodes.c
 *
 * Spec 060 (Data Access Server Facet) FR-2: the Data Access VariableType nodes
 * and their property instance-declarations are served in the base address space
 * with the grounded HasSubtype hierarchy, HasProperty children, HasModellingRule
 * (Mandatory=78 / Optional=80) and HasTypeDefinition->PropertyType(68).
 *
 * Non-circular: every expected NodeId / rule below is a grounded constant from
 * OPC-10000-8 §5.3 (hard-coded here), NOT read back from the table under test.
 */
#include "muc_opcua/config.h"
#include "unity.h"
#include <stdbool.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

#if MUC_OPCUA_DATA_ACCESS
#include "address_space/base_nodes.h"
#include "muc_opcua/address_space.h"

/* Grounded well-known NodeIds (OPC-10000-6 NodeIds.csv). */
#define ND_HASMODELLINGRULE 37u
#define ND_HASTYPEDEF 40u
#define ND_HASSUBTYPE 45u
#define ND_HASPROPERTY 46u
#define ND_PROPERTYTYPE 68u
#define ND_MANDATORY 78u
#define ND_OPTIONAL 80u

static const mu_node_t *find(opcua_uint32_t id) {
    mu_nodeid_t nid = {0, MU_NODEID_NUMERIC, {.numeric = id}};
    return mu_resolve_node(NULL, NULL, NULL, &nid);
}

/* True iff `node` has a forward reference of `ref_type` to numeric `target`. */
static bool has_ref(const mu_node_t *node, opcua_uint32_t ref_type, opcua_uint32_t target) {
    if (node == NULL) {
        return false;
    }
    for (size_t i = 0; i < node->reference_count; ++i) {
        const mu_reference_t *r = &node->references[i];
        if (r->is_forward && r->reference_type_id.identifier_type == MU_NODEID_NUMERIC &&
            r->reference_type_id.identifier.numeric == ref_type && r->target_id.identifier_type == MU_NODEID_NUMERIC &&
            r->target_id.identifier.numeric == target) {
            return true;
        }
    }
    return false;
}

/* A property instance-declaration must carry HasTypeDefinition->PropertyType(68)
 * and exactly the expected HasModellingRule target. */
static void assert_property(opcua_uint32_t inst_id, opcua_uint32_t expected_rule) {
    const mu_node_t *n = find(inst_id);
    char msg[64];
    (void)snprintf(msg, sizeof(msg), "DA property instance %u must resolve", inst_id);
    TEST_ASSERT_NOT_NULL_MESSAGE(n, msg);
    TEST_ASSERT_EQUAL(MU_NODECLASS_VARIABLE, n->node_class);
    TEST_ASSERT_TRUE_MESSAGE(has_ref(n, ND_HASTYPEDEF, ND_PROPERTYTYPE),
                             "property -> HasTypeDefinition PropertyType(68)");
    TEST_ASSERT_TRUE_MESSAGE(has_ref(n, ND_HASMODELLINGRULE, expected_rule), "property -> expected HasModellingRule");
    /* And NOT the other rule. */
    opcua_uint32_t other = expected_rule == ND_MANDATORY ? ND_OPTIONAL : ND_MANDATORY;
    TEST_ASSERT_FALSE(has_ref(n, ND_HASMODELLINGRULE, other));
}

/* DataItemType (2365) is HasSubtype under BaseDataVariableType (63). */
static void test_dataitemtype_subtype_of_basedatavariabletype(void) {
    const mu_node_t *parent = find(63);
    TEST_ASSERT_NOT_NULL(parent);
    TEST_ASSERT_TRUE(has_ref(parent, ND_HASSUBTYPE, 2365));
    const mu_node_t *dit = find(2365);
    TEST_ASSERT_NOT_NULL(dit);
    TEST_ASSERT_EQUAL(MU_NODECLASS_VARIABLETYPE, dit->node_class);
}

/* AnalogItemType (2368): VariableType, HasSubtype-linked under BaseAnalogType
 * (15318), HasProperty EURange (2369) which is Mandatory (78); EngineeringUnits
 * (2371) is Optional (80). */
static void test_analogitemtype(void) {
    const mu_node_t *ait = find(2368);
    TEST_ASSERT_NOT_NULL(ait);
    TEST_ASSERT_EQUAL(MU_NODECLASS_VARIABLETYPE, ait->node_class);

    const mu_node_t *base_analog = find(15318);
    TEST_ASSERT_NOT_NULL(base_analog);
    TEST_ASSERT_TRUE_MESSAGE(has_ref(base_analog, ND_HASSUBTYPE, 2368), "BaseAnalogType HasSubtype-> AnalogItemType");

    TEST_ASSERT_TRUE_MESSAGE(has_ref(ait, ND_HASPROPERTY, 2369), "AnalogItemType HasProperty EURange(2369)");
    assert_property(2369, ND_MANDATORY);
    assert_property(2371, ND_OPTIONAL);
    assert_property(2370, ND_OPTIONAL);
}

/* TwoStateDiscreteType (2373): TrueState(2375) + FalseState(2374) both Mandatory. */
static void test_twostatediscrete(void) {
    const mu_node_t *t = find(2373);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(MU_NODECLASS_VARIABLETYPE, t->node_class);
    TEST_ASSERT_TRUE(has_ref(t, ND_HASPROPERTY, 2375));
    TEST_ASSERT_TRUE(has_ref(t, ND_HASPROPERTY, 2374));
    assert_property(2375, ND_MANDATORY);
    assert_property(2374, ND_MANDATORY);
    /* Subtype of DiscreteItemType (2372). */
    TEST_ASSERT_TRUE(has_ref(find(2372), ND_HASSUBTYPE, 2373));
}

/* MultiStateValueDiscreteType (11238): EnumValues(11241) + ValueAsText(11461)
 * both Mandatory. */
static void test_multistatevaluediscrete(void) {
    const mu_node_t *t = find(11238);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(MU_NODECLASS_VARIABLETYPE, t->node_class);
    TEST_ASSERT_TRUE(has_ref(t, ND_HASPROPERTY, 11241));
    TEST_ASSERT_TRUE(has_ref(t, ND_HASPROPERTY, 11461));
    assert_property(11241, ND_MANDATORY);
    assert_property(11461, ND_MANDATORY);
    TEST_ASSERT_TRUE(has_ref(find(2372), ND_HASSUBTYPE, 11238));
}

/* AnalogUnitType (17497): EngineeringUnits(17502) is Mandatory (contrast with
 * AnalogItemType where it is Optional). */
static void test_analogunittype(void) {
    const mu_node_t *t = find(17497);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(MU_NODECLASS_VARIABLETYPE, t->node_class);
    TEST_ASSERT_TRUE(has_ref(t, ND_HASPROPERTY, 17502));
    assert_property(17502, ND_MANDATORY);
    TEST_ASSERT_TRUE(has_ref(find(15318), ND_HASSUBTYPE, 17497));
}

/* The ModellingRule Object targets must themselves resolve so browsing a
 * property's HasModellingRule reaches a real node. */
static void test_modellingrule_objects_present(void) {
    const mu_node_t *m = find(78);
    const mu_node_t *o = find(80);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL(MU_NODECLASS_OBJECT, m->node_class);
    TEST_ASSERT_EQUAL(MU_NODECLASS_OBJECT, o->node_class);
}
#endif /* MUC_OPCUA_DATA_ACCESS */

int main(void) {
    UNITY_BEGIN();
#if MUC_OPCUA_DATA_ACCESS
    RUN_TEST(test_dataitemtype_subtype_of_basedatavariabletype);
    RUN_TEST(test_analogitemtype);
    RUN_TEST(test_twostatediscrete);
    RUN_TEST(test_multistatevaluediscrete);
    RUN_TEST(test_analogunittype);
    RUN_TEST(test_modellingrule_objects_present);
#endif
    return UNITY_END();
}
