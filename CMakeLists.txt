cmake_minimum_required(VERSION 3.20)

# T001 Options
option(MUC_OPCUA_BUILD_TESTS "Build tests" OFF)
option(MUC_OPCUA_BUILD_EXAMPLES "Build examples" OFF)
option(MUC_OPCUA_BUILD_FUZZERS "Build fuzzers" OFF)
option(MUC_OPCUA_BUILD_BENCHMARKS "Build benchmark executables" OFF)
if(DEFINED CACHE{MUC_OPCUA_PROFILE})
    set(MUC_OPCUA_PROFILE_EXPLICIT ON)
else()
    set(MUC_OPCUA_PROFILE_EXPLICIT OFF)
endif()
set(MUC_OPCUA_PROFILE "nano" CACHE STRING "Target OPC UA profile (nano, micro, embedded, standard, full, custom)")
set(MUC_OPCUA_CLIENT_PROFILE "none" CACHE STRING "Target OPC UA client profile (none, nano, standard)")

# Legacy shorthand: -DMUC_OPCUA_EMBEDDED_PROFILE=ON / -DMUC_OPCUA_STANDARD_PROFILE=ON
# select a profile without naming MUC_OPCUA_PROFILE. Promote them to the string
# profile here, BEFORE the reset loop below -- that loop force-clears every
# profile-controlled option (including these booleans) whenever the profile is not
# "custom", so a boolean passed on its own would otherwise be silently clobbered
# back to OFF and produce a nano build. STANDARD wins over EMBEDDED (it is the
# larger superset).
if(NOT MUC_OPCUA_PROFILE_EXPLICIT)
    if(MUC_OPCUA_STANDARD_PROFILE)
        set(MUC_OPCUA_PROFILE "standard" CACHE STRING "Target OPC UA profile" FORCE)
        set(MUC_OPCUA_PROFILE_EXPLICIT ON)
    elseif(MUC_OPCUA_EMBEDDED_PROFILE)
        set(MUC_OPCUA_PROFILE "embedded" CACHE STRING "Target OPC UA profile" FORCE)
        set(MUC_OPCUA_PROFILE_EXPLICIT ON)
    endif()
endif()

# Profile resolution
if((MUC_OPCUA_BUILD_TESTS OR MUC_OPCUA_BUILD_EXAMPLES OR MUC_OPCUA_BUILD_FUZZERS OR MUC_OPCUA_BUILD_BENCHMARKS)
   AND MUC_OPCUA_PROFILE STREQUAL "nano"
   AND NOT MUC_OPCUA_PROFILE_EXPLICIT)
    set(MUC_OPCUA_PROFILE "full" CACHE STRING "Target OPC UA profile" FORCE)
endif()

set(MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS
    MUC_OPCUA_SUBSCRIPTIONS
    MUC_OPCUA_SUBSCRIPTIONS_STANDARD
    MUC_OPCUA_USER_AUTH
    MUC_OPCUA_SERVICE_WRITE
    MUC_OPCUA_SECURITY
    MUC_OPCUA_ECC
    MUC_OPCUA_BASE_NODES
    MUC_OPCUA_BASE_TYPE_SYSTEM
    MUC_OPCUA_MULTIPLE_CONNECTIONS
    MUC_OPCUA_EVENTS
    MUC_OPCUA_EMBEDDED_PROFILE
    MUC_OPCUA_SERVICE_REGISTER_NODES
    MUC_OPCUA_SERVICE_HISTORY
    MUC_OPCUA_SERVICE_QUERY
    MUC_OPCUA_SERVICE_NODEMANAGEMENT
    MUC_OPCUA_DYNAMIC_NODES
    MUC_OPCUA_PUBSUB
    MUC_OPCUA_CUSTOM_METHODS
    MUC_OPCUA_SERVER_DIAGNOSTICS
    MUC_OPCUA_MULTI_CHUNK
    MUC_OPCUA_EXTENDED_NODEIDS
    MUC_OPCUA_DATA_ACCESS
    MUC_OPCUA_METHOD_SERVER
    MUC_OPCUA_EVENT_FILTER_WHERE
    MUC_OPCUA_AUDITING
    MUC_OPCUA_COMPLEX_TYPES
    MUC_OPCUA_REDUNDANCY
    MUC_OPCUA_AGGREGATE_FULL
    MUC_OPCUA_REVERSE_CONNECT
    MUC_OPCUA_TIME_SYNC
    MUC_OPCUA_NAMESPACES
    MUC_OPCUA_STANDARD_PROFILE
)

