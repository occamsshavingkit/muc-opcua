cmake_minimum_required(VERSION 3.20)
project(micro_opcua VERSION 0.1.0 LANGUAGES C)

# T001 Options
option(MICRO_OPCUA_BUILD_TESTS "Build tests" OFF)
option(MICRO_OPCUA_BUILD_EXAMPLES "Build examples" OFF)
option(MICRO_OPCUA_BUILD_FUZZERS "Build fuzzers" OFF)
set(MICRO_OPCUA_SANITIZERS "" CACHE STRING "Comma-separated list of sanitizers (e.g. address,undefined)")
set(MICRO_OPCUA_PLATFORM "host" CACHE STRING "Target platform (host, pico, arduino-skeleton)")

# Phase 1 setup will include other cmake files
include(cmake/MicroOpcUaOptions.cmake)
# include(cmake/MicroOpcUaWarnings.cmake)
# include(cmake/MicroOpcUaStaticAnalysis.cmake)

# if(MICRO_OPCUA_BUILD_TESTS OR MICRO_OPCUA_BUILD_FUZZERS)
#   add_subdirectory(tests)
# endif()
