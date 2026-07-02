# tests/integration/CMakeLists.txt

add_executable(test_minimal_server_flow test_minimal_server_flow.c)
target_link_libraries(test_minimal_server_flow PRIVATE muc_opcua muc_opcua_test_support unity)
add_test(NAME test_minimal_server_flow COMMAND test_minimal_server_flow)

add_executable(test_discovery_endpoint_no_session test_discovery_endpoint_no_session.c)
target_link_libraries(test_discovery_endpoint_no_session PRIVATE muc_opcua muc_opcua_test_support unity)
add_test(NAME test_discovery_endpoint_no_session COMMAND test_discovery_endpoint_no_session)

add_executable(test_single_client_limit test_single_client_limit.c)
target_link_libraries(test_single_client_limit PRIVATE muc_opcua muc_opcua_test_support unity)
add_test(NAME test_single_client_limit COMMAND test_single_client_limit)

add_executable(test_server_handshake test_server_handshake.c)
target_link_libraries(test_server_handshake PRIVATE muc_opcua muc_opcua_test_support unity)
add_test(NAME test_server_handshake COMMAND test_server_handshake)

add_executable(test_response_regression test_response_regression.c)
target_link_libraries(test_response_regression PRIVATE muc_opcua muc_opcua_test_support unity)
add_test(NAME test_response_regression COMMAND test_response_regression)

add_executable(test_server_framing test_server_framing.c)
target_link_libraries(test_server_framing PRIVATE muc_opcua muc_opcua_test_support unity)
add_test(NAME test_server_framing COMMAND test_server_framing)

add_executable(test_server_handshake_secure test_server_handshake_secure.c)
target_link_libraries(test_server_handshake_secure PRIVATE muc_opcua muc_opcua_test_support unity)
target_include_directories(test_server_handshake_secure PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_server_handshake_secure COMMAND test_server_handshake_secure)

add_executable(test_connection_robustness test_connection_robustness.c)
target_link_libraries(test_connection_robustness PRIVATE muc_opcua muc_opcua_test_support unity)
target_include_directories(test_connection_robustness PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_connection_robustness COMMAND test_connection_robustness)

add_executable(test_view_services test_view_services.c)
target_link_libraries(test_view_services PRIVATE muc_opcua muc_opcua_test_support unity)
target_include_directories(test_view_services PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_view_services COMMAND test_view_services)

if(MUC_OPCUA_SUBSCRIPTIONS)
    add_executable(test_subscriptions test_subscriptions.c)
    target_link_libraries(test_subscriptions PRIVATE muc_opcua muc_opcua_test_support unity)
    target_include_directories(test_subscriptions PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_subscriptions COMMAND test_subscriptions)
endif()

if(MUC_OPCUA_USER_AUTH)
    add_executable(test_user_auth_plaintext test_user_auth_plaintext.c)
    target_link_libraries(test_user_auth_plaintext PRIVATE muc_opcua muc_opcua_test_support unity)
    target_include_directories(test_user_auth_plaintext PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_user_auth_plaintext COMMAND test_user_auth_plaintext)

    add_executable(test_user_auth_encrypted test_user_auth_encrypted.c)
    target_link_libraries(test_user_auth_encrypted PRIVATE muc_opcua muc_opcua_test_support unity)
    target_include_directories(test_user_auth_encrypted PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_user_auth_encrypted COMMAND test_user_auth_encrypted)
endif()

if(MUC_OPCUA_USER_AUTH AND MUC_OPCUA_SECURITY)
    # Certificate user tokens need both: MUC_OPCUA_USER_AUTH for ActivateSession's
    # username/certificate token dispatch, and MUC_OPCUA_SECURITY for
    # mu_binary_read_certificate_identity_token (encoding/binary_reader.c is
    # #ifdef MUC_OPCUA_SECURITY for that decoder, a separate condition from the
    # username-token decoder above which only needs MUC_OPCUA_USER_AUTH). The
    # Micro profile enables USER_AUTH but not SECURITY, so this test does not
    # apply there.
    add_executable(test_user_auth_certificate test_user_auth_certificate.c)
    target_link_libraries(test_user_auth_certificate PRIVATE muc_opcua muc_opcua_test_support unity)
    target_include_directories(test_user_auth_certificate PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_user_auth_certificate COMMAND test_user_auth_certificate)

    add_executable(test_user_auth_secure_e2e test_user_auth_secure_e2e.c)
    target_link_libraries(test_user_auth_secure_e2e PRIVATE muc_opcua muc_opcua_test_support unity)
    target_include_directories(test_user_auth_secure_e2e PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_user_auth_secure_e2e COMMAND test_user_auth_secure_e2e)
endif()

if(MUC_OPCUA_SERVICE_WRITE)
    add_executable(test_write_service_integration test_write_service.c)
    target_link_libraries(test_write_service_integration PRIVATE muc_opcua muc_opcua_test_support unity)
    target_include_directories(test_write_service_integration PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_write_service_integration COMMAND test_write_service_integration)
endif()

add_executable(test_secure_handshake_modern test_secure_handshake_modern.c)
target_link_libraries(test_secure_handshake_modern PRIVATE muc_opcua muc_opcua_test_support unity)
target_include_directories(test_secure_handshake_modern PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_secure_handshake_modern COMMAND test_secure_handshake_modern)

if(MUC_OPCUA_EVENTS)
    add_executable(test_event_notifications test_event_notifications.c)
    target_link_libraries(test_event_notifications PRIVATE muc_opcua muc_opcua_test_support unity)
    target_include_directories(test_event_notifications PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME test_event_notifications COMMAND test_event_notifications)
endif()