# --- Profile-option override (subtraction) support -------------------------
#
# A user may want e.g. "standard minus encryption":
#   cmake -S . -B build -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF
# For this to work, the per-profile blocks below must NOT unconditionally
# FORCE every option back to the profile default -- they must skip forcing
# any option the user already gave a value for on THIS configure invocation.
#
# Checking mere presence (DEFINED CACHE{<OPT>}) is NOT enough to detect that:
# option()/set(CACHE) create the cache entry with a default the first time
# they run, after which DEFINED CACHE{<OPT>} is permanently true regardless
# of user intent -- and on a RECONFIGURE of an existing build directory, an
# option left over from a *previous* profile's FORCE is *also* always
# "defined", indistinguishable by presence alone from a value the user just
# passed on the command line. The fix has to compare the option's CURRENT
# VALUE against the value WE (this file) last forced it to
# (_MUC_OPCUA_<OPT>_LAST_FORCED, updated every time we do the forcing): if
# they differ, something outside this file's own forcing changed it -- an
# explicit user override, which wins. If they still match, or we have never
# forced this option in this build directory before, presence is a reliable
# proxy for "the user passed -D<OPT>=... on this configure".
foreach(MUC_OPCUA_PROFILE_OPTION IN LISTS MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS)
    set(_MUC_OPCUA_${MUC_OPCUA_PROFILE_OPTION}_USER_SET OFF)
    if(DEFINED CACHE{${MUC_OPCUA_PROFILE_OPTION}})
        if(DEFINED CACHE{_MUC_OPCUA_${MUC_OPCUA_PROFILE_OPTION}_LAST_FORCED})
            if(NOT "${${MUC_OPCUA_PROFILE_OPTION}}" STREQUAL "${_MUC_OPCUA_${MUC_OPCUA_PROFILE_OPTION}_LAST_FORCED}")
                set(_MUC_OPCUA_${MUC_OPCUA_PROFILE_OPTION}_USER_SET ON)
            endif()
        else()
            # Never forced before in this build directory: any pre-existing
            # cache entry can only have come from a user -D.
            set(_MUC_OPCUA_${MUC_OPCUA_PROFILE_OPTION}_USER_SET ON)
        endif()
    endif()
endforeach()

# Beyond per-option override detection, only re-derive/FORCE options at all
# when MUC_OPCUA_PROFILE itself just changed (including a build directory's
# first-ever configure). This keeps a plain reconfigure of the SAME profile
# with one new -D<OPT>=... a no-fuss no-op for every OTHER option: nothing
# in this file's forcing machinery runs, so the new -D value simply sits in
# the cache untouched.
set(_MUC_OPCUA_PROFILE_PREV "__unset__" CACHE INTERNAL "Last MUC_OPCUA_PROFILE this file resolved (internal)")
if(NOT _MUC_OPCUA_PROFILE_PREV STREQUAL MUC_OPCUA_PROFILE)
    set(MUC_OPCUA_PROFILE_CHANGED ON)
else()
    set(MUC_OPCUA_PROFILE_CHANGED OFF)
endif()

# Apply a profile-controlled option's default value, unless the profile is
# unchanged since the last configure (nothing to re-derive) or the user
# explicitly supplied this option on this configure invocation (an
# intentional subtraction/addition against the profile default, which wins).
# Every time this DOES force a value, it also records that value as the new
# _LAST_FORCED baseline, so future configures can tell "still what we set"
# apart from "changed since, by someone/something other than this file".
macro(muc_opcua_apply_profile_option MUC_OPCUA_OPT MUC_OPCUA_DEFAULT)
    if(MUC_OPCUA_PROFILE_CHANGED AND NOT _MUC_OPCUA_${MUC_OPCUA_OPT}_USER_SET)
        set(${MUC_OPCUA_OPT} ${MUC_OPCUA_DEFAULT} CACHE BOOL "" FORCE)
        set(_MUC_OPCUA_${MUC_OPCUA_OPT}_LAST_FORCED "${MUC_OPCUA_DEFAULT}" CACHE INTERNAL "" FORCE)
    endif()
