# src/CMakeLists.txt
add_library(micro_opcua STATIC
    core/server.c
    core/status.c
    core/tcp_connection.c
    core/message_chunk.c
    core/sequence.c
    core/service_message.c
    core/service_dispatch.c
    core/uasc.c
    services/secure_channel.c
    services/session.c
    services/service_header.c
    services/discovery.c
    services/browse.c
    services/read.c
    address_space/address_space.c
    address_space/node_id.c
    address_space/value_source.c
    encoding/binary_reader.c
    encoding/binary_writer.c
    encoding/binary_string.c
    encoding/binary_nodeid.c
    encoding/binary_extension_object.c
    encoding/binary_variant.c
    encoding/binary_datavalue.c
    generated/opcua_ids.c
)

# The host TCP adapter uses POSIX sockets; only compile it for the host platform.
if(MICRO_OPCUA_IS_HOST)
    target_sources(micro_opcua PRIVATE platform/host_tcp_adapter.c)
endif()

target_include_directories(micro_opcua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

micro_opcua_apply_warnings(micro_opcua)
