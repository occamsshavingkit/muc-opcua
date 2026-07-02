# platform/pico/CMakeLists.txt
if(MUC_OPCUA_PLATFORM STREQUAL "pico")
    add_library(muc_opcua_pico STATIC
        mu_pico_adapter.c
    )

    # Feature 025 (F12): apply the same size discipline as the core library
    # (-Os + -ffunction-sections/-fdata-sections, --gc-sections exported to
    # consumers, optional LTO) so the Pico adapter and example do not inherit the
    # SDK default optimization and bloat flash.
    muc_opcua_apply_codegen(muc_opcua_pico)

    # PUBLIC so the public headers and pico_stdlib propagate to executables
    # (e.g. pico_minimal_server) that link this adapter library.
    target_link_libraries(muc_opcua_pico PUBLIC
        pico_stdlib
        muc_opcua
    )
    
    target_include_directories(muc_opcua_pico PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(MUC_OPCUA_BUILD_EXAMPLES)
        add_executable(pico_minimal_server pico_minimal_server.c)
        muc_opcua_apply_codegen(pico_minimal_server)
        target_link_libraries(pico_minimal_server PRIVATE
            muc_opcua_pico
        )
        pico_enable_stdio_usb(pico_minimal_server 1)
        pico_enable_stdio_uart(pico_minimal_server 0)
        pico_add_extra_outputs(pico_minimal_server)
    endif()
endif()
