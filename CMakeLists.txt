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

# --- Kconfig-driven feature resolution (kconfiglib, vendored) ----------------
# Feature symbol values come from /Kconfig. The profile string selects
# configs/<profile>.defconfig; any legacy -DMUC_OPCUA_<X>=ON/OFF is translated to a
# Kconfig override fragment merged on top (Kconfig `depends on` still vetoes a
# dependency-violating override). The generated muc_opcua_config.cmake sets
# MUC_OPCUA_<X> ON/OFF, which src/CMakeLists.txt uses for source selection AND its
# target_compile_definitions -- so the -D the compiler sees is byte-identical to the
# pre-Kconfig build. This replaces the option()/apply_profile_option/reset-loop +
# per-profile blocks + FEATURE_DEPENDENCIES machinery. Capacities stay in
# capacities.h (keyed on the MUC_OPCUA_PROFILE_<NAME> marker src/CMakeLists.txt emits).
if(NOT MUC_OPCUA_PROFILE MATCHES "^(nano|micro|embedded|standard|full|custom)$")
    message(FATAL_ERROR "Unknown MUC_OPCUA_PROFILE='${MUC_OPCUA_PROFILE}'")
endif()

set(MUC_OPCUA_KCONFIG_FEATURES
    SERVICE_READ SERVICE_BROWSE SERVICE_DISCOVERY SERVICE_REGISTER_NODES SERVICE_WRITE
    SERVICE_HISTORY SERVICE_QUERY SERVICE_NODEMANAGEMENT BASE_NODES BASE_TYPE_SYSTEM
    EXTENDED_NODEIDS NAMESPACES DYNAMIC_NODES COMPLEX_TYPES USER_AUTH MULTIPLE_CONNECTIONS
    SESSION_TIMEOUT SECURITY ECC MULTI_CHUNK TIME_SYNC AUDITING SUBSCRIPTIONS
    SUBSCRIPTIONS_STANDARD EVENTS EVENT_FILTER_WHERE AGGREGATE_FULL REDUNDANCY METHOD_SERVER
    CUSTOM_METHODS PUBSUB DATA_ACCESS REVERSE_CONNECT SERVER_DIAGNOSTICS READ_CACHE
    STANDARD_PROFILE)

# Legacy -D overrides are the ONLY cache entries for these symbols (option() is gone),
# so DEFINED CACHE{} reliably detects a user-supplied -DMUC_OPCUA_<X>=ON/OFF.
set(_MUC_OPCUA_KCFG_FRAG "${CMAKE_BINARY_DIR}/kconfig_overrides.config")
file(WRITE ${_MUC_OPCUA_KCFG_FRAG} "")
foreach(_sym IN LISTS MUC_OPCUA_KCONFIG_FEATURES)
    if(DEFINED CACHE{MUC_OPCUA_${_sym}})
        if(${MUC_OPCUA_${_sym}})
            file(APPEND ${_MUC_OPCUA_KCFG_FRAG} "MUC_OPCUA_${_sym}=y\n")
        else()
            file(APPEND ${_MUC_OPCUA_KCFG_FRAG} "# MUC_OPCUA_${_sym} is not set\n")
        endif()
    endif()
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_MUC_OPCUA_CONFIG_CMAKE "${CMAKE_BINARY_DIR}/muc_opcua_config.cmake")
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/kconfig/gen_config.py
            ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig
            ${CMAKE_CURRENT_SOURCE_DIR}/configs/${MUC_OPCUA_PROFILE}.defconfig
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig
    ${CMAKE_CURRENT_SOURCE_DIR}/configs/${MUC_OPCUA_PROFILE}.defconfig)

# ALLOW_HEAP is derived from the profile tier (MCU tiers are strictly no-heap per the
# constitution) -- not a Kconfig facet. option() below provides the default; force OFF
# for nano/micro/embedded.
if(MUC_OPCUA_PROFILE STREQUAL "nano" OR MUC_OPCUA_PROFILE STREQUAL "micro"
   OR MUC_OPCUA_PROFILE STREQUAL "embedded")
    set(MUC_OPCUA_ALLOW_HEAP OFF CACHE BOOL "" FORCE)
endif()

# SecurityPolicy Basic256Sha256 support (asym/sym chunk crypto, ~10 KB code).
# Turn OFF for a SecurityPolicy-None-only build to drop the crypto layer entirely.

# ECC SecurityPolicies (spec 059): ECC-curve25519 (Ed25519 + X25519 +
# ChaCha20-Poly1305) and ECC-nistP256 (ECDSA + P-256 + AES128-CBC/HMAC). Optional
# at every OPC UA server profile (the "Security ECC Policy" CU is optional); this
# implements it so the optional CU can be advertised. Requires MUC_OPCUA_SECURITY.

# Optional services — turn OFF to drop a service's code from a minimal build.
# (OpenSecureChannel / Session services are always present.)
# The library's standard Base Information node set (Server object hierarchy,
# ServerStatus, ServerCapabilities). Turn OFF if the integrator supplies a complete
# address space and does not want the default nodes.
# Data-change subscriptions (the Micro profile: Subscription + MonitoredItem service
# sets, the Embedded Data Change Subscription Server Facet). Turn OFF for a Nano-only
# build to drop the subscription engine and its fixed-size state entirely.
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
# above already set -- and had drifted out of sync. They are now folded into those
# blocks (the single source of truth). Direct -DMUC_OPCUA_{EMBEDDED,STANDARD}_PROFILE=ON
# use is promoted to the matching profile near the top of this file; illegal
# custom-profile combinations are still caught at compile time by features.h.


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
