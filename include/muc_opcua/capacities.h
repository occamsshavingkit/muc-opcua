/* include/muc_opcua/capacities.h
 *
 * GENERATED from profiles/opcua-profile-manifest.yaml by
 * scripts/profile_manifest/generate.py -- do not edit; regenerate with:
 *   python3 scripts/profile_manifest/generate.py \
 *       --manifest profiles/opcua-profile-manifest.yaml --outputs capacities_h
 *
 * SINGLE SOURCE OF TRUTH for every tunable capacity dimension.
 *
 * Each capacity is resolved by a three-stage cascade, in order:
 *
 *   1. DEFAULT  -- an unconditional baseline (the minimal/nano values).
 *   2. PROFILE  -- if an internal cascade symbol is active (Kconfig emits
 *                  MUC_OPCUA_INTERN_PROFILE_<TOKEN>), redefine to that
 *                  profile's value.  The cascade follows the selected
 *                  base profile (OPC-10000-7 §4.3).
 *   3. USER     -- if the public MU_MAX_* macro is defined (emitted by
 *                  src/CMakeLists.txt from the Kconfig-resolved int symbol),
 *                  redefine to it. The public knob wins over any profile.
 *
 * Precedence: default < profile < user (Kconfig-resolved).
 *
 * ALL library/application/test code compiles off the MU_INTERN_* macros below.
 * The public MU_MAX_* macros are INPUT: they are emitted from resolved Kconfig
 * capacity int symbols by src/CMakeLists.txt, so changes via -DMU_MAX_*=N,
 * menuconfig, or MUC_OPCUA_KCONFIG_CONFIG all reach the compiler here.
 */
#ifndef MUC_OPCUA_CAPACITIES_H
#define MUC_OPCUA_CAPACITIES_H

/* =====================================================================
 * Profile-varying capacities (all three stages).
 * ===================================================================== */

/* ---- Max sessions ------------------------------------------------ */
/* Public override: MU_MAX_SESSIONS */
#define MU_INTERN_MAX_SESSIONS 2
#if defined(MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
#undef MU_INTERN_MAX_SESSIONS
#define MU_INTERN_MAX_SESSIONS 100
#elif defined(MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER)
#undef MU_INTERN_MAX_SESSIONS
#define MU_INTERN_MAX_SESSIONS 50
#endif
#ifdef MU_MAX_SESSIONS
#undef MU_INTERN_MAX_SESSIONS
#define MU_INTERN_MAX_SESSIONS MU_MAX_SESSIONS
#endif

/* ---- Max connections --------------------------------------------- */
/* Public override: MU_MAX_CONNECTIONS */
#define MU_INTERN_MAX_CONNECTIONS 1
#if defined(MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 100
#elif defined(MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 50
#elif defined(MUC_OPCUA_INTERN_PROFILE_EMBEDDED_2025_UA_SERVER)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 4
#elif defined(MUC_OPCUA_INTERN_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER)
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS 2
#endif
#ifdef MU_MAX_CONNECTIONS
#undef MU_INTERN_MAX_CONNECTIONS
#define MU_INTERN_MAX_CONNECTIONS MU_MAX_CONNECTIONS
#endif

/* ---- Max subscriptions ------------------------------------------- */
/* Public override: MU_MAX_SUBSCRIPTIONS */
#define MU_INTERN_MAX_SUBSCRIPTIONS 2
#if defined(MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
#undef MU_INTERN_MAX_SUBSCRIPTIONS
#define MU_INTERN_MAX_SUBSCRIPTIONS 100
#elif defined(MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER)
#undef MU_INTERN_MAX_SUBSCRIPTIONS
#define MU_INTERN_MAX_SUBSCRIPTIONS 50
#endif
#ifdef MU_MAX_SUBSCRIPTIONS
#undef MU_INTERN_MAX_SUBSCRIPTIONS
#define MU_INTERN_MAX_SUBSCRIPTIONS MU_MAX_SUBSCRIPTIONS
#endif

/* ---- Max monitored items ----------------------------------------- */
/* Public override: MU_MAX_MONITORED_ITEMS */
#define MU_INTERN_MAX_MONITORED_ITEMS 8
#if defined(MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS 2000
#elif defined(MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER)
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS 1000
#elif defined(MUC_OPCUA_INTERN_PROFILE_EMBEDDED_2025_UA_SERVER)
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS 100
#endif
#ifdef MU_MAX_MONITORED_ITEMS
#undef MU_INTERN_MAX_MONITORED_ITEMS
#define MU_INTERN_MAX_MONITORED_ITEMS MU_MAX_MONITORED_ITEMS
#endif

