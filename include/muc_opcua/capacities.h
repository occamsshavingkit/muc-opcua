/* include/muc_opcua/capacities.h
 *
 * SINGLE SOURCE OF TRUTH for every tunable capacity dimension.
 *
 * Each capacity is resolved by a three-stage cascade, in order:
 *
 *   1. DEFAULT  -- an unconditional baseline (the `standard` profile values).
 *                  Because this stage always runs, the internal macro is
 *                  ALWAYS defined: a build with no profile and no overrides is
 *                  fully specified, and silent fallthrough is impossible.
 *   2. PROFILE  -- if a profile is declared (CMake emits exactly one
 *                  MUC_OPCUA_PROFILE_<NAME>), redefine to that profile's value.
 *   3. USER     -- if the integrator defined the PUBLIC MU_MAX_* knob (e.g.
 *                  -DMU_MAX_SESSIONS=32), redefine to it. The public knob wins
 *                  over any profile, with or without a profile selected.
 *
 * Precedence: default < profile < user.
 *
 * ALL library/application/test code compiles off the MU_INTERN_* macros below.
 * The public MU_MAX_* macros are pure INPUT: they are read only in stage 3 here
 * and are otherwise never referenced by code. They remain the documented -D
 * override knob (docs/api-reference.md). Adding or retuning a capacity is a
 * localized edit in this one file.
 *
 * The profile selector MUC_OPCUA_PROFILE_<NAME> is emitted by src/CMakeLists.txt
 * for nano/micro/embedded/standard/full. For MUC_OPCUA_PROFILE=custom (or a
 * build that never ran the project's CMake), no selector is defined and stage 1
 * applies unless the integrator -D's the public knob.
 */
#ifndef MUC_OPCUA_CAPACITIES_H
#define MUC_OPCUA_CAPACITIES_H

/* =====================================================================
 * Profile-varying capacities (all three stages).
 * Baseline == the MINIMAL (nano) values: a profile-less / custom build gets the
 * smallest, storage-consistent footprint and scales UP via a profile or a public
 * -D. (A standard-sized baseline would give a featureless no-profile build a
 * 50-session in-struct array with no matching MU_SERVER_STORAGE_BYTES, tripping
 * the storage-coverage assert in server_internal.h.) Larger profiles therefore
 * each carry an explicit stage-2 block.
 * ===================================================================== */

/* ---- Max concurrent Sessions ---------------------------------------- */
#define MU_INTERN_MAX_SESSIONS 2 /* stage 1: minimal baseline */
#if defined(MUC_OPCUA_PROFILE_STANDARD)
#undef MU_INTERN_MAX_SESSIONS
#define MU_INTERN_MAX_SESSIONS 50 /* stage 2: profile */
#elif defined(MUC_OPCUA_PROFILE_FULL)
#undef MU_INTERN_MAX_SESSIONS
#define MU_INTERN_MAX_SESSIONS 100
#endif
#ifdef MU_MAX_SESSIONS
#undef MU_INTERN_MAX_SESSIONS
#define MU_INTERN_MAX_SESSIONS MU_MAX_SESSIONS /* stage 3: integrator override */
#endif

/* ---- Max concurrent TCP connections --------------------------------- */
/* connections >= sessions: each concurrent session may be an independent
 * client holding its own SecureChannel (docs/conformance/profile-micro.md).
 * NOTE: the connection pool array (mu_connection_t conns[]) and the multi-
 * connection accept path are gated on MUC_OPCUA_MULTIPLE_CONNECTIONS; when
 * that flag is off the single-connection path is used regardless of this
 * value (and MU_MULTIPLE_CONNECTIONS_STORAGE_BYTES contributes 0). */
#define MU_INTERN_MAX_CONNECTIONS 1 /* stage 1: minimal baseline */
#if defined(MUC_OPCUA_PROFILE_MICRO)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 2
#elif defined(MUC_OPCUA_PROFILE_EMBEDDED)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 4
#elif defined(MUC_OPCUA_PROFILE_STANDARD)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 50
#elif defined(MUC_OPCUA_PROFILE_FULL)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 100
#endif
#ifdef MU_MAX_CONNECTIONS
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS MU_MAX_CONNECTIONS
#endif

/* ---- Max concurrent Subscriptions ----------------------------------- */
#define MU_INTERN_MAX_SUBSCRIPTIONS 2 /* stage 1: minimal baseline */
#if defined(MUC_OPCUA_PROFILE_STANDARD)
#undef MU_INTERN_MAX_SUBSCRIPTIONS
#define MU_INTERN_MAX_SUBSCRIPTIONS 50
#elif defined(MUC_OPCUA_PROFILE_FULL)
#undef MU_INTERN_MAX_SUBSCRIPTIONS
#define MU_INTERN_MAX_SUBSCRIPTIONS 100
#endif
#ifdef MU_MAX_SUBSCRIPTIONS
#undef MU_INTERN_MAX_SUBSCRIPTIONS
#define MU_INTERN_MAX_SUBSCRIPTIONS MU_MAX_SUBSCRIPTIONS
#endif

/* ---- Max MonitoredItems --------------------------------------------- */
#define MU_INTERN_MAX_MONITORED_ITEMS 8 /* stage 1: minimal baseline */
#if defined(MUC_OPCUA_PROFILE_EMBEDDED)
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS 100
#elif defined(MUC_OPCUA_PROFILE_STANDARD)
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS 1000
#elif defined(MUC_OPCUA_PROFILE_FULL)
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS 2000
#endif
#ifdef MU_MAX_MONITORED_ITEMS
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS MU_MAX_MONITORED_ITEMS
#endif

