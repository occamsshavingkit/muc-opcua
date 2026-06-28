# MicroOpcUaOptions.cmake

if(NOT DEFINED MICRO_OPCUA_PLATFORM)
    set(MICRO_OPCUA_PLATFORM "host" CACHE STRING "Target platform (host, pico, arduino-skeleton)")
endif()

option(MICRO_OPCUA_LTO "Enable link-time optimization (interprocedural optimization) for size-optimized firmware builds" OFF)
option(MICRO_OPCUA_STACK_USAGE "Measure secured-OPN worst-case stack usage" OFF)
set(MICRO_OPCUA_STACK_USAGE_LIMIT 10240 CACHE STRING "Maximum secured-OPN stack usage budget in bytes")

message(STATUS "Configuring Micro OPC UA for platform: ${MICRO_OPCUA_PLATFORM}")

if(MICRO_OPCUA_PLATFORM STREQUAL "host")
    # Host build specific options
    set(MICRO_OPCUA_IS_HOST ON)
    set(MICRO_OPCUA_IS_PICO OFF)
    set(MICRO_OPCUA_IS_ARDUINO OFF)
elseif(MICRO_OPCUA_PLATFORM STREQUAL "pico")
    # Pico build specific options
    set(MICRO_OPCUA_IS_HOST OFF)
    set(MICRO_OPCUA_IS_PICO ON)
    set(MICRO_OPCUA_IS_ARDUINO OFF)
elseif(MICRO_OPCUA_PLATFORM STREQUAL "arduino-skeleton")
    # Arduino build specific options
    set(MICRO_OPCUA_IS_HOST OFF)
    set(MICRO_OPCUA_IS_PICO OFF)
    set(MICRO_OPCUA_IS_ARDUINO ON)
endif()

option(MICRO_OPCUA_USER_AUTH "Build UserName/Password user identity authentication" ON)
option(MICRO_OPCUA_SERVICE_WRITE "Build the Write service" ON)
option(MICRO_OPCUA_MULTIPLE_CONNECTIONS "Support concurrent client TCP connections" ON)
option(MICRO_OPCUA_EVENTS "Build event and Alarm & Condition support" ON)
option(MICRO_OPCUA_HAVE_MBEDTLS "Build with Mbed TLS cryptographic platform adapter" OFF)
option(MICRO_OPCUA_HAVE_WOLFSSL "Build with wolfSSL cryptographic platform adapter" OFF)
option(MICRO_OPCUA_CUSTOM_METHODS "Build support for custom method calls" ON)
option(MICRO_OPCUA_SERVER_DIAGNOSTICS "Build support for server diagnostics summary nodes" ON)
option(MICRO_OPCUA_DYNAMIC_NODES "Build support for AddNodes/DeleteNodes dynamic node management" ON)


