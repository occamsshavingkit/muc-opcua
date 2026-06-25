# cmake/MicroOpcUaWarnings.cmake

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(MICRO_OPCUA_C_WARNINGS "")

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
    list(APPEND MICRO_OPCUA_C_WARNINGS
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wcast-align
        -Wunused
        -Wformat=2
    )
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    list(APPEND MICRO_OPCUA_C_WARNINGS
        /W4
        /WX
    )
endif()

function(micro_opcua_apply_warnings target_name)
    target_compile_options(${target_name} PRIVATE ${MICRO_OPCUA_C_WARNINGS})
endfunction()
