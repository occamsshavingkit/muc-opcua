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
# profile here, which in turn selects configs/<profile>.defconfig as the Kconfig
# base. The defconfig sets the corresponding OPC-named profile choice symbol
# (MUC_OPCUA_PROFILE_STANDARD_2017_UA_SERVER /
# MUC_OPCUA_PROFILE_EMBEDDED_2017_UA_SERVER; OPC-10000-7 §4.3) that drives the
# internal cascade symbols seeding Facet/CU/capacity defaults. STANDARD wins
# over EMBEDDED (it is the larger superset).
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

# --- Kconfig-driven feature + capacity resolution (kconfiglib, vendored) ------
# Feature symbol values come from /Kconfig. The profile string selects
# configs/<profile>.defconfig; any legacy -DMUC_OPCUA_<X>=ON/OFF is translated to a
# Kconfig override fragment merged on top (Kconfig `depends on` still vetoes a
# dependency-violating override). The generated muc_opcua_config.cmake sets
# MUC_OPCUA_<X> ON/OFF for bool symbols and MUC_OPCUA_<X> <N> for int symbols
# (capacities). src/CMakeLists.txt consumes both: bools drive source selection
# and target_compile_definitions; capacity ints are emitted as the public
# MU_MAX_* compile definitions that capacities.h stage 3 consumes. Legacy
# -DMU_MAX_*=N overrides are translated to the Kconfig fragment below, so the
# resolved Kconfig value is the single compile-time source of truth.
if(NOT MUC_OPCUA_PROFILE MATCHES "^(nano|micro|embedded|standard|full|custom)$")
    message(FATAL_ERROR "Unknown MUC_OPCUA_PROFILE='${MUC_OPCUA_PROFILE}'")
endif()

set(MUC_OPCUA_KCONFIG_FEATURES
    # --- Computed OPC-name-derived symbols (OPC-10000-7 §4.2, §4.3) ---
    # Prefixed entries use the symbol name as both the cache variable and the
    # override fragment line (CONFIG_ is empty).  Bare entries below are legacy
    # project-centric symbols whose cache variable is MUC_OPCUA_<NAME>.
    #
    # Profile effective choice symbols (OPC-10000-7 §4.3)
    MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER
    MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER
    MUC_OPCUA_PROFILE_EMBEDDED_2025_UA_SERVER
    MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER
    MUC_OPCUA_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES
    MUC_OPCUA_PROFILE_CUSTOM
    # Internal profile cascade symbols (OPC-10000-7 §4.3)
    MUC_OPCUA_INTERN_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER
    MUC_OPCUA_INTERN_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER
    MUC_OPCUA_INTERN_PROFILE_EMBEDDED_2025_UA_SERVER
    MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER
    MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES
    # Advertised-profile marker (internal build marker, drives base_nodes.c
    # ServerProfileArray + capacities.h tier; CMake-only, never a C define)
    MUC_OPCUA_MARKER_STANDARD_PROFILE
    # Facet group toggles (OPC-10000-7 §4.2)
    MUC_OPCUA_FACET_CORE_2022_SERVER
    MUC_OPCUA_FACET_EMBEDDED_DATACHANGE_SUBSCRIPTION_2022_SERVER
    MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER
    MUC_OPCUA_FACET_STANDARD_DATACHANGE_SUBSCRIPTION_2022_SERVER
    MUC_OPCUA_FACET_GLOBAL_CERTIFICATE_MANAGEMENT_SERVER
    MUC_OPCUA_FACET_USER_TOKEN_USER_NAME_PASSWORD_SERVER
    MUC_OPCUA_FACET_USER_TOKEN_X509_CERTIFICATE_SERVER
    # Conformance Unit symbols (OPC-10000-7 §4.2)
    MUC_OPCUA_CU_ATTRIBUTE_READ
    MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH
    MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS
    MUC_OPCUA_CU_VIEW_REGISTERNODES
    MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE
    MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET
    MUC_OPCUA_CU_QUERY
    MUC_OPCUA_CU_NODEMANAGEMENT
    # --- Legacy project-centric symbols still emitted by the generator ---
    # These remain in generated Kconfig and are needed for existing source
    # gating (#ifdef MUC_OPCUA_<NAME>) and build gating until T050/T051
    # migrate source gates to the OPC-name-derived symbols above.  Do not
    # remove prematurely -- doing so would silently drop legacy
    # -DMUC_OPCUA_<NAME>=ON/OFF overrides.
    BASE_NODES BASE_TYPE_SYSTEM EXTENDED_NODEIDS NAMESPACES DYNAMIC_NODES
    COMPLEX_TYPES USER_AUTH MULTIPLE_CONNECTIONS SESSION_TIMEOUT SECURITY ECC
    MULTI_CHUNK TIME_SYNC AUDITING SUBSCRIPTIONS SUBSCRIPTIONS_STANDARD EVENTS
    EVENT_FILTER_WHERE AGGREGATE_FULL REDUNDANCY METHOD_SERVER CUSTOM_METHODS
    PUBSUB DATA_ACCESS REVERSE_CONNECT SERVER_DIAGNOSTICS
    # Project options (non-OPC implementation controls)
    READ_CACHE)

