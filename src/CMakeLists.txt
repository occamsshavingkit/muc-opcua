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
if(MICRO_OPCUA_SERVICE_WRITE)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_WRITE=1)
endif()
if(MICRO_OPCUA_SERVICE_NODEMANAGEMENT)
    target_sources(micro_opcua PRIVATE services/node_management.c)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_NODEMANAGEMENT=1)
endif()
if(MICRO_OPCUA_SERVICE_QUERY)
    target_sources(micro_opcua PRIVATE services/query.c encoding/binary_query.c)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_QUERY=1)
endif()
# base_nodes.c is always compiled (it owns the always-available mu_resolve_node);
# this flag gates the node-table *content* so the standard node set is dropped.
if(MICRO_OPCUA_BASE_NODES)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_BASE_NODES=1)
    if(MICRO_OPCUA_BASE_TYPE_SYSTEM)
        target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_BASE_TYPE_SYSTEM=1)
    endif()
endif()

# The data-change subscription engine (Micro profile). Source + dispatch handlers are
# gated under MICRO_OPCUA_SUBSCRIPTIONS so a Nano build drops them at the source level.
if(MICRO_OPCUA_SUBSCRIPTIONS)
    target_sources(micro_opcua PRIVATE services/subscription.c)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SUBSCRIPTIONS=1)
    if(MICRO_OPCUA_SUBSCRIPTIONS_STANDARD)
        target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SUBSCRIPTIONS_STANDARD=1)
    endif()
endif()

if(MICRO_OPCUA_PUBSUB)
    target_sources(micro_opcua PRIVATE
        core/pubsub.c
        encoding/uadp_encoder.c
    )
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_PUBSUB=1)
    if(MICRO_OPCUA_IS_HOST)
        target_sources(micro_opcua PRIVATE platform/host_udp_adapter.c)
    endif()
endif()

if(MICRO_OPCUA_USER_AUTH)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_USER_AUTH=1)
endif()

if(MICRO_OPCUA_SERVICE_WRITE)
    target_sources(micro_opcua PRIVATE services/write.c)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVICE_WRITE=1)
endif()

if(MICRO_OPCUA_MULTIPLE_CONNECTIONS)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_MULTIPLE_CONNECTIONS=1)
endif()

if(MICRO_OPCUA_EVENTS)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_EVENTS=1)
endif()

if(MICRO_OPCUA_CUSTOM_METHODS)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_CUSTOM_METHODS=1)
endif()

if(MICRO_OPCUA_SERVER_DIAGNOSTICS)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_SERVER_DIAGNOSTICS=1)
endif()

if(MICRO_OPCUA_DYNAMIC_NODES)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_DYNAMIC_NODES=1)
endif()


foreach(MU_CAPACITY_DEFINE
        MU_MAX_SUBSCRIPTIONS
        MU_MAX_MONITORED_ITEMS
        MU_MAX_PUBLISH_REQUESTS
        MU_MONITORED_QUEUE_DEPTH
        MU_MAX_TRIGGER_LINKS)
    if(DEFINED ${MU_CAPACITY_DEFINE})
        target_compile_definitions(micro_opcua PUBLIC ${MU_CAPACITY_DEFINE}=${${MU_CAPACITY_DEFINE}})
    endif()
endforeach()

# SecurityPolicy Basic256Sha256: the asym/sym chunk crypto and key derivation.
# Excluded entirely from a None-only build to keep the embedded footprint minimal.
# (security_policy.c stays in the core: it only provides shared policy/mode enums.)
if(MICRO_OPCUA_SECURITY)
    target_sources(micro_opcua PRIVATE
        security/key_derivation.c
        security/certificate.c
        security/asym_chunk.c
        security/sym_chunk.c
        security/trustlist.c
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

if(MICRO_OPCUA_HAVE_MBEDTLS)
    target_sources(micro_opcua PRIVATE platform/mbedtls_crypto_adapter.c)
    target_link_libraries(micro_opcua PUBLIC mbedcrypto mbedx509)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_HAVE_MBEDTLS=1)
endif()

if(MICRO_OPCUA_HAVE_WOLFSSL)
    target_sources(micro_opcua PRIVATE platform/wolfssl_crypto_adapter.c)
    target_link_libraries(micro_opcua PUBLIC wolfssl)
    target_compile_definitions(micro_opcua PUBLIC MICRO_OPCUA_HAVE_WOLFSSL=1)
endif()

target_include_directories(micro_opcua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

micro_opcua_apply_warnings(micro_opcua)
micro_opcua_apply_codegen(micro_opcua)
