# src/CMakeLists.txt
add_library(micro_opcua STATIC
    core/server.c
    core/status.c
)

target_include_directories(micro_opcua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

micro_opcua_apply_warnings(micro_opcua)