# Legacy -D overrides are the ONLY cache entries for these symbols (option() is gone),
# so DEFINED CACHE{} reliably detects a user-supplied -DMUC_OPCUA_<X>=ON/OFF.
# CONFIG_ is empty, so the fragment line uses the bare Kconfig symbol name.  Fully
# prefixed entries (MUC_OPCUA_...) also use the same name as the cache variable.
set(_MUC_OPCUA_KCFG_FRAG "${CMAKE_BINARY_DIR}/kconfig_overrides.config")
file(WRITE ${_MUC_OPCUA_KCFG_FRAG} "")
foreach(_sym IN LISTS MUC_OPCUA_KCONFIG_FEATURES)
    if(_sym MATCHES "^MUC_OPCUA_")
        set(_cache_sym "${_sym}")
    else()
        set(_cache_sym "MUC_OPCUA_${_sym}")
    endif()
    if(DEFINED CACHE{${_cache_sym}})
        if(${${_cache_sym}})
            file(APPEND ${_MUC_OPCUA_KCFG_FRAG} "${_sym}=y\n")
        else()
            file(APPEND ${_MUC_OPCUA_KCFG_FRAG} "# ${_sym} is not set\n")
        endif()
    endif()
endforeach()

# Capacity int symbols (generated Kconfig + generated capacities.h).  Legacy
# -DMU_MAX_*=N overrides are translated into the Kconfig fragment as
# <SYM>=N (bare Kconfig symbol; CONFIG_ is empty) so the resolved value is
# visible in .config / menuconfig.  The compile-time capacity still comes
# from the generated capacities.h cascade (default < profile < user), which
# reads the same -DMU_MAX_* directly at stage 3.
# Format: "KCONFIG_SYMBOL=LEGACY_CACHE_VAR" pairs.
set(MUC_OPCUA_KCONFIG_CAPACITIES
    "MAX_SESSIONS=MU_MAX_SESSIONS"
    "MAX_CONNECTIONS=MU_MAX_CONNECTIONS"
    "MAX_SUBSCRIPTIONS=MU_MAX_SUBSCRIPTIONS"
    "MAX_MONITORED_ITEMS=MU_MAX_MONITORED_ITEMS"
    "MAX_PUBLISH_REQUESTS=MU_MAX_PUBLISH_REQUESTS"
    "MONITORED_QUEUE_DEPTH=MU_MONITORED_QUEUE_DEPTH"
    "MAX_ARRAY_LENGTH=MU_MAX_ARRAY_LENGTH"
    "MAX_TRIGGER_LINKS=MU_MAX_TRIGGER_LINKS"
    "MAX_WHERE_ELEMENTS=MU_MAX_WHERE_ELEMENTS"
    "MAX_WHERE_OPERANDS=MU_MAX_WHERE_OPERANDS"
    "WHERE_BLOB_BYTES=MU_WHERE_BLOB_BYTES"
    "MAX_ADDRESS_SPACE_NODES=MU_MAX_ADDRESS_SPACE_NODES"
    "MAX_DYNAMIC_NODES=MU_MAX_DYNAMIC_NODES"
    "MAX_DYNAMIC_REFERENCES=MU_MAX_DYNAMIC_REFERENCES"
    "MAX_DYNAMIC_BROWSE_NAME_LENGTH=MU_MAX_DYNAMIC_BROWSE_NAME_LENGTH"
    "MAX_DYNAMIC_DISPLAY_NAME_LENGTH=MU_MAX_DYNAMIC_DISPLAY_NAME_LENGTH"
    "MAX_DYNAMIC_STRING_NODEID_LENGTH=MU_MAX_DYNAMIC_STRING_NODEID_LENGTH"
    "MAX_QUERY_CONTINUATION_POINTS=MU_MAX_QUERY_CONTINUATION_POINTS"
    "MAX_CONDITIONS=MU_MAX_CONDITIONS"
    "MAX_SECURE_CHANNELS=MU_MAX_SECURE_CHANNELS"
    "MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH=MU_MAX_DYNAMIC_REFERENCE_STRING_NODEID_LENGTH"
)
foreach(_pair IN LISTS MUC_OPCUA_KCONFIG_CAPACITIES)
    string(REPLACE "=" ";" _kv "${_pair}")
    list(GET _kv 0 _kcfg_sym)
    list(GET _kv 1 _legacy_var)
    if(DEFINED CACHE{${_legacy_var}})
        file(APPEND ${_MUC_OPCUA_KCFG_FRAG} "${_kcfg_sym}=${${_legacy_var}}\n")
    endif()
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Base config: a user-supplied .config (e.g. saved from `menuconfig`) wins over the
# profile defconfig, so custom edits persist across reconfigures.
set(MUC_OPCUA_KCONFIG_CONFIG "" CACHE FILEPATH
    "Path to a Kconfig .config to build from (overrides the profile defconfig; e.g. one saved by the menuconfig target)")
