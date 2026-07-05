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

if(NOT MUC_OPCUA_PROFILE STREQUAL "custom")
    foreach(MUC_OPCUA_PROFILE_OPTION IN LISTS MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS)
        set(${MUC_OPCUA_PROFILE_OPTION} OFF CACHE BOOL "" FORCE)
    endforeach()
endif()

if(MUC_OPCUA_PROFILE STREQUAL "nano")
    # Nano: keep everything OFF (options default to OFF)
elseif(MUC_OPCUA_PROFILE STREQUAL "micro")
    set(MUC_OPCUA_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_USER_AUTH ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_WRITE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_MULTI_CHUNK ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EXTENDED_NODEIDS ON CACHE BOOL "" FORCE)
elseif(MUC_OPCUA_PROFILE STREQUAL "embedded")
    set(MUC_OPCUA_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_USER_AUTH ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_WRITE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SECURITY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_TYPE_SYSTEM ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_MULTIPLE_CONNECTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EVENTS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EMBEDDED_PROFILE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_MULTI_CHUNK ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EXTENDED_NODEIDS ON CACHE BOOL "" FORCE)
elseif(MUC_OPCUA_PROFILE STREQUAL "standard")
    set(MUC_OPCUA_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_USER_AUTH ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_WRITE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SECURITY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_TYPE_SYSTEM ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_MULTIPLE_CONNECTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EVENTS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EMBEDDED_PROFILE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_REGISTER_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_HISTORY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_QUERY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_NODEMANAGEMENT ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_DYNAMIC_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_PUBSUB ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_CUSTOM_METHODS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVER_DIAGNOSTICS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_MULTI_CHUNK ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EXTENDED_NODEIDS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_DATA_ACCESS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_METHOD_SERVER ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EVENT_FILTER_WHERE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_AUDITING ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_COMPLEX_TYPES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_REDUNDANCY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_AGGREGATE_FULL ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_REVERSE_CONNECT ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_TIME_SYNC ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_NAMESPACES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_STANDARD_PROFILE ON CACHE BOOL "" FORCE)
    if(NOT DEFINED MU_MAX_SESSIONS)
        set(MU_MAX_SESSIONS 50 CACHE STRING "Standard profile minimum sessions" FORCE)
    endif()
    if(NOT DEFINED MU_MAX_SUBSCRIPTIONS)
        set(MU_MAX_SUBSCRIPTIONS 50 CACHE STRING "Standard profile minimum subscriptions" FORCE)
    endif()
    if(NOT DEFINED MU_MAX_MONITORED_ITEMS)
        set(MU_MAX_MONITORED_ITEMS 1000 CACHE STRING "Standard profile minimum monitored items" FORCE)
    endif()
    if(NOT DEFINED MU_MAX_PUBLISH_REQUESTS)
        set(MU_MAX_PUBLISH_REQUESTS 50 CACHE STRING "Standard profile minimum parallel Publish requests" FORCE)
    endif()
elseif(MUC_OPCUA_PROFILE STREQUAL "full")
    set(MUC_OPCUA_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_USER_AUTH ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_WRITE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SECURITY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_TYPE_SYSTEM ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_MULTIPLE_CONNECTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EVENTS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EMBEDDED_PROFILE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_REGISTER_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_HISTORY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_QUERY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVICE_NODEMANAGEMENT ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_DYNAMIC_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_PUBSUB ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_CUSTOM_METHODS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SERVER_DIAGNOSTICS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_MULTI_CHUNK ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EXTENDED_NODEIDS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_DATA_ACCESS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_METHOD_SERVER ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_EVENT_FILTER_WHERE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_AUDITING ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_COMPLEX_TYPES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_REDUNDANCY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_AGGREGATE_FULL ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_REVERSE_CONNECT ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_TIME_SYNC ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_NAMESPACES ON CACHE BOOL "" FORCE)
endif()

