/*
 * test_capacity_propagation.c — Spec 056 regression guard.
 *
 * Proves that per-profile capacity macros actually reach the compiler, using
 * the profile marker macros (MUC_OPCUA_STANDARD_PROFILE / _EMBEDDED_PROFILE)
 * that are emitted as PUBLIC compile definitions on the muc_opcua target.
 *
 * The checks are compile-time _Static_assert: if a capacity is silently pinned
 * to its config.h default because it was never propagated (the spec-056 bug),
 * this translation unit fails to compile for the affected profile — breaking
 * that profile's slot in the per-profile ctest matrix.
 */

#include "muc_opcua/config.h"
#include "unity.h"

/*
 * Invariant (config.h): secure channels always track connections 1:1. Holds for
 * every profile; if MU_MAX_SECURE_CHANNELS is ever propagated independently and
 * drifts from MU_MAX_CONNECTIONS, this fires.
 */
_Static_assert(MU_MAX_SECURE_CHANNELS == MU_MAX_CONNECTIONS,
               "MU_MAX_SECURE_CHANNELS must equal MU_MAX_CONNECTIONS (1:1 invariant)");

#if defined(MUC_OPCUA_STANDARD_PROFILE)
/*
 * standard AND full both define MUC_OPCUA_STANDARD_PROFILE. Both advertise many
 * sessions (50 / 100) and MUST scale their transport/secure-channel pool past
 * the 4-connection multi-connect default. This is the assertion that FAILS
 * today (MU_MAX_CONNECTIONS == 4), proving defect 1.
 */
_Static_assert(MU_MAX_CONNECTIONS >= 8,
               "standard/full must scale MU_MAX_CONNECTIONS with session count "
               "(spec 056: capacity flag was not propagated)");
_Static_assert(MU_MAX_SESSIONS >= 50,
               "standard/full session capacity must be propagated");
#elif defined(MUC_OPCUA_EMBEDDED_PROFILE)
/*
 * embedded: multi-connect default of 4 connections, and MU_MAX_SESSIONS must be
 * set explicitly by the profile (not left to a coincidental config.h default).
 */
_Static_assert(MU_MAX_CONNECTIONS >= 4,
               "embedded must propagate its connection capacity");
_Static_assert(MU_MAX_SESSIONS == 2,
               "embedded session capacity must be explicitly propagated (spec 056 defect 3)");
#endif

/* A runtime assertion so Unity has an executable body; the real guards are the
 * _Static_asserts above, which run at compile time for every profile. */
static void test_capacity_invariant_holds_at_runtime(void)
{
    TEST_ASSERT_EQUAL_INT(MU_MAX_CONNECTIONS, MU_MAX_SECURE_CHANNELS);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, MU_MAX_CONNECTIONS);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_capacity_invariant_holds_at_runtime);
    return UNITY_END();
}
