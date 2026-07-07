# src/CMakeLists.txt
add_library(muc_opcua STATIC
    core/server/init.c
    core/server/poll.c
    core/server/data_chunk.c
    core/server/process_message.c
    core/server/dispatch.c
    core/status.c
    core/tcp_connection.c
    core/message_chunk.c
    core/sequence.c
    core/service_message.c
    core/service_dispatch/dispatch_core.c
    core/service_dispatch/response_encode.c
    core/service_dispatch/reader_helpers.c
    core/service_dispatch/osc_handler.c
    core/service_dispatch/handler_stubs.c
    core/service_dispatch/create_session.c
    core/service_dispatch/activate_session.c
    core/service_dispatch/close_session.c
    core/uasc.c
    services/secure_channel.c
    services/session.c
    services/service_header.c
    services/discovery.c
    services/alarms_conditions.c
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
    encoding/variant_type.c
    security/security_policy.c
    generated/opcua_ids.c
)

if(MUC_OPCUA_ALLOW_HEAP)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_ALLOW_HEAP=1)
else()
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_ALLOW_HEAP=0)
endif()

# Optional services compile out at the source + dispatch-table level.
if(MUC_OPCUA_SERVICE_READ)
    target_sources(muc_opcua PRIVATE services/read/cache.c services/read/read_attribute.c services/read/read_process.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_READ=1)
endif()
if(MUC_OPCUA_SERVICE_BROWSE)
    target_sources(muc_opcua PRIVATE services/browse/browse.c services/browse/browse_next.c services/browse/translate_paths.c core/service_dispatch/view_handler.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_BROWSE=1)
endif()
if(MUC_OPCUA_SERVICE_DISCOVERY)
    target_sources(muc_opcua PRIVATE
        core/dispatch_discovery/common.c
        core/dispatch_discovery/get_endpoints.c
        core/dispatch_discovery/find_servers.c
        core/dispatch_discovery/register_server.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_DISCOVERY=1)
endif()
if(MUC_OPCUA_SERVICE_REGISTER_NODES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_REGISTER_NODES=1)
endif()
if(MUC_OPCUA_SERVICE_WRITE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_WRITE=1)
endif()
# Read/Write attribute dispatch handlers. dispatch_attribute.c carries both
# sets internally (#ifdef MUC_OPCUA_SERVICE_READ / ..._WRITE), so compile it
# whenever either service is enabled.
if(MUC_OPCUA_SERVICE_READ OR MUC_OPCUA_SERVICE_WRITE)
    target_sources(muc_opcua PRIVATE core/service_dispatch/attribute_handler.c)
endif()
if(MUC_OPCUA_SERVICE_NODEMANAGEMENT)
    target_sources(muc_opcua PRIVATE services/node_management/add_nodes.c services/node_management/add_references.c services/node_management/delete.c core/dispatch_node_mgmt.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_NODEMANAGEMENT=1)
endif()
if(MUC_OPCUA_SERVICE_QUERY)
    target_sources(muc_opcua PRIVATE services/query.c encoding/binary_query.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_QUERY=1)
endif()
# base_nodes.c is always compiled (it owns the always-available mu_resolve_node);
# this flag gates the node-table *content* so the standard node set is dropped.
if(MUC_OPCUA_BASE_NODES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_BASE_NODES=1)
endif()
if(MUC_OPCUA_BASE_TYPE_SYSTEM)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_BASE_TYPE_SYSTEM=1)
endif()