/* ---- Max publish requests ---------------------------------------- */
/* Public override: MU_MAX_PUBLISH_REQUESTS */
#define MU_INTERN_MAX_PUBLISH_REQUESTS 4
#if defined(MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS 100
#elif defined(MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER)
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS 50
#elif defined(MUC_OPCUA_INTERN_PROFILE_EMBEDDED_2025_UA_SERVER)
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS 5
#endif
#ifdef MU_MAX_PUBLISH_REQUESTS
#undef MU_INTERN_MAX_PUBLISH_REQUESTS
#define MU_INTERN_MAX_PUBLISH_REQUESTS MU_MAX_PUBLISH_REQUESTS
#endif

/* ---- Monitored queue depth --------------------------------------- */
/* Public override: MU_MONITORED_QUEUE_DEPTH */
#define MU_INTERN_MONITORED_QUEUE_DEPTH 1
#if defined(MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
#undef MU_INTERN_MONITORED_QUEUE_DEPTH
#define MU_INTERN_MONITORED_QUEUE_DEPTH 5
#elif defined(MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER)
#undef MU_INTERN_MONITORED_QUEUE_DEPTH
#define MU_INTERN_MONITORED_QUEUE_DEPTH 5
#elif defined(MUC_OPCUA_INTERN_PROFILE_EMBEDDED_2025_UA_SERVER)
#undef MU_INTERN_MONITORED_QUEUE_DEPTH
#define MU_INTERN_MONITORED_QUEUE_DEPTH 2
#endif
#ifdef MU_MONITORED_QUEUE_DEPTH
#undef MU_INTERN_MONITORED_QUEUE_DEPTH
#define MU_INTERN_MONITORED_QUEUE_DEPTH MU_MONITORED_QUEUE_DEPTH
#endif

/* ---- Max array length -------------------------------------------- */
/* Public override: MU_MAX_ARRAY_LENGTH */
#define MU_INTERN_MAX_ARRAY_LENGTH 512
#if defined(MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES)
#undef MU_INTERN_MAX_ARRAY_LENGTH
#define MU_INTERN_MAX_ARRAY_LENGTH 8192
#elif defined(MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER)
#undef MU_INTERN_MAX_ARRAY_LENGTH
#define MU_INTERN_MAX_ARRAY_LENGTH 8192
#elif defined(MUC_OPCUA_INTERN_PROFILE_EMBEDDED_2025_UA_SERVER)
#undef MU_INTERN_MAX_ARRAY_LENGTH
#define MU_INTERN_MAX_ARRAY_LENGTH 2048
#endif
#ifdef MU_MAX_ARRAY_LENGTH
#undef MU_INTERN_MAX_ARRAY_LENGTH
#define MU_INTERN_MAX_ARRAY_LENGTH MU_MAX_ARRAY_LENGTH
#endif

/* =====================================================================
 * Profile-invariant capacities (stage 1 baseline + stage 3 user override).
 * ===================================================================== */

/* ---- Max trigger links ------------------------------------------- */
/* Public override: MU_MAX_TRIGGER_LINKS */
#define MU_INTERN_MAX_TRIGGER_LINKS 4
#ifdef MU_MAX_TRIGGER_LINKS
#undef MU_INTERN_MAX_TRIGGER_LINKS
#define MU_INTERN_MAX_TRIGGER_LINKS MU_MAX_TRIGGER_LINKS
#endif

/* ---- Max where elements ------------------------------------------ */
/* Public override: MU_MAX_WHERE_ELEMENTS */
#define MU_INTERN_MAX_WHERE_ELEMENTS 8
#ifdef MU_MAX_WHERE_ELEMENTS
#undef MU_INTERN_MAX_WHERE_ELEMENTS
#define MU_INTERN_MAX_WHERE_ELEMENTS MU_MAX_WHERE_ELEMENTS
#endif

/* ---- Max where operands ------------------------------------------ */
/* Public override: MU_MAX_WHERE_OPERANDS */
#define MU_INTERN_MAX_WHERE_OPERANDS 16
#ifdef MU_MAX_WHERE_OPERANDS
#undef MU_INTERN_MAX_WHERE_OPERANDS
#define MU_INTERN_MAX_WHERE_OPERANDS MU_MAX_WHERE_OPERANDS
#endif

