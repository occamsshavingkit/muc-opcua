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
    security/secure_util.c
    generated/opcua_ids.c
)

# Make src/ the include root so moved CU sources can resolve headers via
# src-rooted paths (e.g. "core/server/common.h") regardless of directory depth.
target_include_directories(muc_opcua PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MUC_OPCUA_ALLOW_HEAP)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_ALLOW_HEAP=1)
else()
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_ALLOW_HEAP=0)
endif()

# =============================================================================
# Per-Conformance-Unit (CU) source gating.
# One if() block per CU/facet symbol. Each block:
#   1. file(GLOB[_RECURSE]) on the CU directory under src/cu/<facet>/<cu>/
#   2. Adds the sources to muc_opcua
#   3. Emits the CU/facet compile definition
# Legacy symbol aliases (MUC_OPCUA_<OLD_NAME>=1) are emitted alongside CU
# defines where include/ or tests/ still reference them. SERVICE_* bridge
# defines that were unreferenced outside src/CMakeLists.txt have been removed.
# =============================================================================

# -- CU: Attribute Read (facet: Core 2022 Server) --
if(MUC_OPCUA_CU_ATTRIBUTE_READ)
    file(GLOB _sources cu/core_2022_server/attribute_read/*.c)
    list(REMOVE_ITEM _sources
        ${CMAKE_CURRENT_SOURCE_DIR}/cu/core_2022_server/attribute_read/cache.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ATTRIBUTE_READ=1)
endif()

# Opt-in Read value cache (maxAge optimization, OPC-10000-4 5.11.2). Compiled
# only when explicitly enabled because its maxAge handling is the only
# floating-point in the Read path.
if(MUC_OPCUA_READ_CACHE)
    target_sources(muc_opcua PRIVATE cu/core_2022_server/attribute_read/cache.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_READ_CACHE=1)
endif()

# -- CU: View Basic/BrowseNext and TranslateBrowsePath (facet: Core 2022 Server) --
if(MUC_OPCUA_CU_VIEW_BASIC_2 OR MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH)
    target_sources(muc_opcua PRIVATE
        cu/core_2022_server/view_basic_translate/browse.c
        cu/core_2022_server/view_basic_translate/view_handler.c)

    if(MUC_OPCUA_CU_VIEW_BASIC_2)
        target_sources(muc_opcua PRIVATE
            cu/core_2022_server/view_basic_translate/browse_next.c)
    endif()

    if(MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH)
        target_sources(muc_opcua PRIVATE
            cu/core_2022_server/view_basic_translate/translate_paths.c)
    endif()
endif()

if(MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_VIEW_BASIC_TRANSLATEBROWSEPATH=1)
endif()

if(MUC_OPCUA_CU_VIEW_BASIC_2)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_VIEW_BASIC_2=1)
endif()

if(MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_VIEW_TRANSLATEBROWSEPATH=1)
endif()

# -- CU: Discovery FindServers-Self-GetEndpoints compatibility gate / dedicated Discovery CUs --
set(MUC_OPCUA_DISCOVERY_FIND_SERVERS_ENABLED OFF)
if(MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF)
    set(MUC_OPCUA_DISCOVERY_FIND_SERVERS_ENABLED ON)
endif()

if(MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF_GET_ENDPOINTS=1)
endif()

if(MUC_OPCUA_DISCOVERY_FIND_SERVERS_ENABLED OR MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS)
    target_sources(muc_opcua PRIVATE cu/core_2022_server/discovery/common.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_DISCOVERY_SHARED=1)
endif()

if(MUC_OPCUA_DISCOVERY_FIND_SERVERS_ENABLED)
    target_sources(muc_opcua PRIVATE
        cu/core_2022_server/discovery/find_servers.c
        cu/core_2022_server/discovery/register_server.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_DISCOVERY_FIND_SERVERS_ENABLED=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_DISCOVERY_FIND_SERVERS_SELF=1)
endif()

if(MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS)
    target_sources(muc_opcua PRIVATE cu/core_2022_server/discovery/get_endpoints.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_DISCOVERY_GET_ENDPOINTS=1)
endif()

# -- CU: View RegisterNodes (define-only, no source files yet) --
if(MUC_OPCUA_CU_VIEW_REGISTERNODES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_VIEW_REGISTERNODES=1)
endif()

# -- CU: Attribute Write Values (OPC-10000-4 5.11.4) --
# MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE remains a compatibility aggregate for
# full-profile builds; custom builds can enable the dedicated Write Value CU
# independently with MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES.
if(MUC_OPCUA_WRITE_VALUE_ENABLED)
    file(GLOB _sources cu/core_2022_server/attribute_write/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_WRITE=1)
endif()
if(MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES=1)
endif()
if(MUC_OPCUA_CU_SESSION_CHANGE_USER)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_SESSION_CHANGE_USER=1)
endif()
if(MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_TIMESTAMP=1)
endif()
if(MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ATTRIBUTE_WRITE_INDEX_RANGE=1)
endif()
if(MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_CORE_2017_ATTRIBUTE_WRITE=1)
endif()

# Read/Write attribute dispatch handler. attribute_handler.c carries both sets
# internally (#ifdef MUC_OPCUA_CU_ATTRIBUTE_READ / MUC_OPCUA_SERVICE_WRITE),
# so compile it whenever either CU is enabled. File is shared infrastructure
# and remains at src/core/service_dispatch/.
if(MUC_OPCUA_CU_ATTRIBUTE_READ OR MUC_OPCUA_WRITE_VALUE_ENABLED)
    target_sources(muc_opcua PRIVATE core/service_dispatch/attribute_handler.c)
endif()

# -- CU: NodeManagement (facet: Core 2022 Server) --
if(MUC_OPCUA_CU_NODEMANAGEMENT)
    file(GLOB _sources cu/core_2022_server/nodemanagement/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_NODEMANAGEMENT=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_NODEMANAGEMENT=1)
endif()

# -- CU: Query (facet: Core 2022 Server) --
if(MUC_OPCUA_CU_QUERY)
    file(GLOB _sources cu/core_2022_server/query/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_QUERY=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_QUERY=1)
endif()

# -- Facet: Core 2022 Server (supersedes legacy BASE_NODES) --
# base_nodes.c is always compiled (owns mu_resolve_node); this flag gates the
# node-table *content* so the standard node set is dropped.
if(MUC_OPCUA_FACET_CORE_2022_SERVER)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_FACET_CORE_2022_SERVER=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_BASE_NODES=1)
endif()

# -- Facet: Exposes Type System Server (supersedes legacy BASE_TYPE_SYSTEM) --
if(MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_BASE_TYPE_SYSTEM=1)
endif()

# -- Define-only nano/type-system CUs backed by base_nodes.c / public flags --
if(MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ADDRESS_SPACE_ADDIN_REFERENCE=1)
endif()
if(MUC_OPCUA_CU_BASE_INFO_LOCALTIME)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_INFO_LOCALTIME=1)
endif()
if(MUC_OPCUA_CU_BASE_INFO_DATATYPES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_INFO_DATATYPES=1)
endif()
if(MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_INFO_ARGUMENT_TYPE=1)
endif()
if(MUC_OPCUA_CU_BASE_INFO_BASE_TYPES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_INFO_BASE_TYPES=1)
endif()
if(MUC_OPCUA_CU_BASE_INFO_SERVERTYPE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_INFO_SERVERTYPE=1)
endif()
if(MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION=1)
endif()
if(MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ADDRESS_SPACE_ATOMICITY=1)
endif()
if(MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_ADDRESS_SPACE_NONVOLATILE_CONSTANT=1)
endif()

# -- CU: Subscription Basic (Micro profile data-change engine) --
# Sources live under src/cu/core_2022_server/subscription/basic/; pubsub/uadp
# files at the same level are excluded here and gated separately by CU_PUBSUB.
if(MUC_OPCUA_CU_SUBSCRIPTION_BASIC)
    file(GLOB _sources cu/core_2022_server/subscription/basic/*.c)
    list(REMOVE_ITEM _sources
        ${CMAKE_CURRENT_SOURCE_DIR}/cu/core_2022_server/subscription/basic/pubsub.c
        ${CMAKE_CURRENT_SOURCE_DIR}/cu/core_2022_server/subscription/basic/uadp_encoder.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_SUBSCRIPTION_BASIC=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SUBSCRIPTIONS=1)

    # -- CU: Subscription Standard (aggregates, triggering) --
    # dispatch_method.c requires the triple condition (Subscriptions + Standard
    # + BaseInfo type system) and is excluded from the STANDARD glob to preserve
    # the original MU_DISPATCH_CALL_ENABLED gating; see the dedicated block below.
    if(MUC_OPCUA_CU_SUBSCRIPTION_STANDARD)
        file(GLOB _sources cu/core_2022_server/subscription/standard/*.c)
        list(REMOVE_ITEM _sources
            ${CMAKE_CURRENT_SOURCE_DIR}/cu/core_2022_server/subscription/standard/dispatch_method.c)
        target_sources(muc_opcua PRIVATE ${_sources})
        target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_SUBSCRIPTION_STANDARD=1)
        target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SUBSCRIPTIONS_STANDARD=1)
    endif()
endif()

# Call service dispatch (OPC-10000-4 5.12.2.2). The Call handler is gated by
# the same compound condition as the original MU_DISPATCH_CALL_ENABLED macro
# in service_dispatch.c: Subscriptions + STANDARD aggregates + BaseInfo
# type system. The file also wraps itself in the matching #if internally.
if(MUC_OPCUA_CU_SUBSCRIPTION_BASIC AND MUC_OPCUA_CU_SUBSCRIPTION_STANDARD AND MUC_OPCUA_FACET_EXPOSES_TYPE_SYSTEM_SERVER)
    target_sources(muc_opcua PRIVATE cu/core_2022_server/subscription/standard/dispatch_method.c)
endif()

# -- CU: PubSub (facet: Core 2022 Server) --
# pubsub.c + uadp_encoder.c live alongside the Subscription Basic sources but
# are independently gated by MUC_OPCUA_CU_PUBSUB.
if(MUC_OPCUA_CU_PUBSUB)
    target_sources(muc_opcua PRIVATE
        cu/core_2022_server/subscription/basic/pubsub.c
        cu/core_2022_server/subscription/basic/uadp_encoder.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_PUBSUB=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_PUBSUB=1)
    if(MUC_OPCUA_IS_HOST)
        target_sources(muc_opcua PRIVATE platform/host_udp_adapter.c)
    endif()
endif()

# -- CU: Historical Access Server Facet --
if(MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET)
    file(GLOB _sources cu/core_2022_server/historical_access/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVICE_HISTORY=1)
endif()

# -- Define-only CU flags (gated entirely on CU symbols; legacy aliases kept
# for include/ and tests/ backward compatibility) --
if(MUC_OPCUA_CU_USER_AUTH)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_USER_AUTH=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_USER_AUTH=1)
endif()

# -- CU: User Token JWT (spec 093, OPC-10000-7 CU 1697) --
# Freestanding JWT/JWS validator sources are gated on MUC_OPCUA_HAVE_OPENSSL
# (set in the OpenSSL detection block further below) because the crypto
# verification wrapper currently only ships an OpenSSL backend. The actual
# target_sources/compile-definition block lives below the OpenSSL/mbedTLS/
# wolfSSL detection so the CMake variable is defined by the time it runs.
# Non-OpenSSL backends will be enabled by relaxing that condition once
# crypto_jwt.c grows the corresponding dispatch.

# -- CU: Authorization Service Server (spec 093, OPC-10000-7 CU 1629) --
# Define-only for now: drives AuthorizationServiceConfigurationType
# address-space nodes (added in a later task).
if(MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER=1)
endif()
if(MUC_OPCUA_CU_MULTIPLE_CONNECTIONS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_MULTIPLE_CONNECTIONS=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_MULTIPLE_CONNECTIONS=1)
endif()
if(MUC_OPCUA_CU_EVENTS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_EVENTS=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_EVENTS=1)
endif()
if(MUC_OPCUA_CU_DATA_ACCESS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_DATA_ACCESS=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_DATA_ACCESS=1)
endif()
if(MUC_OPCUA_CU_METHOD_SERVER)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_METHOD_SERVER=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_METHOD_SERVER=1)
endif()

# -- CU: KeyCredential Service (spec 094, OPC-10000-7 CU 2113) --
# Depends on METHOD_SERVER for method dispatch. The 4 Method handlers each
# check server->config.key_credential_adapter and return Bad_NotSupported
# when no adapter is configured.
if(MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE)
    target_sources(muc_opcua PRIVATE cu/core_2022_server/key_credential/key_credential.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE=1)
endif()

# -- CU: User Role Management (spec 095, OPC-10000-7 CU 2080) --
# Depends on METHOD_SERVER for method dispatch.
if(MUC_OPCUA_CU_USER_ROLE_MANAGEMENT)
    target_sources(muc_opcua PRIVATE cu/core_2022_server/role_management/role_management.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_USER_ROLE_MANAGEMENT=1)
endif()

# -- CU: Certificate Management (spec 096, OPC-10000-7 CU 2105) --
# Type-system only: no Methods in this iteration. The ObjectType nodes
# live entirely in base_nodes.c behind #ifdef. No separate .c file.
if(MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT=1)
endif()

if(MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL=1)
    file(GLOB _sources cu/core_2022_server/certificate_manager/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
endif()

# -- CU: Event Filter Where --
if(MUC_OPCUA_CU_EVENT_FILTER_WHERE)
    file(GLOB _sources cu/core_2022_server/event_filter_where/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_EVENT_FILTER_WHERE=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_EVENT_FILTER_WHERE=1)
endif()

if(MUC_OPCUA_CU_CUSTOM_METHODS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_CUSTOM_METHODS=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CUSTOM_METHODS=1)
endif()

# -- CU: Redundancy (transfer subscriptions, OPC-10000-4 5.14) --
if(MUC_OPCUA_CU_REDUNDANCY)
    file(GLOB _sources cu/core_2022_server/redundancy/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_REDUNDANCY=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_REDUNDANCY=1)
endif()

if(MUC_OPCUA_CU_NAMESPACES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_NAMESPACES=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_NAMESPACES=1)
endif()

if(MUC_OPCUA_MARKER_STANDARD_PROFILE)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_MARKER_STANDARD_PROFILE=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_STANDARD_PROFILE=1)
endif()

if(MUC_OPCUA_CU_REVERSE_CONNECT)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_REVERSE_CONNECT=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_REVERSE_CONNECT=1)
endif()

# -- CU 3192: Base Info Diagnostics --
if(MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS)
    file(GLOB _sources cu/core_2022_server/diagnostics/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_INFO_DIAGNOSTICS=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SERVER_DIAGNOSTICS=1)
endif()

# -- CU 3983: Base Services Diagnostics --
if(MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_BASE_SERVICES_DIAGNOSTICS=1)
endif()

if(MUC_OPCUA_CU_DYNAMIC_NODES)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_DYNAMIC_NODES=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_DYNAMIC_NODES=1)
endif()
if(MUC_OPCUA_CU_MULTI_CHUNK)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_MULTI_CHUNK=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_MULTI_CHUNK=1)
endif()
if(MUC_OPCUA_CU_SESSION_TIMEOUT)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_SESSION_TIMEOUT=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SESSION_TIMEOUT=1)
endif()
if(MUC_OPCUA_CU_TIME_SYNC)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_TIME_SYNC=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_TIME_SYNC=1)
endif()
if(MUC_OPCUA_CU_EXTENDED_NODEIDS)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_EXTENDED_NODEIDS=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_EXTENDED_NODEIDS=1)
endif()

# -- CU: Complex Types (address_space + encoding binary_complex) --
if(MUC_OPCUA_CU_COMPLEX_TYPES)
    file(GLOB _sources cu/core_2022_server/complex_types/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_COMPLEX_TYPES=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_COMPLEX_TYPES=1)
endif()

# -- CU: Auditing (server-side audit event generation) --
if(MUC_OPCUA_CU_AUDITING)
    file(GLOB _sources cu/core_2022_server/auditing/*.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_AUDITING=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_AUDITING=1)
endif()

if(MUC_OPCUA_CU_AGGREGATE_FULL)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_AGGREGATE_FULL=1)
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

if(MUC_OPCUA_CLIENT_STANDARD)
    target_sources(muc_opcua PRIVATE
        client/client_subscription.c
        client/client_monitored_item.c
        client/client_publish.c
        client/client_write.c
        client/client_call.c
    )
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CLIENT_STANDARD=1)
endif()


# Capacities are resolved by Kconfig (int symbols in /Kconfig, sourced from the
# manifest). gen_config.py writes resolved values to muc_opcua_config.cmake as
# set(MUC_OPCUA_<SYM> <N>), which CMakeLists.txt includes before add_subdirectory(src).
# Here we emit the public MU_MAX_* (and MU_MONITORED_*, MU_WHERE_*) compile
# definitions that capacities.h stage 3 consumes, so any capacity change -- whether
# via -DMU_MAX_SESSIONS=N (translated to the Kconfig fragment), menuconfig, or a
# custom .config -- reaches the compiler through one path: Kconfig resolution.
#
# Legacy -DMU_MAX_*=N still works: CMakeLists.txt translates it to the Kconfig
# fragment, Kconfig resolves it, and the resolved value becomes the compile definition.
if(NOT MUC_OPCUA_PROFILE STREQUAL "custom")
    string(TOUPPER "${MUC_OPCUA_PROFILE}" MU_PROFILE_MARKER_SUFFIX)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_PROFILE_${MU_PROFILE_MARKER_SUFFIX}=1)
endif()

# Emit resolved Kconfig capacity values as the public override macros consumed by
# capacities.h stage 3 (#ifdef MU_MAX_SESSIONS -> #define MU_INTERN_* MU_MAX_*).
foreach(_pair IN LISTS MUC_OPCUA_KCONFIG_CAPACITIES)
    string(REPLACE "=" ";" _kv "${_pair}")
    list(GET _kv 0 _kcfg_sym)
    list(GET _kv 1 _legacy_var)
    if(DEFINED MUC_OPCUA_${_kcfg_sym})
        target_compile_definitions(muc_opcua PUBLIC ${_legacy_var}=${MUC_OPCUA_${_kcfg_sym}})
    endif()
endforeach()

# -- Facet: Core 2022 Server security (Basic256Sha256 chunk crypto + key derivation) --
# Security code is part of the Core 2022 Server Facet. security_policy.c stays
# in the always-compiled core (shared policy/mode enums only). ECC
# (security/ecc.c) is droppable and gated separately by CU_SECURITY_ECC below;
# excluded from the FACET glob so non-ECC profiles pay no .text.
if(MUC_OPCUA_FACET_CORE_2022_SERVER)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_FACET_CORE_2022_SERVER=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SECURITY=1)
endif()

# -- Secure-channel message crypto (spec 072) --
# Decoupled from the facet umbrella so the SecurityPolicy-None-only nano profile
# does not compile the symmetric/asymmetric chunk crypto, certificate, trustlist,
# or key-derivation translation units. security_policy.c (policy/mode enums used by
# the None path AND the non-None rejection) stays in the always-compiled core list
# above; ecc.c is gated separately below (and depends on this gate via Kconfig).
if(MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
    file(GLOB_RECURSE _sources cu/core_2022_server/security/*.c)
    list(REMOVE_ITEM _sources
        ${CMAKE_CURRENT_SOURCE_DIR}/cu/core_2022_server/security/ecc.c)
    target_sources(muc_opcua PRIVATE ${_sources})
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_SECURE_CHANNEL_CRYPTO=1)
endif()

# -- CU: Security ECC (spec 059) --
# Droppable translation unit: absent unless MUC_OPCUA_CU_SECURITY_ECC is on.
# Backend-specific ECC implementations are added inside each HAVE_* block below.
if(MUC_OPCUA_CU_SECURITY_ECC)
    target_sources(muc_opcua PRIVATE cu/core_2022_server/security/ecc.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_SECURITY_ECC=1)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_ECC=1)
endif()

# The host TCP adapter uses POSIX sockets; only compile it for the host platform.
if(MUC_OPCUA_IS_HOST)
    target_sources(muc_opcua PRIVATE platform/host_tcp_adapter.c)
    if(MUC_OPCUA_MDNS_DISCOVERY)
        target_sources(muc_opcua PRIVATE platform/posix_mdns.c)
        target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_MDNS_DISCOVERY=1)
    endif()

    # The host crypto adapter (SecurityPolicy Basic256Sha256) is backed by OpenSSL.
    if(MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
        find_package(OpenSSL)
        if(OpenSSL_FOUND)
            set(MUC_OPCUA_HAVE_OPENSSL ON CACHE INTERNAL "OpenSSL backend available")
            target_sources(muc_opcua PRIVATE
                platform/host_crypto/common.c
                platform/host_crypto/hash.c
                platform/host_crypto/cipher.c
                platform/host_crypto/asymmetric.c
                platform/host_crypto/sign.c
            )
            if(MUC_OPCUA_CU_SECURITY_ECC)
                target_sources(muc_opcua PRIVATE platform/host_crypto/ecc.c)
            endif()
            target_link_libraries(muc_opcua PUBLIC OpenSSL::Crypto)
            target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_HAVE_OPENSSL=1)
        endif()
    endif()
endif()

if(MUC_OPCUA_HAVE_MBEDTLS AND MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
    target_sources(muc_opcua PRIVATE platform/mbedtls_crypto_adapter.c)
    target_link_libraries(muc_opcua PUBLIC mbedcrypto mbedx509)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_HAVE_MBEDTLS=1)
endif()

if(MUC_OPCUA_HAVE_WOLFSSL AND MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
    target_sources(muc_opcua PRIVATE
        platform/wolfssl_crypto/cipher.c
        platform/wolfssl_crypto/asymmetric.c
        platform/wolfssl_crypto/hash.c
        platform/wolfssl_crypto/sign.c
    )
    if(MUC_OPCUA_CU_SECURITY_ECC)
        target_sources(muc_opcua PRIVATE platform/wolfssl_crypto/ecc.c)
    endif()
    target_link_libraries(muc_opcua PUBLIC wolfssl)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_HAVE_WOLFSSL=1)
endif()

# -- CU: User Token JWT (spec 093, OPC-10000-7 CU 1697) --
# Freestanding JWT/JWS validator (Base64url decoder, JSON claim scanner,
# RSA/ECDSA signature verification wrapper). Gated on USER_AUTH because the
# JWT token type is dispatched from the existing user-token path.
#
# The signature verification wrapper (crypto_jwt.c) currently only has an
# OpenSSL backend, so the whole CU is additionally gated on
# MUC_OPCUA_HAVE_OPENSSL to avoid shipping an unusable validator on
# mbedTLS/wolfSSL-only builds. The OpenSSL detection block above sets
# MUC_OPCUA_HAVE_OPENSSL only when host + SECURE_CHANNEL_CRYPTO + find_package
# succeed; relax this condition once crypto_jwt.c grows mbedTLS/wolfSSL
# dispatch.
if(MUC_OPCUA_CU_USER_TOKEN_JWT AND MUC_OPCUA_HAVE_OPENSSL)
    target_sources(muc_opcua PRIVATE
        cu/core_2022_server/authorization/base64url.c
        cu/core_2022_server/authorization/claim_scanner.c
        cu/core_2022_server/authorization/jwt.c
        cu/core_2022_server/authorization/crypto_jwt.c)
    target_compile_definitions(muc_opcua PUBLIC MUC_OPCUA_CU_USER_TOKEN_JWT=1)
endif()

target_include_directories(muc_opcua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

muc_opcua_apply_warnings(muc_opcua)
muc_opcua_apply_codegen(muc_opcua)