# The data-change subscription engine (Micro profile). Source + dispatch handlers are
# gated under MUC_OPCUA_SUBSCRIPTIONS so a Nano build drops them at the source level.
if(MUC_OPCUA_SUBSCRIPTIONS)
    target_sources(muc_opcua PRIVATE
        services/subscription_monitor.c
        services/subscription_publish/publish_due.c
        services/subscription_publish/tick.c
        services/subscription_publish/notification.c
        services/subscription_publish/deadband.c
        services/subscription_publish/retransmit.c
        core/service_dispatch/subscription_crud.c
        core/service_dispatch/monitored_items.c
        core/service_dispatch/filter_reader.c
        core/service_dispatch/subscription_helpers.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SUBSCRIPTIONS=1)
    if(MUC_OPCUA_SUBSCRIPTIONS_STANDARD)
        target_sources(muc_opcua PRIVATE services/subscription_aggregate.c core/service_dispatch/triggering.c)
        target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SUBSCRIPTIONS_STANDARD=1)
    endif()
endif()

# Call service dispatch (OPC-10000-4 5.12.2.2). The Call handler is gated by
# the same compound condition as the original MU_DISPATCH_CALL_ENABLED macro
# in service_dispatch.c: Subscriptions + STANDARD aggregates + BaseInfo
# type system. The file also wraps itself in the matching #if internally.
if(MUC_OPCUA_SUBSCRIPTIONS AND MUC_OPCUA_SUBSCRIPTIONS_STANDARD AND MUC_OPCUA_BASE_TYPE_SYSTEM)
    target_sources(muc_opcua PRIVATE core/dispatch_method.c)
endif()

if(MUC_OPCUA_PUBSUB)
    target_sources(muc_opcua PRIVATE
        core/pubsub.c
        encoding/uadp_encoder.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_PUBSUB=1)
    if(MUC_OPCUA_IS_HOST)
        target_sources(muc_opcua PRIVATE platform/host_udp_adapter.c)
    endif()
endif()

if(MUC_OPCUA_USER_AUTH)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_USER_AUTH=1)
endif()

if(MUC_OPCUA_SERVICE_WRITE)
    target_sources(muc_opcua PRIVATE services/write.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_WRITE=1)
endif()

if(MUC_OPCUA_SERVICE_HISTORY)
    target_sources(muc_opcua PRIVATE services/history.c core/service_dispatch/history_handler.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_HISTORY=1)
endif()

if(MUC_OPCUA_MULTIPLE_CONNECTIONS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_MULTIPLE_CONNECTIONS=1)
endif()

if(MUC_OPCUA_EVENTS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_EVENTS=1)
endif()

if(MUC_OPCUA_DATA_ACCESS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_DATA_ACCESS=1)
endif()

if(MUC_OPCUA_METHOD_SERVER)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_METHOD_SERVER=1)
endif()

if(MUC_OPCUA_EVENT_FILTER_WHERE)
    target_sources(muc_opcua PRIVATE services/event_filter.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_EVENT_FILTER_WHERE=1)
endif()

if(MUC_OPCUA_CUSTOM_METHODS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CUSTOM_METHODS=1)
endif()

if(MUC_OPCUA_REDUNDANCY)
    target_sources(muc_opcua PRIVATE core/service_dispatch/transfer_handler.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_REDUNDANCY=1)
endif()

if(MUC_OPCUA_NAMESPACES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_NAMESPACES=1)
endif()

if(MUC_OPCUA_STANDARD_PROFILE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_STANDARD_PROFILE=1)
endif()

if(MUC_OPCUA_REVERSE_CONNECT)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_REVERSE_CONNECT=1)
endif()

if(MUC_OPCUA_SERVER_DIAGNOSTICS)
    target_sources(muc_opcua PRIVATE services/diagnostics.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVER_DIAGNOSTICS=1)
endif()

if(MUC_OPCUA_DYNAMIC_NODES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_DYNAMIC_NODES=1)
endif()

if(MUC_OPCUA_MULTI_CHUNK)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_MULTI_CHUNK=1)
endif()

if(MUC_OPCUA_SESSION_TIMEOUT)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SESSION_TIMEOUT=1)
endif()

if(MUC_OPCUA_TIME_SYNC)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_TIME_SYNC=1)
endif()