/* ---- Where blob bytes -------------------------------------------- */
/* Public override: MU_WHERE_BLOB_BYTES */
#define MU_INTERN_WHERE_BLOB_BYTES 64
#ifdef MU_WHERE_BLOB_BYTES
#undef MU_INTERN_WHERE_BLOB_BYTES
#define MU_INTERN_WHERE_BLOB_BYTES MU_WHERE_BLOB_BYTES
#endif

/* ---- Max address space nodes ------------------------------------- */
/* Public override: MU_MAX_ADDRESS_SPACE_NODES */
#define MU_INTERN_MAX_ADDRESS_SPACE_NODES 64
#ifdef MU_MAX_ADDRESS_SPACE_NODES
#undef MU_INTERN_MAX_ADDRESS_SPACE_NODES
#define MU_INTERN_MAX_ADDRESS_SPACE_NODES MU_MAX_ADDRESS_SPACE_NODES
#endif

/* ---- Max dynamic nodes ------------------------------------------- */
/* Public override: MU_MAX_DYNAMIC_NODES */
#define MU_INTERN_MAX_DYNAMIC_NODES 32
#ifdef MU_MAX_DYNAMIC_NODES
#undef MU_INTERN_MAX_DYNAMIC_NODES
#define MU_INTERN_MAX_DYNAMIC_NODES MU_MAX_DYNAMIC_NODES
#endif

/* ---- Max dynamic references -------------------------------------- */
/* Public override: MU_MAX_DYNAMIC_REFERENCES */
#define MU_INTERN_MAX_DYNAMIC_REFERENCES 64
#ifdef MU_MAX_DYNAMIC_REFERENCES
#undef MU_INTERN_MAX_DYNAMIC_REFERENCES
#define MU_INTERN_MAX_DYNAMIC_REFERENCES MU_MAX_DYNAMIC_REFERENCES
#endif

/* ---- Max dynamic browse name length ------------------------------ */
/* Public override: MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH */
#define MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH 64
#ifdef MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH
#define MU_INTERN_MAX_DYNAMIC_BROWSE_NAME_LENGTH MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH
#endif

/* ---- Max dynamic display name length ----------------------------- */
/* Public override: MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH */
#define MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH 64
#ifdef MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH
#define MU_INTERN_MAX_DYNAMIC_DISPLAY_NAME_LENGTH MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH
#endif

/* ---- Max dynamic string nodeid length ---------------------------- */
/* Public override: MU_MAX_DYNAMIC_STRING_NODEID_LENGTH */
#define MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH 64
#ifdef MU_MAX_DYNAMIC_STRING_NODEID_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH
#define MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH MU_MAX_DYNAMIC_STRING_NODEID_LENGTH
#endif

/* ---- Max query continuation points ------------------------------- */
/* Public override: MU_MAX_QUERY_CONTINUATION_POINTS */
#define MU_INTERN_MAX_QUERY_CONTINUATION_POINTS 2
#ifdef MU_MAX_QUERY_CONTINUATION_POINTS
#undef MU_INTERN_MAX_QUERY_CONTINUATION_POINTS
#define MU_INTERN_MAX_QUERY_CONTINUATION_POINTS MU_MAX_QUERY_CONTINUATION_POINTS
#endif

/* ---- Max conditions ---------------------------------------------- */
/* Public override: MU_MAX_CONDITIONS */
#define MU_INTERN_MAX_CONDITIONS 10
#ifdef MU_MAX_CONDITIONS
#undef MU_INTERN_MAX_CONDITIONS
#define MU_INTERN_MAX_CONDITIONS MU_MAX_CONDITIONS
#endif

/* =====================================================================
 * Derived capacities (depend on a resolved capacity above; must come last).
 * ===================================================================== */

/* ---- Max secure channels ----------------------------------------- */
/* Public override: MU_MAX_SECURE_CHANNELS */
#define MU_INTERN_MAX_SECURE_CHANNELS MU_INTERN_MAX_CONNECTIONS
#ifdef MU_MAX_SECURE_CHANNELS
#undef MU_INTERN_MAX_SECURE_CHANNELS
#define MU_INTERN_MAX_SECURE_CHANNELS MU_MAX_SECURE_CHANNELS
#endif

/* ---- Max dynamic reference string nodeid length ------------------ */
/* Public override: MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH */
#define MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH MU_INTERN_MAX_DYNAMIC_STRING_NODEID_LENGTH
#ifdef MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH
#undef MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH
#define MU_INTERN_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH
#endif

#endif /* MUC_OPCUA_CAPACITIES_H */