if(MUC_OPCUA_KCONFIG_CONFIG AND EXISTS "${MUC_OPCUA_KCONFIG_CONFIG}")
    set(_MUC_OPCUA_KCFG_BASE "${MUC_OPCUA_KCONFIG_CONFIG}")
else()
    set(_MUC_OPCUA_KCFG_BASE "${CMAKE_CURRENT_SOURCE_DIR}/configs/${MUC_OPCUA_PROFILE}.defconfig")
endif()

set(_MUC_OPCUA_CONFIG_CMAKE "${CMAKE_BINARY_DIR}/muc_opcua_config.cmake")
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/kconfig/gen_config.py
            ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig
            ${_MUC_OPCUA_KCFG_BASE}
            ${_MUC_OPCUA_CONFIG_CMAKE}
            ${CMAKE_BINARY_DIR}/muc_opcua_autoconf.h
            ${_MUC_OPCUA_KCFG_FRAG}
    RESULT_VARIABLE _muc_opcua_kcfg_rc
    OUTPUT_VARIABLE _muc_opcua_kcfg_out ERROR_VARIABLE _muc_opcua_kcfg_out)
if(NOT _muc_opcua_kcfg_rc EQUAL 0)
    message(FATAL_ERROR "Kconfig resolution failed (rc=${_muc_opcua_kcfg_rc}):\n${_muc_opcua_kcfg_out}")
endif()
include(${_MUC_OPCUA_CONFIG_CMAKE})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig ${_MUC_OPCUA_KCFG_BASE})

# Dependency-aware config browsing/editing. `menuconfig` edits ${BUILD}/.config (seeded
# from the resolved config above); reconfigure with -DMUC_OPCUA_KCONFIG_CONFIG=${BUILD}/.config
# to apply. `savedefconfig` exports a minimal defconfig for committing under configs/.
add_custom_target(menuconfig
    COMMAND ${CMAKE_COMMAND} -E env CONFIG_=MUC_OPCUA_ KCONFIG_CONFIG=${CMAKE_BINARY_DIR}/.config
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/kconfig/menuconfig.py
            ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Editing ${CMAKE_BINARY_DIR}/.config -- reconfigure with -DMUC_OPCUA_KCONFIG_CONFIG=${CMAKE_BINARY_DIR}/.config to apply"
    USES_TERMINAL VERBATIM)
add_custom_target(savedefconfig
    COMMAND ${CMAKE_COMMAND} -E env CONFIG_=MUC_OPCUA_
            ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/kconfig/savedefconfig.py
            ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig ${CMAKE_BINARY_DIR}/.config ${CMAKE_BINARY_DIR}/defconfig
    COMMENT "Minimal defconfig written to ${CMAKE_BINARY_DIR}/defconfig"
    VERBATIM)

# ALLOW_HEAP is derived from the profile tier (MCU tiers are strictly no-heap per the
# constitution) -- not a Kconfig facet. option() below provides the default; force OFF
# for nano/micro/embedded.
if(MUC_OPCUA_PROFILE STREQUAL "nano" OR MUC_OPCUA_PROFILE STREQUAL "micro"
   OR MUC_OPCUA_PROFILE STREQUAL "embedded")
    set(MUC_OPCUA_ALLOW_HEAP OFF CACHE BOOL "" FORCE)
endif()

# Feature flags (SECURITY, services, facets, subscriptions, ...) are declared in /Kconfig
# and resolved above -- see `menuconfig` for the browsable tree with help + dependencies.
# The remaining options below are NOT Kconfig facets: the client tier and the ALLOW_HEAP
# memory-model knob.
option(MUC_OPCUA_CLIENT_NANO "Build Nano Embedded UA Client Profile support" OFF)
option(MUC_OPCUA_ALLOW_HEAP "Allow heap allocation in optional adapters/features" ON)
# Read value cache (OPC-10000-4 5.11.2 maxAge optimization). OFF by default: the
# cache is a latency optimization, not a conformance requirement (a server may
# always return a fresh "best effort" value), and it pulls double-precision
# soft-float into the Read path. Opt in for speed on FPU-capable / non-size-critical
# targets. Independent of MUC_OPCUA_PROFILE (not reset by the profile machinery).

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
# Cross-feature dependencies are now enforced by `depends on` in /Kconfig
# (kconfiglib vetoes a dependency-violating -D override at configure time).

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
# above already set -- and had drifted out of sync. They are now folded into the
# shorthand block near the top of this file (the single source of truth), which
# promotes the profile string. The internal cascade symbols
# (MUC_OPCUA_INTERN_PROFILE_*; OPC-10000-7 §4.3) are derived automatically by
# Kconfig from the profile choice symbol. Direct
# -DMUC_OPCUA_{EMBEDDED,STANDARD}_PROFILE=ON use is promoted to the matching
# profile near the top of this file; illegal custom-profile combinations are still
# caught at compile time by features.h.


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