endmacro()

if(NOT MUC_OPCUA_PROFILE STREQUAL "custom" AND MUC_OPCUA_PROFILE_CHANGED)
    foreach(MUC_OPCUA_PROFILE_OPTION IN LISTS MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS)
        if(NOT _MUC_OPCUA_${MUC_OPCUA_PROFILE_OPTION}_USER_SET)
            set(${MUC_OPCUA_PROFILE_OPTION} OFF CACHE BOOL "" FORCE)
            set(_MUC_OPCUA_${MUC_OPCUA_PROFILE_OPTION}_LAST_FORCED OFF CACHE INTERNAL "" FORCE)
        endif()
    endforeach()
endif()

# MUC_OPCUA_PROFILE is the single source of truth. Each block below sets only the
# feature options and the derived *_PROFILE markers (consumed by code #if gates
# and features.h dependency checks) for its tier, via muc_opcua_apply_profile_option
# (see above) so a per-option -D can override any individual default -- e.g.
# -DMUC_OPCUA_PROFILE=standard -DMUC_OPCUA_SECURITY=OFF builds "standard minus
# encryption" instead of silently restoring SECURITY=ON. Overriding a dependency
# an enabled feature requires (e.g. dropping SECURITY while ECC stays on) is
# still caught: see the MUC_OPCUA_ECC/MUC_OPCUA_SECURITY check in src/CMakeLists.txt
# and the #error guards in include/muc_opcua/features.h. See
# docs/build-and-gating.md for the full flag reference and worked examples.
#
# CAPACITIES are NOT set here. They are resolved in
# include/muc_opcua/capacities.h via a default<profile<user cascade, keyed on the
# MUC_OPCUA_PROFILE_<NAME> marker that src/CMakeLists.txt emits. An integrator's
# public -DMU_MAX_*=<n> reaches the compiler directly and overrides the profile
# preset. Pick -DMUC_OPCUA_PROFILE=custom to hand-select features (capacities then
# come from the standard baseline unless -D'd).
# Named profiles are STRICT: each equals exactly the mandatory facet set of its
# namesake OPC UA Server Profile (spec 067, grounded against the OPC profile DB).
# Optional facets are integrator opt-ins via -DMUC_OPCUA_<FEATURE>=ON. Only `full`
# turns everything on. Do NOT add "nice to have" extras to a named profile.
if(MUC_OPCUA_PROFILE STREQUAL "nano")
    # NanoEmbeddedDevice2017 = Core2017Facet + UserNamePassword + UA-TCP.
    # SERVICE_READ/BROWSE/DISCOVERY default ON. No subscriptions/security/write.
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_NODES ON)              # Base Info Core Structure (Server object)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_REGISTER_NODES ON) # Core: View RegisterNodes
    muc_opcua_apply_profile_option(MUC_OPCUA_USER_AUTH ON)              # Core: UserName/Password (facet 1096)
    # Embedded/MCU tier: the core makes no heap allocations (constitution).
    if(MUC_OPCUA_PROFILE_CHANGED)
        set(MUC_OPCUA_ALLOW_HEAP OFF CACHE BOOL "" FORCE)
    endif()
elseif(MUC_OPCUA_PROFILE STREQUAL "micro")
    # MicroEmbeddedDevice2017 = Nano + EmbeddedDataChangeSubscription + Session Min 2 Parallel.
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_REGISTER_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_USER_AUTH ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SUBSCRIPTIONS ON)          # EmbeddedDataChangeSubscription (basic)
    muc_opcua_apply_profile_option(MUC_OPCUA_MULTIPLE_CONNECTIONS ON)   # Session Minimum 2 Parallel
    if(MUC_OPCUA_PROFILE_CHANGED)
        set(MUC_OPCUA_ALLOW_HEAP OFF CACHE BOOL "" FORCE)
    endif()
