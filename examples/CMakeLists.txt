# examples/CMakeLists.txt
# The minimal_server example uses the POSIX host TCP adapter, so it is host-only.
# The Pico example is built from platform/pico/CMakeLists.txt when targeting pico.
if(MICRO_OPCUA_IS_HOST)
    add_executable(minimal_server minimal_server/main.c minimal_server/static_address_space.c)
    target_link_libraries(minimal_server PRIVATE micro_opcua)
if(MICRO_OPCUA_PUBSUB)
    add_executable(pubsub_server pubsub_server/main.c)
    target_link_libraries(pubsub_server PRIVATE micro_opcua)
    if(MICRO_OPCUA_HAVE_OPENSSL)
        target_link_libraries(pubsub_server PRIVATE OpenSSL::Crypto)
    endif()
endif()
endif()
