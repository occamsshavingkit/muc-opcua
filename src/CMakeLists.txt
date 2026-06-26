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
    address_space/address_space.c
    address_space/base_nodes.c
    address_space/node_id.c
    address_space/value_source.c
    encoding/binary_reader.c
    encoding/binary_writer.c
    encoding/binary_string.c
    encoding/binary_nodeid.c
    encoding/binary_extension_object.c
    encoding/binary_variant.c
    encoding/binary_datavalue.c
    security/security_policy.c
    generated/opcua_ids.c
)

# Optional services compile out at the source + dispatch-table level.
if(MICRO_OPCUA_SERVICE_READ)
    target_sources(micro_opcua PRIVATE services/read.c)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_READ=1)
endif()
if(MICRO_OPCUA_SERVICE_BROWSE)
    target_sources(micro_opcua PRIVATE services/browse.c)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_BROWSE=1)
endif()
if(MICRO_OPCUA_SERVICE_DISCOVERY)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_DISCOVERY=1)
endif()
if(MICRO_OPCUA_SERVICE_REGISTER_NODES)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_REGISTER_NODES=1)
endif()
# base_nodes.c is always compiled (it owns the always-available mu_resolve_node);
# this flag gates the node-table *content* so the standard node set is dropped.
if(MICRO_OPCUA_BASE_NODES)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_BASE_NODES=1)
endif()

# SecurityPolicy Basic256Sha256: the asym/sym chunk crypto and key derivation.
# Excluded entirely from a None-only build to keep the embedded footprint minimal.
# (security_policy.c stays in the core: it only provides shared policy/mode enums.)
if(MICRO_OPCUA_SECURITY)
    target_sources(micro_opcua PRIVATE
        security/key_derivation.c
        security/certificate.c
        security/asym_chunk.c
        security/sym_chunk.c
    )
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SECURITY=1)
endif()

# The host TCP adapter uses POSIX sockets; only compile it for the host platform.
if(MICRO_OPCUA_IS_HOST)
    target_sources(micro_opcua PRIVATE platform/host_tcp_adapter.c)

    # The host crypto adapter (SecurityPolicy Basic256Sha256) is backed by OpenSSL.
    if(MICRO_OPCUA_SECURITY)
        find_package(OpenSSL)
        if(OpenSSL_FOUND)
            target_sources(micro_opcua PRIVATE platform/host_crypto_adapter.c)
            target_link_libraries(micro_opcua PUBLIC OpenSSL::Crypto)
            target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_HAVE_OPENSSL=1)
        endif()
    endif()
endif()

target_include_directories(micro_opcua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

micro_opcua_apply_warnings(micro_opcua)
micro_opcua_apply_codegen(micro_opcua)