elseif(MUC_OPCUA_PROFILE STREQUAL "embedded")
    # EmbeddedUA2017 = Micro + StandardDataChangeSubscription2017 + Security + Base Info Type System.
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_REGISTER_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_USER_AUTH ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SUBSCRIPTIONS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON) # + built-in GetMonitoredItems/ResendData, deadband, triggering
    muc_opcua_apply_profile_option(MUC_OPCUA_MULTIPLE_CONNECTIONS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SECURITY ON)               # Security Policy Required + AppInstance cert
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_TYPE_SYSTEM ON)       # Base Info Type System
    muc_opcua_apply_profile_option(MUC_OPCUA_EMBEDDED_PROFILE ON)
    if(MUC_OPCUA_PROFILE_CHANGED)
        set(MUC_OPCUA_ALLOW_HEAP OFF CACHE BOOL "" FORCE)
    endif()
    # Embedded 2017 capacity minima: see MU_INTERN_* embedded column in capacities.h.
elseif(MUC_OPCUA_PROFILE STREQUAL "standard")
    # StandardUA2017 = Embedded + EnhancedDataChangeSubscription2017 (capacity) + X509 user token.
    # STRICT: no Write/History/Query/NodeManagement/PubSub/DataAccess/Events/Diagnostics/
    # ReverseConnect/Redundancy/Aggregate/ECC/etc. -- those are optional facets (opt-in).
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_REGISTER_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_USER_AUTH ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SUBSCRIPTIONS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_MULTIPLE_CONNECTIONS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SECURITY ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_TYPE_SYSTEM ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_EMBEDDED_PROFILE ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_STANDARD_PROFILE ON)       # advertises StandardUA2017; Enhanced capacity + X509
    # Standard 2017 capacity minima: see MU_INTERN_* standard column in capacities.h.
elseif(MUC_OPCUA_PROFILE STREQUAL "full")
    # Full: superset of standard ("everything on") with generous capacities. Not a
    # distinct OPC UA profile -- it still advertises StandardUA2017 via the
    # MUC_OPCUA_STANDARD_PROFILE marker below.
    muc_opcua_apply_profile_option(MUC_OPCUA_SUBSCRIPTIONS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_USER_AUTH ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_WRITE ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SECURITY ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_BASE_TYPE_SYSTEM ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_MULTIPLE_CONNECTIONS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_EVENTS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_EMBEDDED_PROFILE ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_REGISTER_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_HISTORY ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_QUERY ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVICE_NODEMANAGEMENT ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_DYNAMIC_NODES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_PUBSUB ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_CUSTOM_METHODS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_SERVER_DIAGNOSTICS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_MULTI_CHUNK ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_EXTENDED_NODEIDS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_DATA_ACCESS ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_METHOD_SERVER ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_EVENT_FILTER_WHERE ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_AUDITING ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_COMPLEX_TYPES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_REDUNDANCY ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_AGGREGATE_FULL ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_REVERSE_CONNECT ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_TIME_SYNC ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_NAMESPACES ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_ECC ON)
    muc_opcua_apply_profile_option(MUC_OPCUA_STANDARD_PROFILE ON)
    # Full capacities: see MU_INTERN_* full column in capacities.h.
endif()

set(_MUC_OPCUA_PROFILE_PREV "${MUC_OPCUA_PROFILE}" CACHE INTERNAL "Last MUC_OPCUA_PROFILE this file resolved (internal)" FORCE)

# SecurityPolicy Basic256Sha256 support (asym/sym chunk crypto, ~10 KB code).
# Turn OFF for a SecurityPolicy-None-only build to drop the crypto layer entirely.
option(MUC_OPCUA_SECURITY "Build SecurityPolicy Basic256Sha256 support" OFF)

# ECC SecurityPolicies (spec 059): ECC-curve25519 (Ed25519 + X25519 +
# ChaCha20-Poly1305) and ECC-nistP256 (ECDSA + P-256 + AES128-CBC/HMAC). Optional
# at every OPC UA server profile (the "Security ECC Policy" CU is optional); this
# implements it so the optional CU can be advertised. Requires MUC_OPCUA_SECURITY.
option(MUC_OPCUA_ECC "Build ECC SecurityPolicies (curve25519 + nistP256)" OFF)