# Full profile: "everything on" with generous capacities. Not an OPC UA profile,
# just a kitchen-sink build target with higher limits than the OPC standard minima.
# Runs AFTER the STANDARD_PROFILE enforcement block so it can force-override.
if(MUC_OPCUA_PROFILE STREQUAL "full")
    set(MU_MAX_SESSIONS 100 CACHE STRING "Full profile session capacity" FORCE)
    set(MU_MAX_SUBSCRIPTIONS 100 CACHE STRING "Full profile subscription capacity" FORCE)
    set(MU_MAX_MONITORED_ITEMS 2000 CACHE STRING "Full profile monitored item capacity" FORCE)
    set(MU_MAX_PUBLISH_REQUESTS 100 CACHE STRING "Full profile parallel Publish capacity" FORCE)
endif()

# SecurityPolicy Basic256Sha256 support (asym/sym chunk crypto, ~10 KB code).
# Turn OFF for a SecurityPolicy-None-only build to drop the crypto layer entirely.
option(MUC_OPCUA_SECURITY "Build SecurityPolicy Basic256Sha256 support" OFF)

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
option(MUC_OPCUA_SESSION_TIMEOUT "Enable session timeout enforcement" OFF)
set(MUC_OPCUA_SANITIZERS "" CACHE STRING "Comma-separated list of sanitizers (e.g. address,undefined)")
if(MUC_OPCUA_SANITIZERS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${MUC_OPCUA_SANITIZERS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${MUC_OPCUA_SANITIZERS}")
endif()
set(MUC_OPCUA_PLATFORM "host" CACHE STRING "Target platform (host, external, pico, arduino-skeleton)")

if(MUC_OPCUA_STANDARD_PROFILE)
    if(DEFINED MUC_OPCUA_PROFILE AND NOT MUC_OPCUA_PROFILE STREQUAL "")
        if(NOT MUC_OPCUA_PROFILE STREQUAL "standard" AND NOT MUC_OPCUA_PROFILE STREQUAL "full")
            message(FATAL_ERROR "MUC_OPCUA_STANDARD_PROFILE requires MUC_OPCUA_PROFILE=standard or full, not '${MUC_OPCUA_PROFILE}'")
        endif()
    endif()
    set(MUC_OPCUA_SECURITY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_NODES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_BASE_TYPE_SYSTEM ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_DATA_ACCESS ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_METHOD_SERVER ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_CUSTOM_METHODS ON CACHE BOOL "" FORCE) # METHOD_SERVER uses CUSTOM_METHODS infra
    set(MUC_OPCUA_EVENT_FILTER_WHERE ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_AUDITING ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_COMPLEX_TYPES ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_REDUNDANCY ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_AGGREGATE_FULL ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_REVERSE_CONNECT ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_TIME_SYNC ON CACHE BOOL "" FORCE)
    set(MUC_OPCUA_NAMESPACES ON CACHE BOOL "" FORCE)
    if(NOT DEFINED MU_MAX_SESSIONS)
        set(MU_MAX_SESSIONS 50 CACHE STRING "Standard profile minimum sessions" FORCE)
    endif()
    if(NOT DEFINED MU_MAX_SUBSCRIPTIONS)
        set(MU_MAX_SUBSCRIPTIONS 50 CACHE STRING "Standard profile minimum subscriptions" FORCE)
    endif()
    if(NOT DEFINED MU_MAX_MONITORED_ITEMS)
        set(MU_MAX_MONITORED_ITEMS 1000 CACHE STRING "Standard profile minimum monitored items" FORCE)
    endif()
    if(NOT DEFINED MU_MAX_PUBLISH_REQUESTS)
        set(MU_MAX_PUBLISH_REQUESTS 50 CACHE STRING "Standard profile minimum parallel Publish requests" FORCE)
    endif()
endif()

if(MUC_OPCUA_EMBEDDED_PROFILE)
    set(MUC_OPCUA_SECURITY ON CACHE BOOL "Build SecurityPolicy Basic256Sha256 support" FORCE)
    set(MUC_OPCUA_SUBSCRIPTIONS ON CACHE BOOL "Build the data-change subscription engine (Micro profile)" FORCE)
    set(MUC_OPCUA_SUBSCRIPTIONS_STANDARD ON CACHE BOOL "Build the Standard DataChange Subscription 2017 facet additions" FORCE)
    set(MUC_OPCUA_BASE_NODES ON CACHE BOOL "Build the standard Base Information node set" FORCE)
    set(MUC_OPCUA_BASE_TYPE_SYSTEM ON CACHE BOOL "Expose the Base Info Type System node set used by the embedded profile" FORCE)
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