/* ---- Max parallel Publish requests ---------------------------------- */
#define MU_INTERN_MAX_PUBLISH_REQUESTS 4 /* stage 1: minimal baseline */
#if defined(MUC_OPCUA_PROFILE_EMBEDDED)
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS 5
#elif defined(MUC_OPCUA_PROFILE_STANDARD)
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS 50
#elif defined(MUC_OPCUA_PROFILE_FULL)
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS 100
#endif
#ifdef MU_MAX_PUBLISH_REQUESTS
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS MU_MAX_PUBLISH_REQUESTS
#endif

/* ---- MonitoredItem queue depth -------------------------------------- */
#define MU_INTERN_MONITORED_QUEUE_DEPTH 1 /* stage 1: minimal baseline */
#if defined(MUC_OPCUA_PROFILE_EMBEDDED) || defined(MUC_OPCUA_PROFILE_STANDARD) || defined(MUC_OPCUA_PROFILE_FULL)
#undef MU_INTERN_MONITORED_QUEUE_DEPTH
#define MU_INTERN_MONITORED_QUEUE_DEPTH 2
#endif
#ifdef MU_MONITORED_QUEUE_DEPTH
#undef MU_INTERN_MONITORED_QUEUE_DEPTH
#define MU_INTERN_MONITORED_QUEUE_DEPTH MU_MONITORED_QUEUE_DEPTH
#endif

/* =====================================================================
 * Profile-invariant capacities (stage 1 baseline + stage 3 user override).
 * These do not vary per profile today; they still funnel through this file so
 * every capacity is resolved in one place and is -D-overridable.
 * ===================================================================== */

/* ---- Triggering links per MonitoredItem ----------------------------- */
#define MU_INTERN_MAX_TRIGGER_LINKS 4
#ifdef MU_MAX_TRIGGER_LINKS
#undef MU_INTERN_MAX_TRIGGER_LINKS
#define MU_INTERN_MAX_TRIGGER_LINKS MU_MAX_TRIGGER_LINKS
#endif

/* ---- Address-space index capacity ----------------------------------- */
#define MU_INTERN_MAX_ADDRESS_SPACE_NODES 64
#ifdef MU_MAX_ADDRESS_SPACE_NODES
#undef MU_INTERN_MAX_ADDRESS_SPACE_NODES
#define MU_INTERN_MAX_ADDRESS_SPACE_NODES MU_MAX_ADDRESS_SPACE_NODES
#endif

/* ---- Dynamic (NodeManagement) address-space limits ------------------ */
#define MU_INTERN_MAX_DYNAMIC_NODES 32
#ifdef MU_MAX_DYNAMIC_NODES
#undef MU_INTERN_MAX_DYNAMIC_NODES
#define MU_INTERN_MAX_DYNAMIC_NODES MU_MAX_DYNAMIC_NODES
#endif

#define MU_INTERN_MAX_DYNAMIC_REFERENCES 64
#ifdef MU_MAX_DYNAMIC_REFERENCES
#undef MU_INTERN_MAX_DYNAMIC_REFERENCES
#define MU_INTERN_MAX_DYNAMIC_REFERENCES MU_MAX_DYNAMIC_REFERENCES
#endif

#define MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH 64
#ifdef MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH
#define MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH
#endif

#define MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH 64
#ifdef MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH
#define MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH
#endif

#define MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH 64
#ifdef MU_MAX_DYNAMIC_STRING_NODEID_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH
#define MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH MU_MAX_DYNAMIC_STRING_NODEID_LENGTH
#endif

/* ---- Max Query continuation points ---------------------------------- */
#define MU_INTERN_MAX_QUERY_CONTINUATION_POINTS 2
#ifdef MU_MAX_QUERY_CONTINUATION_POINTS
#undef MU_INTERN_MAX_QUERY_CONTINUATION_POINTS
#define MU_INTERN_MAX_QUERY_CONTINUATION_POINTS MU_MAX_QUERY_CONTINUATION_POINTS
#endif

/* ---- Max Alarms & Conditions ---------------------------------------- */
#define MU_INTERN_MAX_CONDITIONS 10
#ifdef MU_MAX_CONDITIONS
#undef MU_INTERN_MAX_CONDITIONS
#define MU_INTERN_MAX_CONDITIONS MU_MAX_CONDITIONS
#endif

/* =====================================================================
 * Derived capacities (depend on a resolved capacity above; must come last).
 * ===================================================================== */

/* Secure channels track connections 1:1 (one channel per connection, see
 * mu_connection_t). Integrator may still override the public knob directly. */
#define MU_INTERN_MAX_SECURE_CHANNELS MU_INTERN_MAX_CONNECTIONS
#ifdef MU_MAX_SECURE_CHANNELS
#undef MU_INTERN_MAX_SECURE_CHANNELS
#define MU_INTERN_MAX_SECURE_CHANNELS MU_MAX_SECURE_CHANNELS
#endif

/* Reference string-NodeId length defaults to the string-NodeId length. */
#define MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH
#ifdef MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH
#define MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH
#endif

#endif /* MUC_OPCUA_CAPACITIES_H */