# Optional services — turn OFF to drop a service's code from a minimal build.
# (OpenSecureChannel / Session services are always present.)
option(MUC_OPCUA_SERVICE_READ "Build the Read service" ON)
option(MUC_OPCUA_SERVICE_BROWSE "Build the Browse + BrowseNext + TranslateBrowsePaths services" ON)
option(MUC_OPCUA_SERVICE_DISCOVERY "Build GetEndpoints/FindServers" ON)
option(MUC_OPCUA_SERVICE_REGISTER_NODES "Build RegisterNodes/UnregisterNodes" OFF)
option(MUC_OPCUA_SERVICE_WRITE "Build the optional Write service (Value attribute only)" OFF)
option(MUC_OPCUA_SERVICE_HISTORY "Build Historical Access" OFF)
option(MUC_OPCUA_SERVICE_QUERY "Build the Query services" OFF)
option(MUC_OPCUA_SERVICE_NODEMANAGEMENT "Build the optional NodeManagement service set" OFF)
# The library's standard Base Information node set (Server object hierarchy,
# ServerStatus, ServerCapabilities). Turn OFF if the integrator supplies a complete
# address space and does not want the default nodes.
option(MUC_OPCUA_BASE_NODES "Build the standard Base Information node set" OFF)
option(MUC_OPCUA_BASE_TYPE_SYSTEM "Expose the Base Info Type System node set used by the embedded profile" OFF)
# Data-change subscriptions (the Micro profile: Subscription + MonitoredItem service
# sets, the Embedded Data Change Subscription Server Facet). Turn OFF for a Nano-only
# build to drop the subscription engine and its fixed-size state entirely.
option(MUC_OPCUA_SUBSCRIPTIONS "Build the data-change subscription engine (Micro profile)" OFF)
option(MUC_OPCUA_SUBSCRIPTIONS_STANDARD "Build the Standard DataChange Subscription 2017 facet additions" OFF)
option(MUC_OPCUA_EMBEDDED_PROFILE "Apply Embedded 2017 profile gates and capacity minima" OFF)
option(MUC_OPCUA_MULTI_CHUNK "Support multi-chunk (continuation) messages" OFF)
option(MUC_OPCUA_EXTENDED_NODEIDS "Support GUID and Opaque NodeId formats" OFF)
option(MUC_OPCUA_PUBSUB "Build Publish/Subscribe capabilities" OFF)
option(MUC_OPCUA_DATA_ACCESS "Build Data Access Server Facet (percent deadband, EURange, AnalogItem metadata)" OFF)
option(MUC_OPCUA_METHOD_SERVER "Build Method Server Facet (arbitrary custom method dispatch)" OFF)
option(MUC_OPCUA_EVENT_FILTER_WHERE "Build EventFilter where-clause evaluation engine" OFF)
option(MUC_OPCUA_AUDITING "Build Auditing Server Facet (audit event types)" OFF)
option(MUC_OPCUA_COMPLEX_TYPES "Build ComplexType Server Facet (custom structures and enumerations)" OFF)
option(MUC_OPCUA_REDUNDANCY "Build Client Redundancy Facet (TransferSubscriptions)" OFF)
option(MUC_OPCUA_AGGREGATE_FULL "Build full aggregate function set (42 OPC-10000-13 aggregates)" OFF)
option(MUC_OPCUA_REVERSE_CONNECT "Build Reverse Connect Facet (server-initiated connections)" OFF)
option(MUC_OPCUA_TIME_SYNC "Build Security Time Synchronization (timestamp population)" OFF)
option(MUC_OPCUA_NAMESPACES "Build Namespaces metadata node per OPC-10000-5 6.2.10" OFF)
option(MUC_OPCUA_STANDARD_PROFILE "Apply Standard 2017 profile gates and capacity minima" OFF)
option(MUC_OPCUA_CLIENT_NANO "Build Nano Embedded UA Client Profile support" OFF)
option(MUC_OPCUA_ALLOW_HEAP "Allow heap allocation in optional adapters/features" ON)
option(MUC_OPCUA_SESSION_TIMEOUT "Enable session timeout enforcement" OFF)
# Read value cache (OPC-10000-4 5.11.2 maxAge optimization). OFF by default: the
# cache is a latency optimization, not a conformance requirement (a server may
# always return a fresh "best effort" value), and it pulls double-precision
# soft-float into the Read path. Opt in for speed on FPU-capable / non-size-critical
# targets. Independent of MUC_OPCUA_PROFILE (not reset by the profile machinery).
option(MUC_OPCUA_READ_CACHE "Build the Read value cache (maxAge optimization; trades size for speed)" OFF)

