cmake_minimum_required(VERSION 3.20)

# T001 Options
option(MICRO_OPCUA_BUILD_TESTS "Build tests" OFF)
option(MICRO_OPCUA_BUILD_EXAMPLES "Build examples" OFF)
option(MICRO_OPCUA_BUILD_FUZZERS "Build fuzzers" OFF)
# SecurityPolicy Basic256Sha256 support (asym/sym chunk crypto, ~10 KB code).
# Turn OFF for a SecurityPolicy-None-only build to drop the crypto layer entirely.
option(MICRO_OPCUA_SECURITY "Build SecurityPolicy Basic256Sha256 support" ON)

# Optional services — turn OFF to drop a service's code from a minimal build.
# (OpenSecureChannel / Session services are always present.)
option(MICRO_OPCUA_SERVICE_READ "Build the Read service" ON)
option(MICRO_OPCUA_SERVICE_BROWSE "Build the Browse + BrowseNext + TranslateBrowsePaths services" ON)
option(MICRO_OPCUA_SERVICE_DISCOVERY "Build GetEndpoints/FindServers" ON)
option(MICRO_OPCUA_SERVICE_REGISTER_NODES "Build RegisterNodes/UnregisterNodes" ON)
option(MICRO_OPCUA_SERVICE_WRITE "Build the optional Write service (Value attribute only)" ON)
option(MICRO_OPCUA_SERVICE_QUERY "Build the Query services" ON)
option(MICRO_OPCUA_SERVICE_NODEMANAGEMENT "Build the optional NodeManagement service set" ON)
option(MICRO_OPCUA_SERVICE_HISTORY "Build Historical Access" ON)
# The library's standard Base Information node set (Server object hierarchy,
# ServerStatus, ServerCapabilities). Turn OFF if the integrator supplies a complete
# address space and does not want the default nodes.
option(MICRO_OPCUA_BASE_NODES "Build the standard Base Information node set" ON)
option(MICRO_OPCUA_BASE_TYPE_SYSTEM "Expose the Base Info Type System node set used by the embedded profile" OFF)
# Data-change subscriptions (the Micro profile: Subscription + MonitoredItem service
# sets, the Embedded Data Change Subscription Server Facet). Turn OFF for a Nano-only
# build to drop the subscription engine and its fixed-size state entirely.
option(MICRO_OPCUA_SUBSCRIPTIONS "Build the data-change subscription engine (Micro profile)" ON)
option(MICRO_OPCUA_SUBSCRIPTIONS_STANDARD "Build the Standard DataChange Subscription 2017 facet additions" OFF)
option(MICRO_OPCUA_EMBEDDED_PROFILE "Apply Embedded 2017 profile gates and capacity minima" OFF)
option(MICRO_OPCUA_PUBSUB "Build Publish/Subscribe capabilities" ON)
set(MICRO_OPCUA_SANITIZERS "" CACHE STRING "Comma-separated list of sanitizers (e.g. address,undefined)")
set(MICRO_OPCUA_PLATFORM "host" CACHE STRING "Target platform (host, pico, arduino-skeleton)")

if(MICRO_OPCUA_EMBEDDED_PROFILE)
    set(MICRO_OPCUA_SECURITY ON CACHE BOOL "Build SecurityPolicy Basic256Sha256 support" FORCE)
    set(MICRO_OPCUA_SUBSCRIPTIONS ON CACHE BOOL "Build the data-change subscription engine (Micro profile)" FORCE)
    set(MICRO_OPCUA_SUBSCRIPTIONS_STANDARD ON CACHE BOOL "Build the Standard DataChange Subscription 2017 facet additions" FORCE)
    set(MICRO_OPCUA_BASE_NODES ON CACHE BOOL "Build the standard Base Information node set" FORCE)
    set(MICRO_OPCUA_BASE_TYPE_SYSTEM ON CACHE BOOL "Expose the Base Info Type System node set used by the embedded profile" FORCE)
    if(NOT DEFINED MU_MAX_SUBSCRIPTIONS)
        set(MU_MAX_SUBSCRIPTIONS 2 CACHE STRING "Embedded profile minimum subscriptions")
    endif()
    if(NOT DEFINED MU_MAX_MONITORED_ITEMS)
        set(MU_MAX_MONITORED_ITEMS 100 CACHE STRING "Embedded profile minimum monitored items")
    endif()
    if(NOT DEFINED MU_MAX_PUBLISH_REQUESTS)
        set(MU_MAX_PUBLISH_REQUESTS 5 CACHE STRING "Embedded profile minimum parallel Publish requests")
    endif()
    if(NOT DEFINED MU_MONITORED_QUEUE_DEPTH)
        set(MU_MONITORED_QUEUE_DEPTH 2 CACHE STRING "Embedded profile minimum monitored-item queue depth")
    endif()
    if(NOT DEFINED MU_MAX_TRIGGER_LINKS)
        set(MU_MAX_TRIGGER_LINKS 4 CACHE STRING "Embedded profile triggering link capacity")
    endif()
endif()

# The Pico SDK installs its cross-compilation toolchain, so it must be imported
# BEFORE project(). Requires PICO_SDK_PATH to point at a Pico SDK checkout.
if(MICRO_OPCUA_PLATFORM STREQUAL "pico")
    include(pico_sdk_import.cmake)
endif()

project(micro_opcua VERSION 0.1.0 LANGUAGES C)

# The Pico SDK builds C++/assembly support code and must be initialized after project().
if(MICRO_OPCUA_PLATFORM STREQUAL "pico")
    enable_language(CXX ASM)
    pico_sdk_init()
endif()

include(CTest)

# Phase 1 setup will include other cmake files
include(cmake/MicroOpcUaOptions.cmake)
include(cmake/MicroOpcUaWarnings.cmake)
include(cmake/MicroOpcUaCodegen.cmake)
include(cmake/MicroOpcUaStaticAnalysis.cmake)

add_subdirectory(src)

if(MICRO_OPCUA_STACK_USAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(micro_opcua PRIVATE -fstack-usage)
    else()
        message(FATAL_ERROR "MICRO_OPCUA_STACK_USAGE requires a C compiler that supports -fstack-usage")
    endif()
endif()

if(MICRO_OPCUA_BUILD_TESTS OR MICRO_OPCUA_BUILD_FUZZERS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MICRO_OPCUA_STACK_USAGE)
    add_custom_target(check_stack_usage
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_stack_usage.sh
            --build-dir ${CMAKE_BINARY_DIR}
            --threshold ${MICRO_OPCUA_STACK_USAGE_LIMIT}
        COMMENT "Checking secured-OPN stack usage budget"
        VERBATIM
    )

    if(BUILD_TESTING)
        add_test(NAME test_stack_budget
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_stack_usage.sh
                --build-dir ${CMAKE_BINARY_DIR}
                --threshold ${MICRO_OPCUA_STACK_USAGE_LIMIT}
        )
    endif()
endif()

if(MICRO_OPCUA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MICRO_OPCUA_PLATFORM STREQUAL "pico")
    add_subdirectory(platform/pico)
endif()
