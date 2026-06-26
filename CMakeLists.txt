cmake_minimum_required(VERSION 3.20)

# T001 Options
option(MICRO_OPCUA_BUILD_TESTS "Build tests" OFF)
option(MICRO_OPCUA_BUILD_EXAMPLES "Build examples" OFF)
option(MICRO_OPCUA_BUILD_FUZZERS "Build fuzzers" OFF)
set(MICRO_OPCUA_SANITIZERS "" CACHE STRING "Comma-separated list of sanitizers (e.g. address,undefined)")
set(MICRO_OPCUA_PLATFORM "host" CACHE STRING "Target platform (host, pico, arduino-skeleton)")

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

# Phase 1 setup will include other cmake files
include(cmake/MicroOpcUaOptions.cmake)
include(cmake/MicroOpcUaWarnings.cmake)
include(cmake/MicroOpcUaStaticAnalysis.cmake)

add_subdirectory(src)

if(MICRO_OPCUA_BUILD_TESTS OR MICRO_OPCUA_BUILD_FUZZERS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MICRO_OPCUA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MICRO_OPCUA_PLATFORM STREQUAL "pico")
    add_subdirectory(platform/pico)
endif()