# --- Cross-feature dependency checks (configure-time) -----------------------
#
# Before the profile-option override mechanism above, every named profile
# FORCE-set every controlled option together, so an inconsistent combination
# (a feature ON while something it needs is OFF) was structurally
# unreachable. Now that -D can subtract/add individual flags, it IS reachable
# -- so it must fail loudly rather than silently link a half-wired feature.
# This mirrors, at configure time, every "X requires Y" check already
# enforced at COMPILE time in include/muc_opcua/features.h (which stays as a
# backstop for anyone who bypasses this file's profile logic entirely, e.g. a
# build system wrapping this one with raw -D's). Keep both lists in sync;
# see docs/build-and-gating.md for the semantics this implements.
# Each entry is "OPTION:REQUIRES" (colon-delimited, not semicolon-delimited --
# CMake lists are fundamentally semicolon-separated strings with no true
# nesting, so a semicolon "pair" here would silently flatten into the
# surrounding list instead of staying a 2-element group).
set(MUC_OPCUA_FEATURE_DEPENDENCIES
    "MUC_OPCUA_ECC:MUC_OPCUA_SECURITY"
    "MUC_OPCUA_SUBSCRIPTIONS_STANDARD:MUC_OPCUA_SUBSCRIPTIONS"
    "MUC_OPCUA_EVENTS:MUC_OPCUA_SUBSCRIPTIONS"
    "MUC_OPCUA_BASE_TYPE_SYSTEM:MUC_OPCUA_BASE_NODES"
    "MUC_OPCUA_DATA_ACCESS:MUC_OPCUA_BASE_NODES"
    "MUC_OPCUA_EVENT_FILTER_WHERE:MUC_OPCUA_EVENTS"
    "MUC_OPCUA_AUDITING:MUC_OPCUA_EVENTS"
    "MUC_OPCUA_COMPLEX_TYPES:MUC_OPCUA_BASE_NODES"
    "MUC_OPCUA_AGGREGATE_FULL:MUC_OPCUA_SUBSCRIPTIONS_STANDARD"
    "MUC_OPCUA_REDUNDANCY:MUC_OPCUA_SUBSCRIPTIONS"
    "MUC_OPCUA_NAMESPACES:MUC_OPCUA_BASE_NODES"
    # StandardUA2017 mandates neither Data Access nor the Method Server (arbitrary Call)
    # facet -- both are optional opt-ins (spec 067). No STANDARD_PROFILE dependency here.
)
foreach(MUC_OPCUA_DEP_PAIR IN LISTS MUC_OPCUA_FEATURE_DEPENDENCIES)
    string(REPLACE ":" ";" MUC_OPCUA_DEP_PAIR_LIST "${MUC_OPCUA_DEP_PAIR}")
    list(GET MUC_OPCUA_DEP_PAIR_LIST 0 MUC_OPCUA_DEP_OPT)
    list(GET MUC_OPCUA_DEP_PAIR_LIST 1 MUC_OPCUA_DEP_REQUIRES)
    if(${MUC_OPCUA_DEP_OPT} AND NOT ${MUC_OPCUA_DEP_REQUIRES})
        message(FATAL_ERROR
            "${MUC_OPCUA_DEP_OPT} requires ${MUC_OPCUA_DEP_REQUIRES} "
            "(pass -D${MUC_OPCUA_DEP_OPT}=OFF alongside your override, or "
            "also enable ${MUC_OPCUA_DEP_REQUIRES})")
    endif()
endforeach()

