# MucOpcUaOptions.cmake

if(NOT DEFINED MUC_OPCUA_PLATFORM)
    set(MUC_OPCUA_PLATFORM "host" CACHE STRING "Target platform (host, external, pico, arduino-skeleton)")
endif()

option(MUC_OPCUA_LTO "Enable link-time optimization (interprocedural optimization) for size-optimized firmware builds" OFF)
option(MUC_OPCUA_STACK_USAGE "Measure secured-OPN worst-case stack usage" OFF)
set(MUC_OPCUA_STACK_USAGE_LIMIT 10240 CACHE STRING "Maximum secured-OPN stack usage budget in bytes")

message(STATUS "Configuring muc-opcua for platform: ${MUC_OPCUA_PLATFORM}")

if(MUC_OPCUA_PLATFORM STREQUAL "host")
    # Host build specific options
    set(MUC_OPCUA_IS_HOST ON)
    set(MUC_OPCUA_IS_PICO OFF)
    set(MUC_OPCUA_IS_ARDUINO OFF)
elseif(MUC_OPCUA_PLATFORM STREQUAL "pico")
    # Pico build specific options
    set(MUC_OPCUA_IS_HOST OFF)
    set(MUC_OPCUA_IS_PICO ON)
    set(MUC_OPCUA_IS_ARDUINO OFF)
elseif(MUC_OPCUA_PLATFORM STREQUAL "external")
    # Application-owned platform integration; do not compile in-tree adapters.
    set(MUC_OPCUA_IS_HOST OFF)
    set(MUC_OPCUA_IS_PICO OFF)
    set(MUC_OPCUA_IS_ARDUINO OFF)
elseif(MUC_OPCUA_PLATFORM STREQUAL "arduino-skeleton")
    # Arduino build specific options
    set(MUC_OPCUA_IS_HOST OFF)
    set(MUC_OPCUA_IS_PICO OFF)
    set(MUC_OPCUA_IS_ARDUINO ON)
else()
    message(FATAL_ERROR "Unsupported MUC_OPCUA_PLATFORM: ${MUC_OPCUA_PLATFORM}")
endif()

option(MUC_OPCUA_USER_AUTH "Build UserName/Password user identity authentication" OFF)
option(MUC_OPCUA_SERVICE_WRITE "Build the Write service" OFF)
option(MUC_OPCUA_MULTIPLE_CONNECTIONS "Support concurrent client TCP connections" OFF)
option(MUC_OPCUA_EVENTS "Build event and Alarm & Condition support" OFF)
option(MUC_OPCUA_HAVE_MBEDTLS "Build with Mbed TLS cryptographic platform adapter" OFF)
option(MUC_OPCUA_HAVE_WOLFSSL "Build with wolfSSL cryptographic platform adapter" OFF)
option(MUC_OPCUA_CUSTOM_METHODS "Build support for custom method calls" OFF)
option(MUC_OPCUA_SERVER_DIAGNOSTICS "Build support for server diagnostics summary nodes" OFF)
option(MUC_OPCUA_DYNAMIC_NODES "Build support for AddNodes/DeleteNodes dynamic node management" OFF)