if(MUC_OPCUA_EXTENDED_NODEIDS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_EXTENDED_NODEIDS=1)
endif()

if(MUC_OPCUA_COMPLEX_TYPES)
    target_sources(muc_opcua PRIVATE
        address_space/complex_types.c
        encoding/binary_complex.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_COMPLEX_TYPES=1)
endif()

if(MUC_OPCUA_AUDITING)
    target_sources(muc_opcua PRIVATE services/audit_events.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_AUDITING=1)
endif()

if(MUC_OPCUA_AGGREGATE_FULL)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_AGGREGATE_FULL=1)
endif()

if(MUC_OPCUA_CLIENT_NANO)
    target_sources(muc_opcua PRIVATE
        client/client.c
        client/client_secure_channel.c
        client/client_session.c
        client/client_read.c
        client/client_browse.c
        client/client_state.c
        client/client_errors.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CLIENT_NANO=1)
    if(MUC_OPCUA_IS_HOST)
        target_sources(muc_opcua PRIVATE platform/posix_client_transport.c)
    endif()
endif()


foreach(MU_CAPACITY_DEFINE
        MU_MAX_SUBSCRIPTIONS
        MU_MAX_MONITORED_ITEMS
        MU_MAX_PUBLISH_REQUESTS
        MU_MONITORED_QUEUE_DEPTH
        MU_MAX_TRIGGER_LINKS)
    if(DEFINED ${MU_CAPACITY_DEFINE})
        target_compile_definitions(muc_opcua PUBLIC ${MU_CAPACITY_DEFINE}=${${MU_CAPACITY_DEFINE}})
    endif()
endforeach()

# SecurityPolicy Basic256Sha256: the asym/sym chunk crypto and key derivation.
# Excluded entirely from a None-only build to keep the embedded footprint minimal.
# (security_policy.c stays in the core: it only provides shared policy/mode enums.)
if(MUC_OPCUA_SECURITY)
    target_sources(muc_opcua PRIVATE
        security/key_derivation.c
        security/certificate.c
        security/asym_chunk/wrap.c
        security/asym_chunk/unwrap.c
        security/sym_chunk.c
        security/trustlist.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SECURITY=1)
endif()

# The host TCP adapter uses POSIX sockets; only compile it for the host platform.
if(MUC_OPCUA_IS_HOST)
    target_sources(muc_opcua PRIVATE platform/host_tcp_adapter.c)

    # The host crypto adapter (SecurityPolicy Basic256Sha256) is backed by OpenSSL.
    if(MUC_OPCUA_SECURITY)
        find_package(OpenSSL)
        if(OpenSSL_FOUND)
            target_sources(muc_opcua PRIVATE
                platform/host_crypto/common.c
                platform/host_crypto/hash.c
                platform/host_crypto/cipher.c
                platform/host_crypto/asymmetric.c
                platform/host_crypto/sign.c
            )
            target_link_libraries(muc_opcua PUBLIC OpenSSL::Crypto)
            target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_HAVE_OPENSSL=1)
        endif()
    endif()
endif()

if(MUC_OPCUA_HAVE_MBEDTLS)
    target_sources(muc_opcua PRIVATE platform/mbedtls_crypto_adapter.c)
    target_link_libraries(muc_opcua PUBLIC mbedcrypto mbedx509)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_HAVE_MBEDTLS=1)
endif()

if(MUC_OPCUA_HAVE_WOLFSSL)
    target_sources(muc_opcua PRIVATE
        platform/wolfssl_crypto/cipher.c
        platform/wolfssl_crypto/asymmetric.c
        platform/wolfssl_crypto/hash.c
        platform/wolfssl_crypto/sign.c
    )
    target_link_libraries(muc_opcua PUBLIC wolfssl)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_HAVE_WOLFSSL=1)
endif()

target_include_directories(muc_opcua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

muc_opcua_apply_warnings(muc_opcua)
muc_opcua_apply_codegen(muc_opcua)
