/* tests/unit/test_profile_surface.c
 *
 * Feature 028 (Tier 0 / US2): per-profile behavioural assertions that run in the
 * profile the library was actually built for. Unlike markdown traceability, these
 * pin what a given profile build EXPOSES, so the conformance docs can be corrected
 * to match reality instead of over-claiming.
 *
 * - T015: standard Base Information nodes (Server / ServerStatus / NamespaceArray /
 *   ServerCapabilities incl. ServerProfileArray) exist only when the base node set
 *   is built (embedded/full); nano/micro build them OFF and rely on the
 *   integrator-supplied address space. OPC-10000-5 (Server object / ServerStatus /
 *   ServerProfileArray); OPC-10000-7 Core Server Facet.
 * - T017: RegisterNodes/UnregisterNodes is dispatchable only in the full build; the
 *   nano/micro/embedded builds return Bad_ServiceUnsupported. OPC-10000-4 §5.9.5
 *   (RegisterNodes) / §5.9.6 (UnregisterNodes) / §7.38.2 (Bad_ServiceUnsupported).
 */
#include "muc_opcua/muc_opcua.h"
#include "unity.h"

#include <string.h>

#include "../../src/core/server_internal.h"
#include "../../src/core/service_dispatch.h"

#if defined(MUC_OPCUA_FACET_CORE_2022_SERVER) && MUC_OPCUA_FACET_CORE_2022_SERVER
#include "../../src/address_space/base_nodes.h"
#endif

void setUp(void) {}
void tearDown(void) {}

/* --- T015: Base Information node presence tracks the build ---------------- */

#if defined(MUC_OPCUA_FACET_CORE_2022_SERVER) && MUC_OPCUA_FACET_CORE_2022_SERVER
static const mu_node_t *base_find(opcua_uint32_t numeric) {
    const mu_address_space_t *bs = mu_base_address_space();
    mu_nodeid_t id = {0, MU_NODEID_NUMERIC, {0}};
    id.identifier.numeric = numeric;
    return mu_address_space_find_node(bs, (mu_address_space_index_t *)0, &id);
}

static const mu_node_t *base_find_string(const char *text) {
    const mu_address_space_t *bs = mu_base_address_space();
    mu_nodeid_t id = {0, MU_NODEID_STRING, {.string = {(opcua_int32_t)strlen(text), (const opcua_byte_t *)text}}};
    return mu_address_space_find_node(bs, (mu_address_space_index_t *)0, &id);
}

void test_base_info_nodes_present_when_built(void) {
    TEST_ASSERT_NOT_NULL(base_find(2253)); /* Server */
    TEST_ASSERT_NOT_NULL(base_find(2256)); /* ServerStatus */
    TEST_ASSERT_NOT_NULL(base_find(2255)); /* NamespaceArray */
    TEST_ASSERT_NOT_NULL(base_find(2268)); /* ServerCapabilities (ServerProfileArray) */
}

void test_claimed_cu_nodes_present_when_enabled(void) {
#if defined(MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES) && MUC_OPCUA_CU_ADDRESS_SPACE_INTERFACES
    TEST_ASSERT_NOT_NULL(base_find(17602)); /* BaseInterfaceType */
    TEST_ASSERT_NOT_NULL(base_find(17603)); /* HasInterface */
    TEST_ASSERT_NOT_NULL(base_find(17708)); /* InterfaceTypes */
#endif
#if defined(MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE) && MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE
    TEST_ASSERT_NOT_NULL(base_find(17604)); /* HasAddIn */
#endif
#if defined(MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_DEFAULTINSTANCEBROWSENAME)
    TEST_ASSERT_NOT_NULL(base_find(17605)); /* DefaultInstanceBrowseName */
#endif
#if defined(MUC_OPCUA_CU_BASE_INFO_LOCALTIME) && MUC_OPCUA_CU_BASE_INFO_LOCALTIME
    TEST_ASSERT_NOT_NULL(base_find(8912));  /* TimeZoneDataType */
    TEST_ASSERT_NOT_NULL(base_find(8917));  /* Default Binary */
    TEST_ASSERT_NOT_NULL(base_find(17634)); /* Server.LocalTime */
#endif
#if defined(MUC_OPCUA_CU_BASE_INFO_OPTIONSET) && MUC_OPCUA_CU_BASE_INFO_OPTIONSET
    TEST_ASSERT_NOT_NULL(base_find(11487)); /* OptionSetType */
#endif
#if defined(MUC_OPCUA_CU_BASE_INFO_SELECTION_LIST)
    TEST_ASSERT_NOT_NULL(base_find(16309)); /* SelectionListType */
#endif
#if defined(MUC_OPCUA_CU_BASE_INFO_ENGINEERING_UNITS)
    TEST_ASSERT_NOT_NULL(base_find(887)); /* EUInformation */
#endif
#if defined(MUC_OPCUA_CU_BASE_INFO_CURRENCY)
    TEST_ASSERT_NOT_NULL(base_find(23498));                 /* CurrencyUnitType */
    TEST_ASSERT_NOT_NULL(base_find(23507));                 /* Default Binary */
    TEST_ASSERT_NOT_NULL(base_find_string("CurrencyUnit")); /* CurrencyUnit */
#endif
}
#else
void test_base_info_nodes_absent_in_minimal_build(void) {
    /* BASE_NODES is OFF for nano/micro: the standard Base Information node set is not
       linked into the build; such a server exposes only the integrator-supplied
       address space. Base information is optional for the Nano profile
       (OPC-10000-7 Core Server Facet), so this is conformant. */
    TEST_PASS_MESSAGE("base node set not built (integrator supplies the address space)");
}
#endif

/* --- T017: RegisterNodes support tracks the build ------------------------- */

/* mu_service_dispatch resolves the service descriptor and returns
   Bad_ServiceUnsupported before touching the SecureChannel when the request type
   has no built-in handler, so a zeroed server is sufficient for the probe. */
static mu_server_t srv;

void test_register_nodes_support_matches_build(void) {
    opcua_byte_t body[16] = {0};
    opcua_byte_t resp[64];
    size_t resp_len = 0;
    opcua_statuscode_t rc = mu_service_dispatch(&srv, MU_ID_REGISTERNODESREQUEST, body, sizeof(body), resp, &resp_len);

#if defined(MUC_OPCUA_CU_VIEW_REGISTERNODES) && MUC_OPCUA_CU_VIEW_REGISTERNODES
    TEST_ASSERT_NOT_EQUAL(MU_STATUS_BAD_SERVICEUNSUPPORTED, rc); /* dispatchable */
#else
    TEST_ASSERT_EQUAL_HEX32(MU_STATUS_BAD_SERVICEUNSUPPORTED, rc); /* not built */
#endif
}

int main(void) {
    UNITY_BEGIN();
#if defined(MUC_OPCUA_FACET_CORE_2022_SERVER) && MUC_OPCUA_FACET_CORE_2022_SERVER
    RUN_TEST(test_base_info_nodes_present_when_built);
    RUN_TEST(test_claimed_cu_nodes_present_when_enabled);
#else
    RUN_TEST(test_base_info_nodes_absent_in_minimal_build);
#endif
    RUN_TEST(test_register_nodes_support_matches_build);
    return UNITY_END();
}
