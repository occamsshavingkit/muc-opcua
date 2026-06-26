# platform/pico/CMakeLists.txt
if(MICRO_OPCUA_PLATFORM STREQUAL "pico")
    add_library(micro_opcua_pico STATIC
        mu_pico_adapter.c
    )
    
    # PUBLIC so the public headers and pico_stdlib propagate to executables
    # (e.g. pico_minimal_server) that link this adapter library.
    target_link_libraries(micro_opcua_pico PUBLIC
        pico_stdlib
        micro_opcua
    )
    
    target_include_directories(micro_opcua_pico PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(MICRO_OPCUA_BUILD_EXAMPLES)
        add_executable(pico_minimal_server pico_minimal_server.c)
        target_link_libraries(pico_minimal_server PRIVATE
            micro_opcua_pico
        )
        pico_enable_stdio_usb(pico_minimal_server 1)
        pico_enable_stdio_uart(pico_minimal_server 0)
        pico_add_extra_outputs(pico_minimal_server)
    endif()
endif()