set(MUC_OPCUA_SANITIZERS "" CACHE STRING "Comma-separated list of sanitizers (e.g. address,undefined)")
if(MUC_OPCUA_SANITIZERS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${MUC_OPCUA_SANITIZERS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${MUC_OPCUA_SANITIZERS}")
endif()

if(MUC_OPCUA_CLIENT_PROFILE STREQUAL "none")
elseif(MUC_OPCUA_CLIENT_PROFILE STREQUAL "nano")
    set(MUC_OPCUA_CLIENT_NANO ON CACHE BOOL "Build Nano Embedded UA Client Profile support" FORCE)
elseif(MUC_OPCUA_CLIENT_PROFILE STREQUAL "standard")
    set(MUC_OPCUA_CLIENT_NANO ON CACHE BOOL "Build Nano Embedded UA Client Profile support" FORCE)
    set(MUC_OPCUA_CLIENT_STANDARD ON CACHE BOOL "Build Standard UA Client 2025 Profile support" FORCE)
elseif(MUC_OPCUA_CLIENT_PROFILE STREQUAL "micro" OR
       MUC_OPCUA_CLIENT_PROFILE STREQUAL "embedded" OR
       MUC_OPCUA_CLIENT_PROFILE STREQUAL "full")
    message(FATAL_ERROR "MUC_OPCUA_CLIENT_PROFILE='${MUC_OPCUA_CLIENT_PROFILE}' is planned but not implemented; use 'nano' or 'none'")
else()
    message(FATAL_ERROR "Unknown MUC_OPCUA_CLIENT_PROFILE='${MUC_OPCUA_CLIENT_PROFILE}'")
endif()
set(MUC_OPCUA_PLATFORM "host" CACHE STRING "Target platform (host, external, pico, arduino-skeleton)")

# The MUC_OPCUA_{EMBEDDED,STANDARD}_PROFILE feature/capacity fan-out lived here in
# two standalone blocks that re-derived what the MUC_OPCUA_PROFILE string blocks
# above already set -- and had drifted out of sync. They are now folded into those
# blocks (the single source of truth). Direct -DMUC_OPCUA_{EMBEDDED,STANDARD}_PROFILE=ON
# use is promoted to the matching profile near the top of this file; illegal
# custom-profile combinations are still caught at compile time by features.h.

if(MUC_OPCUA_MULTI_CHUNK OR MUC_OPCUA_MULTIPLE_CONNECTIONS)
    set(MUC_OPCUA_SESSION_TIMEOUT ON CACHE BOOL "Enable session timeout enforcement" FORCE)
endif()

# The Pico SDK installs its cross-compilation toolchain, so it must be imported
# BEFORE project(). Requires PICO_SDK_PATH to point at a Pico SDK checkout.
if(MUC_OPCUA_PLATFORM STREQUAL "pico")
    include(pico_sdk_import.cmake)
endif()

project(muc_opcua VERSION 0.1.0 LANGUAGES C)

# The Pico SDK builds C++/assembly support code and must be initialized after project().
if(MUC_OPCUA_PLATFORM STREQUAL "pico")
    enable_language(CXX ASM)
    pico_sdk_init()
endif()

include(CTest)

# Phase 1 setup will include other cmake files
include(cmake/MucOpcUaOptions.cmake)


include(cmake/MucOpcUaWarnings.cmake)
include(cmake/MucOpcUaCodegen.cmake)
include(cmake/MucOpcUaStaticAnalysis.cmake)

add_subdirectory(src)

if(MUC_OPCUA_STACK_USAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(muc_opcua PRIVATE -fstack-usage)
    else()
        message(FATAL_ERROR "MUC_OPCUA_STACK_USAGE requires a C compiler that supports -fstack-usage")
    endif()
endif()

if(MUC_OPCUA_BUILD_TESTS OR MUC_OPCUA_BUILD_FUZZERS OR MUC_OPCUA_BUILD_BENCHMARKS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MUC_OPCUA_STACK_USAGE)
    add_custom_target(check_stack_usage
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_stack_usage.sh
            --build-dir ${CMAKE_BINARY_DIR}
            --threshold ${MUC_OPCUA_STACK_USAGE_LIMIT}
        COMMENT "Checking secured-OPN stack usage budget"
        VERBATIM
    )

    if(BUILD_TESTING)
        add_test(NAME test_stack_budget
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_stack_usage.sh
                --build-dir ${CMAKE_BINARY_DIR}
                --threshold ${MUC_OPCUA_STACK_USAGE_LIMIT}
        )
    endif()
endif()

if(MUC_OPCUA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MUC_OPCUA_PLATFORM STREQUAL "pico")
    add_subdirectory(platform/pico)
endif()

# Generate version.h from template (T027-T028)
set(MUC_OPCUA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(MUC_OPCUA_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(MUC_OPCUA_VERSION_PATCH ${PROJECT_VERSION_PATCH})
configure_file(
    ${CMAKE_SOURCE_DIR}/include/muc_opcua/version.h.in
    ${CMAKE_BINARY_DIR}/include/muc_opcua/version.h
    @ONLY
)
target_include_directories(muc_opcua PUBLIC ${CMAKE_BINARY_DIR}/include)
