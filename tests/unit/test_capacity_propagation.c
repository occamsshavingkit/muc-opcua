/*
 * test_capacity_propagation.c — Spec 056 regression guard.
 *
 * Proves the per-profile capacity cascade (include/muc_opcua/capacities.h)
 * actually reaches the compiler. All assertions are on the MU_INTERN_* macros
 * that code compiles off, keyed on the MUC_OPCUA_PROFILE_<NAME> selector CMake
 * emits. A propagation regression breaks this translation unit's compile for the
 * affected profile in the per-profile ctest matrix.
 */

#include "muc_opcua/config.h"
#include "unity.h"

/* Invariant, every profile: secure channels track connections 1:1. */
_Static_assert(MU_INTERN_MAX_SECURE_CHANNELS == MU_INTERN_MAX_CONNECTIONS,
               "MU_INTERN_MAX_SECURE_CHANNELS must equal MU_INTERN_MAX_CONNECTIONS (1:1 invariant)");

/* Core spec-056 rule: any profile that accepts multiple transport connections
 * must be able to give each concurrent session its own SecureChannel. (nano is
 * single-connection and multiplexes its sessions, so it is exempt — guarded by
 * the flag rather than the profile name.) */
#ifdef MUC_OPCUA_MULTIPLE_CONNECTIONS
_Static_assert(MU_INTERN_MAX_CONNECTIONS >= MU_INTERN_MAX_SESSIONS,
               "a multi-connection profile must accept one SecureChannel per concurrent session "
               "(spec 056: capacity must scale with sessions)");
#endif

/* Exact per-profile values, proving the profile selector reaches capacities.h
 * and each column resolves as intended. */
#if defined(MUC_OPCUA_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
_Static_assert(MU_INTERN_MAX_SESSIONS == 100 && MU_INTERN_MAX_CONNECTIONS == 100, "full: sessions=connections=100");
#elif defined(MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER)
_Static_assert(MU_INTERN_MAX_SESSIONS == 50 && MU_INTERN_MAX_CONNECTIONS == 50, "standard: sessions=connections=50");
#elif defined(MUC_OPCUA_PROFILE_EMBEDDED_2025_UA_SERVER)
_Static_assert(MU_INTERN_MAX_SESSIONS == 2 && MU_INTERN_MAX_CONNECTIONS == 4, "embedded: sessions=2, connections=4");
#elif defined(MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER)
_Static_assert(MU_INTERN_MAX_SESSIONS == 2 && MU_INTERN_MAX_CONNECTIONS == 2,
               "micro: sessions=2, connections=2 (>=2 clients each with own channel)");
#elif defined(MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER)
_Static_assert(MU_INTERN_MAX_SESSIONS == 2 && MU_INTERN_MAX_CONNECTIONS == 1,
               "nano: single connection, sessions multiplexed");
#endif

/* Cascade stage 3: whenever the integrator defines the public knob, the internal
 * value must equal it. Inert in the plain ctest matrix (no -D); documents and
 * guards the override wiring for override builds. */
#ifdef MU_MAX_SESSIONS
_Static_assert(MU_INTERN_MAX_SESSIONS == MU_MAX_SESSIONS, "stage 3: public -DMU_MAX_SESSIONS override must win");
#endif
#ifdef MU_MAX_CONNECTIONS
_Static_assert(MU_INTERN_MAX_CONNECTIONS == MU_MAX_CONNECTIONS,
               "stage 3: public -DMU_MAX_CONNECTIONS override must win");
#endif

void setUp(void) {}
void tearDown(void) {}

/* Runtime body so Unity has something to execute; the real guards are the
 * compile-time _Static_asserts above (evaluated for every profile). */
static void test_capacity_invariant_holds_at_runtime(void) {
    TEST_ASSERT_EQUAL_INT(MU_INTERN_MAX_CONNECTIONS, MU_INTERN_MAX_SECURE_CHANNELS);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, MU_INTERN_MAX_CONNECTIONS);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, MU_INTERN_MAX_SESSIONS);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_capacity_invariant_holds_at_runtime);
    return UNITY_END();
}
