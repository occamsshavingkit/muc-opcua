# src/CMakeLists.txt
add_library(micro_opcua STATIC
    core/server.c
    core/status.c
    address_space/address_space.c
    address_space/node_id.c
    address_space/value_source.c
    encoding/binary_reader.c
    encoding/binary_writer.c
    encoding/binary_string.c
    encoding/binary_nodeid.c
    encoding/binary_extension_object.c
)

target_include_directories(micro_opcua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

micro_opcua_apply_warnings(micro_opcua)
